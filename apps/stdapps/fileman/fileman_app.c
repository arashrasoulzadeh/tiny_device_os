#include "app_framework.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// --- State ---
typedef struct {
    char path[128];
    int selected;
    int offset;
} fileman_state_t;

static fileman_state_t g_fm;
static vfs_dirent_t* g_entries;
static int g_count;
static app_helper_t* g_app;

// --- Helpers ---
static void refresh_list(void) {
    if (g_entries) {
        free(g_entries);
    }

    vfs_dir_t* dir = vfs_opendir(g_fm.path);
    if (!dir) {
        g_count = 0;
        g_entries = NULL;
        return;
    }

    int count = 0;
    vfs_dirent_t ent;
    while (vfs_readdir(dir, &ent) == 0) count++;
    vfs_closedir(dir);

    dir = vfs_opendir(g_fm.path);
    if (!dir) {
        g_count = 0;
        g_entries = NULL;
        return;
    }

    g_entries = malloc(count * sizeof(vfs_dirent_t));
    g_count = 0;

    while (vfs_readdir(dir, &g_entries[g_count]) == 0) {
        g_count++;
    }
    vfs_closedir(dir);

    for (int i = 0; i < g_count - 1; i++) {
        for (int j = i + 1; j < g_count; j++) {
            if (g_entries[i].is_dir && !g_entries[j].is_dir) continue;
            if (!g_entries[i].is_dir && g_entries[j].is_dir) {
                vfs_dirent_t tmp = g_entries[i];
                g_entries[i] = g_entries[j];
                g_entries[j] = tmp;
            } else if (strcmp(g_entries[i].name, g_entries[j].name) > 0) {
                vfs_dirent_t tmp = g_entries[i];
                g_entries[i] = g_entries[j];
                g_entries[j] = tmp;
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
    if (g_fm.selected < g_count - 1) {
        g_fm.selected++;
        int scale = g_app->ui.ui.text_scale;
        int max_visible = (g_app->ui.ui.content_h - 18 * scale) / (10 * scale);
        if (g_fm.selected >= g_fm.offset + max_visible) g_fm.offset = g_fm.selected - max_visible + 1;
        app_mark_dirty(app);
    }
}

static void on_select(void* app, void* user) {
    (void)user; (void)app;
    if (g_count == 0) return;

    vfs_dirent_t* entry = &g_entries[g_fm.selected];
    /* g_fm.path can be up to sizeof(g_fm.path)-1 chars and entry->name up
     * to VFS_NAME_MAX-1 - a 128-byte buffer can't always hold
     * "path" + "/" + "name" in the worst case (gcc's -Wformat-truncation
     * correctly flags this), so size for the real worst case instead of
     * just silencing the warning. */
    char new_path[sizeof(g_fm.path) + 1 + VFS_NAME_MAX];
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

static void on_ready(app_helper_t* app) {
    int selected;
    int offset;
    g_app = app;
    if (!app_helper_has_state(app)) {
        strncpy(g_fm.path, "/flash", sizeof(g_fm.path) - 1);
        g_fm.path[sizeof(g_fm.path) - 1] = '\0';
        g_fm.selected = 0;
        g_fm.offset = 0;
    }
    selected = g_fm.selected;
    offset = g_fm.offset;
    g_entries = NULL;
    g_count = 0;
    refresh_list();
    if (app_helper_has_state(app)) {
        if (selected >= g_count) {
            selected = g_count > 0 ? g_count - 1 : 0;
        }
        g_fm.selected = selected;
        g_fm.offset = offset;
    }
    APP_INFO("File Manager ready - /flash");
}

static void on_draw(app_helper_t* app) {
    int scale = app->ui.ui.text_scale;
    int max_visible = (app->ui.ui.content_h - 18 * scale) / (10 * scale);

    // Path header
    app_ui_textf(&app->ui, 0, 0, "Path: %s", g_fm.path);
    app_ui_text(&app->ui, 0, 8 * scale, "----------------");

    for (int i = g_fm.offset; i < g_count && i < g_fm.offset + max_visible; i++) {
        int y = 18 * scale + (i - g_fm.offset) * 10 * scale;
        vfs_dirent_t* entry = &g_entries[i];
        bool is_selected = (i == g_fm.selected);

        char size_str[16];
        if (entry->is_dir) {
            size_str[0] = '\0';
        } else {
            snprintf(size_str, sizeof(size_str), "%u B", (unsigned)entry->size);
        }

        app_ui_textf(&app->ui, 0, y, "%s%s  %s %s",
                      is_selected ? ">" : " ",
                      entry->is_dir ? "[DIR] " : "     ",
                      entry->name,
                      size_str);
    }

    // Status bar
    char status[64];
    snprintf(status, sizeof(status), "Items: %d  Sel: %d", g_count, g_fm.selected);
    (void)status;
}

static void on_cleanup(app_helper_t* app) {
    (void)app;
    if (g_entries) {
        free(g_entries);
        g_entries = NULL;
        g_count = 0;
    }
    APP_INFO("File Manager closed");
}

static const app_ui_key_def_t fileman_keys[] = {
    {SIM_KEY_UP, on_up, NULL},
    {SIM_KEY_DOWN, on_down, NULL},
    {SIM_KEY_ENTER, on_select, NULL},
    {SIM_KEY_ESCAPE, on_back, NULL},
    {SIM_KEY_ESCAPE, (app_key_fn_t)app_request_exit_key, NULL},
    {0, NULL, NULL},
};

APP_HELPER(fileman_app, "fileman", .title = "FILE MANAGER",
           .help = "Up/Dn:Nav Sel:Open Bk:Back",
           .type = APP_TYPE_TOOL, .fps = 30, .live = true, .keys = fileman_keys, .state = &g_fm,
           .state_size = sizeof(g_fm), .on_ready = on_ready,
           .on_draw = on_draw, .on_cleanup = on_cleanup)
