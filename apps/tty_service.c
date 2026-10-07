#include "tty_service.h"

#include "alloc.h"
#include "app.h"
#include "app_kit.h"
#include "ardubot_keys.h"
#include "clock_service.h"
#include "device_info.h"
#include "os_time.h"
#include "scheduler.h"
#include "sensor_service.h"
#include "sim_gpio.h"
#include "vfs.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TTY_LINE 128
#define TTY_OUT 2048
#define TTY_ARGS 8
#define TTY_CLI 8
#define TTY_HTOP_MS 1000u

#define TTY_MODE_LINE 0
#define TTY_MODE_ATTACH 1
#define TTY_MODE_HTOP 2

#define TTY_IN_NORMAL 0
#define TTY_IN_ESC 1
#define TTY_IN_CSI 2

static char g_line[TTY_LINE];
static int g_len;
static char g_out[TTY_OUT];
static size_t g_out_len;
static int g_mode;
static int g_esc;
static int g_htop_armed;
static int g_closed;
static uint32_t g_htop_ms;
static const link_handlers_t *g_handlers;
static void (*g_reboot)(void);

static const char *g_cli_name[TTY_CLI];
static tty_cli_fn g_cli_fn[TTY_CLI];
static int g_cli_n;

static void tty_puts(const char *text) {
  size_t n;
  if (!text) {
    return;
  }
  n = strlen(text);
  if (g_out_len + n >= sizeof(g_out)) {
    n = sizeof(g_out) - 1u - g_out_len;
  }
  if (n == 0) {
    return;
  }
  memcpy(g_out + g_out_len, text, n);
  g_out_len += n;
  g_out[g_out_len] = '\0';
}

static void tty_prompt(void) { tty_puts("ardubot$ "); }

static void tty_task_table(void) {
  int count = 0;
  int i;
  const task_tcb_t *slots = scheduler_get_task_slots(&count);
  tty_puts("\033[2J\033[H");
  tty_puts("name             state     prio\n");
  for (i = 0; i < count; i++) {
    char row[64];
    if (slots[i].name[0] == '\0') {
      continue;
    }
    snprintf(row, sizeof(row), "%-16s %-9d %d\n", slots[i].name,
             (int)slots[i].state, (int)slots[i].priority);
    tty_puts(row);
  }
}

static const char *state_name(app_state_t state) {
  switch (state) {
  case APP_STATE_INSTALLED:
    return "installed";
  case APP_STATE_STOPPED:
    return "stopped";
  case APP_STATE_RUNNING:
    return "running";
  case APP_STATE_SUSPENDED:
    return "suspended";
  case APP_STATE_ERROR:
    return "error";
  default:
    return "absent";
  }
}

static int parse_args(char *line, char **argv, int max_argv) {
  int argc = 0;
  char *p = line;
  while (*p && argc < max_argv) {
    while (*p && isspace((unsigned char)*p)) {
      p++;
    }
    if (!*p) {
      break;
    }
    argv[argc++] = p;
    while (*p && !isspace((unsigned char)*p)) {
      p++;
    }
    if (*p) {
      *p++ = '\0';
    }
  }
  return argc;
}

static sim_key_t key_from_name(const char *name) {
  if (strcmp(name, "up") == 0) {
    return SIM_KEY_UP;
  }
  if (strcmp(name, "down") == 0) {
    return SIM_KEY_DOWN;
  }
  if (strcmp(name, "left") == 0) {
    return SIM_KEY_LEFT;
  }
  if (strcmp(name, "right") == 0) {
    return SIM_KEY_RIGHT;
  }
  if (strcmp(name, "enter") == 0 || strcmp(name, "select") == 0) {
    return SIM_KEY_ENTER;
  }
  if (strcmp(name, "escape") == 0) {
    return SIM_KEY_ESCAPE;
  }
  return SIM_KEY_UNKNOWN;
}

static void tap_key(sim_key_t key) {
  sim_gpio_handle_key(key, true);
  sim_gpio_handle_key(key, false);
}

