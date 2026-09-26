#include "stdlog.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>

#ifdef ARDUINO
#include <Arduino.h>
#endif

static const char* level_strings[] = {
    "DEBUG",
    "INFO",
    "WARN",
    "ERROR",
};

static const char* level_colors[] = {
    "\033[36m",  // DEBUG - cyan
    "\033[32m",  // INFO - green
    "\033[33m",  // WARN - yellow
    "\033[31m",  // ERROR - red
};

static const char* color_reset = "\033[0m";

stdlog_config_t g_stdlog_config = {
    .min_level = LOG_LEVEL_DEBUG,
    .use_colors = true,
    .use_timestamp = true,
    .use_level_prefix = true,
    .output = NULL,
};

void stdlog_init(void) {
    g_stdlog_config.output = stdout;
    g_stdlog_config.min_level = LOG_LEVEL_DEBUG;
    g_stdlog_config.use_colors = true;
    g_stdlog_config.use_timestamp = true;
    g_stdlog_config.use_level_prefix = true;
}

void stdlog_set_level(log_level_t level) {
    g_stdlog_config.min_level = level;
}

void stdlog_set_output(FILE* output) {
    g_stdlog_config.output = output ? output : stdout;
}

void stdlog_set_colors(bool enable) {
    g_stdlog_config.use_colors = enable;
}

void stdlog_set_timestamp(bool enable) {
    g_stdlog_config.use_timestamp = enable;
}

void stdlog_vlog(log_level_t level, const char* file, int line, const char* func, const char* fmt, va_list args) {
    if (level < g_stdlog_config.min_level) return;
    if (!g_stdlog_config.output) return;

    FILE* out = g_stdlog_config.output;
    
    // Timestamp
    if (g_stdlog_config.use_timestamp) {
        time_t now = time(NULL);
        struct tm* tm_info = localtime(&now);
        char time_buf[32];
        strftime(time_buf, sizeof(time_buf), "%H:%M:%S", tm_info);
        fprintf(out, "[%s] ", time_buf);
    }
    
    // Level prefix
    if (g_stdlog_config.use_level_prefix) {
        if (g_stdlog_config.use_colors) {
            fprintf(out, "%s[%s]%s ", level_colors[level], level_strings[level], color_reset);
        } else {
            fprintf(out, "[%s] ", level_strings[level]);
        }
    }
    
    // File:line:func
    if (file && func) {
        fprintf(out, "[%s:%d:%s] ", file, line, func);
    }
    
    // Message
    vfprintf(out, fmt, args);
    fprintf(out, "\n");
    fflush(out);
}

void stdlog_hexdump(const char* label, const void* data, size_t len) {
    if (LOG_LEVEL_DEBUG < g_stdlog_config.min_level) return;
    if (!g_stdlog_config.output) return;
    
    FILE* out = g_stdlog_config.output;
    const uint8_t* data_ptr = (const uint8_t*)data;
    
    fprintf(out, "%s (%zu bytes):\n", label, len);
    for (size_t i = 0; i < len; i += 16) {
        fprintf(out, "%04zx: ", i);
        for (size_t j = 0; j < 16; j++) {
            if (i + j < len) {
                fprintf(out, "%02x ", data_ptr[i + j]);
            } else {
                fprintf(out, "   ");
            }
        }
        fprintf(out, " ");
        for (size_t j = 0; j < 16; j++) {
            if (i + j < len) {
                char c = data_ptr[i + j];
                fprintf(out, "%c", (c >= 32 && c <= 126) ? c : '.');
            }
        }
        fprintf(out, "\n");
    }
    fflush(out);
}

void stdlog_flush(void) {
    if (g_stdlog_config.output) {
        fflush(g_stdlog_config.output);
    }
}

// Arduino/Serial implementation for hardware
#ifdef ARDUINO
void stdlog_init_serial(unsigned long baud) {
    Serial.begin(baud);
    while (!Serial) {
        ; // Wait for serial port to connect
    }
    g_stdlog_config.output = (FILE*)&Serial; // Note: This is pseudo-code, would need adaptation for Arduino
}
#endif