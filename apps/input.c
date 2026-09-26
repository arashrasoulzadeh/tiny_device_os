#include "input.h"
#include "scheduler.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define INPUT_EVENT_QUEUE_SIZE 64

static struct {
    input_device_t devices[INPUT_MAX_DEVICES];
    uint32_t device_count;
    input_config_t config;
    input_event_t event_queue[INPUT_EVENT_QUEUE_SIZE];
    uint32_t queue_head;
    uint32_t queue_tail;
    uint32_t queue_count;
    uint32_t last_key_states[INPUT_KEY_MAX];
    uint32_t key_repeat_states[INPUT_KEY_MAX];
    int g_lock;
} g_input = {0};

static void input_lock(void) {
    while (__sync_lock_test_and_set(&g_input.g_lock, 1)) {}
}

static void input_unlock(void) {
    __sync_lock_release(&g_input.g_lock);
}

static int input_queue_event(const input_event_t* event) {
    if (g_input.queue_count >= INPUT_EVENT_QUEUE_SIZE) return -1;
    
    g_input.event_queue[g_input.queue_tail] = *event;
    g_input.queue_tail = (g_input.queue_tail + 1) % INPUT_EVENT_QUEUE_SIZE;
    g_input.queue_count++;
    return 0;
}

int input_init(const input_config_t* config) {
    memset(&g_input, 0, sizeof(g_input));
    
    if (config) {
        g_input.config = *config;
    } else {
        g_input.config.tap_threshold_ms = 300;
        g_input.config.long_press_threshold_ms = 500;
        g_input.config.double_tap_threshold_ms = 400;
        g_input.config.hold_threshold_ms = 1000;
        g_input.config.swipe_threshold_px = 30;
        g_input.config.drag_threshold_px = 10;
        g_input.config.pinch_threshold_px = 20;
    }
    
    return 0;
}

void input_deinit(void) {
    input_lock();
    memset(&g_input, 0, sizeof(g_input));
    input_unlock();
}

int input_device_register(input_device_type_t type, const char* name, input_callback_t cb, void* arg) {
    if (!name) return -1;
    
    input_lock();
    
    if (g_input.device_count >= INPUT_MAX_DEVICES) {
        input_unlock();
        return -1;
    }
    
    for (uint32_t i = 0; i < g_input.device_count; i++) {
        if (strcmp(g_input.devices[i].name, name) == 0) {
            input_unlock();
            return -1;
        }
    }
    
    input_device_t* dev = &g_input.devices[g_input.device_count++];
    memset(dev, 0, sizeof(input_device_t));
    strncpy(dev->name, name, 31);
    dev->type = type;
    dev->callback = cb;
    dev->callback_arg = arg;
    dev->active = true;
    
    input_unlock();
    return 0;
}

int input_device_unregister(const char* name) {
    if (!name) return -1;
    
    input_lock();
    
    for (uint32_t i = 0; i < g_input.device_count; i++) {
        if (strcmp(g_input.devices[i].name, name) == 0) {
            for (uint32_t j = i; j < g_input.device_count - 1; j++) {
                g_input.devices[j] = g_input.devices[j + 1];
            }
            g_input.device_count--;
            input_unlock();
            return 0;
        }
    }
    
    input_unlock();
    return -1;
}

input_device_t* input_device_find(const char* name) {
    if (!name) return NULL;
    
    input_lock();
    for (uint32_t i = 0; i < g_input.device_count; i++) {
        if (strcmp(g_input.devices[i].name, name) == 0) {
            input_unlock();
            return &g_input.devices[i];
        }
    }
    input_unlock();
    return NULL;
}

int input_post_event(const input_event_t* event) {
    if (!event) return -1;
    return input_queue_event(event);
}

int input_set_mapping(uint32_t app_id, input_map_type_t type, uint32_t source_key, uint32_t target_action) {
    input_lock();
    
    for (uint32_t i = 0; i < g_input.device_count; i++) {
        input_device_t* dev = &g_input.devices[i];
        if (dev->mapping_count < INPUT_MAP_MAX) {
            input_mapping_t* map = &dev->mappings[dev->mapping_count++];
            map->type = type;
            map->source_key = source_key;
            map->target_action = target_action;
            map->app_id = app_id;
            map->enabled = true;
            input_unlock();
            return 0;
        }
    }
    
    input_unlock();
    return -1;
}

int input_clear_mappings(uint32_t app_id) {
    input_lock();
    
    for (uint32_t i = 0; i < g_input.device_count; i++) {
        input_device_t* dev = &g_input.devices[i];
        for (uint32_t j = 0; j < dev->mapping_count; j++) {
            if (dev->mappings[j].app_id == app_id) {
                for (uint32_t k = j; k < dev->mapping_count - 1; k++) {
                    dev->mappings[k] = dev->mappings[k + 1];
                }
                dev->mapping_count--;
                j--;
            }
        }
    }
    
    input_unlock();
    return 0;
}

