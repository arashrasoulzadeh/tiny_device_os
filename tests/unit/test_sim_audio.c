#include "unity.h"
#include "sim_audio.h"
#include <string.h>

void setUp(void) {
}

void tearDown(void) {
    sim_audio_cleanup();
}

void test_audio_init_cleanup(void) {
    int ret = sim_audio_init(44100, 2, 512);
    TEST_ASSERT_EQUAL(0, ret);
    
    sim_audio_cleanup();
}

static void audio_callback(void* buf, uint32_t frames, void* arg) {
    *(int*)arg = 1;
    memset(buf, 0, frames * 4);
}

void test_audio_callback(void) {
    int cb_called = 0;
    uint8_t buffer[1024];
    
    sim_audio_init(44100, 2, 512);
    sim_audio_set_callback(audio_callback, &cb_called);
    
    sim_audio_start();
    sim_audio_stop();
    
    TEST_ASSERT_EQUAL(1, cb_called);
    
    sim_audio_cleanup();
}

void test_audio_volume(void) {
    sim_audio_init(44100, 2, 512);
    
    sim_audio_set_volume(0.5f);
    TEST_ASSERT_EQUAL_FLOAT(0.5f, sim_audio_get_volume());
    
    sim_audio_set_volume(1.5f);
    TEST_ASSERT_EQUAL_FLOAT(1.0f, sim_audio_get_volume());
    
    sim_audio_set_volume(-0.5f);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, sim_audio_get_volume());
    
    sim_audio_cleanup();
}

void test_audio_mute(void) {
    sim_audio_init(44100, 2, 512);
    
    TEST_ASSERT_FALSE(sim_audio_get_mute());
    
    sim_audio_set_mute(true);
    TEST_ASSERT_TRUE(sim_audio_get_mute());
    
    sim_audio_set_mute(false);
    TEST_ASSERT_FALSE(sim_audio_get_mute());
    
    sim_audio_cleanup();
}

int main(void) {
    UNITY_BEGIN();
    
    RUN_TEST(test_audio_init_cleanup);
    RUN_TEST(test_audio_callback);
    RUN_TEST(test_audio_volume);
    RUN_TEST(test_audio_mute);
    
    return UNITY_END();
}