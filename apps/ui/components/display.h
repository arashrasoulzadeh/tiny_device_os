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
#define APP_DISPLAY_HEIGHT 64
#endif