static void attach_key(sim_key_t key) {
  char line[32];
  tap_key(key);
  snprintf(line, sizeof(line), "key %d\n", (int)key);
  tty_puts(line);
}

static int cli_find(const char *name) {
  int i;
  for (i = 0; i < g_cli_n; i++) {
    if (strcmp(g_cli_name[i], name) == 0) {
      return i;
    }
  }
  return -1;
}

static int cli_sensors(int argc, char **argv) {
  int i;
  int n;
  const char *only = NULL;
  (void)argc;
  if (argv && argv[0] && strcmp(argv[0], "sensor") == 0) {
    only = argc > 1 ? argv[1] : NULL;
    if (!only) {
      tty_puts("usage: sensor <key>\n");
      return -1;
    }
  }
  n = sensor_service_count();
  if (n <= 0) {
    tty_puts("no sensors\n");
    return 0;
  }
  for (i = 0; i < n; i++) {
    const char *key = sensor_service_key(i);
    int32_t value = 0;
    char row[64];
    if (!key) {
      continue;
    }
    if (only && strcmp(only, key) != 0) {
      continue;
    }
    if (sensor_get(key, &value) != 0) {
      snprintf(row, sizeof(row), "%s ?\n", key);
    } else {
      snprintf(row, sizeof(row), "%s %ld\n", key, (long)value);
    }
    tty_puts(row);
    if (only) {
      return 0;
    }
  }
  if (only) {
    tty_puts("unknown sensor\n");
    return -1;
  }
  return 0;
}

static int cli_info(int argc, char **argv) {
  device_info_t info;
  char row[80];
  (void)argc;
  (void)argv;
  if (device_info_query(&info) != 0) {
    tty_puts("info failed\n");
    return -1;
  }
  snprintf(row, sizeof(row), "%s %s\n",
           info.os_name ? info.os_name : "ArdubotOS",
           info.os_version ? info.os_version : "");
  tty_puts(row);
  snprintf(row, sizeof(row), "%s %ux%u\n", info.arch ? info.arch : "",
           info.display_w, info.display_h);
  tty_puts(row);
  return 0;
}

static int cli_clock(int argc, char **argv) {
  char row[48];
  (void)argc;
  (void)argv;
  snprintf(row, sizeof(row), "%ld\n", (long)os_clock_now());
  tty_puts(row);
  return 0;
}

static void register_builtin_cli(void) {
  g_cli_n = 0;
  tty_cli_register("sensors", cli_sensors);
  tty_cli_register("info", cli_info);
  tty_cli_register("clock", cli_clock);
}

void tty_service_reset(void) {
  memset(g_line, 0, sizeof(g_line));
  g_len = 0;
  g_out_len = 0;
  g_mode = TTY_MODE_LINE;
  g_esc = TTY_IN_NORMAL;
  g_htop_armed = 0;
  g_closed = 0;
  g_htop_ms = 0;
  register_builtin_cli();
}

void tty_service_bind(const link_handlers_t *handlers) {
  g_handlers = handlers;
}

void tty_service_set_reboot(void (*reboot)(void)) { g_reboot = reboot; }

int tty_cli_register(const char *name, tty_cli_fn fn) {
  int existing;
  if (!name || !fn) {
    return -1;
  }
  existing = cli_find(name);
  if (existing >= 0) {
    g_cli_fn[existing] = fn;
    return 0;
  }
  if (g_cli_n >= TTY_CLI) {
    return -1;
  }
  g_cli_name[g_cli_n] = name;
  g_cli_fn[g_cli_n] = fn;
  g_cli_n++;
  return 0;
}

void tty_service_open(void) {
  g_closed = 0;
  g_mode = TTY_MODE_LINE;
  tty_prompt();
}

int tty_service_read(uint8_t *dst, size_t cap) {
  size_t n;
  if (!dst || cap == 0 || g_out_len == 0) {
    return 0;
  }
  n = g_out_len < cap ? g_out_len : cap;
  memcpy(dst, g_out, n);
  memmove(g_out, g_out + n, g_out_len - n);
  g_out_len -= n;
  return (int)n;
}

