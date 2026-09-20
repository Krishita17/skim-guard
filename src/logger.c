/*
 * SkimGuard — SD-card session logger implementation.
 *
 * Author / sole contributor: Krishita Sanjay Choksi.
 * SPDX-License-Identifier: MIT
 */
#include "logger.h"

#include <furi.h>
#include <furi_hal_rtc.h>
#include <storage/storage.h>

#ifndef SKIMGUARD_DATA_DIR
#define SKIMGUARD_DATA_DIR "/ext/apps_data/skimguard"
#endif

#define LOGGER_HEADER "t_s,distance_cm,reading,present\n"

struct Logger {
    Storage* storage;
    File* file;
    bool active;
    char filename[48];
    char path[96];
};

Logger* logger_alloc(void) {
    Logger* logger = malloc(sizeof(Logger));
    memset(logger, 0, sizeof(*logger));
    logger->storage = furi_record_open(RECORD_STORAGE);
    logger->file = storage_file_alloc(logger->storage);
    logger->active = false;
    logger->filename[0] = '\0';
    return logger;
}

void logger_free(Logger* logger) {
    if(!logger) return;
    logger_stop(logger);
    storage_file_free(logger->file);
    furi_record_close(RECORD_STORAGE);
    free(logger);
}

bool logger_start(Logger* logger) {
    if(logger->active) return true;

    /* Ensure the app data directory exists (ignore "already exists"). */
    storage_common_mkdir(logger->storage, SKIMGUARD_DATA_DIR);

    uint32_t ts = furi_hal_rtc_get_timestamp();
    snprintf(logger->filename, sizeof(logger->filename), "session_%lu.csv", (unsigned long)ts);
    snprintf(logger->path, sizeof(logger->path), "%s/%s", SKIMGUARD_DATA_DIR, logger->filename);

    if(!storage_file_open(logger->file, logger->path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_close(logger->file);
        logger->filename[0] = '\0';
        return false;
    }
    storage_file_write(logger->file, LOGGER_HEADER, strlen(LOGGER_HEADER));
    logger->active = true;
    return true;
}

void logger_stop(Logger* logger) {
    if(!logger->active) return;
    storage_file_close(logger->file);
    logger->active = false;
}

bool logger_is_active(const Logger* logger) {
    return logger->active;
}

void logger_write(
    Logger* logger,
    float t_s,
    uint16_t reading,
    float proximity_pct,
    bool present) {
    UNUSED(proximity_pct);
    if(!logger->active) return;
    char line[48];
    /* distance_cm left blank (unknown on device) to match the shared schema. */
    int n = snprintf(
        line, sizeof(line), "%.2f,,%u,%d\n", (double)t_s, (unsigned)reading, present ? 1 : 0);
    if(n > 0) storage_file_write(logger->file, line, (size_t)n);
}

const char* logger_filename(const Logger* logger) {
    return logger->filename;
}
