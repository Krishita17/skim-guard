/*
 * SkimGuard — detection pipeline implementation.
 *
 * Author / sole contributor: Krishita Sanjay Choksi.
 * SPDX-License-Identifier: MIT
 */
#include "detection.h"

#include <math.h>
#include <string.h>

static float sg_maxf(float a, float b) {
    return a > b ? a : b;
}

static float sg_sens(const Detector* d) {
    return d->sens_mult > 0.0f ? d->sens_mult : SG_SENS_MED;
}

void detector_init(Detector* d) {
    memset(d, 0, sizeof(*d));
    d->noise = SG_NOISE_MIN;
    d->sens_mult = SG_SENS_MED;
}

void detector_force_calibration(Detector* d, float baseline, float noise) {
    d->baseline = baseline;
    d->noise = sg_maxf(noise, SG_NOISE_MIN);
    d->smoothed = baseline;
    d->slow = baseline;
    d->calibrated = true;
    d->present = false;
    d->hist_len = 0;
    d->hist_head = 0;
    d->absent_ticks = 0;
    d->prev_present = false;
    d->samples_since_edge = 0;
    d->period_ema = 0.0f;
    d->edge_count = 0;
    if(d->sens_mult <= 0.0f) d->sens_mult = SG_SENS_MED;
}

void detector_set_sensitivity(Detector* d, float mult) {
    d->sens_mult = mult > 0.0f ? mult : SG_SENS_MED;
}

float detector_on_threshold(const Detector* d) {
    float t = SG_ON_SIGMA * d->noise;
    if(t < SG_ON_MIN_COUNTS) t = SG_ON_MIN_COUNTS;
    return t * sg_sens(d);
}

float detector_off_threshold(const Detector* d) {
    return SG_OFF_SIGMA * d->noise * sg_sens(d);
}

float detector_estimate_distance_cm(float signal, bool* valid) {
    /* Invert reading model: sat = signal/full_scale; H = k*sat/(1-sat);
     * (a^2+d^2)^1.5 = power*a^2 / H. Nominal reference reader assumed. */
    float sat = signal / SG_FULL_SCALE;
    if(sat <= 0.001f) {
        if(valid) *valid = false;
        return SG_EST_MAX_CM;
    }
    if(sat >= 0.999f) {
        if(valid) *valid = true;
        return 0.0f;
    }
    float h = SG_EST_KSAT * sat / (1.0f - sat);
    float a = SG_EST_COIL_CM;
    float rhs = SG_EST_POWER * a * a / h;
    float ad2 = powf(rhs, 2.0f / 3.0f); /* a^2 + d^2 */
    float d2 = ad2 - a * a;
    if(d2 <= 0.0f) {
        if(valid) *valid = true;
        return 0.0f;
    }
    float dist = sqrtf(d2);
    if(dist > SG_EST_MAX_CM) {
        if(valid) *valid = false;
        return SG_EST_MAX_CM;
    }
    if(valid) *valid = true;
    return dist;
}

static void hist_push(Detector* d, bool present) {
    if(d->hist_len < SG_WINDOW_SAMPLES) {
        d->hist[(d->hist_head + d->hist_len) % SG_WINDOW_SAMPLES] = present ? 1 : 0;
        d->hist_len++;
    } else {
        d->hist[d->hist_head] = present ? 1 : 0;
        d->hist_head = (d->hist_head + 1) % SG_WINDOW_SAMPLES;
    }
}

static SgCharacterization characterize(const Detector* d) {
    if(d->hist_len < (SG_WINDOW_SAMPLES / 2)) return SgCharNone;
    uint16_t sum = 0;
    for(uint16_t i = 0; i < d->hist_len; i++) {
        sum += d->hist[(d->hist_head + i) % SG_WINDOW_SAMPLES];
    }
    float duty = (float)sum / (float)d->hist_len;
    if(duty <= 0.02f) return SgCharNone;
    if(duty >= SG_INTERMITTENT_LO && duty <= SG_INTERMITTENT_HI) return SgCharIntermittent;
    if(duty > SG_INTERMITTENT_HI) return SgCharSteady;
    return SgCharNone;
}