static void leave_mode(void) {
  g_mode = TTY_MODE_LINE;
  g_esc = TTY_IN_NORMAL;
  g_len = 0;
  g_line[0] = '\0';
  tty_puts("\n");
  tty_prompt();
}

static int cmd_help(int argc, char **argv) {
  (void)argc;
  (void)argv;
  tty_puts("apps switch fg run kill key\n");
  tty_puts("sensors sensor top htop version date\n");
  tty_puts("help mem uptime ls cat clear\n");
  tty_puts("notify msg event restart exit\n");
  tty_puts("Ctrl+Z leaves an app, Ctrl+C cancels\n");
  return 0;
}

static int cmd_apps(int argc, char **argv) {
  app_t *apps[APP_MAX];
  size_t count = 0;
  size_t i;
  const char *fg = app_kit_foreground_name();
  (void)argc;
  (void)argv;
  if (app_list(apps, APP_MAX, &count) != 0) {
    tty_puts("apps failed\n");
    return -1;
  }
  for (i = 0; i < count; i++) {
    char row[80];
    int mark = fg && strcmp(fg, apps[i]->name) == 0;
    snprintf(row, sizeof(row), "%s %s%s\n", apps[i]->name,
             state_name(apps[i]->state), mark ? " *" : "");
    tty_puts(row);
  }
  if (count == 0) {
    tty_puts("no apps\n");
  }
  return 0;
}

static int focus_app(const char *name, int attach) {
  if (app_kit_switch(name) != 0) {
    tty_puts("unknown app\n");
    return -1;
  }
  if (attach) {
    g_mode = TTY_MODE_ATTACH;
    g_esc = TTY_IN_NORMAL;
    tty_puts("attached ");
    tty_puts(name);
    tty_puts("\n");
  }
  return 0;
}

static int cmd_switch(int argc, char **argv) {
  if (argc < 2) {
    tty_puts("usage: switch <app>\n");
    return -1;
  }
  return focus_app(argv[1], 0);
}

static int cmd_fg(int argc, char **argv) {
  if (argc < 2) {
    tty_puts("usage: fg <app>\n");
    return -1;
  }
  return focus_app(argv[1], 1);
}

static int cmd_run(int argc, char **argv) {
  int idx;
  if (argc < 2) {
    tty_puts("usage: run <app>\n");
    return -1;
  }
  idx = cli_find(argv[1]);
  if (idx >= 0) {
    return g_cli_fn[idx](argc - 1, argv + 1);
  }
  return focus_app(argv[1], 1);
}

static int cmd_kill(int argc, char **argv) {
  app_t *app;
  const char *fg;
  int was_fg;
  if (argc < 2) {
    tty_puts("usage: kill <app>\n");
    return -1;
  }
  app = app_find(argv[1]);
  if (!app) {
    tty_puts("unknown app\n");
    return -1;
  }
  fg = app_kit_foreground_name();
  was_fg = fg && strcmp(fg, argv[1]) == 0;
  if (app_kit_request_stop(argv[1]) == 0) {
    tty_puts("killed\n");
    return 0;
  }
  if (app->state == APP_STATE_SUSPENDED) {
    (void)app_resume(argv[1]);
  }
  if (app->state != APP_STATE_RUNNING || app_stop(argv[1]) != 0) {
    tty_puts("not running\n");
    return -1;
  }
  if (was_fg && app_find("launcher")) {
    (void)app_kit_switch("launcher");
  }
  tty_puts("killed\n");
  return 0;
}

static int cmd_key(int argc, char **argv) {
  sim_key_t key;
  int pressed = 1;
  if (argc < 2) {
    tty_puts("usage: key <name>\n");
    return -1;
  }
  key = key_from_name(argv[1]);
  if (key == SIM_KEY_UNKNOWN) {
    tty_puts("unknown key\n");
    return -1;
  }
  if (argc > 2 && strcmp(argv[2], "up") == 0) {
    pressed = 0;
  }
  sim_gpio_handle_key(key, pressed ? true : false);
  tty_puts(pressed ? "key down\n" : "key up\n");
  return 0;
}

