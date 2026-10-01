#include "app_framework.h"
#include "app_kit.h"
#include "app.h"
#include "vfs.h"
#include "alloc.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

// --- Shell State ---
#define SHELL_MAX_LINE 128
#define SHELL_MAX_ARGS 8
#define SHELL_HISTORY_MAX 8

static char g_line[SHELL_MAX_LINE];
static int g_line_len = 0;
static int g_cursor = 0;
static char* g_history[SHELL_HISTORY_MAX];
static int g_history_count = 0;
static int g_history_pos = 0;
static app_ui_t g_ui;
static int g_last_key = 0;

// --- Built-in Commands ---
typedef struct {
    const char* name;
    const char* help;
    int (*func)(int argc, char** argv);
} shell_cmd_t;

static int cmd_help(int argc, char** argv);
static int cmd_echo(int argc, char** argv);
static int cmd_clear(int argc, char** argv);
static int cmd_ls(int argc, char** argv);
static int cmd_cat(int argc, char** argv);
static int cmd_reboot(int argc, char** argv);
static int cmd_apps(int argc, char** argv);
static int cmd_mem(int argc, char** argv);
static int cmd_uptime(int argc, char** argv);
static int cmd_version(int argc, char** argv);

static shell_cmd_t g_commands[] = {
    {"help", "Show this help", cmd_help},
    {"echo", "Toggle echo on/off", cmd_echo},
    {"clear", "Clear screen", cmd_clear},
    {"ls", "List files in /flash", cmd_ls},
    {"cat", "Display file contents", cmd_cat},
    {"apps", "List installed apps", cmd_apps},
    {"mem", "Show memory usage", cmd_mem},
    {"uptime", "Show system uptime", cmd_uptime},
    {"version", "Show OS version", cmd_version},
    {"reboot", "Reboot system", cmd_reboot},
};

static int g_num_commands = sizeof(g_commands) / sizeof(g_commands[0]);

// --- History ---
static void history_add(const char* line) {
    if (g_history_count == SHELL_HISTORY_MAX) {
        free(g_history[0]);
        memmove(g_history, g_history + 1, (SHELL_HISTORY_MAX - 1) * sizeof(char*));
        g_history_count--;
    }
    g_history[g_history_count] = strdup(line);
    g_history_count++;
    g_history_pos = g_history_count;
}

static void history_prev(void) {
    if (g_history_pos > 0) {
        g_history_pos--;
        strncpy(g_line, g_history[g_history_pos], SHELL_MAX_LINE - 1);
        g_line[SHELL_MAX_LINE - 1] = '\0';
        g_line_len = strlen(g_line);
        g_cursor = g_line_len;
    }
}

static void history_next(void) {
    if (g_history_pos < g_history_count - 1) {
        g_history_pos++;
        strncpy(g_line, g_history[g_history_pos], SHELL_MAX_LINE - 1);
        g_line[SHELL_MAX_LINE - 1] = '\0';
        g_line_len = strlen(g_line);
        g_cursor = g_line_len;
    } else if (g_history_pos == g_history_count - 1) {
        g_history_pos = g_history_count;
        g_line[0] = '\0';
        g_line_len = 0;
        g_cursor = 0;
    }
}

// --- Command Parsing ---
static int parse_args(char* line, char** argv, int max_argv) {
    int argc = 0;
    char* p = line;
    while (*p && argc < max_argv) {
        while (*p && isspace(*p)) p++;
        if (!*p) break;
        argv[argc++] = p;
        while (*p && !isspace(*p)) p++;
        if (*p) *p++ = '\0';
    }
    return argc;
}

// --- Built-in Commands ---
static int cmd_help(int argc, char** argv) {
    (void)argc; (void)argv;
    printf("Available commands:\n");
    for (int i = 0; i < g_num_commands; i++) {
        printf("  %-10s %s\n", g_commands[i].name, g_commands[i].help);
    }
    printf("\nHistory: UP/DOWN arrows\n");
    return 0;
}

static int cmd_echo(int argc, char** argv) {
    if (argc > 1) {
        if (strcmp(argv[1], "on") == 0) {
            // echo on
        } else if (strcmp(argv[1], "off") == 0) {
            // echo off
        }
    }
    printf("Echo: %s\n", "on");
    return 0;
}

