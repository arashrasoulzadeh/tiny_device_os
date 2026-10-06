#include "unity.h"
#include "app_helper.h"
#include "app_runtime.h"
#include "scheduler.h"

#include <string.h>

void setUp(void) {
    scheduler_init();
}

void tearDown(void) {}

void test_set_state_round_trips_and_rejects_a_bad_buffer(void) {
    int value = 7;
    int loaded = 0;
    TEST_ASSERT_EQUAL(-1, app_set_state(NULL, &value, sizeof(value)));
    TEST_ASSERT_EQUAL(-1, app_set_state("counter", NULL, sizeof(value)));
    TEST_ASSERT_EQUAL(-1, app_set_state("counter", &value, 0));
    TEST_ASSERT_EQUAL(-1, app_get_state("missing", &loaded, sizeof(loaded)));

    TEST_ASSERT_EQUAL(0, app_set_state("counter", &value, sizeof(value)));
    TEST_ASSERT_EQUAL(0, app_get_state("counter", &loaded, sizeof(loaded)));
    TEST_ASSERT_EQUAL(7, loaded);

    value = 9;
    TEST_ASSERT_EQUAL(0, app_set_state("counter", &value, sizeof(value)));
    TEST_ASSERT_EQUAL(0, app_get_state("counter", &loaded, sizeof(loaded)));
    TEST_ASSERT_EQUAL(9, loaded);
    TEST_ASSERT_EQUAL(-1, app_get_state("counter", &loaded, sizeof(loaded) - 1));
}

void test_capture_and_apply_use_the_bound_object(void) {
    int live = 3;
    int other = 0;
    TEST_ASSERT_EQUAL(0, app_state_bind("demo", &live, sizeof(live)));
    TEST_ASSERT_EQUAL(-1, app_state_apply("demo"));

    live = 11;
    TEST_ASSERT_EQUAL(0, app_state_capture("demo"));
    live = 0;
    TEST_ASSERT_EQUAL(0, app_state_apply("demo"));
    TEST_ASSERT_EQUAL(11, live);

    TEST_ASSERT_EQUAL(0, app_get_state("demo", &other, sizeof(other)));
    TEST_ASSERT_EQUAL(11, other);
    TEST_ASSERT_EQUAL(0, app_state_capture("nobody"));
}

static int g_live = 4;

static void on_ready(app_helper_t* app) {
    TEST_ASSERT_TRUE(app_helper_has_state(app));
    TEST_ASSERT_EQUAL(4, g_live);
    g_live = 15;
}

void test_helper_loads_on_start_and_saves_on_quit(void) {
    app_helper_desc_t desc = {
        .name = "box",
        .title = "BOX",
        .state = &g_live,
        .state_size = sizeof(g_live),
        .on_ready = on_ready,
    };
    app_helper_t app;
    int loaded = 0;

    g_live = 4;
    TEST_ASSERT_EQUAL(0, app_set_state("box", &g_live, sizeof(g_live)));
    g_live = 0;

    TEST_ASSERT_EQUAL(0, app_helper_start(&app, NULL, &desc));
    TEST_ASSERT_EQUAL(15, g_live);
    app_helper_stop(&app);

    TEST_ASSERT_EQUAL(0, app_get_state("box", &loaded, sizeof(loaded)));
    TEST_ASSERT_EQUAL(15, loaded);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_set_state_round_trips_and_rejects_a_bad_buffer);
    RUN_TEST(test_capture_and_apply_use_the_bound_object);
    RUN_TEST(test_helper_loads_on_start_and_saves_on_quit);
    return UNITY_END();
}
