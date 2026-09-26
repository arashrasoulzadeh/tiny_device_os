#include "event.h"
#include "scheduler.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static event_system_t g_event_system = {0};
static int g_event_lock = 0;

static void event_lock(void) {
    while (__sync_lock_test_and_set(&g_event_lock, 1)) {}
}

static void event_unlock(void) {
    __sync_lock_release(&g_event_lock);
}

int event_system_init(void) {
    memset(&g_event_system, 0, sizeof(g_event_system));
    g_event_lock = 0;
    return 0;
}

void event_system_deinit(void) {
    memset(&g_event_system, 0, sizeof(g_event_system));
}

static event_topic_t* event_find_or_create_topic(const char* topic) {
    if (!topic) return NULL;
    
    for (uint32_t i = 0; i < g_event_system.topic_count; i++) {
        if (strcmp(g_event_system.topics[i].subs[0].topic, topic) == 0) {
            return &g_event_system.topics[i];
        }
    }
    
    if (g_event_system.topic_count >= 32) return NULL;
    
    event_topic_t* t = &g_event_system.topics[g_event_system.topic_count++];
    memset(t, 0, sizeof(event_topic_t));
    return t;
}

static event_topic_t* event_find_topic(const char* topic) {
    if (!topic) return NULL;
    
    for (uint32_t i = 0; i < g_event_system.topic_count; i++) {
        if (strcmp(g_event_system.topics[i].subs[0].topic, topic) == 0) {
            return &g_event_system.topics[i];
        }
    }
    return NULL;
}

int event_subscribe(uint32_t app_id, const char* topic, event_callback_t callback, void* arg, event_priority_t priority) {
    if (!topic || !callback) return -1;
    
    event_lock();
    
    event_topic_t* t = event_find_or_create_topic(topic);
    if (!t) {
        event_unlock();
        return -1;
    }
    
    if (t->count >= EVENT_MAX_SUBSCRIBERS) {
        event_unlock();
        return -1;
    }
    
    event_subscription_t* sub = &t->subs[t->count++];
    strncpy(sub->topic, topic, EVENT_TOPIC_MAX - 1);
    sub->callback = callback;
    sub->arg = arg;
    sub->priority = priority;
    sub->app_id = app_id;
    sub->active = true;
    
    // Sort by priority (highest first)
    for (uint32_t i = t->count - 1; i > 0; i--) {
        if (t->subs[i].priority > t->subs[i - 1].priority) {
            event_subscription_t tmp = t->subs[i];
            t->subs[i] = t->subs[i - 1];
            t->subs[i - 1] = tmp;
        } else break;
    }
    
    event_unlock();
    return 0;
}

int event_unsubscribe(uint32_t app_id, const char* topic) {
    if (!topic) return -1;
    
    event_lock();
    
    event_topic_t* t = event_find_topic(topic);
    if (!t) {
        event_unlock();
        return -1;
    }
    
    for (uint32_t i = 0; i < t->count; i++) {
        if (t->subs[i].app_id == app_id) {
            // Remove by shifting
            for (uint32_t j = i; j < t->count - 1; j++) {
                t->subs[j] = t->subs[j + 1];
            }
            t->count--;
            break;
        }
    }
    
    event_unlock();
    return 0;
}

static int event_deliver(const event_t* event) {
    event_topic_t* t = event_find_topic(event->topic);
    if (!t) return 0;
    
    int delivered = 0;
    for (uint32_t i = 0; i < t->count; i++) {
        event_subscription_t* sub = &t->subs[i];
        if (sub->active && sub->callback) {
            sub->callback(event, sub->arg);
            delivered++;
        }
    }
    return delivered;
}

int event_publish(const char* topic, const void* data, uint32_t data_size, event_type_t type, uint32_t source_app_id) {
    if (!topic) return -1;
    if (data_size > EVENT_MAX_DATA) return -1;
    
    event_t event = {0};
    event.type = type;
    strncpy(event.topic, topic, EVENT_TOPIC_MAX - 1);
    event.timestamp = scheduler_get_tick_count();
    event.source_app_id = source_app_id;
    event.data_size = data_size;
    if (data && data_size > 0) {
        memcpy(event.data, data, data_size);
    }
    
    event_lock();
    
    // For async delivery, add to queue
    if (g_event_system.queue_count < EVENT_QUEUE_SIZE) {
        g_event_system.queue[g_event_system.queue_tail] = event;
        g_event_system.queue_tail = (g_event_system.queue_tail + 1) % EVENT_QUEUE_SIZE;
        g_event_system.queue_count++;
        event_unlock();
        return 0;
    }
    
    event_unlock();
    return -1; // Queue full
}

int event_publish_sync(const char* topic, const void* data, uint32_t data_size, event_type_t type, uint32_t source_app_id) {
    if (!topic) return -1;
    if (data_size > EVENT_MAX_DATA) return -1;
    
    event_t event = {0};
    event.type = type;
    strncpy(event.topic, topic, EVENT_TOPIC_MAX - 1);
    event.timestamp = scheduler_get_tick_count();
    event.source_app_id = source_app_id;
    event.data_size = data_size;
    if (data && data_size > 0) {
        memcpy(event.data, data, data_size);
    }
    
    event_lock();
    int delivered = event_deliver(&event);
    event_unlock();
    
    return delivered;
}

int event_process_queue(void) {
    event_lock();
    
    int processed = 0;
    while (g_event_system.queue_count > 0) {
        event_t event = g_event_system.queue[g_event_system.queue_head];
        g_event_system.queue_head = (g_event_system.queue_head + 1) % EVENT_QUEUE_SIZE;
        g_event_system.queue_count--;
        
        event_unlock();
        event_deliver(&event);
        event_lock();
        processed++;
    }
    
    event_unlock();
    return processed;
}

void event_cleanup_app(uint32_t app_id) {
    event_lock();
    
    for (uint32_t i = 0; i < g_event_system.topic_count; i++) {
        event_topic_t* t = &g_event_system.topics[i];
        for (uint32_t j = 0; j < t->count; j++) {
            if (t->subs[j].app_id == app_id) {
                t->subs[j].active = false;
            }
        }
    }
    
    event_unlock();
}

int event_get_stats(uint32_t* published, uint32_t* queued, uint32_t* subscribers) {
    event_lock();
    
    uint32_t pub = 0, sub = 0;
    for (uint32_t i = 0; i < g_event_system.topic_count; i++) {
        sub += g_event_system.topics[i].count;
    }
    
    if (published) *published = pub;
    if (queued) *queued = g_event_system.queue_count;
    if (subscribers) *subscribers = sub;
    
    event_unlock();
    return 0;
}