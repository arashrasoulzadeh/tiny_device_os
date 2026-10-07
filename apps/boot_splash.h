#pragma once

/**
 * Boot splash. Title is "ArdubotOS" plus the last 8 hex chars of the commit.
 * Each step names the service or app being loaded and advances the bar.
 */

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* "ArdubotOS " and 8 hash chars. Returns the length, or -1 if buf is too small.
 */
int boot_splash_format_title(char *buf, size_t cap, const char *hash);

/* Status line is the step label. NULL becomes an empty string. */
int boot_splash_format_status(char *buf, size_t cap, const char *label);

/* Filled width of the progress bar. Same math as app_bar_fill_px. */
int boot_splash_bar_fill(int done, int total, int width_px);

void boot_splash_open(void);
void boot_splash_show(const char *label, int done, int total);
void boot_splash_close(void);

#ifdef __cplusplus
}
#endif
