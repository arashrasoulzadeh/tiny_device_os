#pragma once

/* Unified app framework. Include this from an app.
 *
 *   fw/ui.h      pixels, text, frame timing
 *   fw/io.h      keys, settings store, files
 *   fw/layout.h  canvas, screen, menu, catalog, status
 *   fw/events.h  os_event_subscribe / os_event_publish
 *
 * Name, version, author, description, title, and help come from app.json.
 */

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "app.h"
#include "app_kit.h"
#include "app_types.h"
#include "app_ui.h"
#include "ardubot_keys.h"
#include "config_store.h"
#include "display.h"
#include "fw/app.h"
#include "hal_display.h"
#include "hal_gpio.h"
#include "hal_i2c.h"
#include "hal_spi.h"
#include "hal_storage.h"
#include "hal_uart.h"
#include "notify_service.h"
#include "os_time.h"
#include "scheduler.h"
#include "sim_gpio.h"
#include "ssd1306_model.h"
#include "stdlog.h"
#include "syscall.h"
#include "vfs.h"
