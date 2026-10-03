#include "unity.h"
#include "input.h"
#include "scheduler.h"
#include <string.h>

/* apps/input.c had zero tests and, for the gesture classification logic
 * specifically, zero callers: process_key_event() (tap/double-tap/
 * long-press/hold detection) was defined but never invoked by anything,
 * including input_post_event() itself, which just queued whatever raw
 * event it was given. Fixed by routing KEY_DOWN/KEY_UP through it; this
 * file is the first real exercise of that classification logic. */

static input_config_t g_cfg;

void setUp(void) {
    scheduler_init();
    g_cfg = (input_config_t){
        .tap_threshold_ms = 5,
        .long_press_threshold_ms = 20,
        .double_tap_threshold_ms = 15,
        .hold_threshold_ms = 50,
        .swipe_threshold_px = 30,
        .drag_threshold_px = 10,
        .pinch_threshold_px = 20,
    };
    input_init(&g_cfg);
}

void tearDown(void) {
    input_deinit();
}

static input_event_t g_captured[8];
static int g_captured_count;

static void capture_cb(const input_event_t* event, void* arg) {
    (void)arg;
    if (g_captured_count < 8) {
        g_captured[g_captured_count++] = *event;
    }
}

static void tick(int n) {
    for (int i = 0; i < n; i++) scheduler_tick();
}

void test_quick_release_under_tap_threshold_stays_key_up(void) {
    TEST_ASSERT_EQUAL(0, input_device_register(INPUT_DEV_BUTTONS, "btns", capture_cb, NULL));
    g_captured_count = 0;

    input_event_t down = {.type = INPUT_EVENT_KEY_DOWN, .key = INPUT_KEY_A};
    input_post_event(&down);
    tick(2);  // under tap_threshold_ms=5
    input_event_t up = {.type = INPUT_EVENT_KEY_UP, .key = INPUT_KEY_A};
    input_post_event(&up);

    input_process_events();
    TEST_ASSERT_EQUAL(2, g_captured_count);
    TEST_ASSERT_EQUAL(INPUT_EVENT_KEY_DOWN, g_captured[0].type);
    TEST_ASSERT_EQUAL(INPUT_EVENT_KEY_UP, g_captured[1].type);
}

void test_release_within_tap_window_is_classified_as_tap(void) {
    input_device_register(INPUT_DEV_BUTTONS, "btns", capture_cb, NULL);
    g_captured_count = 0;

    input_event_t down = {.type = INPUT_EVENT_KEY_DOWN, .key = INPUT_KEY_B};
    input_post_event(&down);
    tick(10);  // >= tap_threshold_ms(5), < long_press_threshold_ms(20)
    input_event_t up = {.type = INPUT_EVENT_KEY_UP, .key = INPUT_KEY_B};
    input_post_event(&up);

    input_process_events();
    TEST_ASSERT_EQUAL(2, g_captured_count);
    TEST_ASSERT_EQUAL(INPUT_EVENT_BUTTON_TAP, g_captured[1].type);
}

void test_release_within_long_press_window_is_classified_as_long_tap(void) {
    input_device_register(INPUT_DEV_BUTTONS, "btns", capture_cb, NULL);
    g_captured_count = 0;

    input_event_t down = {.type = INPUT_EVENT_KEY_DOWN, .key = INPUT_KEY_C};
    input_post_event(&down);
    tick(25);  // >= long_press_threshold_ms(20), < hold_threshold_ms(50)
    input_event_t up = {.type = INPUT_EVENT_KEY_UP, .key = INPUT_KEY_C};
    input_post_event(&up);

    input_process_events();
    TEST_ASSERT_EQUAL(INPUT_EVENT_BUTTON_LONG_TAP, g_captured[1].type);
}

