#include "unity.h"
#include "app_framework.h"
#include "app_ui.h"
#include "scheduler.h"
#include <string.h>

void setUp(void) {
    scheduler_init();
}

void tearDown(void) {}

void test_app_ui_init_links_desc_from_real_app(void) {
    app_desc_t desc = {0};
    desc.name = "launcher";

    app_ctx_t real_ctx;
    memset(&real_ctx, 0, sizeof(real_ctx));
    real_ctx.desc = &desc;

    app_ui_config_t cfg;
    app_ui_config_ui(&cfg, "TEST", "help");

    app_ui_t ui;
    TEST_ASSERT_EQUAL(0, app_ui_init(&ui, &real_ctx, &cfg));

    /* Regression: without linking desc, app_open()/app_request_exit() on
     * ui.ctx silently no-op because they bail out on a NULL desc - this is
     * why Enter did nothing in the launcher despite arrow keys working. */
    TEST_ASSERT_EQUAL_PTR(&desc, ui.ctx.desc);
    TEST_ASSERT_EQUAL_STRING("launcher", ui.ctx.desc->name);

    app_ui_deinit(&ui);
}

void test_app_ui_init_tolerates_null_real_app(void) {
    app_ui_config_t cfg;
    app_ui_config_ui(&cfg, "TEST", "help");

    app_ui_t ui;
    TEST_ASSERT_EQUAL(0, app_ui_init(&ui, NULL, &cfg));
    TEST_ASSERT_NULL(ui.ctx.desc);

    app_ui_deinit(&ui);
}

/* app_ui_bind_gesture() bridges this app_ui_t to apps/input.c's gesture
 * recognizer: it creates the recognizer, registers one forwarding device
 * for it, and registers the GPIO edge binding. The GPIO-edge-read step
 * itself (gesture_key_trampoline, static in app_ui.c) is gated by the
 * same app_kit_is_foreground_desc() check app_ui_bind_key()'s own
 * trampoline uses, which - same as that one - needs a real, running,
 * focused app (app_install_manifest + app_start) to exercise; neither
 * trampoline is unit-tested at that level in this file for that reason.
 * What's tested here is everything app_ui_bind_gesture() sets up: the
 * recognizer gets created, the gesture registers, and the forwarding
 * device correctly reaches the callback when fed an event directly -
 * the same input_post_event()/input_process_events() calls the
 * trampoline itself would make after reading the pin. */

static int g_gesture_calls;
static input_event_type_t g_gesture_last_type;

static void on_gesture(const input_event_t* event, void* arg) {
    (void)arg;
    g_gesture_calls++;
    g_gesture_last_type = event->type;
}

void test_bind_gesture_creates_recognizer_and_registers(void) {
    input_init(NULL);

    app_ui_config_t cfg;
    app_ui_config_ui(&cfg, "TEST", "help");
    app_ui_t ui;
    TEST_ASSERT_EQUAL(0, app_ui_init(&ui, NULL, &cfg));
    TEST_ASSERT_NULL(ui.gestures);

    TEST_ASSERT_EQUAL(0, app_ui_bind_gesture(&ui, SIM_KEY_ENTER,
                                              INPUT_EVENT_BUTTON_LONG_TAP, on_gesture, NULL));
    TEST_ASSERT_NOT_NULL(ui.gestures);

    app_ui_deinit(&ui);
    input_deinit();
}

void test_bind_gesture_second_call_reuses_the_same_recognizer(void) {
    input_init(NULL);

    app_ui_config_t cfg;
    app_ui_config_ui(&cfg, "TEST", "help");
    app_ui_t ui;
    app_ui_init(&ui, NULL, &cfg);

    app_ui_bind_gesture(&ui, SIM_KEY_ENTER, INPUT_EVENT_BUTTON_TAP, on_gesture, NULL);
    input_recognizer_t* first = ui.gestures;

    app_ui_bind_gesture(&ui, SIM_KEY_ESCAPE, INPUT_EVENT_BUTTON_LONG_TAP, on_gesture, NULL);
    TEST_ASSERT_EQUAL_PTR(first, ui.gestures);

    app_ui_deinit(&ui);
    input_deinit();
}

extern int g_next_pin;

/* Regression test: app_ui_bind_gesture() used to allocate a new GPIO pin
 * (and a new HAL_GPIO_IRQ_BOTH edge binding) on every call, even for a
 * key it had already wired up. Binding both TAP and LONG_TAP on the same
 * key - ordinary usage, e.g. "short press selects, long press goes back" -
 * left two pins both mapped to that one sim_key_t, so sim_gpio_handle_key()
 * (which fires every pin mapped to a key) ran the trampoline twice per
 * physical press, posting and processing the same key event twice. */
void test_bind_gesture_reuses_gpio_wiring_for_the_same_key(void) {
    input_init(NULL);

    app_ui_config_t cfg;
    app_ui_config_ui(&cfg, "TEST", "help");
    app_ui_t ui;
    app_ui_init(&ui, NULL, &cfg);

    int before = g_next_pin;
    app_ui_bind_gesture(&ui, SIM_KEY_ENTER, INPUT_EVENT_BUTTON_TAP, on_gesture, NULL);
    int after_first = g_next_pin;
    app_ui_bind_gesture(&ui, SIM_KEY_ENTER, INPUT_EVENT_BUTTON_LONG_TAP, on_gesture, NULL);
    int after_second = g_next_pin;

    TEST_ASSERT_EQUAL(1, after_first - before);   // first call: one new pin
    TEST_ASSERT_EQUAL(0, after_second - after_first);  // same key again: no new pin

    // A genuinely different key still gets its own pin.
    app_ui_bind_gesture(&ui, SIM_KEY_ESCAPE, INPUT_EVENT_BUTTON_TAP, on_gesture, NULL);
    TEST_ASSERT_EQUAL(1, g_next_pin - after_second);

    app_ui_deinit(&ui);
    input_deinit();
}

void test_bind_gesture_forwarding_device_reaches_callback(void) {
    input_init(NULL);

    app_ui_config_t cfg;
    app_ui_config_ui(&cfg, "TEST", "help");
    app_ui_t ui;
    app_ui_init(&ui, NULL, &cfg);
    app_ui_bind_gesture(&ui, SIM_KEY_ENTER, INPUT_EVENT_BUTTON_LONG_TAP, on_gesture, NULL);

    g_gesture_calls = 0;
    // Same two calls gesture_key_trampoline makes after reading the pin -
    // exercises the device-forwarding bridge app_ui_bind_gesture() set up.
    input_event_t ev = {.type = INPUT_EVENT_BUTTON_LONG_TAP, .key = INPUT_KEY_ENTER};
    input_post_event(&ev);
    input_process_events();

    TEST_ASSERT_EQUAL(1, g_gesture_calls);
    TEST_ASSERT_EQUAL(INPUT_EVENT_BUTTON_LONG_TAP, g_gesture_last_type);

    app_ui_deinit(&ui);
    input_deinit();
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_app_ui_init_links_desc_from_real_app);
    RUN_TEST(test_app_ui_init_tolerates_null_real_app);
    RUN_TEST(test_bind_gesture_creates_recognizer_and_registers);
    RUN_TEST(test_bind_gesture_second_call_reuses_the_same_recognizer);
    RUN_TEST(test_bind_gesture_reuses_gpio_wiring_for_the_same_key);
    RUN_TEST(test_bind_gesture_forwarding_device_reaches_callback);
    return UNITY_END();
}
