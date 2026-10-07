#pragma once

/* Log macros and the manifest builder. A screen includes app_framework.h.
 * Pixels are fw/ui.h. Files and pins are fw/io.h. */

#include <stdarg.h>
#include <string.h>

#include "app.h"
#include "app_types.h"
#include "stdlog.h"

#ifdef __cplusplus
extern "C" {
#endif

// Manifest builder function - call in app init or constructor
static inline app_manifest_t *
app_manifest_create(const char *name, const char *version, app_type_t type,
                    uint32_t min_os_ver, void (*entry)(void),
                    uint32_t stack_size, uint32_t heap_size,
                    const capability_t *caps, uint32_t cap_count,
                    const char *author, const char *description) {
  static app_manifest_t manifest = {0};
  manifest.type = type;
  manifest.min_os_version = min_os_ver;
  manifest.entry_point = (uintptr_t)entry;
  manifest.stack_size = stack_size > 0 ? stack_size : APP_STACK_SMALL;
  manifest.heap_size = heap_size > 0 ? heap_size : APP_HEAP_SMALL;

  if (name)
    strncpy(manifest.name, name, APP_NAME_MAX - 1);
  if (version)
    strncpy(manifest.version, version, 15);
  if (author)
    strncpy(manifest.author, author, 63);
  if (description)
    strncpy(manifest.description, description, 255);

  if (caps && cap_count > 0) {
    manifest.capability_count = (cap_count < 16) ? cap_count : 16;
    for (uint32_t i = 0; i < manifest.capability_count; i++) {
      manifest.capabilities[i] = caps[i];
    }
  }

  return &manifest;
}

// Internal logging function using stdlog_vlog correctly
static inline void _app_log(log_level_t level, const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  stdlog_vlog(level, __FILE__, __LINE__, __func__, fmt, args);
  va_end(args);
}

/* `(fmt, ...)` + `##__VA_ARGS__` (GNU's comma-swallowing extension) let a
 * caller pass just a literal string with no extra format args - but this
 * project builds with CMAKE_C_EXTENSIONS OFF (strict -std=c11), where
 * ISO C requires at least one argument for `...` and `##__VA_ARGS__`
 * itself isn't standard either; gcc 13 flags both under -Werror. Folding
 * the whole call into a single `...` avoids a separately-required `fmt`
 * parameter entirely - `"[APP] "` and the format string are still
 * adjacent string-literal tokens after substitution, so the compiler's
 * ordinary string-literal concatenation still joins them exactly as
 * before (e.g. APP_INFO("x") -> _app_log(..., "[APP] " "x"); APP_INFO("n=%d",
 * n)
 * -> _app_log(..., "[APP] " "n=%d", n)). */

// Use standard logging with app name prefix
#define APP_LOG_DEBUG(...) _app_log(LOG_LEVEL_DEBUG, "[APP] " __VA_ARGS__)
#define APP_LOG_INFO(...) _app_log(LOG_LEVEL_INFO, "[APP] " __VA_ARGS__)
#define APP_LOG_WARN(...) _app_log(LOG_LEVEL_WARN, "[APP] " __VA_ARGS__)
#define APP_LOG_ERROR(...) _app_log(LOG_LEVEL_ERROR, "[APP] " __VA_ARGS__)

// Short aliases
#define APP_DEBUG(...) _app_log(LOG_LEVEL_DEBUG, "[APP] " __VA_ARGS__)
#define APP_INFO(...) _app_log(LOG_LEVEL_INFO, "[APP] " __VA_ARGS__)
#define APP_WARN(...) _app_log(LOG_LEVEL_WARN, "[APP] " __VA_ARGS__)
#define APP_ERROR(...) _app_log(LOG_LEVEL_ERROR, "[APP] " __VA_ARGS__)

#ifdef __cplusplus
}
#endif
