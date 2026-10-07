#include "notify_service.h"

#include "fw/ui.h"
#include "os_time.h"

#include <string.h>

#define NOTIFY_TITLE_MAX 48
#define NOTIFY_BODY_MAX 96
#define NOTIFY_RGB(r, g, b)                                                    \
  ((uint16_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | (((b) & 0xF8) >> 3)))

typedef struct {
  char title[NOTIFY_TITLE_MAX];
  char body[NOTIFY_BODY_MAX];
  notify_extent_t extent;
  uint8_t band_percent;
  uint32_t duration_ms;
  time_ms_t show_at;
  time_ms_t exit_at;
  int exit_from;
  bool dismissed;
} notify_card_t;

static notify_card_t g_q[NOTIFY_QUEUE_CAP];
static int g_head = 0;
static int g_count = 0;
static bool g_started = false;
static bool g_use_now = false;
static time_ms_t g_now = 0;
static bool g_painted = false;

static time_ms_t notify_now(void) {
  if (g_use_now) {
    return g_now;
  }
  return time_now_ms();
}

static void copy_text(char *dst, size_t cap, const char *src) {
  size_t i = 0;
  if (!src) {
    dst[0] = '\0';
    return;
  }
  while (i + 1 < cap && src[i] != '\0') {
    dst[i] = src[i];
    i++;
  }
  dst[i] = '\0';
}

static int text_scale(void) { return (APP_DISPLAY_HEIGHT > 64) ? 2 : 1; }

static int text_line(void) { return 8 * text_scale(); }

static notify_card_t *current_card(void) {
  if (g_count <= 0) {
    return NULL;
  }
  return &g_q[g_head];
}

static void card_size(const notify_card_t *card, int *w, int *h) {
  int height;
  int percent;
  int min_h = text_line();
  *w = APP_DISPLAY_WIDTH;
  if (!card || card->extent == NOTIFY_EXTENT_FULL) {
    *h = APP_DISPLAY_HEIGHT;
    return;
  }
  percent =
      card->band_percent == 0 ? NOTIFY_BAND_PERCENT : (int)card->band_percent;
  if (percent > 100) {
    percent = 100;
  }
  height = (APP_DISPLAY_HEIGHT * percent) / 100;
  if (height < min_h) {
    height = min_h;
  }
  if (height > APP_DISPLAY_HEIGHT) {
    height = APP_DISPLAY_HEIGHT;
  }
  *h = height;
}

static uint32_t hold_ms(const notify_card_t *card) {
  if (!card || card->duration_ms == 0) {
    return NOTIFY_HOLD_MS;
  }
  return card->duration_ms;
}

static bool card_finished(const notify_card_t *card, time_ms_t now) {
  time_ms_t life;
  if (!card) {
    return true;
  }
  if (card->dismissed) {
    if (now < card->exit_at) {
      return false;
    }
    return (now - card->exit_at) >= NOTIFY_ANIM_MS;
  }
  if (now < card->show_at) {
    return false;
  }
  life = NOTIFY_ANIM_MS + hold_ms(card) + NOTIFY_ANIM_MS;
  return (now - card->show_at) >= life;
}

static void pop_card(time_ms_t now) {
  if (g_count <= 0) {
    return;
  }
  g_head = (g_head + 1) % NOTIFY_QUEUE_CAP;
  g_count--;
  if (g_count > 0) {
    g_q[g_head].show_at = now;
    g_q[g_head].dismissed = false;
    g_q[g_head].exit_at = 0;
    g_q[g_head].exit_from = 0;
  }
}

void notify_service_set_now_ms(uint32_t ms) {
  g_use_now = true;
  g_now = ms;
}

int notify_service_start(void) {
  memset(g_q, 0, sizeof(g_q));
  g_head = 0;
  g_count = 0;
  g_use_now = false;
  g_now = 0;
  g_painted = false;
  g_started = true;
  return 0;
}

int notify_post(const notify_spec_t *spec) {
  notify_card_t *slot;
  int index;
  if (!g_started || !spec || !spec->title || spec->title[0] == '\0') {
    return -1;
  }
  if (g_count >= NOTIFY_QUEUE_CAP) {
    return -1;
  }
  index = (g_head + g_count) % NOTIFY_QUEUE_CAP;
  slot = &g_q[index];
  memset(slot, 0, sizeof(*slot));
  copy_text(slot->title, sizeof(slot->title), spec->title);
  copy_text(slot->body, sizeof(slot->body), spec->body);
  slot->extent = spec->extent;
  slot->band_percent = spec->band_percent;
  slot->duration_ms = spec->duration_ms;
  if (g_count == 0) {
    slot->show_at = notify_now();
  }
  g_count++;
  return 0;
}

void notify_service_pump(void) {
  time_ms_t now;
  if (!g_started) {
    return;
  }
  now = notify_now();
  while (g_count > 0 && card_finished(current_card(), now)) {
    pop_card(now);
  }
}

notify_phase_t notify_service_phase(void) {
  const notify_card_t *card = current_card();
  time_ms_t now;
  time_ms_t enter_end;
  time_ms_t hold_end;
  if (!card) {
    return NOTIFY_PHASE_IDLE;
  }
  now = notify_now();
  if (card->dismissed) {
    return NOTIFY_PHASE_EXIT;
  }
  enter_end = card->show_at + NOTIFY_ANIM_MS;
  hold_end = enter_end + hold_ms(card);
  if (now < enter_end) {
    return NOTIFY_PHASE_ENTER;
  }
  if (now < hold_end) {
    return NOTIFY_PHASE_HOLD;
  }
  return NOTIFY_PHASE_EXIT;
}

