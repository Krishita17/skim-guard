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
