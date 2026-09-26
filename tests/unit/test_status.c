#include "unity.h"
#include "status.h"

#include <string.h>

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

void test_status_link_roundtrip(void) {
    app_status_set_link(APP_STATUS_LINK_UP, -60);
    TEST_ASSERT_EQUAL(APP_STATUS_LINK_UP, app_status_link());
    TEST_ASSERT_EQUAL(-60, app_status_rssi());
}

void test_signal_bars_scale_with_rssi(void) {
    TEST_ASSERT_EQUAL(0, app_status_signal_bars(APP_STATUS_LINK_OFF, -40));
    TEST_ASSERT_EQUAL(0, app_status_signal_bars(APP_STATUS_LINK_DOWN, -40));
    TEST_ASSERT_EQUAL(4, app_status_signal_bars(APP_STATUS_LINK_UP, -50));
    TEST_ASSERT_EQUAL(3, app_status_signal_bars(APP_STATUS_LINK_UP, -60));
    TEST_ASSERT_EQUAL(2, app_status_signal_bars(APP_STATUS_LINK_UP, -70));
    TEST_ASSERT_EQUAL(1, app_status_signal_bars(APP_STATUS_LINK_UP, -90));
}

static bool g_fb[128][8];

static void paint(int x, int y, bool on, void* user) {
    (void)user;
    if (!on || x < 0 || x >= 128 || y < 0 || y >= 8) {
        return;
    }
    g_fb[x][y] = true;
}

static int count_px;

static void count_on(int x, int y, bool on, void* user) {
    (void)x;
    (void)y;
    (void)user;
    if (on) {
        count_px++;
    }
}

void test_status_blit_draws_pixels(void) {
    count_px = 0;
    app_status_blit(128, 75, APP_STATUS_LINK_UP, -50, count_on, NULL);
    TEST_ASSERT_TRUE(count_px > 20);
}

void test_status_blit_wifi_and_signal(void) {
    const int wifi_x = APP_STATUS_WIFI_X(128);
    const int sig_x = APP_STATUS_SIGNAL_X(128);
    const int batt_x = APP_STATUS_BATT_X(128);
    int batt_px = 0;
    int x;
    int y;

    memset(g_fb, 0, sizeof(g_fb));
    app_status_blit(128, 80, APP_STATUS_LINK_UP, -50, paint, NULL);

    /* Full Wi-Fi fan: center dot on, slash pixel off. */
    TEST_ASSERT_TRUE(g_fb[wifi_x + 3][5]);
    TEST_ASSERT_FALSE(g_fb[wifi_x + 1][5]);
    /* Four signal bars: tallest bar reaches the top row. */
    TEST_ASSERT_TRUE(g_fb[sig_x + 6][0]);
    TEST_ASSERT_TRUE(g_fb[sig_x][6]);

    for (x = batt_x; x < batt_x + 11; x++) {
        for (y = 0; y < 8; y++) {
            if (g_fb[x][y]) {
                batt_px++;
            }
        }
    }
    TEST_ASSERT_TRUE(batt_px > 10);

    memset(g_fb, 0, sizeof(g_fb));
    app_status_blit(128, 80, APP_STATUS_LINK_DOWN, -40, paint, NULL);
    /* Disconnected: slash through the Wi-Fi glyph, no center dot, empty bars. */
    TEST_ASSERT_TRUE(g_fb[wifi_x + 1][5]);
    TEST_ASSERT_FALSE(g_fb[wifi_x + 3][5]);
    TEST_ASSERT_FALSE(g_fb[sig_x + 6][0]);
    TEST_ASSERT_TRUE(g_fb[sig_x + 6][6]);

    memset(g_fb, 0, sizeof(g_fb));
    app_status_blit(128, 80, APP_STATUS_LINK_UP, -90, paint, NULL);
    /* One bar: dot only, outer arc and tall bar stay off. */
    TEST_ASSERT_TRUE(g_fb[wifi_x + 3][5]);
    TEST_ASSERT_FALSE(g_fb[wifi_x + 1][0]);
    TEST_ASSERT_FALSE(g_fb[sig_x + 6][0]);
    TEST_ASSERT_TRUE(g_fb[sig_x][6]);
    TEST_ASSERT_TRUE(g_fb[sig_x][5]);
}

void test_redraw_capped_at_10hz(void) {
    TEST_ASSERT_TRUE(app_status_redraw_due(0, 0, false));
    TEST_ASSERT_FALSE(app_status_redraw_due(50, 0, true));
    TEST_ASSERT_FALSE(app_status_redraw_due(99, 0, true));
    TEST_ASSERT_TRUE(app_status_redraw_due(100, 0, true));
    TEST_ASSERT_TRUE(app_status_redraw_due(250, 100, true));
    /* millis wrap */
    TEST_ASSERT_FALSE(app_status_redraw_due(0x10u, 0xFFFFFFF0u, true));
    TEST_ASSERT_TRUE(app_status_redraw_due(200u, 0xFFFFFF00u, true));
}

void test_signal_bars_hold_before_repaint(void) {
    int pending = 0;
    uint32_t since = 0;

    TEST_ASSERT_EQUAL(0, app_status_stable_bars(0, 4, 1000, &pending, &since));
    TEST_ASSERT_EQUAL(4, pending);
    TEST_ASSERT_EQUAL(0, app_status_stable_bars(0, 4, 1000 + APP_STATUS_BAR_HOLD_MS - 1, &pending,
                                                &since));
    TEST_ASSERT_EQUAL(4, app_status_stable_bars(0, 4, 1000 + APP_STATUS_BAR_HOLD_MS, &pending,
                                                &since));

    /* A bounce back to the shown count cancels the pending change. */
    pending = 2;
    since = 0;
    TEST_ASSERT_EQUAL(3, app_status_stable_bars(3, 1, 10, &pending, &since));
    TEST_ASSERT_EQUAL(3, app_status_stable_bars(3, 3, 11, &pending, &since));
    TEST_ASSERT_EQUAL(3, pending);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_status_battery_clamps);
    RUN_TEST(test_status_link_roundtrip);
    RUN_TEST(test_signal_bars_scale_with_rssi);
    RUN_TEST(test_status_blit_draws_pixels);
    RUN_TEST(test_status_blit_wifi_and_signal);
    RUN_TEST(test_redraw_capped_at_10hz);
    RUN_TEST(test_signal_bars_hold_before_repaint);
    return UNITY_END();
}
