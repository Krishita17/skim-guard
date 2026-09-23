/*
 * SkimGuard — shared detection constants.
 *
 * These MUST stay in lock-step with eval/detector.py (PARAMS). The Python
 * reference detector and this firmware run the identical algorithm so the
 * off-device evaluation characterizes exactly what the device does.
 *
 * Author / sole contributor: Krishita Sanjay Choksi.
 * SPDX-License-Identifier: MIT
 */
#pragma once

/* Sensor full-scale (ADC-style counts). */
#define SG_FULL_SCALE 1023.0f

/* Sampling. */
#define SG_SAMPLE_HZ 20u
#define SG_SAMPLE_PERIOD_MS (1000u / SG_SAMPLE_HZ)

/* Calibration / smoothing. */
#define SG_CALIB_SAMPLES 20u
#define SG_EMA_ALPHA_FAST 0.35f
#define SG_EMA_ALPHA_SLOW 0.08f
#define SG_NOISE_MIN 4.0f

/* Detection thresholds, expressed in multiples of measured noise so the app
 * self-tunes to the room it is switched on in. */
#define SG_ON_SIGMA 6.0f
#define SG_OFF_SIGMA 3.5f
#define SG_ON_MIN_COUNTS 25.0f

/* Proximity + feedback. */
#define SG_PROX_REF_COUNTS 650.0f
#define SG_CLICK_MS_FAR 900.0f
#define SG_CLICK_MS_NEAR 45.0f
#define SG_TREND_EPS 6.0f

/* Steady vs intermittent characterization. */
#define SG_WINDOW_SAMPLES 40u
#define SG_INTERMITTENT_LO 0.20f
#define SG_INTERMITTENT_HI 0.85f

/* Detection confidence: signal expressed in noise-sigmas, scaled to 0..100%.
 * At SG_CONF_FULL_SIGMA above baseline the confidence reads 100%. */
#define SG_CONF_FULL_SIGMA 40.0f

/* Approximate (uncalibrated) distance estimate — inverts the same on-axis loop
 * + saturation model the simulator uses, assuming a NOMINAL reference reader.
 * It is an estimate for a typical reader, not a calibrated measurement. */
#define SG_EST_POWER 1.0f
#define SG_EST_COIL_CM 3.0f
#define SG_EST_KSAT 0.15f
#define SG_EST_MAX_CM 40.0f /* clamp: beyond this we report "far/unknown" */

/* Baseline drift auto-compensation: when confidently absent for a sustained
 * period, slowly re-learn the floor so thermal/environmental drift does not
 * accumulate into a false positive. Adapts only while absent, and slowly. */
#define SG_DRIFT_HOLD_SAMPLES 60u /* ~3 s clear before adapting */
#define SG_DRIFT_RATE 0.01f /* fraction of the gap closed per sample */

/* Poll-rate (cadence) estimation for intermittent emitters. */
#define SG_POLL_EMA 0.4f

/* Sensitivity presets multiply the noise-relative thresholds. */
#define SG_SENS_HIGH 0.7f /* trips earlier (noisy-tolerant off) */
#define SG_SENS_MED 1.0f
#define SG_SENS_LOW 1.5f