static int cmd_top(int argc, char **argv) {
  (void)argc;
  (void)argv;
  tty_task_table();
  return 0;
}

static int cmd_htop(int argc, char **argv) {
  (void)argc;
  (void)argv;
  g_mode = TTY_MODE_HTOP;
  g_htop_armed = 0;
  g_htop_ms = 0;
  tty_task_table();
  tty_puts("Ctrl+C returns\n");
  return 0;
}

static int cmd_version(int argc, char **argv) {
  (void)argc;
  (void)argv;
  return cli_info(0, NULL);
}

static int cmd_date(int argc, char **argv) {
  char row[64];
  long when;
  char *end = NULL;
  if (argc >= 3 && strcmp(argv[1], "set") == 0) {
    when = strtol(argv[2], &end, 10);
    if (!end || end == argv[2]) {
      tty_puts("usage: date set <unix>\n");
      return -1;
    }
    if (os_clock_set((time_t)when) != 0) {
      tty_puts("date failed\n");
      return -1;
    }
    if (clock_service_set((time_t)when) != 0) {
      snprintf(row, sizeof(row), "date %ld (not stored)\n",
               (long)os_clock_now());
    } else {
      snprintf(row, sizeof(row), "date %ld\n", (long)os_clock_now());
    }
    tty_puts(row);
    return 0;
  }
  snprintf(row, sizeof(row), "%ld\n", (long)os_clock_now());
  tty_puts(row);
  return 0;
}

static int cmd_mem(int argc, char **argv) {
  char row[64];
  (void)argc;
  (void)argv;
  snprintf(row, sizeof(row), "free %lu\n", (unsigned long)os_get_free_heap());
  tty_puts(row);
  snprintf(row, sizeof(row), "min %lu\n",
           (unsigned long)os_get_min_free_heap());
  tty_puts(row);
  return 0;
}

static int cmd_uptime(int argc, char **argv) {
  char row[48];
  uint32_t ms = time_now_ms();
  (void)argc;
  (void)argv;
  snprintf(row, sizeof(row), "%lu ms\n", (unsigned long)ms);
  tty_puts(row);
  return 0;
}

static int cmd_ls(int argc, char **argv) {
  vfs_dir_t *dir;
  vfs_dirent_t ent;
  (void)argc;
  (void)argv;
  dir = vfs_opendir("/flash");
  if (!dir) {
    tty_puts("cannot open /flash\n");
    return -1;
  }
  while (vfs_readdir(dir, &ent) == 0) {
    char row[80];
    snprintf(row, sizeof(row), "%s%s\n", ent.name, ent.is_dir ? "/" : "");
    tty_puts(row);
  }
  vfs_closedir(dir);
  return 0;
}

static int cmd_cat(int argc, char **argv) {
  char path[128];
  vfs_file_t *file = NULL;
  char buf[64];
  ssize_t n;
  if (argc < 2) {
    tty_puts("usage: cat <file>\n");
    return -1;
  }
  snprintf(path, sizeof(path), "/flash/%s", argv[1]);
  if (vfs_open(path, VFS_MODE_READ, &file) != 0) {
    tty_puts("cannot open\n");
    return -1;
  }
  while ((n = vfs_read(file, buf, sizeof(buf) - 1)) > 0) {
    buf[n] = '\0';
    tty_puts(buf);
  }
  tty_puts("\n");
  vfs_close(file);
  return 0;
}

static int cmd_clear(int argc, char **argv) {
  (void)argc;
  (void)argv;
  tty_puts("\033[2J\033[H");
  return 0;
}

static int cmd_notify(int argc, char **argv) {
  if (argc < 2 || !g_handlers || !g_handlers->notify) {
    tty_puts("usage: notify <title>\n");
    return -1;
  }
  if (g_handlers->notify(argv[1], NULL, 0, 0, g_handlers->user) != 0) {
    tty_puts("notify failed\n");
    return -1;
  }
  tty_puts("ok\n");
  return 0;
}

