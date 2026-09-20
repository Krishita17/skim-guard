/*
 * SkimGuard — SD-card session logger.
 *
 * Streams live sweeps to a CSV on the Flipper's SD card using the SAME schema
 * as the off-device pipeline (t_s,distance_cm,reading,present), so a capture
 * taken on the device loads directly into sim/eval with no conversion:
 *
 *     /ext/apps_data/skimguard/session_<timestamp>.csv
 *
 * distance_cm is left blank on device (unknown); annotate it by hand later if
 * you swept at measured distances. This is what turns SkimGuard's evaluation
 * from purely simulated into empirical on your own hardware.
 *
 * Author / sole contributor: Krishita Sanjay Choksi.
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct Logger Logger;

Logger* logger_alloc(void);
void logger_free(Logger* logger);

/* Open a fresh timestamped CSV and write the header. Returns false on failure
 * (e.g. no SD card); the app then simply reports logging unavailable. */
bool logger_start(Logger* logger);

/* Flush and close the current file. Safe to call when not active. */
void logger_stop(Logger* logger);

bool logger_is_active(const Logger* logger);

/* Append one sample row. No-op when not active. */
void logger_write(Logger* logger, float t_s, uint16_t reading, float proximity_pct, bool present);

/* Basename of the current/last file (for the UI), or "" if none. */
const char* logger_filename(const Logger* logger);
