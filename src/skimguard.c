/*
 * SkimGuard — passive 13.56 MHz NFC reader (skimmer) detector for Flipper Zero.
 *
 * Receive-only: senses the RF field an active reader can't help but emit and
 * turns it into an EMF-style meter with accelerating geiger clicks, a room
 * verdict, an A/B (clean vs compromised) booth mode, and a watch/alert mode.
 * It NEVER transmits.
 *
 * Author / sole contributor: Krishita Sanjay Choksi.
 * SPDX-License-Identifier: MIT
 */
#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <furi_hal_speaker.h>
#include <math.h>
#include <stdio.h>

#include "skimguard_config.h"
#include "detection.h"
#include "field_sensor.h"

typedef enum {
    ModeSweep = 0, /* free-hand hunt: meter + clicks */
    ModeAB, /* guided clean-vs-compromised */
    ModeWatch, /* armed sentry: alert if a reader appears */
    ModeCount,
} SgMode;

typedef enum {
    AbStepClean = 0, /* sweep the known-clean terminal first */
    AbStepCompromised, /* now sweep the suspect one */
} AbStep;

typedef struct {
    FuriMutex* mutex;
    FuriMessageQueue* input_queue;
    ViewPort* view_port;
    Gui* gui;
    FuriTimer* timer;

    Detector detector;
    DetectionResult result;

    SgMode mode;
    AbStep ab_step;
    bool ab_clean_seen_quiet;

    bool sound_on;
    bool speaker_held;
    float click_accum_ms;
    int click_hold_ticks; /* >0 while a click tone is sounding */

    bool watch_armed;
    bool watch_alerted;

    volatile bool running;
} SkimGuard;

/* ------------------------------------------------------------------ */
/* Sound                                                               */
/* ------------------------------------------------------------------ */
static void sound_acquire(SkimGuard* sg) {
    if(sg->sound_on && !sg->speaker_held) {
        if(furi_hal_speaker_acquire(30)) {
            sg->speaker_held = true;
        }
    }
}

static void sound_release(SkimGuard* sg) {
    if(sg->speaker_held) {
        furi_hal_speaker_stop();
        furi_hal_speaker_release();
        sg->speaker_held = false;
    }
}

static void click_start(SkimGuard* sg, float proximity_pct) {
    if(!sg->speaker_held) return;
    /* Higher pitch as you close in — the geiger effect. */
    float freq = 1800.0f + 2600.0f * (proximity_pct / 100.0f);
    furi_hal_speaker_start(freq, 0.5f);
    sg->click_hold_ticks = 1; /* one sample period */
}

static void click_stop(SkimGuard* sg) {
    if(sg->speaker_held) furi_hal_speaker_stop();
    sg->click_hold_ticks = 0;
}

/* ------------------------------------------------------------------ */
/* Sampling timer                                                      */
/* ------------------------------------------------------------------ */
static void sample_cb(void* ctx) {
    SkimGuard* sg = ctx;
    uint16_t raw = field_sensor_read();

    furi_mutex_acquire(sg->mutex, FuriWaitForever);

    DetectionResult r;
    detector_push(&sg->detector, (float)raw, &r);
    sg->result = r;

    /* Watch mode: latch an alert the first time a reader appears. */
    if(sg->mode == ModeWatch && sg->watch_armed && r.present) {
        sg->watch_alerted = true;
    }

    /* A/B mode: remember that the clean terminal read quiet. */
    if(sg->mode == ModeAB && sg->ab_step == AbStepClean && !r.present) {
        sg->ab_clean_seen_quiet = true;
    }

    /* Click cadence. */
    if(sg->click_hold_ticks > 0) {
        sg->click_hold_ticks--;
        if(sg->click_hold_ticks == 0) click_stop(sg);
    }
    bool clicky = sg->sound_on && r.present && isfinite(r.click_ms);
    /* In watch mode we chirp only when alerted; otherwise it would be a
     * silent sentry. */
    if(sg->mode == ModeWatch) clicky = clicky && sg->watch_alerted;
    if(clicky) {
        sg->click_accum_ms += (float)SG_SAMPLE_PERIOD_MS;
        if(sg->click_accum_ms >= r.click_ms) {
            sg->click_accum_ms = 0.0f;
            click_start(sg, r.proximity_pct);
        }
    } else {
        sg->click_accum_ms = 0.0f;
    }

    furi_mutex_release(sg->mutex);
    view_port_update(sg->view_port);
}

