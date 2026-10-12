#pragma once

/**
 * Build-time panel geometry.
 *
 * CMake sets APP_DISPLAY_WIDTH / APP_DISPLAY_HEIGHT from the active device
 * target (see apps/CMakeLists.txt). A PlatformIO image leaves them unset
 * and takes lcd.width / lcd.height from device_config.h, so a 128x64
 * SH1106 profile is not laid out as a 128x32 SSD1306. Defaults match the
 * small SSD1306 used when neither source is present.
 */

#if defined(__has_include)
#if __has_include("device_config.h")
#include "device_config.h"
#endif
#endif

#ifndef APP_DISPLAY_WIDTH
#ifdef ARDUBOT_LCD_WIDTH
#define APP_DISPLAY_WIDTH ARDUBOT_LCD_WIDTH
#else
#define APP_DISPLAY_WIDTH 128
#endif
#endif

#ifndef APP_DISPLAY_HEIGHT
#ifdef ARDUBOT_LCD_HEIGHT
#define APP_DISPLAY_HEIGHT ARDUBOT_LCD_HEIGHT
#else
#define APP_DISPLAY_HEIGHT 32
#endif
#endif

/**
 * Inset of every standard app's content box, on all four sides: left and
 * right of the panel, below the title bar, and above the help bar.
 * app_ui_config_ui() applies it. Panels taller than 64px default to 16 so
 * a board does not have to opt in. Override with -DARDUBOT_UI_PADDING=N
 * (device_config_esp32c6.yaml app_kit.ui_padding, mirrored in
 * platformio.ini). The status band (header_app.c) is reserved above that
 * box when the panel is large enough. Games and fullscreen apps use
 * app_ui_config_game() and stay full-bleed.
 */
#ifndef ARDUBOT_UI_PADDING
#if APP_DISPLAY_HEIGHT > 64
#define ARDUBOT_UI_PADDING 16
#else
#define ARDUBOT_UI_PADDING 0
#endif
#endif

#ifndef ARDUBOT_DEFAULT_FPS
#define ARDUBOT_DEFAULT_FPS 30
#endif
