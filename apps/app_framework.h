#pragma once

/* Include this from an app. Name, version, author, description, title, and
 * help come from app.json.
 *
 * Pixels and the frame clock are fw/ui.h. Files, the settings store, and
 * GPIO are fw/io.h. Include those only when the screen uses them.
 */

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "app_helper.h"
#include "ardubot_keys.h"
#include "fw/app.h"
