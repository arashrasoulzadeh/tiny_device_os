#include "unity.h"
#include "event.h"
#include "fw/events.h"
#include "scheduler.h"
#include <string.h>

/* kernel/event.c existed before this test file but had zero coverage and
 * no callers anywhere in the codebase - this is the first real exercise of
 * it (pub/sub, priority ordering, the async queue, and app-scoped cleanup). */

void setUp(void) {
    scheduler_init();
    event_system_init();
}

void tearDown(void) {
    event_system_deinit();
}

static int g_calls;
static uint32_t g_last_size;
static char g_last_data[EVENT_MAX_DATA];
static event_type_t g_last_type;

static void record_cb(const event_t* event, void* arg) {
    (void)arg;
    g_calls++;
    g_last_size = event->data_size;
    g_last_type = event->type;
    if (event->data_size > 0) {
        memcpy(g_last_data, event->data, event->data_size);
    }
}

void test_sync_publish_with_no_subscribers_is_a_safe_noop(void) {
    TEST_ASSERT_EQUAL(0, event_publish_sync("nobody.home", NULL, 0, EVENT_TYPE_CUSTOM, 1));
}

void test_sync_publish_delivers_data_to_subscriber(void) {
    g_calls = 0;
    TEST_ASSERT_EQUAL(0, event_subscribe(1, "battery.changed", record_cb, NULL, EVENT_PRIO_NORMAL));

    int level = 42;
    TEST_ASSERT_EQUAL(1, event_publish_sync("battery.changed", &level, sizeof(level),
                                             EVENT_TYPE_POWER, 99));
    TEST_ASSERT_EQUAL(1, g_calls);
    TEST_ASSERT_EQUAL(sizeof(level), g_last_size);
    TEST_ASSERT_EQUAL(EVENT_TYPE_POWER, g_last_type);
    int got;
    memcpy(&got, g_last_data, sizeof(got));
    TEST_ASSERT_EQUAL(42, got);
}

void test_topics_are_independent(void) {
    g_calls = 0;
    TEST_ASSERT_EQUAL(0, event_subscribe(1, "topic.a", record_cb, NULL, EVENT_PRIO_NORMAL));
    TEST_ASSERT_EQUAL(0, event_publish_sync("topic.b", NULL, 0, EVENT_TYPE_CUSTOM, 1));
    TEST_ASSERT_EQUAL(0, g_calls);
}

void test_unsubscribe_stops_delivery(void) {
    g_calls = 0;
    TEST_ASSERT_EQUAL(0, event_subscribe(1, "topic.a", record_cb, NULL, EVENT_PRIO_NORMAL));
    TEST_ASSERT_EQUAL(0, event_unsubscribe(1, "topic.a"));
    TEST_ASSERT_EQUAL(0, event_publish_sync("topic.a", NULL, 0, EVENT_TYPE_CUSTOM, 1));
    TEST_ASSERT_EQUAL(0, g_calls);
}

static int g_order[4];
static int g_order_count;
static void order_cb_lo(const event_t* e, void* arg) { (void)e; (void)arg; g_order[g_order_count++] = 0; }
static void order_cb_hi(const event_t* e, void* arg) { (void)e; (void)arg; g_order[g_order_count++] = 1; }
static void order_cb_crit(const event_t* e, void* arg) { (void)e; (void)arg; g_order[g_order_count++] = 2; }

void test_higher_priority_subscribers_are_delivered_first(void) {
    g_order_count = 0;
    TEST_ASSERT_EQUAL(0, event_subscribe(1, "topic.a", order_cb_lo, NULL, EVENT_PRIO_LOW));
    TEST_ASSERT_EQUAL(0, event_subscribe(2, "topic.a", order_cb_crit, NULL, EVENT_PRIO_CRITICAL));
    TEST_ASSERT_EQUAL(0, event_subscribe(3, "topic.a", order_cb_hi, NULL, EVENT_PRIO_HIGH));

    TEST_ASSERT_EQUAL(3, event_publish_sync("topic.a", NULL, 0, EVENT_TYPE_CUSTOM, 1));
    TEST_ASSERT_EQUAL(3, g_order_count);
    TEST_ASSERT_EQUAL(2, g_order[0]);  // critical
    TEST_ASSERT_EQUAL(1, g_order[1]);  // high
    TEST_ASSERT_EQUAL(0, g_order[2]);  // low
}

void test_async_publish_is_not_delivered_until_queue_processed(void) {
    g_calls = 0;
    TEST_ASSERT_EQUAL(0, event_subscribe(1, "topic.a", record_cb, NULL, EVENT_PRIO_NORMAL));
    TEST_ASSERT_EQUAL(0, event_publish("topic.a", NULL, 0, EVENT_TYPE_CUSTOM, 1));
    TEST_ASSERT_EQUAL(0, g_calls);

    TEST_ASSERT_EQUAL(1, event_process_queue());
    TEST_ASSERT_EQUAL(1, g_calls);
}

void test_cleanup_app_deactivates_its_subscriptions(void) {
    g_calls = 0;
    TEST_ASSERT_EQUAL(0, event_subscribe(5, "topic.a", record_cb, NULL, EVENT_PRIO_NORMAL));
    event_cleanup_app(5);
    TEST_ASSERT_EQUAL(0, event_publish_sync("topic.a", NULL, 0, EVENT_TYPE_CUSTOM, 1));
    TEST_ASSERT_EQUAL(0, g_calls);
}

void test_os_event_publish_reaches_a_subscriber(void) {
    uint8_t key = 3;
    g_calls = 0;
    TEST_ASSERT_EQUAL(0, os_event_subscribe(1, FW_EVENT_INPUT, record_cb, NULL));
    TEST_ASSERT_EQUAL(1, os_event_publish(FW_EVENT_INPUT, &key, sizeof(key)));
    TEST_ASSERT_EQUAL(1, g_calls);
    TEST_ASSERT_EQUAL(EVENT_TYPE_UI, g_last_type);
    TEST_ASSERT_EQUAL_UINT32(sizeof(key), g_last_size);
    TEST_ASSERT_EQUAL(0, os_event_unsubscribe(1, FW_EVENT_INPUT));
    TEST_ASSERT_EQUAL(0, os_event_publish(FW_EVENT_INPUT, &key, sizeof(key)));
    TEST_ASSERT_EQUAL(1, g_calls);
}

void test_publish_rejects_oversized_payload(void) {
    char big[EVENT_MAX_DATA + 1] = {0};
    TEST_ASSERT_EQUAL(-1, event_publish_sync("topic.a", big, sizeof(big), EVENT_TYPE_CUSTOM, 1));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_sync_publish_with_no_subscribers_is_a_safe_noop);
    RUN_TEST(test_sync_publish_delivers_data_to_subscriber);
    RUN_TEST(test_topics_are_independent);
    RUN_TEST(test_unsubscribe_stops_delivery);
    RUN_TEST(test_higher_priority_subscribers_are_delivered_first);
    RUN_TEST(test_async_publish_is_not_delivered_until_queue_processed);
    RUN_TEST(test_cleanup_app_deactivates_its_subscriptions);
    RUN_TEST(test_os_event_publish_reaches_a_subscriber);
    RUN_TEST(test_publish_rejects_oversized_payload);
    return UNITY_END();
}
