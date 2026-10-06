#pragma once

/**
 * Shared design tokens for every app_ui_t-based stdapp.
 *
 * Before this file, each app picked its own RGB565 literals and gap
 * sizes independently (info_app.c, pomodoro_app.c, counter_app.c, and
 * app_ui.c's own title/help bar colors all differed, even for colors
 * that meant the same thing - e.g. three different "warning orange"
 * values). Apps should use these constants instead of inventing their
 * own, so the whole app suite reads as one consistent design rather than
 * a collection of differently-styled screens.
 *
 * Not every existing stdapp has been migrated to these tokens yet - only
 * the ones touched during this pass (apps/app_ui.c's chrome, info_app.c,
 * pomodoro_app.c, counter_app.c). Migrate others as they're next worked
 * on, rather than as a separate sweep.
 */

#include "app_ui.h" /* APP_UI_RGB565 */

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------------
 * Core palette - backgrounds, text, chrome (title/help bars)
 * ------------------------------------------------------------------- */
#define ARDUBOT_COLOR_BG APP_UI_RGB565(0, 0, 0)
#define ARDUBOT_COLOR_TEXT APP_UI_RGB565(255, 255, 255)
#define ARDUBOT_COLOR_TEXT_MUTED APP_UI_RGB565(160, 160, 160)

#define ARDUBOT_COLOR_TITLE_BG APP_UI_RGB565(0, 60, 80)
#define ARDUBOT_COLOR_TITLE_TEXT APP_UI_RGB565(0, 252, 248)
#define ARDUBOT_COLOR_HELP_TEXT APP_UI_RGB565(140, 140, 140)

/* ---------------------------------------------------------------------
 * Semantic / status colors - meaning stays the same everywhere they're
 * used, instead of each app picking its own shade of "warning".
 * ------------------------------------------------------------------- */
#define ARDUBOT_COLOR_ACCENT_WARM APP_UI_RGB565(255, 90, 0)  /* active/demanding (e.g. "WORK") */
#define ARDUBOT_COLOR_ACCENT_COOL APP_UI_RGB565(0, 170, 140) /* calm/idle (e.g. "REST") */
#define ARDUBOT_COLOR_SUCCESS APP_UI_RGB565(0, 200, 0)       /* positive value / good state */
#define ARDUBOT_COLOR_WARNING APP_UI_RGB565(255, 160, 0)     /* getting close to a limit */
#define ARDUBOT_COLOR_DANGER APP_UI_RGB565(255, 60, 60)      /* negative value / critical */
#define ARDUBOT_COLOR_TRACK APP_UI_RGB565(40, 40, 40)        /* bar/gauge empty-track background */

/* ---------------------------------------------------------------------
 * Spacing - raw pixel gaps between elements, independent of
 * app_ui_config_t's content inset (ARDUBOT_UI_PADDING on all four sides,
 * apps/ui/components/display.h), which is the screen edge, not
 * spacing between elements within the content area.
 * ------------------------------------------------------------------- */
#define ARDUBOT_SPACE_XS 2
#define ARDUBOT_SPACE_SM 4
#define ARDUBOT_SPACE_MD 8
#define ARDUBOT_SPACE_LG 12
#define ARDUBOT_SPACE_XL 16

/* ---------------------------------------------------------------------
 * Typography scale - passed as the `scale` argument to
 * app_display_text_color() wherever a non-default size is needed.
 * app_ui_config_t.text_scale (set from the device's own
 * APP_DISPLAY_HEIGHT) remains the "normal body text" size used by
 * app_ui_text()/app_ui_line() and isn't duplicated here.
 * ------------------------------------------------------------------- */
#define ARDUBOT_TYPE_SCALE_SMALL 1
#define ARDUBOT_TYPE_SCALE_LARGE 4

#ifdef __cplusplus
}
#endif
