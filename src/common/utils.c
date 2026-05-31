#include "utils.h"
#include <stdio.h>
#include <stdarg.h>
#include <time.h>

uint32_t fnv1a_hash(const char *str, size_t len) {
    uint32_t hash = 2166136261U;
    for (size_t i = 0; i < len; i++) {
        hash ^= (uint8_t)str[i];
        hash *= 16777619U;
    }
    return hash;
}

static void log_internal(const char *level, const char *fmt, va_list args) {
    time_t now = time(NULL);
    struct tm tm_info;
    gmtime_r(&now, &tm_info);
    char timestamp[26];
    strftime(timestamp, 26, "%Y-%m-%dT%H:%M:%SZ", &tm_info);

    fprintf(stderr, "{\"ts\":\"%s\",\"level\":\"%s\",", timestamp, level);
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "}\n");
}

void log_info(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    log_internal("info", fmt, args);
    va_end(args);
}

void log_error(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    log_internal("error", fmt, args);
    va_end(args);
}
