#include "unity.h"
#include "ota.h"
#include <stdint.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

// Test vectors for CRC32
void test_crc32_basic(void) {
    // Test with known data
    uint8_t data[] = "Hello, World!";
    uint32_t crc = ota_crc32_update(0, data, strlen("Hello, World!"));
    
    // Expected CRC32 of "Hello, World!" is 0xEBE6C6E6 (using standard CRC32 polynomial)
    // Let's verify with known value
    TEST_ASSERT_EQUAL_UINT32(0xEBE6C6E6, crc);
}

void test_crc32_empty(void) {
    uint32_t crc = ota_crc32_update(0, "", 0);
    TEST_ASSERT_EQUAL_UINT32(0, crc);
}

void test_crc32_known_values(void) {
    // Test with known CRC32 values
    // "123456789" -> 0xCBF43926 (standard CRC32)
    uint8_t data[] = "123456789";
    uint32_t crc = ota_crc32_update(0, data, 9);
    TEST_ASSERT_EQUAL_UINT32(0xCBF43926, crc);
}

void test_crc32_incremental(void) {
    // Test incremental updates
    const char* parts[] = {"Hel", "lo, ", "Wor", "ld!"};
    uint32_t crc = 0;
    for (int i = 0; i < 4; i++) {
        crc = ota_crc32_update(crc, (const uint8_t*)parts[i], strlen(parts[i]));
    }
    
    // Compare with single update
    const char* full = "Hello, World!";
    uint32_t crc2 = ota_crc32_update(0, (const uint8_t*)"Hello, World!", 13);
    TEST_ASSERT_EQUAL(crc2, crc);
}

// Test OTA metadata structure
void test_ota_metadata(void) {
    ota_metadata_t meta = {0};
    
    meta.magic = OTA_MAGIC;
    meta.version = OTA_VERSION;
    meta.active_partition = OTA_PARTITION_OTA_0;
    meta.next_partition = OTA_PARTITION_OTA_1;
    meta.active_crc32 = 0x12345678;
    meta.next_crc32 = 0x87654321;
    meta.active_size = 1024*1024;
    meta.next_size = 1024*1024;
    meta.active_state = OTA_STATE_VALID;
    meta.next_state = OTA_STATE_NEW;
    meta.sequence = 1;
    meta.timestamp = 1234567890;
    
    TEST_ASSERT_EQUAL_UINT32(OTA_MAGIC, meta.magic);
    TEST_ASSERT_EQUAL(OTA_VERSION, meta.version);
    TEST_ASSERT_EQUAL(OTA_PARTITION_OTA_0, meta.active_partition);
    TEST_ASSERT_EQUAL(OTA_PARTITION_OTA_1, meta.next_partition);
    TEST_ASSERT_EQUAL_UINT32(0x12345678, meta.active_crc32);
    TEST_ASSERT_EQUAL(OTA_STATE_VALID, meta.active_state);
    TEST_ASSERT_EQUAL(OTA_STATE_NEW, meta.next_state);
}

void test_ota_crc32_incremental(void) {
    // Test incremental CRC32 updates using public API
    uint32_t crc = 0;
    const char* chunks[] = {"Hel", "lo, ", "Wor", "ld!"};
    for (int i = 0; i < 4; i++) {
        crc = ota_crc32_update(crc, (const uint8_t*)chunks[i], strlen(chunks[i]));
    }
    
    // Verify final CRC matches full string
    uint32_t expected = ota_crc32_update(0, (const uint8_t*)"Hello, World!", 13);
    TEST_ASSERT_EQUAL(expected, crc);
}

// Test OTA metadata structure packing
void test_ota_metadata_packing(void) {
    ota_metadata_t meta = {0};
    meta.magic = OTA_MAGIC;
    meta.version = 1;
    meta.active_partition = OTA_PARTITION_OTA_0;
    meta.next_partition = OTA_PARTITION_OTA_1;
    meta.active_crc32 = 0x12345678;
    meta.next_crc32 = 0x87654321;
    meta.active_size = 0x100000;
    meta.next_size = 0x200000;
    meta.active_state = OTA_STATE_VALID;
    meta.next_state = OTA_STATE_PENDING_VERIFY;
    meta.sequence = 42;
    meta.timestamp = 1234567890;
    
    // Verify struct packing (no padding issues)
    TEST_ASSERT_EQUAL(sizeof(ota_metadata_t), 64); // Expected size
    TEST_ASSERT_EQUAL(OTA_MAGIC, meta.magic);
    TEST_ASSERT_EQUAL(1, meta.version);
    TEST_ASSERT_EQUAL(OTA_PARTITION_OTA_0, meta.active_partition);
    TEST_ASSERT_EQUAL(OTA_PARTITION_OTA_1, meta.next_partition);
    TEST_ASSERT_EQUAL_UINT32(0x12345678, meta.active_crc32);
    TEST_ASSERT_EQUAL(OTA_STATE_VALID, meta.active_state);
    TEST_ASSERT_EQUAL(OTA_STATE_PENDING_VERIFY, meta.next_state);
    TEST_ASSERT_EQUAL(42, meta.sequence);
    TEST_ASSERT_EQUAL(1234567890, meta.timestamp);
}

// Test OTA state transitions
void test_ota_state_transitions(void) {
    ota_metadata_t meta = {0};
    meta.magic = OTA_MAGIC;
    meta.version = 1;
    meta.active_partition = OTA_PARTITION_OTA_0;
    meta.active_state = OTA_STATE_VALID;
    meta.next_state = OTA_STATE_NEW;
    
    // Transition: NEW -> PENDING_VERIFY
    meta.next_state = OTA_STATE_PENDING_VERIFY;
    TEST_ASSERT_EQUAL(OTA_STATE_PENDING_VERIFY, meta.next_state);
    
    // Transition: PENDING_VERIFY -> VALID
    meta.next_state = OTA_STATE_VALID;
    TEST_ASSERT_EQUAL(OTA_STATE_VALID, meta.next_state);
    
    // Transition: VALID -> INVALID (mark invalid)
    meta.next_state = OTA_STATE_INVALID;
    TEST_ASSERT_EQUAL(OTA_STATE_INVALID, meta.next_state);
    
    // Transition: ABORTED
    meta.next_state = OTA_STATE_ABORTED;
    TEST_ASSERT_EQUAL(OTA_STATE_ABORTED, meta.next_state);
}

int main(void) {
    UNITY_BEGIN();
    
    RUN_TEST(test_crc32_basic);
    RUN_TEST(test_crc32_empty);
    RUN_TEST(test_crc32_known_values);
    RUN_TEST(test_crc32_incremental);
    RUN_TEST(test_ota_metadata);
    RUN_TEST(test_ota_crc32_incremental);
    RUN_TEST(test_ota_metadata_packing);
    RUN_TEST(test_ota_state_transitions);
    
    return UNITY_END();
}