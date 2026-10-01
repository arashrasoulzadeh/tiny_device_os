#include "app_framework.h"
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
static app_ui_t g_ui;

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
static void on_up(void* app, void* user) {
    (void)user; (void)app;
    if (g_fm.selected > 0) {
        g_fm.selected--;
        if (g_fm.selected < g_fm.offset) g_fm.offset = g_fm.selected;
        app_mark_dirty(app);
    }
}

static void on_down(void* app, void* user) {
    (void)user; (void)app;
    if (g_fm.selected < g_fm.count - 1) {
        g_fm.selected++;
        int scale = g_ui.ui.text_scale;
        int max_visible = (g_ui.ui.content_h - 18 * scale) / (10 * scale);
        if (g_fm.selected >= g_fm.offset + max_visible) g_fm.offset = g_fm.selected - max_visible + 1;
        app_mark_dirty(app);
    }
}

static void on_select(void* app, void* user) {
    (void)user; (void)app;
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

static void on_back(void* app, void* user) {
    (void)user; (void)app;
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
static void fileman_init(void* app) {
    app_ui_config_t cfg;
    app_ui_config_ui(&cfg, "FILE MANAGER", "Up/Dn:Nav Sel:Open Bk:Back");
    app_ui_init(&g_ui, app, &cfg);

    strncpy(g_fm.path, "/flash", sizeof(g_fm.path) - 1);
    g_fm.path[sizeof(g_fm.path) - 1] = '\0';
    g_fm.entries = NULL;
    g_fm.count = 0;
    g_fm.selected = 0;
    g_fm.offset = 0;

    app_ui_bind_key(&g_ui, SIM_KEY_UP, on_up, NULL);
    app_ui_bind_key(&g_ui, SIM_KEY_DOWN, on_down, NULL);
    app_ui_bind_key(&g_ui, SIM_KEY_ENTER, on_select, NULL);
    app_ui_bind_key(&g_ui, SIM_KEY_ESCAPE, on_back, NULL);
    app_ui_bind_key(&g_ui, SIM_KEY_ESCAPE, (app_key_fn_t)app_request_exit, NULL);

    refresh_list();
    APP_INFO("File Manager ready - /flash");
}

static void fileman_frame(void* app) {
    (void)app;
    app_ui_begin_frame(&g_ui);

    int scale = g_ui.ui.text_scale;
    int max_visible = (g_ui.ui.content_h - 18 * scale) / (10 * scale);

    // Path header
    app_ui_textf(&g_ui, 0, 0, "Path: %s", g_fm.path);
    app_ui_text(&g_ui, 0, 8 * scale, "----------------");

    for (int i = g_fm.offset; i < g_fm.count && i < g_fm.offset + max_visible; i++) {
        int y = 18 * scale + (i - g_fm.offset) * 10 * scale;
        vfs_dirent_t* entry = &g_fm.entries[i];
        bool is_selected = (i == g_fm.selected);

        char size_str[16];
        if (entry->is_dir) {
            size_str[0] = '\0';
        } else {
            snprintf(size_str, sizeof(size_str), "%u B", (unsigned)entry->size);
        }

        app_ui_textf(&g_ui, 0, y, "%s%s  %s %s",
                      i == g_fm.selected ? ">" : " ",
                      entry->is_dir ? "[DIR] " : "     ",
                      entry->name,
                      size_str);
    }

    // Status bar
    char status[64];
    snprintf(status, sizeof(status), "Items: %d  Sel: %d", g_fm.count, g_fm.selected);
    app_ui_end_frame(&g_ui);
}

static void fileman_cleanup(void* app) {
    (void)app;
    if (g_fm.entries) {
        free(g_fm.entries);
        g_fm.entries = NULL;
        g_fm.count = 0;
    }
    app_ui_deinit(&g_ui);
    APP_INFO("File Manager closed");
}

APP_DEFINE(fileman_app, "fileman", .version = "1.0.0", .author = "ArdubotOS",
           .description = "File manager - browse flash/SD",
           .type = APP_TYPE_TOOL, .fps = 30,
           .on_init = fileman_init, .on_frame = fileman_frame, .on_cleanup = fileman_cleanup)