/* ------------------------------------------------------------------ */
/* Rendering                                                           */
/* ------------------------------------------------------------------ */
static const char* mode_name(SgMode m) {
    switch(m) {
    case ModeSweep:
        return "Sweep";
    case ModeAB:
        return "A/B";
    case ModeWatch:
        return "Watch";
    default:
        return "?";
    }
}

static void draw_meter(Canvas* c, int x, int y, int w, int h, float pct) {
    canvas_draw_frame(c, x, y, w, h);
    int fill = (int)((w - 2) * (pct / 100.0f));
    if(fill > 0) canvas_draw_box(c, x + 1, y + 1, fill, h - 2);
}

static void draw_sweep(Canvas* c, const DetectionResult* r) {
    char line[40];
    canvas_set_font(c, FontSecondary);

    if(!r->present) {
        canvas_draw_str(c, 4, 26, "CLEAR - no reader field");
    } else {
        const char* tr = r->trend == SgTrendWarmer ? "warmer >>>" :
                         r->trend == SgTrendColder ? "<<< colder" :
                                                     "holding";
        snprintf(line, sizeof(line), "READER  %s", tr);
        canvas_set_font(c, FontPrimary);
        canvas_draw_str(c, 4, 24, "READER NEAR");
        canvas_set_font(c, FontSecondary);
        canvas_draw_str(c, 4, 35, tr);
    }

    draw_meter(c, 4, 40, 120, 12, r->proximity_pct);

    snprintf(line, sizeof(line), "%d%%", (int)(r->proximity_pct + 0.5f));
    canvas_draw_str_aligned(c, 124, 62, AlignRight, AlignBottom, line);

    const char* ch = r->characterization == SgCharSteady        ? "steady" :
                     r->characterization == SgCharIntermittent ? "intermittent" :
                                                                  "";
    if(ch[0]) canvas_draw_str(c, 4, 62, ch);
}

static void draw_ab(Canvas* c, const SkimGuard* sg, const DetectionResult* r) {
    canvas_set_font(c, FontSecondary);
    if(sg->ab_step == AbStepClean) {
        canvas_draw_str(c, 4, 24, "Step 1: sweep CLEAN box");
        canvas_draw_str(c, 4, 35, r->present ? "hmm - field here?!" : "quiet - good baseline");
    } else {
        canvas_draw_str(c, 4, 24, "Step 2: sweep SUSPECT box");
        canvas_draw_str(c, 4, 35, r->present ? "READER FOUND" : "sweeping...");
    }
    draw_meter(c, 4, 40, 120, 12, r->proximity_pct);
    canvas_draw_str(c, 4, 62, "OK: next step");
}

static void draw_watch(Canvas* c, const SkimGuard* sg, const DetectionResult* r) {
    canvas_set_font(c, FontPrimary);
    if(sg->watch_alerted) {
        canvas_draw_str(c, 4, 24, "! READER DETECTED");
        canvas_set_font(c, FontSecondary);
        canvas_draw_str(c, 4, 36, "a reader appeared while armed");
    } else if(sg->watch_armed) {
        canvas_draw_str(c, 4, 24, "ARMED - watching");
        canvas_set_font(c, FontSecondary);
        canvas_draw_str(c, 4, 36, "silent until a reader shows");
    } else {
        canvas_draw_str(c, 4, 24, "Watch idle");
        canvas_set_font(c, FontSecondary);
        canvas_draw_str(c, 4, 36, "OK: arm sentry");
    }
    draw_meter(c, 4, 44, 120, 10, r->proximity_pct);
    (void)r;
}

static void draw_cb(Canvas* canvas, void* ctx) {
    SkimGuard* sg = ctx;
    furi_mutex_acquire(sg->mutex, FuriWaitForever);
    DetectionResult r = sg->result;
    SgMode mode = sg->mode;
    bool sound = sg->sound_on;
    furi_mutex_release(sg->mutex);

    canvas_clear(canvas);

    /* Header */
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 11, "SkimGuard");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 126, 10, AlignRight, AlignBottom, mode_name(mode));
    if(sound) canvas_draw_str(canvas, 82, 10, "snd");
    canvas_draw_line(canvas, 0, 13, 128, 13);

    if(field_sensor_is_simulated()) {
        canvas_draw_str(canvas, 2, 62, "SIM");
    }

    switch(mode) {
    case ModeSweep:
        draw_sweep(canvas, &r);
        break;
    case ModeAB:
        draw_ab(canvas, sg, &r);
        break;
    case ModeWatch:
        draw_watch(canvas, sg, &r);
        break;
    default:
        break;
    }
}