void test_release_past_hold_threshold_is_classified_as_release_with_duration(void) {
    input_device_register(INPUT_DEV_BUTTONS, "btns", capture_cb, NULL);
    g_captured_count = 0;

    input_event_t down = {.type = INPUT_EVENT_KEY_DOWN, .key = INPUT_KEY_D};
    input_post_event(&down);
    tick(60);  // >= hold_threshold_ms(50)
    input_event_t up = {.type = INPUT_EVENT_KEY_UP, .key = INPUT_KEY_D};
    input_post_event(&up);

    input_process_events();
    TEST_ASSERT_EQUAL(INPUT_EVENT_BUTTON_RELEASE, g_captured[1].type);
    TEST_ASSERT_TRUE(g_captured[1].duration_ms >= 50);
}

void test_second_press_within_double_tap_window_is_classified(void) {
    input_device_register(INPUT_DEV_BUTTONS, "btns", capture_cb, NULL);
    g_captured_count = 0;

    // key_repeat_states[key]==0 doubles as "never pressed" (see the NOTE
    // in process_key_event) - tick forward first so the real first press
    // below doesn't land on tick 0 and get mistaken for that sentinel.
    tick(1);

    input_event_t down = {.type = INPUT_EVENT_KEY_DOWN, .key = INPUT_KEY_E};
    input_post_event(&down);
    tick(2);
    input_event_t up = {.type = INPUT_EVENT_KEY_UP, .key = INPUT_KEY_E};
    input_post_event(&up);
    tick(5);  // within double_tap_threshold_ms(15) of the first press
    input_post_event(&down);

    input_process_events();
    // [0]=first KEY_DOWN, [1]=KEY_UP (quick release), [2]=second press -> double tap
    TEST_ASSERT_EQUAL(3, g_captured_count);
    TEST_ASSERT_EQUAL(INPUT_EVENT_BUTTON_DOUBLE_TAP, g_captured[2].type);
    TEST_ASSERT_EQUAL(2, g_captured[2].tap_count);
}

void test_recognizer_dispatches_only_matching_gesture(void) {
    input_recognizer_t* rec = input_recognizer_create();
    TEST_ASSERT_NOT_NULL(rec);

    g_captured_count = 0;
    TEST_ASSERT_EQUAL(0, input_recognizer_add_gesture(rec, INPUT_EVENT_GESTURE_SWIPE,
                                                       capture_cb, NULL));

    input_event_t swipe = {.type = INPUT_EVENT_GESTURE_SWIPE};
    input_event_t pinch = {.type = INPUT_EVENT_GESTURE_PINCH};

    TEST_ASSERT_EQUAL(1, input_recognizer_dispatch(rec, &swipe));
    TEST_ASSERT_EQUAL(0, input_recognizer_dispatch(rec, &pinch));
    TEST_ASSERT_EQUAL(1, g_captured_count);

    input_recognizer_destroy(rec);
}

void test_key_to_str_matches_real_enum_order(void) {
    // This used to be a mismatched gamepad-style table - INPUT_KEY_C
    // printed as "X", INPUT_KEY_D as "Y".
    TEST_ASSERT_EQUAL_STRING("A", input_key_to_str(INPUT_KEY_A));
    TEST_ASSERT_EQUAL_STRING("C", input_key_to_str(INPUT_KEY_C));
    TEST_ASSERT_EQUAL_STRING("Z", input_key_to_str(INPUT_KEY_Z));
    TEST_ASSERT_EQUAL_STRING("BACKSPACE", input_key_to_str(INPUT_KEY_BACKSPACE));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_quick_release_under_tap_threshold_stays_key_up);
    RUN_TEST(test_release_within_tap_window_is_classified_as_tap);
    RUN_TEST(test_release_within_long_press_window_is_classified_as_long_tap);
    RUN_TEST(test_release_past_hold_threshold_is_classified_as_release_with_duration);
    RUN_TEST(test_second_press_within_double_tap_window_is_classified);
    RUN_TEST(test_recognizer_dispatches_only_matching_gesture);
    RUN_TEST(test_key_to_str_matches_real_enum_order);
    return UNITY_END();
}