static int cmd_msg(int argc, char **argv) {
  if (argc < 3 || !g_handlers || !g_handlers->message) {
    tty_puts("usage: msg <app> <text>\n");
    return -1;
  }
  if (g_handlers->message(argv[1], argv[2], g_handlers->user) != 0) {
    tty_puts("unknown app\n");
    return -1;
  }
  tty_puts("ok\n");
  return 0;
}

static int cmd_event(int argc, char **argv) {
  if (argc < 3 || !g_handlers || !g_handlers->event) {
    tty_puts("usage: event <app> <name>\n");
    return -1;
  }
  if (g_handlers->event(argv[1], argv[2], NULL, 0, g_handlers->user) != 0) {
    tty_puts("unknown app\n");
    return -1;
  }
  tty_puts("ok\n");
  return 0;
}

static int cmd_exit(int argc, char **argv) {
  (void)argc;
  (void)argv;
  tty_puts("bye\n");
  /* EOT. The host closes the port and does not print this byte. */
  tty_puts("\x04");
  g_closed = 1;
  return 0;
}

static int cmd_restart(int argc, char **argv) {
  (void)argc;
  (void)argv;
  tty_puts("restarting\n");
  if (g_reboot) {
    g_reboot();
  }
  return 0;
}

static int exec_line(void) {
  char buf[TTY_LINE];
  char *argv[TTY_ARGS];
  int argc;
  memcpy(buf, g_line, sizeof(buf));
  argc = parse_args(buf, argv, TTY_ARGS);
  g_len = 0;
  g_line[0] = '\0';
  if (argc == 0) {
    return 0;
  }
  if (strcmp(argv[0], "help") == 0) {
    return cmd_help(argc, argv);
  }
  if (strcmp(argv[0], "apps") == 0) {
    return cmd_apps(argc, argv);
  }
  if (strcmp(argv[0], "switch") == 0) {
    return cmd_switch(argc, argv);
  }
  if (strcmp(argv[0], "fg") == 0) {
    return cmd_fg(argc, argv);
  }
  if (strcmp(argv[0], "run") == 0) {
    return cmd_run(argc, argv);
  }
  if (strcmp(argv[0], "kill") == 0) {
    return cmd_kill(argc, argv);
  }
  if (strcmp(argv[0], "key") == 0) {
    return cmd_key(argc, argv);
  }
  if (strcmp(argv[0], "sensors") == 0 || strcmp(argv[0], "sensor") == 0) {
    return cli_sensors(argc, argv);
  }
  if (strcmp(argv[0], "top") == 0) {
    return cmd_top(argc, argv);
  }
  if (strcmp(argv[0], "htop") == 0) {
    return cmd_htop(argc, argv);
  }
  if (strcmp(argv[0], "version") == 0) {
    return cmd_version(argc, argv);
  }
  if (strcmp(argv[0], "date") == 0) {
    return cmd_date(argc, argv);
  }
  if (strcmp(argv[0], "mem") == 0) {
    return cmd_mem(argc, argv);
  }
  if (strcmp(argv[0], "uptime") == 0) {
    return cmd_uptime(argc, argv);
  }
  if (strcmp(argv[0], "ls") == 0) {
    return cmd_ls(argc, argv);
  }
  if (strcmp(argv[0], "cat") == 0) {
    return cmd_cat(argc, argv);
  }
  if (strcmp(argv[0], "clear") == 0) {
    return cmd_clear(argc, argv);
  }
  if (strcmp(argv[0], "notify") == 0) {
    return cmd_notify(argc, argv);
  }
  if (strcmp(argv[0], "msg") == 0) {
    return cmd_msg(argc, argv);
  }
  if (strcmp(argv[0], "event") == 0) {
    return cmd_event(argc, argv);
  }
  if (strcmp(argv[0], "restart") == 0) {
    return cmd_restart(argc, argv);
  }
  if (strcmp(argv[0], "exit") == 0 || strcmp(argv[0], "close") == 0) {
    return cmd_exit(argc, argv);
  }
  tty_puts("unknown command\n");
  return -1;
}

