/*
 * SkimGuard — passive field sensor abstraction.
 *
 * The ONLY hardware-touching module. It reports a bounded reading (0..1023,
 * ADC-style counts) that tracks the strength of an external 13.56 MHz field.
 * It NEVER energizes the Flipper's own field — SkimGuard is receive-only.
 *
 * Two back ends, chosen at compile time:
 *   - default: reads the NFC front end's field amplitude via furi_hal_nfc.
 *   - SKIMGUARD_SIM: replays a built-in synthetic trace so the .fap can run
 *     and be demonstrated with no live reader (also used by host unit tests).
 *
 * Author / sole contributor: Krishita Sanjay Choksi.
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

/* Prepare the front end for passive listening. Returns false if the NFC
 * hardware could not be put into a receive-only posture. */
bool field_sensor_init(void);

/* Release the front end. */
void field_sensor_deinit(void);

/* One passive reading, 0..1023. Higher = stronger external field. */
uint16_t field_sensor_read(void);

/* True if this build is the hardware-free simulator back end. */
bool field_sensor_is_simulated(void);