int input_get_mapping(uint32_t app_id, uint32_t source_key, uint32_t* target_action) {
    input_lock();
    
    for (uint32_t i = 0; i < g_input.device_count; i++) {
        input_device_t* dev = &g_input.devices[i];
        for (uint32_t j = 0; j < dev->mapping_count; j++) {
            if (dev->mappings[j].app_id == app_id && 
                dev->mappings[j].source_key == source_key) {
                if (target_action) *target_action = dev->mappings[j].target_action;
                input_unlock();
                return 0;
            }
        }
    }
    
    input_unlock();
    return -1;
}

static void process_key_event(input_key_t key, bool pressed) {
    uint32_t now = scheduler_get_tick_count();
    input_event_t event = {0};
    
    if (pressed) {
        event.type = INPUT_EVENT_KEY_DOWN;
        g_input.last_key_states[key] = now;
        
        // Check for double tap
        if (g_input.key_repeat_states[key] > 0 && 
            now - g_input.key_repeat_states[key] < g_input.config.double_tap_threshold_ms) {
            event.type = INPUT_EVENT_BUTTON_DOUBLE_TAP;
            event.tap_count = 2;
            g_input.key_repeat_states[key] = 0;
        } else {
            event.tap_count = 1;
            g_input.key_repeat_states[key] = now;
        }
    } else {
        event.type = INPUT_EVENT_KEY_UP;
        uint32_t duration = now - g_input.last_key_states[key];
        
        if (duration >= g_input.config.hold_threshold_ms) {
            event.type = INPUT_EVENT_BUTTON_RELEASE;
            event.duration_ms = duration;
        } else if (duration >= g_input.config.long_press_threshold_ms) {
            event.type = INPUT_EVENT_BUTTON_LONG_TAP;
        } else if (duration >= g_input.config.tap_threshold_ms) {
            event.type = INPUT_EVENT_BUTTON_TAP;
        }
    }
    
    event.key = key;
    event.timestamp = now;
    input_queue_event(&event);
}

static void process_encoder_event(int direction) {
    input_event_t event = {0};
    event.type = direction > 0 ? INPUT_EVENT_ENCODER_CW : INPUT_EVENT_ENCODER_CCW;
    event.timestamp = scheduler_get_tick_count();
    input_queue_event(&event);
}

int input_process_events(void) {
    int processed = 0;
    
    input_lock();
    while (g_input.queue_count > 0) {
        input_event_t event = g_input.event_queue[g_input.queue_head];
        g_input.queue_head = (g_input.queue_head + 1) % INPUT_EVENT_QUEUE_SIZE;
        g_input.queue_count--;
        
        input_unlock();
        
        // Deliver to registered devices
        for (uint32_t i = 0; i < g_input.device_count; i++) {
            input_device_t* dev = &g_input.devices[i];
            if (dev->active && dev->callback) {
                dev->callback(&event, dev->callback_arg);
            }
        }
        
        processed++;
        input_lock();
    }
    input_unlock();
    
    return processed;
}

int input_recognizer_add_gesture(void* rec, input_event_type_t gesture, input_callback_t cb, void* arg) {
    (void)rec; (void)gesture; (void)cb; (void)arg;
    return 0;
}

input_recognizer_t* input_recognizer_create(void) {
    return NULL;
}

void input_recognizer_destroy(input_recognizer_t* rec) {
    (void)rec;
}

const char* input_key_to_str(input_key_t key) {
    static const char* names[] = {
        "UNKNOWN", "UP", "DOWN", "LEFT", "RIGHT", "ENTER", "ESCAPE",
        "BACK", "HOME", "MENU", "VOL_UP", "VOL_DOWN", "POWER",
        "A", "B", "X", "Y", "L", "R",
        "F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "F10", "F11", "F12"
    };
    if (key < sizeof(names)/sizeof(names[0])) return names[key];
    return "UNKNOWN";
}

const char* input_event_to_str(input_event_type_t type) {
    static const char* names[] = {
        "NONE", "KEY_DOWN", "KEY_UP", "KEY_REPEAT",
        "BUTTON_TAP", "BUTTON_LONG_TAP", "BUTTON_DOUBLE_TAP",
        "BUTTON_HOLD", "BUTTON_RELEASE",
        "ENCODER_CW", "ENCODER_CCW",
        "TOUCH_DOWN", "TOUCH_UP", "TOUCH_MOVE",
        "GESTURE_TAP", "GESTURE_LONG_PRESS", "GESTURE_DRAG",
        "GESTURE_SWIPE", "GESTURE_PINCH"
    };
    if (type < sizeof(names)/sizeof(names[0])) return names[type];
    return "UNKNOWN";
}