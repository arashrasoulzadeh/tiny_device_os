#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* Install every builtin that this image actually compiled in.
 * The set lives here so sim and board boot code cannot grow a second
 * copy of an app. Returns 0, or -1 if a compiled-in manifest is missing
 * or already installed. */
int stdapps_install(void);

/* App app_start() should launch after stdapps_install(). Sensors when
 * that app is in the image, otherwise info, clock, pomodoro, or the launcher. */
const char* stdapps_start_name(void);

#ifdef __cplusplus
}
#endif
