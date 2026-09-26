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
static sim_quit_callback_t g_quit_cb = NULL;
static void* g_quit_arg = NULL;

static sim_key_t sdl_key_to_sim(SDL_Keycode key) {
    switch (key) {
        // Navigation
        case SDLK_UP: return SIM_KEY_UP;
        case SDLK_DOWN: return SIM_KEY_DOWN;
        case SDLK_LEFT: return SIM_KEY_LEFT;
        case SDLK_RIGHT: return SIM_KEY_RIGHT;
        case SDLK_RETURN: return SIM_KEY_ENTER;
        case SDLK_ESCAPE: return SIM_KEY_ESCAPE;
        case SDLK_SPACE: return SIM_KEY_SPACE;
        
        // Numbers 0-9, 10 (using keypad or main keyboard)
        case SDLK_0: case SDLK_KP_0: return SIM_KEY_0;
        case SDLK_1: case SDLK_KP_1: return SIM_KEY_1;
        case SDLK_2: case SDLK_KP_2: return SIM_KEY_2;
        case SDLK_3: case SDLK_KP_3: return SIM_KEY_3;
        case SDLK_4: case SDLK_KP_4: return SIM_KEY_4;
        case SDLK_5: case SDLK_KP_5: return SIM_KEY_5;
        case SDLK_6: case SDLK_KP_6: return SIM_KEY_6;
        case SDLK_7: case SDLK_KP_7: return SIM_KEY_7;
        case SDLK_8: case SDLK_KP_8: return SIM_KEY_8;
        case SDLK_9: case SDLK_KP_9: return SIM_KEY_9;
        
        // Symbols
        case SDLK_EQUALS: return SIM_KEY_EQUALS;      // = key
        case SDLK_PLUS: return SIM_KEY_PLUS;          // Keypad +
        case SDLK_MINUS: return SIM_KEY_MINUS;        // - key
        case SDLK_KP_MINUS: return SIM_KEY_MINUS;     // Keypad -
        case SDLK_KP_PLUS: return SIM_KEY_PLUS;       // Keypad +
        
        // Letters
        case SDLK_a: return SIM_KEY_A;
        case SDLK_b: return SIM_KEY_B;
        case SDLK_c: return SIM_KEY_C;
        case SDLK_d: return SIM_KEY_D;
        case SDLK_e: return SIM_KEY_E;
        case SDLK_f: return SIM_KEY_F;
        case SDLK_g: return SIM_KEY_G;
        case SDLK_h: return SIM_KEY_H;
        case SDLK_i: return SIM_KEY_I;
        case SDLK_j: return SIM_KEY_J;
        case SDLK_k: return SIM_KEY_K;
        case SDLK_l: return SIM_KEY_L;
        case SDLK_m: return SIM_KEY_M;
        case SDLK_n: return SIM_KEY_N;
        case SDLK_o: return SIM_KEY_O;
        case SDLK_p: return SIM_KEY_P;
        case SDLK_q: return SIM_KEY_Q;
        case SDLK_r: return SIM_KEY_R;
        case SDLK_s: return SIM_KEY_S;
        case SDLK_t: return SIM_KEY_T;
        case SDLK_u: return SIM_KEY_U;
        case SDLK_v: return SIM_KEY_V;
        case SDLK_w: return SIM_KEY_W;
        case SDLK_x: return SIM_KEY_X;
        case SDLK_y: return SIM_KEY_Y;
        case SDLK_z: return SIM_KEY_Z;
        
        // Function keys
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
        
        // System keys (handled by OS)
        case SDLK_TAB: return SIM_KEY_SYS_NEXT_APP;       // Tab - next app
        case SDLK_BACKQUOTE: return SIM_KEY_SYS_ESCAPE;   // ` - system escape
        case SDLK_PAUSE: return SIM_KEY_SYS_MENU;         // Pause - system menu
        
        // Legacy mappings (for backward compatibility)
        // Only include keys not already handled above
        // (SDLK_a, SDLK_b, SDLK_x, SDLK_l, SDLK_r are already handled in Letters section)
        
        default: return SIM_KEY_UNKNOWN;
    }
}

int sim_video_init(int width, int height, const char* title) {
    g_width = width;
    g_height = height;
    
    // macOS: ensure window gets keyboard focus
    SDL_SetHint(SDL_HINT_VIDEO_MAC_FULLSCREEN_SPACES, "0");
    SDL_SetHint(SDL_HINT_VIDEO_ALLOW_SCREENSAVER, "0");
    SDL_SetHint(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, "1");
    
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) < 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return -1;
    }
    
    g_window = SDL_CreateWindow(
        title,
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        width * 2, height * 2,
        SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_INPUT_FOCUS
    );
    
    if (!g_window) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return -1;
    }
    
    // Raise window and grab input focus (important on macOS)
    SDL_RaiseWindow(g_window);
    SDL_SetWindowInputFocus(g_window);
    // Small delay to let window system catch up
    SDL_Delay(100);
    SDL_RaiseWindow(g_window);
    SDL_SetWindowInputFocus(g_window);
    
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
                if (g_quit_cb) {
                    g_quit_cb(g_quit_arg);
                }
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

void sim_video_set_quit_callback(sim_quit_callback_t cb, void* arg) {
    g_quit_cb = cb;
    g_quit_arg = arg;
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

sim_key_class_t sim_key_get_class(sim_key_t key) {
    switch (key) {
        case SIM_KEY_SYS_NEXT_APP:
        case SIM_KEY_SYS_ESCAPE:
        case SIM_KEY_SYS_MENU:
            return SIM_KEY_CLASS_SYSTEM;
        default:
            return SIM_KEY_CLASS_APP;
    }
}

bool sim_key_is_system(sim_key_t key) {
    return sim_key_get_class(key) == SIM_KEY_CLASS_SYSTEM;
}