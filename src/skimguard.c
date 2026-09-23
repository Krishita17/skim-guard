/*
 * SkimGuard — passive 13.56 MHz NFC reader (skimmer) detector for Flipper Zero.
 *
 * Receive-only: senses the RF field an active reader can't help but emit and
 * turns it into an EMF-style meter, a live waveform, accelerating geiger
 * clicks, peak-hold, haptic + LED feedback, a room verdict, an A/B (clean vs
 * compromised) mode, a watch/alert sentry mode, and SD-card session logging.
 * It NEVER transmits.
 *
 * The passive reader-field-detection approach is credited to the open-source
 * Specter project by at0m-b0mb (see README / docs/prior_art.md). This is an
 * independent implementation with an added measurement/evaluation layer.
 *
 * Author / sole contributor: Krishita Sanjay Choksi.
 * SPDX-License-Identifier: MIT
 */
#include <furi.h>
#include <furi_hal_speaker.h>
#include <furi_hal_vibro.h>
#include <furi_hal_light.h>
#include <gui/gui.h>
#include <input/input.h>
#include <math.h>
#include <stdio.h>

#include "skimguard_config.h"
#include "detection.h"
#include "field_sensor.h"
#include "logger.h"

#define WAVE_N 60u /* waveform history samples (~3 s at 20 Hz) */

typedef enum {
    ModeSweep = 0, /* free-hand hunt: waveform + meter + clicks */
    ModeAB, /* guided clean-vs-compromised */
    ModeWatch, /* armed sentry: alert if a reader appears */
    ModeCount,
} SgMode;

typedef enum {
    AbStepClean = 0,
    AbStepCompromised,
} AbStep;

typedef enum {
    FbMute = 0, /* screen + LED-clear only */
    FbSound, /* + geiger clicks */
    FbFull, /* + haptic + proportional LED */
    FbCount,
} FeedbackLevel;

typedef struct {
    FuriMutex* mutex;
    FuriMessageQueue* input_queue;
    ViewPort* view_port;
    Gui* gui;
    FuriTimer* timer;
    Logger* logger;

    Detector detector;
    DetectionResult result;

    SgMode mode;
    AbStep ab_step;
    bool ab_clean_seen_quiet;

    FeedbackLevel feedback;
    uint8_t sens_idx; /* 0=High, 1=Med, 2=Low */
    bool speaker_held;
    float click_accum_ms;
    int click_hold_ticks;

    /* waveform ring buffer of proximity (0..100) */
    uint8_t wave[WAVE_N];
    uint16_t wave_len;
    uint16_t wave_head;
    float peak_prox;

    float elapsed_s; /* for log timestamps */

    bool watch_armed;
    bool watch_alerted;

    volatile bool running;
} SkimGuard;

/* ------------------------------------------------------------------ */
/* Feedback: speaker, haptic, LED                                      */
/* ------------------------------------------------------------------ */
static void speaker_sync(SkimGuard* sg) {
    bool want = (sg->feedback >= FbSound);
    if(want && !sg->speaker_held) {
        if(furi_hal_speaker_acquire(30)) sg->speaker_held = true;
    } else if(!want && sg->speaker_held) {
        furi_hal_speaker_stop();
        furi_hal_speaker_release();
        sg->speaker_held = false;
    }
}

static void led_off(void) {
    furi_hal_light_set(LightRed, 0);
    furi_hal_light_set(LightGreen, 0);
    furi_hal_light_set(LightBlue, 0);
}

static void led_update(SkimGuard* sg, const DetectionResult* r) {
    if(sg->feedback < FbFull) {
        led_off();
        return;
    }
    if(r->present) {
        uint8_t v = (uint8_t)(r->proximity_pct * 2.55f);
        furi_hal_light_set(LightRed, v);
        furi_hal_light_set(LightGreen, 0);
    } else {
        /* calm green = "clear / listening" */
        furi_hal_light_set(LightRed, 0);
        furi_hal_light_set(LightGreen, 12);
    }
    furi_hal_light_set(LightBlue, 0);
}

