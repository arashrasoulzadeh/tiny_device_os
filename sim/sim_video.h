#pragma once

#include "ardubot_keys.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*sim_key_callback_t)(sim_key_t key, bool pressed, void* arg);
typedef void (*sim_quit_callback_t)(void* arg);

int sim_video_init(int width, int height, const char* title);
void sim_video_cleanup(void);

void sim_video_poll_events(void);
void sim_video_render(void);

void sim_video_set_key_callback(sim_key_callback_t cb, void* arg);
void sim_video_set_quit_callback(sim_quit_callback_t cb, void* arg);

void sim_video_get_size(int* width, int* height);
void sim_video_set_title(const char* title);

bool sim_video_is_headless(void);

uint32_t* sim_video_get_pixels(void);
int sim_video_get_width(void);
int sim_video_get_height(void);

bool sim_key_is_system(sim_key_t key);

void sim_video_ensure_focus(void);

#ifdef __cplusplus
}
#endif