static int cmd_clear(int argc, char** argv) {
    (void)argc; (void)argv;
    printf("\033[2J\033[H");
    return 0;
}

static int cmd_ls(int argc, char** argv) {
    (void)argc; (void)argv;
    vfs_dir_t* dir = vfs_opendir("/flash");
    if (!dir) {
        printf("Failed to open /flash\n");
        return -1;
    }
    vfs_dirent_t ent;
    while (vfs_readdir(dir, &ent) == 0) {
        printf("%s%s  %u bytes\n", ent.name, ent.is_dir ? "/" : "", (unsigned)ent.size);
    }
    vfs_closedir(dir);
    return 0;
}

static int cmd_cat(int argc, char** argv) {
    if (argc < 2) {
        printf("Usage: cat <file>\n");
        return -1;
    }
    char path[128];
    snprintf(path, sizeof(path), "/flash/%s", argv[1]);
    vfs_file_t* f;
    if (vfs_open(path, VFS_MODE_READ, &f) != 0) {
        printf("Cannot open %s\n", path);
        return -1;
    }
    char buf[64];
    ssize_t n;
    while ((n = vfs_read(f, buf, sizeof(buf) - 1)) > 0) {
        buf[n] = '\0';
        printf("%s", buf);
    }
    vfs_close(f);
    return 0;
}

static int cmd_apps(int argc, char** argv) {
    (void)argc; (void)argv;
    app_t* apps[16];
    size_t count = 0;
    if (app_list(apps, 16, &count) == 0) {
        printf("Installed apps (%zu):\n", count);
        for (size_t i = 0; i < count; i++) {
            printf("  %-16s  %s  v%s  %s\n", 
                   apps[i]->name, 
                   apps[i]->type == APP_TYPE_SYSTEM ? "[SYS]" : 
                   apps[i]->type == APP_TYPE_GAME ? "[GME]" : 
                   apps[i]->type == APP_TYPE_TOOL ? "[TOL]" : "[USR]",
                   apps[i]->manifest.version,
                   apps[i]->manifest.description);
        }
    }
    return 0;
}

static int cmd_mem(int argc, char** argv) {
    (void)argc; (void)argv;
    printf("Memory:\n");
    printf("  Free:      %zu bytes\n", os_get_free_heap());
    printf("  Min free:  %zu bytes\n", os_get_min_free_heap());
    return 0;
}

static int cmd_uptime(int argc, char** argv) {
    (void)argc; (void)argv;
    uint32_t ms = time_now_ms();
    uint32_t s = ms / 1000;
    uint32_t h = s / 3600;
    uint32_t m = (s % 3600) / 60;
    s = s % 60;
    printf("Uptime: %02u:%02u:%02u\n", h, m, s);
    return 0;
}

static int cmd_version(int argc, char** argv) {
    (void)argc; (void)argv;
    printf("ArdubotOS\n");
    printf("  Version: 1.0.0\n");
    printf("  Build:   %s %s\n", __DATE__, __TIME__);
    return 0;
}

static int cmd_reboot(int argc, char** argv) {
    (void)argc; (void)argv;
    printf("Rebooting...\n");
    extern bool g_running;
    g_running = false;
    return 0;
}

// --- Command Execution ---
static int find_cmd(const char* name) {
    for (int i = 0; i < g_num_commands; i++) {
        if (strcmp(g_commands[i].name, name) == 0) return i;
    }
    return -1;
}

static void execute_line(const char* line) {
    if (!line || !*line) return;
    
    char* argv[SHELL_MAX_ARGS];
    strncpy(g_line, line, SHELL_MAX_LINE - 1);
    g_line[SHELL_MAX_LINE - 1] = '\0';
    
    int argc = parse_args(g_line, argv, SHELL_MAX_ARGS);
    if (argc == 0) return;
    
    int idx = find_cmd(argv[0]);
    if (idx >= 0) {
        int ret = g_commands[idx].func(argc, argv);
        if (ret != 0) {
            printf("Error: %d\n", ret);
        }
    } else {
        printf("Unknown command: %s\n", argv[0]);
        printf("Type 'help' for available commands\n");
    }
}

// --- Key Handlers ---
static void on_up(void* app, void* user) {
    (void)user; (void)app;
    history_prev();
    app_mark_dirty(app);
}

static void on_down(void* app, void* user) {
    (void)user; (void)app;
    history_next();
    app_mark_dirty(app);
}