static void pulse_start(SkimGuard* sg, float proximity_pct) {
    if(sg->speaker_held) {
        float freq = 1800.0f + 2600.0f * (proximity_pct / 100.0f);
        furi_hal_speaker_start(freq, 0.5f);
    }
    if(sg->feedback >= FbFull) furi_hal_vibro_on(true);
    sg->click_hold_ticks = 1;
}

static void pulse_stop(SkimGuard* sg) {
    if(sg->speaker_held) furi_hal_speaker_stop();
    furi_hal_vibro_on(false);
    sg->click_hold_ticks = 0;
}

/* ------------------------------------------------------------------ */
/* Waveform + peak                                                     */
/* ------------------------------------------------------------------ */
static void wave_push(SkimGuard* sg, float proximity_pct) {
    uint8_t v = (uint8_t)(proximity_pct + 0.5f);
    if(sg->wave_len < WAVE_N) {
        sg->wave[(sg->wave_head + sg->wave_len) % WAVE_N] = v;
        sg->wave_len++;
    } else {
        sg->wave[sg->wave_head] = v;
        sg->wave_head = (sg->wave_head + 1) % WAVE_N;
    }
    if(proximity_pct > sg->peak_prox) sg->peak_prox = proximity_pct;
}

static float sens_mult_of(uint8_t idx) {
    switch(idx) {
    case 0:
        return SG_SENS_HIGH;
    case 2:
        return SG_SENS_LOW;
    default:
        return SG_SENS_MED;
    }
}

static const char* sens_name(uint8_t idx) {
    switch(idx) {
    case 0:
        return "High";
    case 2:
        return "Low";
    default:
        return "Med";
    }
}

static void apply_sensitivity(SkimGuard* sg) {
    detector_set_sensitivity(&sg->detector, sens_mult_of(sg->sens_idx));
}

static const char* verdict_word(float confidence_pct) {
    if(confidence_pct >= 50.0f) return "ACTIVE";
    if(confidence_pct >= 10.0f) return "TRACE";
    return "CLEAR";
}

