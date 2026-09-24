#include "hal_audio.h"
#include <stdlib.h>
#include <string.h>

#define AUDIO_BUFFER_SIZE 4096

struct hal_audio {
    char path[64];
    hal_audio_config_t config;
    uint8_t buffer[AUDIO_BUFFER_SIZE];
    size_t buffer_head, buffer_tail;
    hal_audio_callback_t callback;
    void* callback_arg;
    bool running;
    float volume;
    bool mute;
};

hal_audio_t* hal_audio_open(const char* path, const hal_audio_config_t* config) {
    hal_audio_t* audio = calloc(1, sizeof(hal_audio_t));
    if (!audio) return NULL;
    
    strncpy(audio->path, path, sizeof(audio->path) - 1);
    if (config) {
        audio->config = *config;
    } else {
        audio->config.sample_rate = 44100;
        audio->config.channels = 2;
        audio->config.format = HAL_AUDIO_FORMAT_PCM_S16_LE;
        audio->config.buffer_frames = 512;
        audio->config.period_frames = 256;
        audio->config.output = true;
        audio->config.input = false;
    }
    
    audio->volume = 1.0f;
    audio->mute = false;
    audio->running = false;
    
    return audio;
}

void hal_audio_close(hal_audio_t* audio) {
    if (audio) free(audio);
}

int hal_audio_start(hal_audio_t* audio) {
    if (!audio) return -1;
    audio->running = true;
    audio->buffer_head = audio->buffer_tail = 0;
    return 0;
}

int hal_audio_stop(hal_audio_t* audio) {
    if (!audio) return -1;
    audio->running = false;
    return 0;
}

int hal_audio_write(hal_audio_t* audio, const void* buffer, size_t frames) {
    if (!audio || !buffer || !audio->running) return -1;
    
    size_t frame_bytes = 0;
    switch (audio->config.format) {
        case HAL_AUDIO_FORMAT_PCM_U8: frame_bytes = 1; break;
        case HAL_AUDIO_FORMAT_PCM_S16_LE:
        case HAL_AUDIO_FORMAT_PCM_S16_BE: frame_bytes = 2; break;
        case HAL_AUDIO_FORMAT_PCM_S24_LE:
        case HAL_AUDIO_FORMAT_PCM_S24_BE: frame_bytes = 3; break;
        case HAL_AUDIO_FORMAT_PCM_S32_LE:
        case HAL_AUDIO_FORMAT_PCM_S32_BE:
        case HAL_AUDIO_FORMAT_PCM_F32_LE: frame_bytes = 4; break;
    }
    frame_bytes *= audio->config.channels;
    
    size_t bytes = frames * frame_bytes;
    size_t free = AUDIO_BUFFER_SIZE - audio->buffer_head + audio->buffer_tail;
    if (audio->buffer_head >= audio->buffer_tail) {
        free = AUDIO_BUFFER_SIZE - (audio->buffer_head - audio->buffer_tail);
    }
    
    if (bytes > free) bytes = free;
    
    memcpy(audio->buffer + audio->buffer_head, buffer, bytes);
    audio->buffer_head = (audio->buffer_head + bytes) % AUDIO_BUFFER_SIZE;
    
    return (int)(bytes / frame_bytes);
}

int hal_audio_read(hal_audio_t* audio, void* buffer, size_t frames) {
    (void)audio; (void)buffer; (void)frames;
    return 0;
}

int hal_audio_set_callback(hal_audio_t* audio, hal_audio_callback_t cb, void* arg) {
    if (!audio) return -1;
    audio->callback = cb;
    audio->callback_arg = arg;
    return 0;
}

int hal_audio_get_buffered_frames(const hal_audio_t* audio) {
    if (!audio) return 0;
    size_t bytes = 0;
    if (audio->buffer_head >= audio->buffer_tail) {
        bytes = audio->buffer_head - audio->buffer_tail;
    } else {
        bytes = AUDIO_BUFFER_SIZE - audio->buffer_tail + audio->buffer_head;
    }
    
    size_t frame_bytes = 0;
    switch (audio->config.format) {
        case HAL_AUDIO_FORMAT_PCM_U8: frame_bytes = 1; break;
        case HAL_AUDIO_FORMAT_PCM_S16_LE:
        case HAL_AUDIO_FORMAT_PCM_S16_BE: frame_bytes = 2; break;
        case HAL_AUDIO_FORMAT_PCM_S24_LE:
        case HAL_AUDIO_FORMAT_PCM_S24_BE: frame_bytes = 3; break;
        case HAL_AUDIO_FORMAT_PCM_S32_LE:
        case HAL_AUDIO_FORMAT_PCM_S32_BE:
        case HAL_AUDIO_FORMAT_PCM_F32_LE: frame_bytes = 4; break;
    }
    frame_bytes *= audio->config.channels;
    
    return frame_bytes ? (int)(bytes / frame_bytes) : 0;
}

int hal_audio_get_available_frames(const hal_audio_t* audio) {
    if (!audio) return 0;
    return audio->config.buffer_frames - hal_audio_get_buffered_frames(audio);
}

int hal_audio_set_volume(hal_audio_t* audio, float volume) {
    if (!audio) return -1;
    audio->volume = volume < 0 ? 0 : (volume > 1 ? 1 : volume);
    return 0;
}

float hal_audio_get_volume(const hal_audio_t* audio) {
    return audio ? audio->volume : 1.0f;
}

int hal_audio_set_mute(hal_audio_t* audio, bool mute) {
    if (!audio) return -1;
    audio->mute = mute;
    return 0;
}

bool hal_audio_get_mute(const hal_audio_t* audio) {
    return audio ? audio->mute : false;
}

const char* hal_audio_get_path(const hal_audio_t* audio) {
    return audio ? audio->path : NULL;
}