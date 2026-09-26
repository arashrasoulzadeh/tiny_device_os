#include "unity.h"
#include "status.h"

void setUp(void) {}
void tearDown(void) {}

void test_status_battery_clamps(void) {
    app_status_set_battery_percent(150);
    TEST_ASSERT_EQUAL(100, app_status_battery_percent());
    app_status_set_battery_percent(-10);
    TEST_ASSERT_EQUAL(0, app_status_battery_percent());
    app_status_set_battery_percent(42);
    TEST_ASSERT_EQUAL(42, app_status_battery_percent());
}

static int g_px;

static void count_px(int x, int y, bool on, void* user) {
    (void)x;
    (void)y;
    (void)user;
    if (on) {
        g_px++;
    }
}

void test_status_blit_draws_pixels(void) {
    g_px = 0;
    app_status_blit(128, 75, 2, count_px, NULL);
    TEST_ASSERT_TRUE(g_px > 20);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_status_battery_clamps);
    RUN_TEST(test_status_blit_draws_pixels);
    return UNITY_END();
}
