#include "app.h"
#include "link_bridge.h"
#include "link_session.h"
#include "os_clock.h"
#include "power.h"
#include "scheduler.h"
#include "sensor_service.h"
#include "unity.h"

#include <stdint.h>
#include <string.h>

static const uint8_t KEY[16] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};

static void dummy_entry(void *arg) { (void)arg; }

void setUp(void) {}
void tearDown(void) {}

static int collect(link_session_t *host, const uint8_t *buf, int n, char *out, size_t cap) {
    size_t off = 0;
    size_t used = 0;
    int saw_event = 0;

    out[0] = '\0';
    while (off < (size_t)n) {
        link_frame_t frame;
        link_frame_t plain;
        int got = link_frame_decode(buf + off, (size_t)n - off, &frame);
        if (got < 0) {
            break;
        }
        off += (size_t)got;
        if (frame.channel == LINK_CH_EVENT) {
            saw_event = 1;
        }
        if (!(frame.flags & LINK_FLAG_ENCRYPTED)) {
            continue;
        }
        if (link_session_open(host, &frame, &plain) != 0) {
            continue;
        }
        if (plain.channel == LINK_CH_EVENT) {
            saw_event = 1;
        }
        if (plain.channel != LINK_CH_TTY || used + plain.length + 1 > cap) {
            continue;
        }
        memcpy(out + used, plain.payload, plain.length);
        used += plain.length;
        out[used] = '\0';
    }
    return saw_event;
}

static int handshake(link_session_t *host, char *text, size_t cap) {
    uint8_t hello[80];
    uint8_t tx[1024];
    link_frame_t frame;
    int n;
    int rn;
    int used;

    link_session_init(host, KEY, 1, LINK_DIR_HOST);
    n = link_session_encode_hello(host, 0, 1, hello, sizeof(hello));
    if (n <= 0) {
        return -1;
    }
    rn = link_bridge_rx(hello, (size_t)n, tx, sizeof(tx));
    if (rn <= 0) {
        return -1;
    }
    used = link_frame_decode(tx, (size_t)rn, &frame);
    if (used <= 0 || link_session_accept_hello(host, &frame, 0) != 0) {
        return -1;
    }
    return collect(host, tx + used, rn - used, text, cap);
}

static int feed(link_session_t *host, uint16_t *msg, const char *line, char *out, size_t cap) {
    uint8_t wire[LINK_MAX_FRAME];
    uint8_t tx[1024];
    int sealed;
    int got;

    sealed = link_session_seal(host, LINK_CH_TTY, (*msg)++, (const uint8_t *)line, strlen(line), wire,
                              sizeof(wire));
    if (sealed <= 0) {
        return -1;
    }
    got = link_bridge_rx(wire, (size_t)sealed, tx, sizeof(tx));
    if (got <= 0) {
        return -1;
    }
    return collect(host, tx, got, out, cap);
}

void test_sensors_wait_until_asked(void) {
    link_session_t host;
    uint8_t tx[64];
    char text[512];
    uint16_t msg = 2;

    scheduler_init();
    TEST_ASSERT_EQUAL_INT(0, app_init());
    sensor_service_reset();
    TEST_ASSERT_EQUAL_INT(0, sensor_service_add("cpu", SENSOR_TYPE_CPU, "", 0));
    link_bridge_init(KEY, 1);
    TEST_ASSERT_EQUAL_INT(0, handshake(&host, text, sizeof(text)));
    TEST_ASSERT_NOT_NULL(strstr(text, "ardubot$ "));
    TEST_ASSERT_NULL(strstr(text, "cpu"));
    TEST_ASSERT_EQUAL_INT(0, link_bridge_poll(1000, tx, sizeof(tx)));

    TEST_ASSERT_EQUAL_INT(0, feed(&host, &msg, "sensors\n", text, sizeof(text)));
    TEST_ASSERT_NOT_NULL(strstr(text, "cpu"));
}

void test_apps_switch_key_kill_top_and_date(void) {
    link_session_t host;
    app_manifest_t manifest;
    char text[1024];
    uint16_t msg = 2;
    time_t before;

    scheduler_init();
    power_init();
    TEST_ASSERT_EQUAL_INT(0, app_init());
    sensor_service_reset();
    memset(&manifest, 0, sizeof(manifest));
    memcpy(manifest.name, "sensors", 8);
    manifest.entry_point = (uintptr_t)dummy_entry;
    manifest.stack_size = 4096;
    TEST_ASSERT_EQUAL_INT(0, app_install_manifest(&manifest, "sensors"));
    link_bridge_init(KEY, 1);
    TEST_ASSERT_EQUAL_INT(0, handshake(&host, text, sizeof(text)));

    TEST_ASSERT_EQUAL_INT(0, feed(&host, &msg, "apps\n", text, sizeof(text)));
    TEST_ASSERT_NOT_NULL(strstr(text, "sensors"));

    TEST_ASSERT_EQUAL_INT(0, feed(&host, &msg, "switch sensors\n", text, sizeof(text)));
    TEST_ASSERT_NULL(strstr(text, "unknown app"));
    TEST_ASSERT_EQUAL_INT(0, feed(&host, &msg, "key enter\n", text, sizeof(text)));
    TEST_ASSERT_NOT_NULL(strstr(text, "key down"));

    TEST_ASSERT_EQUAL_INT(0, feed(&host, &msg, "kill missing\n", text, sizeof(text)));
    TEST_ASSERT_NOT_NULL(strstr(text, "unknown app"));

    TEST_ASSERT_EQUAL_INT(0, feed(&host, &msg, "top\n", text, sizeof(text)));
    TEST_ASSERT_NOT_NULL(strstr(text, "idle"));

    before = os_clock_now();
    TEST_ASSERT_EQUAL_INT(0, feed(&host, &msg, "date set 1700000000\n", text, sizeof(text)));
    TEST_ASSERT_NOT_NULL(strstr(text, "1700000000"));
    TEST_ASSERT_INT_WITHIN(2, 1700000000, (int)os_clock_now());
    TEST_ASSERT_TRUE(os_clock_now() != before);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_sensors_wait_until_asked);
    RUN_TEST(test_apps_switch_key_kill_top_and_date);
    return UNITY_END();
}
