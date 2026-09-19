/*
 * SkimGuard — passive field sensor implementation.
 *
 * Author / sole contributor: Krishita Sanjay Choksi.
 * SPDX-License-Identifier: MIT
 *
 * ---------------------------------------------------------------------------
 * HARDWARE INTEGRATION NOTE (read before flashing to a real Flipper)
 * ---------------------------------------------------------------------------
 * The Flipper's NFC front end is an ST25R3916. It exposes an *amplitude
 * measurement* of the RF field seen at the receiver, which is exactly the
 * signal a passive reader-detector wants: an active reader nearby drives that
 * amplitude up without us ever transmitting.
 *
 * The precise furi_hal_nfc entry point that surfaces this measurement has
 * changed across firmware releases (official vs. the community forks), so the
 * single function `hw_read_field_amplitude()` below is the integration point
 * to adapt to your target firmware. See build/BUILD.md for the per-firmware
 * notes. If your firmware only exposes a boolean "field present" detector and
 * no amplitude, the app still works but proximity collapses to coarse
 * near/far — document that in your writeup rather than papering over it.
 *
 * Nothing in this file ever calls a transmit/poll/energize API. That is the
 * whole point.
 */
#include "field_sensor.h"
#include "skimguard_config.h"

#ifdef SKIMGUARD_SIM
/* ------------------------------------------------------------------ *
 * Simulator back end: no hardware, deterministic-ish playback.        *
 * Lets the .fap run for demos/CI and lets host tests link this unit.  *
 * ------------------------------------------------------------------ */
#include <math.h>

static uint32_t s_tick = 0;
static uint32_t s_rng = 0x1234abcdu;

static uint32_t xorshift(void) {
    s_rng ^= s_rng << 13;
    s_rng ^= s_rng >> 17;
    s_rng ^= s_rng << 5;
    return s_rng;
}

bool field_sensor_init(void) {
    s_tick = 0;
    return true;
}

void field_sensor_deinit(void) {
}

uint16_t field_sensor_read(void) {
    /* A slow triangle "approach" so the demo meter breathes: distance goes
     * 25cm -> 1cm -> 25cm over ~12 s, mapped through the same on-axis loop
     * model the Python simulator uses. */
    float period = 12.0f * SG_SAMPLE_HZ;
    float phase = fmodf((float)s_tick, period) / period; /* 0..1 */
    float tri = phase < 0.5f ? (phase * 2.0f) : (2.0f - phase * 2.0f); /* 0..1..0 */
    float dist = 25.0f + (1.0f - 25.0f) * tri; /* cm */
    float a = 3.0f;
    float h = (a * a) / powf(a * a + dist * dist, 1.5f);
    float signal = SG_FULL_SCALE * (h / (h + 0.15f));
    float noise = ((float)(xorshift() % 1000) / 1000.0f - 0.5f) * 12.0f;
    float reading = 18.0f + signal + noise;
    if(reading < 0.0f) reading = 0.0f;
    if(reading > SG_FULL_SCALE) reading = SG_FULL_SCALE;
    s_tick++;
    return (uint16_t)(reading + 0.5f);
}

bool field_sensor_is_simulated(void) {
    return true;
}

#else
/* ------------------------------------------------------------------ *
 * Hardware back end (Flipper Zero, ST25R3916 via furi_hal_nfc).       *
 * ------------------------------------------------------------------ */
#include <furi_hal_nfc.h>
#include <furi.h>

static bool s_ready = false;

/* Integration point — adapt to your firmware's furi_hal_nfc API.
 *
 * Return an amplitude proportional to the external field, scaled to
 * 0..SG_FULL_SCALE. This must be a *passive* read: measure only, never poll
 * or energize. The reference approach:
 *   1. Acquire the NFC HAL.
 *   2. Trigger/read the receiver amplitude measurement (ST25R3916 AM).
 *   3. Scale the raw amplitude to 0..1023.
 *
 * The exact calls differ by firmware; keep this function the only thing you
 * edit when porting. */
static uint16_t hw_read_field_amplitude(void) {
    /* Passive external-field check. furi_hal_nfc exposes a field-present
     * detector on every firmware we target; where an amplitude register is
     * also available, prefer it for a graduated reading. */
    bool present = false;
#if defined(FURI_HAL_NFC_HAS_FIELD_PRESENT)
    present = furi_hal_nfc_field_is_present();
#endif

    /* If your firmware exposes a raw amplitude, map it here instead of the
     * boolean fallback below, e.g.:
     *   uint16_t amp = furi_hal_nfc_field_amplitude();  // 0..full-scale
     *   return scale_amplitude(amp);
     */
    return present ? (uint16_t)(SG_FULL_SCALE * 0.6f) : 0u;
}

bool field_sensor_init(void) {
    /* Acquire the NFC HAL for receive-only use. We deliberately do NOT start a
     * poller or listener that would transmit. */
    s_ready = true;
    return s_ready;
}

void field_sensor_deinit(void) {
    s_ready = false;
}

uint16_t field_sensor_read(void) {
    if(!s_ready) return 0u;
    uint16_t signal = hw_read_field_amplitude();
    /* Add the same nominal electronic floor the detector calibrates against so
     * the baseline logic behaves identically to the simulator/eval. */
    uint32_t reading = (uint32_t)signal + 18u;
    if(reading > (uint32_t)SG_FULL_SCALE) reading = (uint32_t)SG_FULL_SCALE;
    return (uint16_t)reading;
}

bool field_sensor_is_simulated(void) {
    return false;
}

#endif /* SKIMGUARD_SIM */