int notify_service_slide_offset(void) {
  const notify_card_t *card = current_card();
  time_ms_t now;
  time_ms_t enter_end;
  time_ms_t hold_end;
  int w = 0;
  int h = 0;
  uint32_t elapsed;
  int target;
  int delta;
  if (!card) {
    return 0;
  }
  card_size(card, &w, &h);
  (void)w;
  now = notify_now();
  if (card->dismissed) {
    if (now < card->exit_at) {
      return card->exit_from;
    }
    elapsed = now - card->exit_at;
    target = -h;
    if (elapsed >= NOTIFY_ANIM_MS) {
      return target;
    }
    delta = target - card->exit_from;
    return card->exit_from + (delta * (int)elapsed) / NOTIFY_ANIM_MS;
  }
  enter_end = card->show_at + NOTIFY_ANIM_MS;
  hold_end = enter_end + hold_ms(card);
  if (now < card->show_at) {
    return -h;
  }
  if (now < enter_end) {
    elapsed = now - card->show_at;
    return -h + (h * (int)elapsed) / NOTIFY_ANIM_MS;
  }
  if (now < hold_end) {
    return 0;
  }
  elapsed = now - hold_end;
  if (elapsed >= NOTIFY_ANIM_MS) {
    return -h;
  }
  return -(h * (int)elapsed) / NOTIFY_ANIM_MS;
}

void notify_service_card_rect(int *w, int *h) {
  int cw = 0;
  int ch = 0;
  if (current_card()) {
    card_size(current_card(), &cw, &ch);
  }
  if (w) {
    *w = cw;
  }
  if (h) {
    *h = ch;
  }
}

const char *notify_service_title(void) {
  const notify_card_t *card = current_card();
  if (!card) {
    return NULL;
  }
  return card->title;
}

bool notify_service_needs_present(void) {
  notify_phase_t phase = notify_service_phase();
  if (phase == NOTIFY_PHASE_ENTER || phase == NOTIFY_PHASE_EXIT) {
    return true;
  }
  if (phase == NOTIFY_PHASE_IDLE && g_painted) {
    return true;
  }
  return false;
}

bool notify_service_on_key(sim_key_t key, bool pressed) {
  notify_card_t *card;
  if (sim_key_is_system(key) || notify_service_phase() == NOTIFY_PHASE_IDLE) {
    return false;
  }
  if (!pressed) {
    return true;
  }
  card = current_card();
  if (!card || card->dismissed) {
    return true;
  }
  card->exit_from = notify_service_slide_offset();
  card->exit_at = notify_now();
  card->dismissed = true;
  return true;
}

static void fill_clipped(app_display_t *disp, int x, int y, int w, int h,
                         uint16_t color) {
  if (w <= 0 || h <= 0) {
    return;
  }
  if (x < 0) {
    w += x;
    x = 0;
  }
  if (y < 0) {
    h += y;
    y = 0;
  }
  if (w <= 0 || h <= 0 || x >= APP_DISPLAY_WIDTH || y >= APP_DISPLAY_HEIGHT) {
    return;
  }
  if (x + w > APP_DISPLAY_WIDTH) {
    w = APP_DISPLAY_WIDTH - x;
  }
  if (y + h > APP_DISPLAY_HEIGHT) {
    h = APP_DISPLAY_HEIGHT - y;
  }
  app_display_fill_rect_color(disp, x, y, w, h, 0, color);
}

static void draw_line(app_display_t *disp, int y, const char *text, int scale,
                      uint16_t color) {
  int line_h;
  int tw;
  int x;
  if (!text || text[0] == '\0') {
    return;
  }
  line_h = 8 * scale;
  if (y < 0 || y + line_h > APP_DISPLAY_HEIGHT) {
    return;
  }
  tw = app_display_text_width(text, scale);
  x = (APP_DISPLAY_WIDTH - tw) / 2;
  if (x < 0) {
    x = 0;
  }
  app_display_text_color(disp, x, y, text, scale, color);
}

void notify_service_composite(app_display_t *disp) {
  const notify_card_t *card;
  int w = 0;
  int h = 0;
  int offset;
  int scale;
  int line;
  int cap_h;
  int inner_top;
  int inner_h;
  int block;
  int text_y;
  int gap;
  bool two;
  /* Amber card and a dark cap. App chrome is a teal title bar on black. */
  const uint16_t card_color = NOTIFY_RGB(255, 186, 48);
  const uint16_t cap_color = NOTIFY_RGB(40, 18, 0);
  const uint16_t text_color = NOTIFY_RGB(24, 12, 0);

  if (notify_service_phase() == NOTIFY_PHASE_IDLE) {
    g_painted = false;
    return;
  }
  g_painted = true;
  card = current_card();
  if (!disp || !disp->initialized || !card) {
    return;
  }
  card_size(card, &w, &h);
  offset = notify_service_slide_offset();
  fill_clipped(disp, 0, offset, w, h, card_color);

  scale = text_scale();
  line = text_line();
  cap_h = (h >= line + 8) ? 4 : 0;
  if (cap_h > 0) {
    fill_clipped(disp, 0, offset, w, cap_h, cap_color);
  }
  gap = scale;
  inner_top = offset + cap_h + 2;
  inner_h = h - cap_h - 4;
  if (inner_h < line) {
    inner_top = offset + (h - line) / 2;
    inner_h = line;
  }
  two = card->body[0] != '\0' && inner_h >= (line * 2 + gap);
  block = two ? (line * 2 + gap) : line;
  text_y = inner_top;
  if (inner_h > block) {
    text_y = inner_top + (inner_h - block) / 2;
  }
  draw_line(disp, text_y, card->title, scale, text_color);
  if (two) {
    draw_line(disp, text_y + line + gap, card->body, scale, text_color);
  }
}
