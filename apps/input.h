#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define INPUT_MAX_DEVICES 8
#define INPUT_MAX_KEYS 64
#define INPUT_MAX_GESTURES 16
#define INPUT_MAP_MAX 32

typedef enum {
    INPUT_DEV_KEYBOARD = 0,
    INPUT_DEV_KEYPAD,
    INPUT_DEV_NAV5,          // 5-way navigation
    INPUT_DEV_ENCODER,
    INPUT_DEV_TOUCH,
    INPUT_DEV_BUTTONS,
    INPUT_DEV_CUSTOM
} input_device_type_t;

typedef enum {
    INPUT_KEY_UNKNOWN = 0,
    INPUT_KEY_UP,
    INPUT_KEY_DOWN,
    INPUT_KEY_LEFT,
    INPUT_KEY_RIGHT,
    INPUT_KEY_ENTER,
    INPUT_KEY_ESCAPE,
    INPUT_KEY_BACK,
    INPUT_KEY_HOME,
    INPUT_KEY_MENU,
    INPUT_KEY_VOL_UP,
    INPUT_KEY_VOL_DOWN,
    INPUT_KEY_POWER,
    INPUT_KEY_A,
    INPUT_KEY_B,
    INPUT_KEY_C,
    INPUT_KEY_D,
    INPUT_KEY_E,
    INPUT_KEY_F,
    INPUT_KEY_G,
    INPUT_KEY_H,
    INPUT_KEY_I,
    INPUT_KEY_J,
    INPUT_KEY_K,
    INPUT_KEY_L,
    INPUT_KEY_M,
    INPUT_KEY_N,
    INPUT_KEY_O,
    INPUT_KEY_P,
    INPUT_KEY_Q,
    INPUT_KEY_R,
    INPUT_KEY_S,
    INPUT_KEY_T,
    INPUT_KEY_U,
    INPUT_KEY_V,
    INPUT_KEY_W,
    INPUT_KEY_X,
    INPUT_KEY_Y,
    INPUT_KEY_Z,
    INPUT_KEY_SHIFT,
    INPUT_KEY_CTRL,
    INPUT_KEY_ALT,
    INPUT_KEY_META,
    INPUT_KEY_SPACE,
    INPUT_KEY_TAB,
    INPUT_KEY_BACKSPACE,
    INPUT_KEY_MAX
} input_key_t;

typedef enum {
    INPUT_EVENT_NONE = 0,
    INPUT_EVENT_KEY_DOWN,
    INPUT_EVENT_KEY_UP,
    INPUT_EVENT_KEY_REPEAT,
    INPUT_EVENT_BUTTON_TAP,
    INPUT_EVENT_BUTTON_LONG_TAP,
    INPUT_EVENT_BUTTON_DOUBLE_TAP,
    INPUT_EVENT_BUTTON_HOLD,
    INPUT_EVENT_BUTTON_RELEASE,
    INPUT_EVENT_ENCODER_CW,
    INPUT_EVENT_ENCODER_CCW,
    INPUT_EVENT_TOUCH_DOWN,
    INPUT_EVENT_TOUCH_UP,
    INPUT_EVENT_TOUCH_MOVE,
    INPUT_EVENT_GESTURE_TAP,
    INPUT_EVENT_GESTURE_LONG_PRESS,
    INPUT_EVENT_GESTURE_DRAG,
    INPUT_EVENT_GESTURE_SWIPE,
    INPUT_EVENT_GESTURE_PINCH
} input_event_type_t;

typedef struct {
    input_event_type_t type;
    input_key_t key;
    uint32_t timestamp;
    uint32_t app_id;
    int16_t x, y;           // For touch
    int16_t dx, dy;         // For drag/swipe
    uint16_t pressure;      // For touch
    uint32_t duration_ms;   // For hold
    uint8_t tap_count;      // For double tap
} input_event_t;

typedef void (*input_callback_t)(const input_event_t* event, void* arg);

typedef enum {
    INPUT_MAP_TYPE_KEY = 0,
    INPUT_MAP_TYPE_BUTTON,
    INPUT_MAP_TYPE_AXIS
} input_map_type_t;

typedef struct {
    input_map_type_t type;
    uint32_t source_key;
    uint32_t target_action;
    uint32_t app_id;
    bool enabled;
} input_mapping_t;

typedef struct {
    char name[32];
    input_device_type_t type;
    input_mapping_t mappings[INPUT_MAP_MAX];
    uint32_t mapping_count;
    input_callback_t callback;
    void* callback_arg;
    bool active;
} input_device_t;

typedef struct {
    uint32_t tap_threshold_ms;
    uint32_t long_press_threshold_ms;
    uint32_t double_tap_threshold_ms;
    uint32_t hold_threshold_ms;
    uint32_t swipe_threshold_px;
    uint32_t drag_threshold_px;
    uint32_t pinch_threshold_px;
} input_config_t;

typedef struct input_recognizer input_recognizer_t;

input_recognizer_t* input_recognizer_create(void);
void input_recognizer_destroy(input_recognizer_t* rec);

int input_recognizer_add_gesture(void* rec, input_event_type_t gesture, 
                                  input_callback_t cb, void* arg);

int input_init(const input_config_t* config);
void input_deinit(void);

int input_device_register(input_device_type_t type, const char* name, input_callback_t cb, void* arg);
int input_device_unregister(const char* name);
input_device_t* input_device_find(const char* name);

int input_post_event(const input_event_t* event);

int input_set_mapping(uint32_t app_id, input_map_type_t type, uint32_t source_key, uint32_t target_action);
int input_clear_mappings(uint32_t app_id);
int input_get_mapping(uint32_t app_id, uint32_t source_key, uint32_t* target_action);

int input_process_events(void);

const char* input_key_to_str(input_key_t key);
const char* input_event_to_str(input_event_type_t type);

#ifdef __cplusplus
}
#endif