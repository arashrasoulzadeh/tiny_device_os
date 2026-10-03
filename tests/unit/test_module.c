#ifndef _DARWIN_C_SOURCE
#define _DARWIN_C_SOURCE
#endif
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif
#include "unity.h"
#include "module.h"
#include <string.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

/* drivers/module.c (the .ardmod dynamic module format) had zero tests.
 * ardmod_create()/ardmod_load()/CRC verification/symbol lookup are real,
 * working code; ardmod_register_symbols()/ardmod_resolve_deps() are
 * explicit no-op stubs (their own comments say so) - the format can be
 * packaged, verified and introspected, but nothing makes a loaded
 * module's code actually callable or pulls in its declared dependencies
 * yet. Not attempting that here - real code relocation/execution is a
 * much larger, architecture-specific problem than this pass's scope. */

void setUp(void) {}
void tearDown(void) {}

static uint8_t g_code[] = {0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x01, 0x02, 0x03};

void test_create_then_load_round_trips(void) {
    ardmod_symbol_t symbols[] = {
        {.name = "entry", .address = 0, .type = 0},
    };
    uint8_t* packed = NULL;
    size_t packed_size = 0;
    TEST_ASSERT_EQUAL(0, ardmod_create("mymod", ARDMOD_TYPE_DRIVER, g_code, sizeof(g_code),
                                        symbols, 1, NULL, 0, &packed, &packed_size));
    TEST_ASSERT_NOT_NULL(packed);

    ardmod_handle_t* handle = ardmod_load(packed, packed_size);
    TEST_ASSERT_NOT_NULL(handle);
    TEST_ASSERT_EQUAL_STRING("mymod", ardmod_get_name(handle));
    TEST_ASSERT_EQUAL_STRING("1.0.0", ardmod_get_version(handle));

    ardmod_unload(handle);
    free(packed);
}

void test_load_rejects_bad_magic(void) {
    uint8_t* packed = NULL;
    size_t packed_size = 0;
    ardmod_create("mod", ARDMOD_TYPE_DRIVER, g_code, sizeof(g_code), NULL, 0, NULL, 0,
                   &packed, &packed_size);

    ardmod_header_t* header = (ardmod_header_t*)packed;
    header->magic = 0xBADF00D;
    // Recompute the CRC so this fails specifically on the magic check,
    // not incidentally on a stale CRC.
    header->crc32 = 0;
    uint32_t crc;
    ardmod_calculate_crc(packed, packed_size, &crc);
    header->crc32 = crc;

    TEST_ASSERT_NULL(ardmod_load(packed, packed_size));
    free(packed);
}

void test_load_rejects_corrupted_data(void) {
    uint8_t* packed = NULL;
    size_t packed_size = 0;
    ardmod_create("mod", ARDMOD_TYPE_DRIVER, g_code, sizeof(g_code), NULL, 0, NULL, 0,
                   &packed, &packed_size);

    packed[packed_size - 1] ^= 0xFF;  // flip a bit in the code payload

    TEST_ASSERT_NULL(ardmod_load(packed, packed_size));
    free(packed);
}

void test_load_rejects_too_small_buffer(void) {
    uint8_t tiny[4] = {0};
    TEST_ASSERT_NULL(ardmod_load(tiny, sizeof(tiny)));
}

void test_get_symbols_returns_registered_symbols(void) {
    ardmod_symbol_t symbols[] = {
        {.name = "foo", .address = 0x10, .type = 0},
        {.name = "bar", .address = 0x20, .type = 1},
    };
    uint8_t* packed = NULL;
    size_t packed_size = 0;
    ardmod_create("mod", ARDMOD_TYPE_LIBRARY, g_code, sizeof(g_code), symbols, 2, NULL, 0,
                   &packed, &packed_size);

    ardmod_handle_t* handle = ardmod_load(packed, packed_size);
    TEST_ASSERT_NOT_NULL(handle);

    ardmod_symbol_t out[4];
    size_t count = 0;
    TEST_ASSERT_EQUAL(0, ardmod_get_symbols(handle, out, 4, &count));
    TEST_ASSERT_EQUAL(2, count);
    TEST_ASSERT_EQUAL_STRING("foo", out[0].name);
    TEST_ASSERT_EQUAL_STRING("bar", out[1].name);

    ardmod_unload(handle);
    free(packed);
}