bool detector_push(Detector* d, float reading, DetectionResult* out) {
    /* ---- calibration phase ---- */
    if(!d->calibrated) {
        d->calib_sum += reading;
        d->calib_sqsum += reading * reading;
        d->calib_count++;
        if(d->calib_count >= SG_CALIB_SAMPLES) {
            float n = (float)d->calib_count;
            float mean = d->calib_sum / n;
            float var = (d->calib_sqsum / n) - (mean * mean);
            if(var < 0.0f) var = 0.0f;
            float std = sqrtf(var);
            d->baseline = mean;
            d->noise = sg_maxf(std, SG_NOISE_MIN);
            d->smoothed = mean;
            d->slow = mean;
            d->calibrated = true;
        }
        if(out) {
            memset(out, 0, sizeof(*out));
            out->reading = reading;
            out->smoothed = reading;
            out->slow = reading;
            out->baseline = d->baseline;
            out->noise = d->noise;
            out->present = false;
            out->click_ms = INFINITY;
            out->characterization = SgCharNone;
            out->trend = SgTrendSteady;
        }
        return false;
    }

    /* ---- smoothing ---- */
    float prev_slow = d->slow;
    d->smoothed = SG_EMA_ALPHA_FAST * reading + (1.0f - SG_EMA_ALPHA_FAST) * d->smoothed;
    d->slow = SG_EMA_ALPHA_SLOW * reading + (1.0f - SG_EMA_ALPHA_SLOW) * d->slow;

    float signal = d->smoothed - d->baseline;
    if(signal < 0.0f) signal = 0.0f;

    /* ---- hysteresis latch ---- */
    if(d->present) {
        if(signal < detector_off_threshold(d)) d->present = false;
    } else {
        if(signal > detector_on_threshold(d)) d->present = true;
    }

    /* ---- proximity ---- */
    float prox = 100.0f * signal / SG_PROX_REF_COUNTS;
    if(prox < 0.0f) prox = 0.0f;
    if(prox > 100.0f) prox = 100.0f;
    if(!d->present) prox = 0.0f;

    /* ---- trend ---- */
    float delta = d->slow - prev_slow;
    SgTrend trend = SgTrendSteady;
    if(delta > SG_TREND_EPS)
        trend = SgTrendWarmer;
    else if(delta < -SG_TREND_EPS)
        trend = SgTrendColder;

    /* ---- click cadence ---- */
    float frac = prox / 100.0f;
    float click_ms = SG_CLICK_MS_FAR + (SG_CLICK_MS_NEAR - SG_CLICK_MS_FAR) * frac;
    if(!d->present) click_ms = INFINITY;

    /* ---- characterization ---- */
    hist_push(d, d->present);
    SgCharacterization ch = characterize(d);

    /* ---- detection confidence (signal in noise-sigmas, scaled) ---- */
    float nz = d->noise > 0.0f ? d->noise : SG_NOISE_MIN;
    float z = signal / nz;
    float conf = 100.0f * (z - SG_OFF_SIGMA) / (SG_CONF_FULL_SIGMA - SG_OFF_SIGMA);
    if(conf < 0.0f) conf = 0.0f;
    if(conf > 100.0f) conf = 100.0f;

    /* ---- approximate uncalibrated distance ---- */
    bool dist_valid = false;
    float dist_cm = detector_estimate_distance_cm(signal, &dist_valid);
    if(!d->present) dist_valid = false;

    /* ---- poll-rate estimation (rising-edge spacing) ---- */
    if(d->present && !d->prev_present) {
        if(d->edge_count >= 1) {
            float p = (float)d->samples_since_edge;
            if(d->period_ema <= 0.0f)
                d->period_ema = p;
            else
                d->period_ema = SG_POLL_EMA * p + (1.0f - SG_POLL_EMA) * d->period_ema;
        }
        d->samples_since_edge = 0;
        d->edge_count++;
    }
    d->prev_present = d->present;
    if(d->samples_since_edge < 0xFFFF) d->samples_since_edge++;
    float poll_hz = 0.0f;
    if(ch == SgCharIntermittent && d->period_ema > 0.0f) {
        poll_hz = (float)SG_SAMPLE_HZ / d->period_ema;
    }

    /* ---- baseline drift auto-compensation (only while clear) ---- */
    if(!d->present) {
        if(d->absent_ticks < 0xFFFF) d->absent_ticks++;
        if(d->absent_ticks > SG_DRIFT_HOLD_SAMPLES) {
            d->baseline += SG_DRIFT_RATE * (d->smoothed - d->baseline);
        }
    } else {
        d->absent_ticks = 0;
    }

    if(out) {
        out->reading = reading;
        out->smoothed = d->smoothed;
        out->slow = d->slow;
        out->baseline = d->baseline;
        out->noise = d->noise;
        out->signal = signal;
        out->present = d->present;
        out->proximity_pct = prox;
        out->trend = trend;
        out->click_ms = click_ms;
        out->characterization = ch;
        out->confidence_pct = conf;
        out->distance_cm_est = dist_cm;
        out->distance_valid = dist_valid;
        out->poll_hz = poll_hz;
    }
    return d->present;
}
