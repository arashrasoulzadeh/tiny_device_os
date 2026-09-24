#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*sim_audio_callback_t)(void* buffer, uint32_t frames, void* arg);

int sim_audio_init(uint32_t sample_rate, uint16_t channels, uint16_t buffer_frames);
void sim_audio_cleanup(void);

void sim_audio_set_callback(sim_audio_callback_t cb, void* arg);
void sim_audio_start(void);
void sim_audio_stop(void);

int sim_audio_set_volume(float volume);
float sim_audio_get_volume(void);

int sim_audio_set_mute(bool mute);
bool sim_audio_get_mute(void);

#ifdef __cplusplus
}
#endif