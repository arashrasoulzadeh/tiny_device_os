#pragma once

/**
 * Notification service.
 *
 * An OS surface, not an app: no manifest, no focus, and no launcher entry.
 * notify_post() copies a card into a four-deep queue. The focused app keeps
 * drawing; the card is painted on top at flush time, slides in, holds, and
 * slides out. A button press dismisses it. Otherwise it leaves
 * NOTIFY_HOLD_MS after the show animation.
 */

#include "app_types.h"
#include "ardubot_keys.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define NOTIFY_QUEUE_CAP 4
#define NOTIFY_ANIM_MS 200
#define NOTIFY_HOLD_MS 3000
#define NOTIFY_BAND_PERCENT 40

typedef enum {
  NOTIFY_EXTENT_BAND = 0,
  NOTIFY_EXTENT_FULL,
} notify_extent_t;

typedef enum {
  NOTIFY_PHASE_IDLE = 0,
  NOTIFY_PHASE_ENTER,
  NOTIFY_PHASE_HOLD,
  NOTIFY_PHASE_EXIT,
} notify_phase_t;

typedef struct {
  const char *title;
  const char *body; /* optional */
  notify_extent_t extent;
  uint8_t band_percent; /* 0 = default 40; ignored for FULL */
  uint32_t duration_ms; /* 0 = NOTIFY_HOLD_MS after show */
} notify_spec_t;

int notify_service_start(void);
int notify_post(const notify_spec_t *spec);

void notify_service_pump(void);
bool notify_service_needs_present(void);
void notify_service_composite(app_display_t *disp);

/** True when this press or release belongs to the card and must not reach the
 * app. */
bool notify_service_on_key(sim_key_t key, bool pressed);

notify_phase_t notify_service_phase(void);
int notify_service_slide_offset(void);
void notify_service_card_rect(int *w, int *h);
const char *notify_service_title(void);

/**
 * Pin the clock used by the service. The host simulator's time_now_ms()
 * follows the wall clock, so time_set_now_us() does not move it. Production
 * boot leaves this unset and the service reads time_now_ms().
 */
void notify_service_set_now_ms(uint32_t ms);

#ifdef __cplusplus
}
#endif
