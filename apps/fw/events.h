#pragma once

/* Events sub-framework. One bus: subscribe, then publish. */

#include "event.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FW_EVENT_INPUT "ui.input"

static inline int os_event_subscribe(uint32_t app_id, const char* topic, event_callback_t callback,
                                     void* arg) {
    return event_subscribe(app_id, topic, callback, arg, EVENT_PRIO_NORMAL);
}

static inline int os_event_unsubscribe(uint32_t app_id, const char* topic) {
    return event_unsubscribe(app_id, topic);
}

static inline int os_event_publish(const char* topic, const void* data, uint32_t size) {
    return event_publish_sync(topic, data, size, EVENT_TYPE_UI, 0);
}

#ifdef __cplusplus
}
#endif
