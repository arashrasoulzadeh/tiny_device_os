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
 * apps/app_ui.h's app_ui_config_ui() content margin and default app
 * frame rate - see device_config_esp32c6.yaml's app_kit: block for the
 * authoritative values on that board (manually mirrored into
 * platformio.ini's [env:esp32-c6-kernel] build_flags, same as
 * APP_DISPLAY_WIDTH/HEIGHT above - this header isn't wired into the
 * yaml->generated-header pipeline yet).
 */
#ifndef ARDUBOT_UI_PADDING
#define ARDUBOT_UI_PADDING 0
#endif

#ifndef ARDUBOT_DEFAULT_FPS
#define ARDUBOT_DEFAULT_FPS 30
#endif
