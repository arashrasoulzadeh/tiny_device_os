#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define EVENT_MAX_DATA 64
#define EVENT_TOPIC_MAX 32
#define EVENT_MAX_SUBSCRIBERS 16
#define EVENT_QUEUE_SIZE 32

typedef enum {
    EVENT_PRIO_LOW = 0,
    EVENT_PRIO_NORMAL,
    EVENT_PRIO_HIGH,
    EVENT_PRIO_CRITICAL,
    EVENT_PRIO_SYNC
} event_priority_t;

typedef enum {
    EVENT_TYPE_SYSTEM = 0,
    EVENT_TYPE_DRIVER,
    EVENT_TYPE_UI,
    EVENT_TYPE_NETWORK,
    EVENT_TYPE_STORAGE,
    EVENT_TYPE_POWER,
    EVENT_TYPE_CUSTOM
} event_type_t;

typedef struct {
    event_type_t type;
    char topic[EVENT_TOPIC_MAX];
    uint32_t timestamp;
    uint32_t source_app_id;
    uint32_t data_size;
    uint8_t data[EVENT_MAX_DATA];
} event_t;

typedef void (*event_callback_t)(const event_t* event, void* arg);

typedef struct event_subscription {
    char topic[EVENT_TOPIC_MAX];
    event_callback_t callback;
    void* arg;
    event_priority_t priority;
    uint32_t app_id;
    bool active;
} event_subscription_t;

typedef struct {
    event_subscription_t subs[EVENT_MAX_SUBSCRIBERS];
    uint32_t count;
} event_topic_t;

typedef struct {
    event_topic_t topics[32];
    uint32_t topic_count;
    event_t queue[EVENT_QUEUE_SIZE];
    uint32_t queue_head;
    uint32_t queue_tail;
    uint32_t queue_count;
} event_system_t;

int event_system_init(void);
void event_system_deinit(void);

int event_subscribe(uint32_t app_id, const char* topic, event_callback_t callback, void* arg, event_priority_t priority);
int event_unsubscribe(uint32_t app_id, const char* topic);
int event_publish(const char* topic, const void* data, uint32_t data_size, event_type_t type, uint32_t source_app_id);
int event_publish_sync(const char* topic, const void* data, uint32_t data_size, event_type_t type, uint32_t source_app_id);

int event_process_queue(void);

void event_cleanup_app(uint32_t app_id);

int event_get_stats(uint32_t* published, uint32_t* queued, uint32_t* subscribers);

#ifdef __cplusplus
}
#endif