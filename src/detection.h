/*
 * SkimGuard — detection pipeline (device-side).
 *
 * Streaming detector: push one raw sensor reading at a time, read back a
 * DetectionResult. Mirrors eval/detector.py exactly.
 *
 * Author / sole contributor: Krishita Sanjay Choksi.
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "skimguard_config.h"

typedef enum {
    SgCharNone = 0,
    SgCharSteady,
    SgCharIntermittent,
} SgCharacterization;

typedef enum {
    SgTrendColder = -1,
    SgTrendSteady = 0,
    SgTrendWarmer = 1,
} SgTrend;

typedef struct {
    float reading; /* last raw reading */
    float smoothed; /* fast EMA */
    float slow; /* slow EMA (trend reference) */
    float baseline; /* learned quiet-room mean */
    float noise; /* learned quiet-room std */
    float signal; /* smoothed - baseline, clamped >= 0 */
    bool present; /* latched present/absent */
    float proximity_pct; /* 0..100 */
    SgTrend trend;
    float click_ms; /* click interval; INFINITY when silent */
    SgCharacterization characterization;
    float confidence_pct; /* SNR-based detection confidence, 0..100 */
    float distance_cm_est; /* approximate uncalibrated distance (nominal reader) */
    bool distance_valid; /* false when too far/saturated to estimate */
    float poll_hz; /* estimated intermittent poll rate, 0 if unknown */
} DetectionResult;

typedef struct {
    /* calibration */
    uint16_t calib_count;
    float calib_sum;
    float calib_sqsum;
    float baseline;
    float noise;
    bool calibrated;

    /* running state */
    float smoothed;
    float slow;
    bool present;

    /* presence history ring buffer for steady/intermittent */
    uint8_t hist[SG_WINDOW_SAMPLES];
    uint16_t hist_len;
    uint16_t hist_head;

    /* sensitivity multiplier on the noise-relative thresholds */
    float sens_mult;

    /* baseline drift auto-compensation */
    uint16_t absent_ticks;

    /* poll-rate (cadence) estimation */
    bool prev_present;
    uint16_t samples_since_edge;
    float period_ema; /* samples between rising edges */
    uint16_t edge_count;
} Detector;

/* Reset a detector to the pre-calibration state. */
void detector_init(Detector* d);

/* Skip warm-up with a known-clean baseline (used by A/B mode recalibration). */
void detector_force_calibration(Detector* d, float baseline, float noise);

/* Push one reading; fills *out and returns whether a reader is present. */
bool detector_push(Detector* d, float reading, DetectionResult* out);

/* Current ON/OFF thresholds (counts). Exposed for the UI + tests. */
float detector_on_threshold(const Detector* d);
float detector_off_threshold(const Detector* d);

/* Sensitivity multiplier on the noise-relative thresholds (e.g. SG_SENS_LOW).
 * Lower = trips earlier. Default is SG_SENS_MED (1.0). */
void detector_set_sensitivity(Detector* d, float mult);

/* Approximate uncalibrated distance (cm) for a signal, using the nominal
 * reference reader. Sets *valid=false when too far/saturated to estimate.
 * Exposed for tests and the eval pipeline. */
float detector_estimate_distance_cm(float signal, bool* valid);
