#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    LOG_LEVEL_DEBUG = 0,
    LOG_LEVEL_INFO = 1,
    LOG_LEVEL_WARN = 2,
    LOG_LEVEL_ERROR = 3,
    LOG_LEVEL_NONE = 4,
} log_level_t;

typedef struct {
    log_level_t min_level;
    bool use_colors;
    bool use_timestamp;
    bool use_level_prefix;
    FILE* output;
} stdlog_config_t;

extern stdlog_config_t g_stdlog_config;

void stdlog_init(void);
void stdlog_set_level(log_level_t level);
void stdlog_set_output(FILE* output);
void stdlog_set_colors(bool enable);
void stdlog_set_timestamp(bool enable);

void stdlog_vlog(log_level_t level, const char* file, int line, const char* func, const char* fmt, va_list args);

#define stdlog_debug(fmt, ...) stdlog_vlog(LOG_LEVEL_DEBUG, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)
#define stdlog_info(fmt, ...) stdlog_vlog(LOG_LEVEL_INFO, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)
#define stdlog_warn(fmt, ...) stdlog_vlog(LOG_LEVEL_WARN, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)
#define stdlog_error(fmt, ...) stdlog_vlog(LOG_LEVEL_ERROR, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)

void stdlog_hexdump(const char* label, const void* data, size_t len);
void stdlog_flush(void);

#ifdef __cplusplus
}
#endif