static void on_select(void* app, void* user) {
    (void)user; (void)app;
    if (g_line_len > 0) {
        history_add(g_line);
        execute_line(g_line);
        g_line[0] = '\0';
        g_line_len = 0;
        g_cursor = 0;
    }
}

static void on_back(void* app, void* user) {
    (void)user; (void)app;
    if (g_line_len > 0) {
        if (g_cursor > 0) {
            memmove(&g_line[g_cursor - 1], &g_line[g_cursor], g_line_len - g_cursor + 1);
            g_cursor--;
            g_line_len--;
        }
    } else {
        app_request_exit(app);
    }
}

static void on_left(void* app, void* user) {
    (void)user; (void)app;
    if (g_cursor > 0) {
        g_cursor--;
        app_mark_dirty(app);
    }
}

static void on_right(void* app, void* user) {
    (void)user; (void)app;
    if (g_cursor < g_line_len) {
        g_cursor++;
        app_mark_dirty(app);
    }
}

static void on_backspace(void* app, void* user) {
    (void)user; (void)app;
    if (g_cursor > 0) {
        memmove(&g_line[g_cursor - 1], &g_line[g_cursor], g_line_len - g_cursor + 1);
        g_cursor--;
        g_line_len--;
        app_mark_dirty(app);
    }
}

static void on_key_char(void* app, void* user) {
    (void)user;
    if (g_line_len < SHELL_MAX_LINE - 1) {
        static const char* chars = "abcdefghijklmnopqrstuvwxyz0123456789 -_=./@_";
        char c = chars[g_last_key % strlen(chars)];
        g_last_key++;
        
        memmove(&g_line[g_cursor + 1], &g_line[g_cursor], g_line_len - g_cursor + 1);
        g_line[g_cursor] = c;
        g_cursor++;
        g_line_len++;
        app_mark_dirty(app);
    }
}

// --- Lifecycle ---
static void shell_init(void* app) {
    (void)app;
    app_ui_config_t cfg;
    app_ui_config_ui(&cfg, "SHELL", "ArdubotOS Shell  Type 'help'  Up/Down: history");
    app_ui_init(&g_ui, &cfg);
    
    app_ui_bind_key(&g_ui, SIM_KEY_UP, on_up, NULL);
    app_ui_bind_key(&g_ui, SIM_KEY_DOWN, on_down, NULL);
    app_ui_bind_key(&g_ui, SIM_KEY_LEFT, on_left, NULL);
    app_ui_bind_key(&g_ui, SIM_KEY_RIGHT, on_right, NULL);
    app_ui_bind_key(&g_ui, SIM_KEY_ESCAPE, on_backspace, NULL);
    app_ui_bind_key(&g_ui, SIM_KEY_ENTER, on_select, NULL);
    app_ui_bind_key(&g_ui, SIM_KEY_ENTER, on_key_char, NULL);
    
    app_ui_bind_key(&g_ui, SIM_KEY_ESCAPE, (app_key_fn_t)app_request_exit, NULL);
    
    APP_INFO("Shell ready - type commands, UP/DOWN history, Enter: exec/cycle char");
}

static void shell_frame(void* app) {
    (void)app;
    if (!app_is_dirty(app)) {
        return;
    }
    app_ui_begin_frame(app);
    
    // Prompt
    app_ui_text(app, 0, 0, "> ");
    app_ui_text(app, 12, 0, g_line);
    if (g_cursor < 20) {
        app_ui_pixel(app, 12 + g_cursor * 6, 0, true);
    }
    
    // Status
    app_ui_textf(app, 0, ((app_ui_t*)app)->ui.content_h - 8, "ArdubotOS Shell  Type 'help'  Up/Down: history");
    
    app_ui_end_frame(app);
}

static void shell_cleanup(void* app) {
    for (int i = 0; i < g_history_count; i++) {
        free(g_history[i]);
    }
    app_ui_deinit(app);
    APP_INFO("Shell closed");
}

APP_DEFINE(shell_app, "shell", .version = "1.0.0", .author = "ArdubotOS",
           .description = "Interactive shell - type commands, UP/DOWN for history",
           .type = APP_TYPE_SYSTEM, .fps = 30,
           .on_init = shell_init, .on_frame = shell_frame, .on_cleanup = shell_cleanup)
