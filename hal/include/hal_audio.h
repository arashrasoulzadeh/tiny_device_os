#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    HAL_AUDIO_FORMAT_PCM_U8 = 0,
    HAL_AUDIO_FORMAT_PCM_S16_LE,
    HAL_AUDIO_FORMAT_PCM_S16_BE,
    HAL_AUDIO_FORMAT_PCM_S24_LE,
    HAL_AUDIO_FORMAT_PCM_S24_BE,
    HAL_AUDIO_FORMAT_PCM_S32_LE,
    HAL_AUDIO_FORMAT_PCM_S32_BE,
    HAL_AUDIO_FORMAT_PCM_F32_LE
} hal_audio_format_t;

typedef struct hal_audio hal_audio_t;

typedef void (*hal_audio_callback_t)(hal_audio_t* audio, void* buffer, size_t frames, void* arg);

typedef struct {
    uint32_t sample_rate;
    uint16_t channels;
    hal_audio_format_t format;
    uint16_t buffer_frames;
    uint16_t period_frames;
    bool output;
    bool input;
} hal_audio_config_t;

hal_audio_t* hal_audio_open(const char* path, const hal_audio_config_t* config);
void hal_audio_close(hal_audio_t* audio);

int hal_audio_start(hal_audio_t* audio);
int hal_audio_stop(hal_audio_t* audio);

int hal_audio_write(hal_audio_t* audio, const void* buffer, size_t frames);
int hal_audio_read(hal_audio_t* audio, void* buffer, size_t frames);

int hal_audio_set_callback(hal_audio_t* audio, hal_audio_callback_t cb, void* arg);

int hal_audio_get_buffered_frames(const hal_audio_t* audio);
int hal_audio_get_available_frames(const hal_audio_t* audio);

int hal_audio_set_volume(hal_audio_t* audio, float volume);
float hal_audio_get_volume(const hal_audio_t* audio);

int hal_audio_set_mute(hal_audio_t* audio, bool mute);
bool hal_audio_get_mute(const hal_audio_t* audio);

const char* hal_audio_get_path(const hal_audio_t* audio);

// Power management
int hal_audio_suspend(hal_audio_t* audio);
int hal_audio_resume(hal_audio_t* audio);

#ifdef __cplusplus
}
#endif