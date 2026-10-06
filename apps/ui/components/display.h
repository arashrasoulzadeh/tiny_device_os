#pragma once

/**
 * Build-time panel geometry.
 *
 * CMake sets APP_DISPLAY_WIDTH / APP_DISPLAY_HEIGHT from the active device
 * target (see apps/CMakeLists.txt). Defaults match the SSD1306 used by the sim.
 */

#ifndef APP_DISPLAY_WIDTH
#define APP_DISPLAY_WIDTH 128
#endif

#ifndef APP_DISPLAY_HEIGHT
#define APP_DISPLAY_HEIGHT 32
#endif

/**
 * Inset of every standard app's content box, on all four sides: left and
 * right of the panel, below the title bar, and above the help bar.
 * app_ui_config_ui() applies it. Panels taller than 64px default to 16 so
 * a board does not have to opt in. Override with -DARDUBOT_UI_PADDING=N
 * (device_config_esp32c6.yaml app_kit.ui_padding, mirrored in
 * platformio.ini). Games use app_ui_config_game() and stay full-bleed.
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
