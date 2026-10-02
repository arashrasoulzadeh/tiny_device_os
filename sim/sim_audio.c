#include "sim_audio.h"
#include "hal_audio.h"
#include <SDL2/SDL.h>
#include <stdlib.h>
#include <stdio.h>

static SDL_AudioDeviceID g_audio_device = 0;
static hal_audio_t* g_hal_audio = NULL;
static uint32_t g_sample_rate = 44100;
static uint16_t g_channels = 2;

static float g_volume = 1.0f;
static bool g_mute = false;
static hal_audio_callback_t g_callback = NULL;
static void* g_callback_arg = NULL;

static void audio_callback(void* userdata, Uint8* stream, int len) {
    (void)userdata;
    if (g_hal_audio) {
        size_t frame_bytes = g_channels * sizeof(int16_t);
        uint32_t frames = len / frame_bytes;
        int read_frames = hal_audio_read(g_hal_audio, stream, frames);
        if (read_frames < (int)frames) {
            size_t written = read_frames * frame_bytes;
            SDL_memset(stream + written, 0, len - written);
        }
    } else {
        SDL_memset(stream, 0, len);
    }
}

int sim_audio_init(uint32_t sample_rate, uint16_t channels, uint16_t buffer_frames) {
    g_sample_rate = sample_rate;
    g_channels = channels;
    
    // Initialize SDL audio subsystem if not already initialized
    if ((SDL_WasInit(SDL_INIT_AUDIO) & SDL_INIT_AUDIO) == 0) {
        if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
            fprintf(stderr, "SDL_InitSubSystem(AUDIO) failed: %s\n", SDL_GetError());
            return -1;
        }
    }
    
    SDL_AudioSpec want, have;
    SDL_zero(want);
    want.freq = sample_rate;
    want.format = AUDIO_S16SYS;
    want.channels = channels;
    want.samples = buffer_frames;
    want.callback = audio_callback;
    want.userdata = NULL;
    
    g_audio_device = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (g_audio_device == 0) {
        fprintf(stderr, "SDL_OpenAudioDevice failed: %s\n", SDL_GetError());
        return -1;
    }
    
    return 0;
}

void sim_audio_cleanup(void) {
    if (g_audio_device) {
        SDL_CloseAudioDevice(g_audio_device);
        g_audio_device = 0;
    }
    g_hal_audio = NULL;
}

void sim_audio_set_hal_audio(hal_audio_t* audio) {
    g_hal_audio = audio;
}

void sim_audio_start(void) {
    if (g_audio_device) {
        SDL_PauseAudioDevice(g_audio_device, 0);
    }
    if (g_callback && !g_hal_audio) {
        uint8_t dummy_buffer[512];
        g_callback(NULL, dummy_buffer, 128, g_callback_arg);
    }
}

void sim_audio_stop(void) {
    if (g_audio_device) {
        SDL_PauseAudioDevice(g_audio_device, 1);
    }
}

int sim_audio_set_volume(float volume) {
    if (g_hal_audio) {
        return hal_audio_set_volume(g_hal_audio, volume);
    }
    g_volume = volume < 0 ? 0 : (volume > 1 ? 1 : volume);
    return 0;
}

float sim_audio_get_volume(void) {
    if (g_hal_audio) {
        return hal_audio_get_volume(g_hal_audio);
    }
    return g_volume;
}

int sim_audio_set_mute(bool mute) {
    if (g_hal_audio) {
        return hal_audio_set_mute(g_hal_audio, mute);
    }
    g_mute = mute;
    return 0;
}

bool sim_audio_get_mute(void) {
    if (g_hal_audio) {
        return hal_audio_get_mute(g_hal_audio);
    }
    return g_mute;
}

int sim_audio_set_callback(hal_audio_callback_t cb, void* arg) {
    if (g_hal_audio) {
        return hal_audio_set_callback(g_hal_audio, cb, arg);
    }
    g_callback = cb;
    g_callback_arg = arg;
    return 0;
}