static void line_byte(uint8_t byte) {
  char echo[2];
  if (byte == 0x03) {
    g_len = 0;
    g_line[0] = '\0';
    tty_puts("^C\n");
    tty_prompt();
    return;
  }
  if (byte == '\r' || byte == '\n') {
    tty_puts("\n");
    (void)exec_line();
    if (g_mode == TTY_MODE_LINE && !g_closed) {
      tty_prompt();
    }
    return;
  }
  if (byte == 0x7f || byte == 0x08) {
    if (g_len > 0) {
      g_len--;
      g_line[g_len] = '\0';
      tty_puts("\b \b");
    }
    return;
  }
  if (byte < 0x20 || byte > 0x7e) {
    return;
  }
  if (g_len + 1 >= TTY_LINE) {
    return;
  }
  g_line[g_len++] = (char)byte;
  g_line[g_len] = '\0';
  echo[0] = (char)byte;
  echo[1] = '\0';
  tty_puts(echo);
}

static sim_key_t key_from_byte(uint8_t byte) {
  if (byte == '\r' || byte == '\n') {
    return SIM_KEY_ENTER;
  }
  if (byte == 0x1b) {
    return SIM_KEY_ESCAPE;
  }
  if (byte == ' ') {
    return SIM_KEY_SPACE;
  }
  if (byte >= 'a' && byte <= 'z') {
    return (sim_key_t)(SIM_KEY_A + (byte - 'a'));
  }
  if (byte >= 'A' && byte <= 'Z') {
    return (sim_key_t)(SIM_KEY_A + (byte - 'A'));
  }
  if (byte >= '0' && byte <= '9') {
    return (sim_key_t)(SIM_KEY_0 + (byte - '0'));
  }
  return SIM_KEY_UNKNOWN;
}

static void attach_byte(uint8_t byte) {
  sim_key_t key;
  if (byte == 0x03 || byte == 0x1a) {
    leave_mode();
    return;
  }
  if (g_esc == TTY_IN_ESC) {
    g_esc = byte == '[' ? TTY_IN_CSI : TTY_IN_NORMAL;
    if (g_esc == TTY_IN_NORMAL) {
      attach_key(SIM_KEY_ESCAPE);
    }
    return;
  }
  if (g_esc == TTY_IN_CSI) {
    g_esc = TTY_IN_NORMAL;
    if (byte == 'A') {
      attach_key(SIM_KEY_UP);
    } else if (byte == 'B') {
      attach_key(SIM_KEY_DOWN);
    } else if (byte == 'C') {
      attach_key(SIM_KEY_RIGHT);
    } else if (byte == 'D') {
      attach_key(SIM_KEY_LEFT);
    }
    return;
  }
  if (byte == 0x1b) {
    g_esc = TTY_IN_ESC;
    return;
  }
  key = key_from_byte(byte);
  if (key != SIM_KEY_UNKNOWN) {
    attach_key(key);
  }
}

void tty_service_input(const uint8_t *data, size_t n) {
  size_t i;
  if (!data && n > 0) {
    return;
  }
  for (i = 0; i < n; i++) {
    uint8_t byte = data[i];
    if (g_mode == TTY_MODE_HTOP) {
      if (byte == 0x03) {
        leave_mode();
      }
      continue;
    }
    if (g_mode == TTY_MODE_ATTACH) {
      attach_byte(byte);
      continue;
    }
    if (g_esc == TTY_IN_ESC) {
      g_esc = byte == '[' ? TTY_IN_CSI : TTY_IN_NORMAL;
      continue;
    }
    if (g_esc == TTY_IN_CSI) {
      g_esc = TTY_IN_NORMAL;
      continue;
    }
    if (byte == 0x1b) {
      g_esc = TTY_IN_ESC;
      continue;
    }
    line_byte(byte);
  }
}

void tty_service_poll(uint32_t now_ms) {
  if (g_mode != TTY_MODE_HTOP) {
    return;
  }
  if (!g_htop_armed) {
    g_htop_armed = 1;
    g_htop_ms = now_ms;
    return;
  }
  if ((uint32_t)(now_ms - g_htop_ms) < TTY_HTOP_MS) {
    return;
  }
  g_htop_ms = now_ms;
  tty_task_table();
}
