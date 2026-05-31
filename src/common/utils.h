#ifndef RINHA_UTILS_H
#define RINHA_UTILS_H

#include <stdint.h>
#include <stddef.h>

uint32_t fnv1a_hash(const char *str, size_t len);
void log_info(const char *fmt, ...);
void log_error(const char *fmt, ...);

#endif
