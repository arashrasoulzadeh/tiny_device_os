#include "sim_video.h"
#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>

static SDL_Window* g_window = NULL;
static SDL_Renderer* g_renderer = NULL;
static SDL_Texture* g_texture = NULL;
static uint32_t* g_pixels = NULL;
static int g_width = 320;
static int g_height = 240;
static bool g_headless = false;
static sim_key_callback_t g_key_cb = NULL;
static void* g_key_arg = NULL;

static sim_key_t sdl_key_to_sim(SDL_Keycode key) {
    switch (key) {
        case SDLK_UP: return SIM_KEY_UP;
        case SDLK_DOWN: return SIM_KEY_DOWN;
        case SDLK_LEFT: return SIM_KEY_LEFT;
        case SDLK_RIGHT: return SIM_KEY_RIGHT;
        case SDLK_RETURN: return SIM_KEY_ENTER;
        case SDLK_ESCAPE: return SIM_KEY_ESCAPE;
        case SDLK_SPACE: return SIM_KEY_SPACE;
        case SDLK_a: return SIM_KEY_A;
        case SDLK_b: return SIM_KEY_B;
        case SDLK_x: return SIM_KEY_X;
        case SDLK_y: return SIM_KEY_Y;
        case SDLK_l: return SIM_KEY_L;
        case SDLK_r: return SIM_KEY_R;
        case SDLK_F1: return SIM_KEY_F1;
        case SDLK_F2: return SIM_KEY_F2;
        case SDLK_F3: return SIM_KEY_F3;
        case SDLK_F4: return SIM_KEY_F4;
        case SDLK_F5: return SIM_KEY_F5;
        case SDLK_F6: return SIM_KEY_F6;
        case SDLK_F7: return SIM_KEY_F7;
        case SDLK_F8: return SIM_KEY_F8;
        case SDLK_F9: return SIM_KEY_F9;
        case SDLK_F10: return SIM_KEY_F10;
        case SDLK_F11: return SIM_KEY_F11;
        case SDLK_F12: return SIM_KEY_F12;
        default: return SIM_KEY_UNKNOWN;
    }
}

int sim_video_init(int width, int height, const char* title) {
    g_width = width;
    g_height = height;
    
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) < 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return -1;
    }
    
    const char* render_driver = getenv("SDL_RENDER_DRIVER");
    if (!render_driver) render_driver = "software";
    
    g_window = SDL_CreateWindow(
        title,
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        width * 2, height * 2,
        SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI
    );
    
    if (!g_window) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return -1;
    }
    
    g_renderer = SDL_CreateRenderer(g_window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!g_renderer) {
        g_renderer = SDL_CreateRenderer(g_window, -1, SDL_RENDERER_SOFTWARE);
    }
    
    if (!g_renderer) {
        fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(g_window);
        SDL_Quit();
        return -1;
    }
    
    g_texture = SDL_CreateTexture(g_renderer, SDL_PIXELFORMAT_ARGB8888,
                                  SDL_TEXTUREACCESS_STREAMING, width, height);
    if (!g_texture) {
        fprintf(stderr, "SDL_CreateTexture failed: %s\n", SDL_GetError());
        SDL_DestroyRenderer(g_renderer);
        SDL_DestroyWindow(g_window);
        SDL_Quit();
        return -1;
    }
    
    g_pixels = calloc(width * height, sizeof(uint32_t));
    if (!g_pixels) {
        fprintf(stderr, "Failed to allocate pixel buffer\n");
        SDL_DestroyTexture(g_texture);
        SDL_DestroyRenderer(g_renderer);
        SDL_DestroyWindow(g_window);
        SDL_Quit();
        return -1;
    }
    
    SDL_SetRenderDrawColor(g_renderer, 0, 0, 0, 255);
    SDL_RenderClear(g_renderer);
    SDL_RenderPresent(g_renderer);
    
    return 0;
}

void sim_video_cleanup(void) {
    free(g_pixels);
    g_pixels = NULL;
    
    if (g_texture) {
        SDL_DestroyTexture(g_texture);
        g_texture = NULL;
    }
    if (g_renderer) {
        SDL_DestroyRenderer(g_renderer);
        g_renderer = NULL;
    }
    if (g_window) {
        SDL_DestroyWindow(g_window);
        g_window = NULL;
    }
    
    SDL_Quit();
}

void sim_video_poll_events(void) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_QUIT:
                exit(0);
                break;
            case SDL_KEYDOWN:
            case SDL_KEYUP: {
                sim_key_t key = sdl_key_to_sim(event.key.keysym.sym);
                if (key != SIM_KEY_UNKNOWN && g_key_cb) {
                    g_key_cb(key, event.type == SDL_KEYDOWN, g_key_arg);
                }
                break;
            }
            case SDL_WINDOWEVENT:
                if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
                    // Handle resize
                }
                break;
        }
    }
}

void sim_video_render(void) {
    if (!g_renderer || !g_texture || !g_pixels) return;
    
    SDL_UpdateTexture(g_texture, NULL, g_pixels, g_width * sizeof(uint32_t));
    SDL_RenderClear(g_renderer);
    SDL_RenderCopy(g_renderer, g_texture, NULL, NULL);
    SDL_RenderPresent(g_renderer);
}

void sim_video_set_key_callback(sim_key_callback_t cb, void* arg) {
    g_key_cb = cb;
    g_key_arg = arg;
}

void sim_video_get_size(int* width, int* height) {
    if (width) *width = g_width;
    if (height) *height = g_height;
}

void sim_video_set_title(const char* title) {
    if (g_window && title) {
        SDL_SetWindowTitle(g_window, title);
    }
}

bool sim_video_is_headless(void) {
    return g_headless;
}

uint32_t* sim_video_get_pixels(void) {
    return g_pixels;
}

int sim_video_get_width(void) {
    return g_width;
}

int sim_video_get_height(void) {
    return g_height;
}