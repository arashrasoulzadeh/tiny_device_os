#include "sim_audio.h"
#include <SDL2/SDL.h>
#include <stdlib.h>
#include <stdio.h>

static SDL_AudioDeviceID g_audio_device = 0;
static sim_audio_callback_t g_callback = NULL;
static void* g_callback_arg = NULL;
static uint32_t g_sample_rate = 44100;
static uint16_t g_channels = 2;
static float g_volume = 1.0f;
static bool g_mute = false;

static void audio_callback(void* userdata, Uint8* stream, int len) {
    (void)userdata;
    if (g_callback) {
        uint32_t frames = len / (g_channels * sizeof(int16_t));
        g_callback(stream, frames, g_callback_arg);
    } else {
        SDL_memset(stream, 0, len);
    }
}

int sim_audio_init(uint32_t sample_rate, uint16_t channels, uint16_t buffer_frames) {
    g_sample_rate = sample_rate;
    g_channels = channels;
    g_volume = 1.0f;
    g_mute = false;
    
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
}

void sim_audio_set_callback(sim_audio_callback_t cb, void* arg) {
    g_callback = cb;
    g_callback_arg = arg;
}

void sim_audio_start(void) {
    if (g_audio_device) {
        SDL_PauseAudioDevice(g_audio_device, 0);
    }
}

void sim_audio_stop(void) {
    if (g_audio_device) {
        SDL_PauseAudioDevice(g_audio_device, 1);
    }
}

int sim_audio_set_volume(float volume) {
    g_volume = volume < 0.0f ? 0.0f : (volume > 1.0f ? 1.0f : volume);
    return 0;
}

float sim_audio_get_volume(void) {
    return g_volume;
}

int sim_audio_set_mute(bool mute) {
    g_mute = mute;
    return 0;
}

bool sim_audio_get_mute(void) {
    return g_mute;
}