static void reset_session(SkimGuard* sg) {
    detector_init(&sg->detector);
    apply_sensitivity(sg);
    sg->wave_len = 0;
    sg->wave_head = 0;
    sg->peak_prox = 0.0f;
    sg->watch_alerted = false;
    sg->ab_clean_seen_quiet = false;
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

    sg->elapsed_s += (float)SG_SAMPLE_PERIOD_MS / 1000.0f;
    wave_push(sg, r.proximity_pct);

    if(logger_is_active(sg->logger)) {
        logger_write(sg->logger, sg->elapsed_s, raw, r.proximity_pct, r.present);
    }

    if(sg->mode == ModeWatch && sg->watch_armed && r.present) sg->watch_alerted = true;
    if(sg->mode == ModeAB && sg->ab_step == AbStepClean && !r.present) sg->ab_clean_seen_quiet = true;

    /* end a pulse that has run its hold time */
    if(sg->click_hold_ticks > 0) {
        sg->click_hold_ticks--;
        if(sg->click_hold_ticks == 0) pulse_stop(sg);
    }

    bool clicky = (sg->feedback >= FbSound) && r.present && isfinite(r.click_ms);
    if(sg->mode == ModeWatch) clicky = clicky && sg->watch_alerted;
    if(clicky) {
        sg->click_accum_ms += (float)SG_SAMPLE_PERIOD_MS;
        if(sg->click_accum_ms >= r.click_ms) {
            sg->click_accum_ms = 0.0f;
            pulse_start(sg, r.proximity_pct);
        }
    } else {
        sg->click_accum_ms = 0.0f;
    }

    led_update(sg, &r);

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

static const char* feedback_tag(FeedbackLevel f) {
    switch(f) {
    case FbMute:
        return "mute";
    case FbSound:
        return "snd";
    case FbFull:
        return "snd+led";
    default:
        return "?";
    }
}

static void draw_meter(Canvas* c, int x, int y, int w, int h, float pct, float peak) {
    canvas_draw_frame(c, x, y, w, h);
    int fill = (int)((w - 2) * (pct / 100.0f));
    if(fill > 0) canvas_draw_box(c, x + 1, y + 1, fill, h - 2);
    /* peak-hold tick */
    int px = x + 1 + (int)((w - 2) * (peak / 100.0f));
    if(px > x + 1 && px < x + w - 1) canvas_draw_line(c, px, y - 1, px, y + h);
}

static void draw_waveform(Canvas* c, const SkimGuard* sg, int x, int y, int w, int h) {
    /* baseline */
    canvas_draw_line(c, x, y + h, x + w, y + h);
    if(sg->wave_len < 2) return;
    int step_num = w;
    for(uint16_t i = 0; i < sg->wave_len; i++) {
        uint8_t v = sg->wave[(sg->wave_head + i) % WAVE_N];
        int px = x + (int)((long)i * step_num / (sg->wave_len - 1));
        int ph = (int)(h * (v / 100.0f));
        canvas_draw_line(c, px, y + h - ph, px, y + h);
    }
}

static void draw_flags(Canvas* c, const SkimGuard* sg) {
    char buf[40];
    snprintf(
        buf,
        sizeof(buf),
        "%s%s %s pk%d%%",
        feedback_tag(sg->feedback),
        logger_is_active(sg->logger) ? " LOG" : "",
        sens_name(sg->sens_idx),
        (int)(sg->peak_prox + 0.5f));
    canvas_set_font(c, FontSecondary);
    canvas_draw_str(c, 2, 63, buf);
}

static void draw_sweep(Canvas* c, const SkimGuard* sg, const DetectionResult* r) {
    char line[28];
    canvas_set_font(c, FontSecondary);

    if(!r->present) {
        snprintf(line, sizeof(line), "%s", verdict_word(r->confidence_pct));
        canvas_draw_str(c, 2, 23, line);
    } else {
        const char* tr = r->trend == SgTrendWarmer ? "warmer >>>" :
                         r->trend == SgTrendColder ? "<<< colder" :
                                                     "holding";
        snprintf(line, sizeof(line), "%s %s", verdict_word(r->confidence_pct), tr);
        canvas_draw_str(c, 2, 23, line);
    }
    /* confidence readout, right of the status line */
    snprintf(line, sizeof(line), "c%d%%", (int)(r->confidence_pct + 0.5f));
    canvas_draw_str_aligned(c, 126, 23, AlignRight, AlignBottom, line);

    draw_waveform(c, sg, 2, 26, 124, 14);
    draw_meter(c, 2, 43, 124, 10, r->proximity_pct, sg->peak_prox);

    draw_flags(c, sg);

    /* bottom-right: distance estimate > poll rate > characterization */
    line[0] = '\0';
    if(r->present && r->distance_valid) {
        snprintf(line, sizeof(line), "~%dcm", (int)(r->distance_cm_est + 0.5f));
    } else if(r->poll_hz > 0.0f) {
        snprintf(line, sizeof(line), "%d.%01dHz", (int)r->poll_hz,
                 (int)((r->poll_hz - (int)r->poll_hz) * 10.0f));
    } else if(r->characterization == SgCharSteady) {
        snprintf(line, sizeof(line), "steady");
    } else if(r->characterization == SgCharIntermittent) {
        snprintf(line, sizeof(line), "intermit");
    }
    if(line[0]) canvas_draw_str_aligned(c, 126, 63, AlignRight, AlignBottom, line);
}

static void draw_ab(Canvas* c, const SkimGuard* sg, const DetectionResult* r) {
    canvas_set_font(c, FontSecondary);
    if(sg->ab_step == AbStepClean) {
        canvas_draw_str(c, 2, 23, "Step 1: sweep CLEAN box");
        canvas_draw_str(c, 2, 34, r->present ? "field here?! recheck" : "quiet - good baseline");
    } else {
        canvas_draw_str(c, 2, 23, "Step 2: sweep SUSPECT box");
        canvas_draw_str(c, 2, 34, r->present ? "READER FOUND" : "sweeping...");
    }
    draw_meter(c, 2, 40, 124, 10, r->proximity_pct, sg->peak_prox);
    canvas_draw_str(c, 2, 63, "OK:next step");
    char pk[16];
    snprintf(pk, sizeof(pk), "pk%d%%", (int)(sg->peak_prox + 0.5f));
    canvas_draw_str_aligned(c, 126, 63, AlignRight, AlignBottom, pk);
}

static void draw_watch(Canvas* c, const SkimGuard* sg, const DetectionResult* r) {
    canvas_set_font(c, FontPrimary);
    if(sg->watch_alerted) {
        canvas_draw_str(c, 2, 26, "! READER DETECTED");
        canvas_set_font(c, FontSecondary);
        canvas_draw_str(c, 2, 38, "appeared while armed");
    } else if(sg->watch_armed) {
        canvas_draw_str(c, 2, 26, "ARMED - watching");
        canvas_set_font(c, FontSecondary);
        canvas_draw_str(c, 2, 38, "silent until a reader shows");
    } else {
        canvas_draw_str(c, 2, 26, "Watch idle");
        canvas_set_font(c, FontSecondary);
        canvas_draw_str(c, 2, 38, "OK: arm sentry");
    }
    draw_meter(c, 2, 43, 124, 8, r->proximity_pct, sg->peak_prox);
    draw_flags(c, sg);
}

static void draw_cb(Canvas* canvas, void* ctx) {
    SkimGuard* sg = ctx;
    furi_mutex_acquire(sg->mutex, FuriWaitForever);
    DetectionResult r = sg->result;
    SgMode mode = sg->mode;
    furi_mutex_release(sg->mutex);

    canvas_clear(canvas);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 11, "SkimGuard");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 126, 11, AlignRight, AlignBottom, mode_name(mode));
    canvas_draw_line(canvas, 0, 13, 128, 13);

    if(field_sensor_is_simulated()) canvas_draw_str(canvas, 74, 11, "SIM");

    switch(mode) {
    case ModeSweep:
        draw_sweep(canvas, sg, &r);
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

static void toggle_logging(SkimGuard* sg) {
    if(logger_is_active(sg->logger)) {
        logger_stop(sg->logger);
    } else {
        sg->elapsed_s = 0.0f;
        logger_start(sg->logger); /* silently no-ops in UI if it fails */
    }
}

static void handle_input(SkimGuard* sg, const InputEvent* e) {
    bool is_short = (e->type == InputTypeShort);
    bool is_long = (e->type == InputTypeLong);
    if(!is_short && !is_long) return;

    furi_mutex_acquire(sg->mutex, FuriWaitForever);
    switch(e->key) {
    case InputKeyBack:
        sg->running = false;
        break;
    case InputKeyOk:
        if(is_long) {
            reset_session(sg); /* fresh baseline + clear peak/waveform */
        } else if(sg->mode == ModeAB) {
            sg->ab_step = (sg->ab_step == AbStepClean) ? AbStepCompromised : AbStepClean;
            sg->click_accum_ms = 0.0f;
        } else if(sg->mode == ModeWatch) {
            sg->watch_armed = !sg->watch_armed;
            sg->watch_alerted = false;
        } else {
            reset_session(sg);
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
        sg->feedback = (FeedbackLevel)((sg->feedback + 1) % FbCount);
        speaker_sync(sg);
        if(sg->feedback < FbFull) {
            furi_hal_vibro_on(false);
            led_off();
        }
        break;
    case InputKeyDown:
        if(is_long) {
            sg->sens_idx = (uint8_t)((sg->sens_idx + 1) % 3);
            apply_sensitivity(sg);
        } else {
            toggle_logging(sg);
        }
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
    sg->logger = logger_alloc();
    sg->mode = ModeSweep;
    sg->feedback = FbSound;
    sg->sens_idx = 1; /* Med */
    sg->running = true;

    detector_init(&sg->detector);
    apply_sensitivity(sg);

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

    if(sg->speaker_held) {
        furi_hal_speaker_stop();
        furi_hal_speaker_release();
    }
    furi_hal_vibro_on(false);
    led_off();

    logger_free(sg->logger);

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
        skimguard_free(sg);
        return -1;
    }
    speaker_sync(sg);

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
