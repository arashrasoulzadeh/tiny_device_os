#include "unity.h"
#include "alloc.h"

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
    RUN_TEST(test_heap_tracking);
    
    return UNITY_END();
}