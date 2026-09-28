#include "app_kit.h"
#include "vfs.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// --- State ---
typedef struct {
    char path[128];
    vfs_dirent_t* entries;
    int count;
    int selected;
    int offset;
} fileman_state_t;

static fileman_state_t g_fm;
static app_timer_t g_timer;

// --- Helpers ---
static void refresh_list(void) {
    if (g_fm.entries) {
        free(g_fm.entries);
    }

    vfs_dir_t* dir = vfs_opendir(g_fm.path);
    if (!dir) {
        g_fm.count = 0;
        g_fm.entries = NULL;
        return;
    }

    // Count entries first
    int count = 0;
    vfs_dirent_t ent;
    while (vfs_readdir(dir, &ent) == 0) count++;
    vfs_closedir(dir);

    dir = vfs_opendir(g_fm.path);
    if (!dir) {
        g_fm.count = 0;
        g_fm.entries = NULL;
        return;
    }

    g_fm.entries = malloc(count * sizeof(vfs_dirent_t));
    g_fm.count = 0;

    while (vfs_readdir(dir, &g_fm.entries[g_fm.count]) == 0) {
        g_fm.count++;
    }
    vfs_closedir(dir);

    // Sort: directories first, then alphabetically
    for (int i = 0; i < g_fm.count - 1; i++) {
        for (int j = i + 1; j < g_fm.count; j++) {
            if (g_fm.entries[i].is_dir && !g_fm.entries[j].is_dir) continue;
            if (!g_fm.entries[i].is_dir && g_fm.entries[j].is_dir) {
                vfs_dirent_t tmp = g_fm.entries[i];
                g_fm.entries[i] = g_fm.entries[j];
                g_fm.entries[j] = tmp;
            } else if (strcmp(g_fm.entries[i].name, g_fm.entries[j].name) > 0) {
                vfs_dirent_t tmp = g_fm.entries[i];
                g_fm.entries[i] = g_fm.entries[j];
                g_fm.entries[j] = tmp;
            }
        }
    }

    g_fm.selected = 0;
    g_fm.offset = 0;
}

// --- Button Handlers ---
static void on_up(app_ctx_t* app, void* user) {
    (void)user;
    if (g_fm.selected > 0) {
        g_fm.selected--;
        if (g_fm.selected < g_fm.offset) g_fm.offset = g_fm.selected;
        app_mark_dirty(app);
    }
}

static void on_down(app_ctx_t* app, void* user) {
    (void)user;
    if (g_fm.selected < g_fm.count - 1) {
        g_fm.selected++;
        int max_visible = (SSD1306_HEIGHT - 16) / 10;
        if (g_fm.selected >= g_fm.offset + max_visible) g_fm.offset = g_fm.selected - max_visible + 1;
        app_mark_dirty(app);
    }
}

static void on_select(app_ctx_t* app, void* user) {
    (void)user;
    if (g_fm.count == 0) return;

    vfs_dirent_t* entry = &g_fm.entries[g_fm.selected];
    char new_path[128];
    snprintf(new_path, sizeof(new_path), "%s/%s", g_fm.path, entry->name);

    if (entry->is_dir) {
        strncpy(g_fm.path, new_path, sizeof(g_fm.path) - 1);
        g_fm.path[sizeof(g_fm.path) - 1] = '\0';
        g_fm.selected = 0;
        g_fm.offset = 0;
        refresh_list();
        app_mark_dirty(app);
    } else {
        app_request_exit(app);
    }
}

static void on_back(app_ctx_t* app, void* user) {
    (void)user;
    // Go up one directory
    if (strcmp(g_fm.path, "/") != 0) {
        char* last_slash = strrchr(g_fm.path, '/');
        if (last_slash && last_slash > g_fm.path) {
            *last_slash = '\0';
        } else {
            strcpy(g_fm.path, "/");
        }
        g_fm.selected = 0;
        g_fm.offset = 0;
        refresh_list();
        app_mark_dirty(app);
    } else {
        app_request_exit(app);
    }
}

// --- Lifecycle ---
static void fileman_init(app_ctx_t* app) {
    if (app_display_init(&app->display, "/dev/display0") != 0) {
        APP_ERROR("Display init failed");
        return;
    }
    app_timer_init(&g_timer, 30);

    strncpy(g_fm.path, "/flash", sizeof(g_fm.path) - 1);
    g_fm.path[sizeof(g_fm.path) - 1] = '\0';
    g_fm.entries = NULL;
    g_fm.count = 0;
    g_fm.selected = 0;
    g_fm.offset = 0;

    app_bind_key(app, SIM_KEY_UP, on_up, NULL);
    app_bind_key(app, SIM_KEY_DOWN, on_down, NULL);
    app_bind_key(app, SIM_KEY_ENTER, on_select, NULL);
    app_bind_key(app, SIM_KEY_ESCAPE, on_back, NULL);
    app_bind_back(app);

    refresh_list();
    APP_INFO("File Manager ready - /flash");
}

static void fileman_frame(app_ctx_t* app) {
    if (!app_screen_begin(app, "FILE MANAGER")) return;

    int y = 8;
    int max_visible = (SSD1306_HEIGHT - 16) / 10;

    // Path header
    app_textf(app, 0, 0, "Path: %s", g_fm.path);
    app_text(app, 0, 8, "----------------");

    for (int i = g_fm.offset; i < g_fm.count && i < g_fm.offset + max_visible; i++) {
        int y = 18 + (i - g_fm.offset) * 10;
        vfs_dirent_t* entry = &g_fm.entries[i];
        bool is_selected = (i == g_fm.selected);

        app_textf(app, 0, y, "%s%s  %s%u%s",
                  i == g_fm.selected ? ">" : " ",
                  g_fm.entries[i].is_dir ? "[DIR] " : "     ",
                  g_fm.entries[i].name,
                  g_fm.entries[i].is_dir ? 0 : g_fm.entries[i].size,
                  g_fm.entries[i].is_dir ? "" : " B");
    }

    // Status bar
    char status[64];
    snprintf(status, sizeof(status), "Items: %d  Sel: %d", g_fm.count, g_fm.selected);
    app_text(app, 0, SSD1306_HEIGHT - 8, status);

    app_screen_end(app);
}

static void fileman_cleanup(app_ctx_t* app) {
    if (g_fm.entries) {
        free(g_fm.entries);
        g_fm.entries = NULL;
        g_fm.count = 0;
    }
    app_display_deinit(&app->display);
    APP_INFO("File Manager closed");
}

// --- App Definition ---
APP_DEFINE(fileman_app, "fileman", .version = "1.0.0", .author = "ArdubotOS",
           .description = "File manager - browse flash/SD",
           .type = APP_TYPE_TOOL, .fps = 30,
           .on_init = fileman_init, .on_frame = fileman_frame, .on_cleanup = fileman_cleanup)