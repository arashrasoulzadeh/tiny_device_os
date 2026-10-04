#include "unity.h"
#include "alloc.h"
#include <string.h>
#include <stdint.h>

void setUp(void) {
}

void tearDown(void) {
}

void test_os_malloc_should_return_aligned_pointer(void) {
    void* ptr = os_malloc(32);
    TEST_ASSERT_NOT_NULL(ptr);
    TEST_ASSERT_EQUAL_UINT(0, (uintptr_t)ptr % 4);
    os_free(ptr);
}

void test_os_calloc_should_zero_memory(void) {
    int* arr = os_calloc(10, sizeof(int));
    TEST_ASSERT_NOT_NULL(arr);
    
    for (int i = 0; i < 10; i++) {
        TEST_ASSERT_EQUAL(0, arr[i]);
    }
    
    os_free(arr);
}

void test_os_realloc_should_grow(void) {
    int* arr = os_malloc(10 * sizeof(int));
    TEST_ASSERT_NOT_NULL(arr);
    
    for (int i = 0; i < 10; i++) arr[i] = i;
    
    int* new_arr = os_realloc(arr, 20 * sizeof(int));
    TEST_ASSERT_NOT_NULL(new_arr);
    
    for (int i = 0; i < 10; i++) {
        TEST_ASSERT_EQUAL(i, new_arr[i]);
    }
    
    os_free(new_arr);
}

void test_os_realloc_should_shrink(void) {
    int* arr = os_malloc(20 * sizeof(int));
    TEST_ASSERT_NOT_NULL(arr);
    
    int* new_arr = os_realloc(arr, 5 * sizeof(int));
    TEST_ASSERT_NOT_NULL(new_arr);
    
    os_free(new_arr);
}

void test_os_free_null_should_be_safe(void) {
    os_free(NULL);
}

void test_tlsf_pool_operations(void) {
    uint8_t mem[1024];
    tlsf_pool_t* pool = tlsf_create(mem, sizeof(mem));
    TEST_ASSERT_NOT_NULL(pool);
    
    void* ptr1 = tlsf_malloc(pool, 100);
    TEST_ASSERT_NOT_NULL(ptr1);
    
    void* ptr2 = tlsf_malloc(pool, 200);
    TEST_ASSERT_NOT_NULL(ptr2);
    
    tlsf_free(pool, ptr1);
    tlsf_free(pool, ptr2);
    
    size_t free = tlsf_pool_free_size(pool);
    TEST_ASSERT_GREATER_THAN(0, free);
    
    tlsf_destroy(pool);
}

void test_tlsf_memalign(void) {
    uint8_t mem[1024];
    tlsf_pool_t* pool = tlsf_create(mem, sizeof(mem));
    TEST_ASSERT_NOT_NULL(pool);
    
    void* ptr = tlsf_memalign(pool, 16, 100);
    TEST_ASSERT_NOT_NULL(ptr);
    TEST_ASSERT_EQUAL_UINT(0, (uintptr_t)ptr % 16);
    
    tlsf_free(pool, ptr);
    tlsf_destroy(pool);
}

void test_tlsf_memalign_across_many_offsets(void) {
    /* tlsf_memalign used to (a) return the wrong, unaligned block when
     * the discarded-prefix offset was >= MIN_BLOCK_SIZE (it returned
     * the discarded front remainder's pointer instead of the aligned
     * block's), and (b) get the front/aligned block sizes wrong by
     * BLOCK_OVERHEAD, overlapping the aligned block's own header - both
     * confirmed via a standalone sweep. Sweeping pool fragmentation
     * amounts here forces every possible offset case through a single
     * pool, including both a too-small-to-split offset and a
     * large-enough one. */
    static uint8_t mem[4096];

    for (size_t pad = 1; pad <= 200; pad += 7) {
        tlsf_pool_t* pool = tlsf_create(mem, sizeof(mem));
        TEST_ASSERT_NOT_NULL(pool);

        void* junk = tlsf_malloc(pool, pad);
        void* sentinel = tlsf_malloc(pool, 32);
        TEST_ASSERT_NOT_NULL(sentinel);
        memset(sentinel, 0xCD, 32);

        void* ptr = tlsf_memalign(pool, 64, 100);
        TEST_ASSERT_NOT_NULL(ptr);
        TEST_ASSERT_EQUAL_UINT(0, (uintptr_t)ptr % 64);

        memset(ptr, 0xAB, 100);

        unsigned char* s = (unsigned char*)sentinel;
        for (int i = 0; i < 32; i++) {
            TEST_ASSERT_EQUAL_UINT8(0xCD, s[i]);
        }

        TEST_ASSERT_TRUE(tlsf_pool_check(pool));

        tlsf_free(pool, ptr);
        if (junk) tlsf_free(pool, junk);
        tlsf_free(pool, sentinel);
    }
}

void test_heap_tracking(void) {
    size_t free_before = os_get_free_heap();
    
    void* ptr = os_malloc(512);
    TEST_ASSERT_NOT_NULL(ptr);
    
    size_t free_after = os_get_free_heap();
    TEST_ASSERT_LESS_THAN(free_before, free_after);
    
    os_free(ptr);
    
    size_t free_final = os_get_free_heap();
    TEST_ASSERT_EQUAL(free_before, free_final);
}

int main(void) {
    UNITY_BEGIN();
    
    RUN_TEST(test_os_malloc_should_return_aligned_pointer);
    RUN_TEST(test_os_calloc_should_zero_memory);
    RUN_TEST(test_os_realloc_should_grow);
    RUN_TEST(test_os_realloc_should_shrink);
    RUN_TEST(test_os_free_null_should_be_safe);
    RUN_TEST(test_tlsf_pool_operations);
    RUN_TEST(test_tlsf_memalign);
    RUN_TEST(test_tlsf_memalign_across_many_offsets);
    RUN_TEST(test_heap_tracking);
    
    return UNITY_END();
}