void test_get_symbol_finds_by_name(void) {
    ardmod_symbol_t symbols[] = {{.name = "entry", .address = 0x42, .type = 0}};
    uint8_t* packed = NULL;
    size_t packed_size = 0;
    ardmod_create("mod", ARDMOD_TYPE_DRIVER, g_code, sizeof(g_code), symbols, 1, NULL, 0,
                   &packed, &packed_size);

    ardmod_handle_t* handle = ardmod_load(packed, packed_size);
    TEST_ASSERT_NOT_NULL(ardmod_get_symbol(handle, "entry"));
    TEST_ASSERT_NULL(ardmod_get_symbol(handle, "nonexistent"));

    ardmod_unload(handle);
    free(packed);
}

/* Regression: ardmod_verify_crc() used to cast away const and write
 * header->crc32 = 0 directly into the caller's buffer (restoring it
 * after) to compute the CRC - fine for a heap copy, but a real module is
 * meant to be verified straight out of flash (XIP), where that write
 * would corrupt memory the caller doesn't expect touched, or fault
 * outright on genuinely read-only memory. This maps a page PROT_READ
 * only (mprotect) and verifies against it directly - would SIGSEGV
 * before the fix, passes now that nothing writes to `data`. */
void test_verify_crc_does_not_write_to_a_read_only_buffer(void) {
    uint8_t* packed = NULL;
    size_t packed_size = 0;
    ardmod_create("mod", ARDMOD_TYPE_DRIVER, g_code, sizeof(g_code), NULL, 0, NULL, 0,
                   &packed, &packed_size);

    long page_size = sysconf(_SC_PAGESIZE);
    void* page = mmap(NULL, (size_t)page_size, PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    TEST_ASSERT_NOT_NULL(page);
    memcpy(page, packed, packed_size);
    TEST_ASSERT_EQUAL(0, mprotect(page, (size_t)page_size, PROT_READ));

    TEST_ASSERT_TRUE(ardmod_verify_crc((const uint8_t*)page, packed_size));

    mprotect(page, (size_t)page_size, PROT_READ | PROT_WRITE);
    munmap(page, (size_t)page_size);
    free(packed);
}

/* register_symbols()/resolve_deps() are documented no-ops - this just
 * records that fact in a test instead of a comment nobody checks. */
void test_register_symbols_and_resolve_deps_are_still_stubs(void) {
    uint8_t* packed = NULL;
    size_t packed_size = 0;
    ardmod_create("mod", ARDMOD_TYPE_DRIVER, g_code, sizeof(g_code), NULL, 0, NULL, 0,
                   &packed, &packed_size);
    ardmod_handle_t* handle = ardmod_load(packed, packed_size);

    TEST_ASSERT_EQUAL(0, ardmod_register_symbols(handle));
    TEST_ASSERT_EQUAL(0, ardmod_resolve_deps(handle));
    // No way to observe either having actually done anything - that's
    // the point being flagged, not a claim that they work.

    ardmod_unload(handle);
    free(packed);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_create_then_load_round_trips);
    RUN_TEST(test_load_rejects_bad_magic);
    RUN_TEST(test_load_rejects_corrupted_data);
    RUN_TEST(test_load_rejects_too_small_buffer);
    RUN_TEST(test_get_symbols_returns_registered_symbols);
    RUN_TEST(test_get_symbol_finds_by_name);
    RUN_TEST(test_verify_crc_does_not_write_to_a_read_only_buffer);
    RUN_TEST(test_register_symbols_and_resolve_deps_are_still_stubs);
    return UNITY_END();
}