/* ------------------------------------------------------------------ */
/* Input                                                               */
/* ------------------------------------------------------------------ */
static void input_cb(InputEvent* event, void* ctx) {
    SkimGuard* sg = ctx;
    furi_message_queue_put(sg->input_queue, event, FuriWaitForever);
}

static void recalibrate(SkimGuard* sg) {
    detector_init(&sg->detector);
    sg->watch_alerted = false;
    sg->ab_clean_seen_quiet = false;
}

static void handle_input(SkimGuard* sg, const InputEvent* e) {
    if(e->type != InputTypeShort && e->type != InputTypeLong) return;

    furi_mutex_acquire(sg->mutex, FuriWaitForever);
    switch(e->key) {
    case InputKeyBack:
        sg->running = false;
        break;
    case InputKeyOk:
        if(sg->mode == ModeAB) {
            sg->ab_step = (sg->ab_step == AbStepClean) ? AbStepCompromised : AbStepClean;
            sg->click_accum_ms = 0.0f;
        } else if(sg->mode == ModeWatch) {
            sg->watch_armed = !sg->watch_armed;
            sg->watch_alerted = false;
        } else {
            recalibrate(sg); /* fresh baseline for this spot */
        }
        break;
    case InputKeyLeft:
        sg->mode = (SgMode)((sg->mode + ModeCount - 1) % ModeCount);
        sg->click_accum_ms = 0.0f;
        break;
    case InputKeyRight:
        sg->mode = (SgMode)((sg->mode + 1) % ModeCount);
        sg->click_accum_ms = 0.0f;
        break;
    case InputKeyUp:
    case InputKeyDown:
        sg->sound_on = !sg->sound_on;
        if(sg->sound_on)
            sound_acquire(sg);
        else
            sound_release(sg);
        break;
    default:
        break;
    }
    furi_mutex_release(sg->mutex);
}

/* ------------------------------------------------------------------ */
/* App lifecycle                                                       */
/* ------------------------------------------------------------------ */
static SkimGuard* skimguard_alloc(void) {
    SkimGuard* sg = malloc(sizeof(SkimGuard));
    memset(sg, 0, sizeof(*sg));

    sg->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    sg->input_queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    sg->mode = ModeSweep;
    sg->sound_on = true;
    sg->running = true;

    detector_init(&sg->detector);

    sg->view_port = view_port_alloc();
    view_port_draw_callback_set(sg->view_port, draw_cb, sg);
    view_port_input_callback_set(sg->view_port, input_cb, sg);

    sg->gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(sg->gui, sg->view_port, GuiLayerFullscreen);

    sg->timer = furi_timer_alloc(sample_cb, FuriTimerTypePeriodic, sg);
    return sg;
}

static void skimguard_free(SkimGuard* sg) {
    furi_timer_stop(sg->timer);
    furi_timer_free(sg->timer);
    sound_release(sg);

    gui_remove_view_port(sg->gui, sg->view_port);
    furi_record_close(RECORD_GUI);
    view_port_free(sg->view_port);

    furi_message_queue_free(sg->input_queue);
    furi_mutex_free(sg->mutex);
    free(sg);
}

int32_t skimguard_app(void* p) {
    UNUSED(p);
    SkimGuard* sg = skimguard_alloc();

    if(!field_sensor_init()) {
        /* Could not enter receive-only posture; bail cleanly. */
        skimguard_free(sg);
        return -1;
    }
    if(sg->sound_on) sound_acquire(sg);

    furi_timer_start(sg->timer, furi_ms_to_ticks(SG_SAMPLE_PERIOD_MS));

    InputEvent event;
    while(sg->running) {
        if(furi_message_queue_get(sg->input_queue, &event, 100) == FuriStatusOk) {
            handle_input(sg, &event);
        }
    }

    field_sensor_deinit();
    skimguard_free(sg);
    return 0;
}
