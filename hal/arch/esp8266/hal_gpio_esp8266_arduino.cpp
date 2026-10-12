// hal_gpio_* implementation for ESP8266 Arduino framework.
// Uses Arduino pinMode/digitalWrite/digitalRead instead of ESP-IDF
// driver/gpio.h.
#include "hal_gpio.h"
#include <Arduino.h>
#include <stdlib.h>
#include <string.h>

struct hal_gpio {
  char path[32];
  hal_gpio_mode_t mode;
  int pin;
  hal_gpio_irq_t irq_trigger;
  hal_gpio_callback_t irq_cb;
  void *irq_arg;
  bool irq_enabled;
};

// Static array to map pins to gpio structs for ISR
static hal_gpio_t *g_gpio_map[18] = {NULL};

static void IRAM_ATTR gpio_isr_wrapper_0(void) {
  if (g_gpio_map[0])
    g_gpio_map[0]->irq_cb(0, g_gpio_map[0]->irq_arg);
}
static void IRAM_ATTR gpio_isr_wrapper_1(void) {
  if (g_gpio_map[1])
    g_gpio_map[1]->irq_cb(1, g_gpio_map[1]->irq_arg);
}
static void IRAM_ATTR gpio_isr_wrapper_2(void) {
  if (g_gpio_map[2])
    g_gpio_map[2]->irq_cb(2, g_gpio_map[2]->irq_arg);
}
static void IRAM_ATTR gpio_isr_wrapper_3(void) {
  if (g_gpio_map[3])
    g_gpio_map[3]->irq_cb(3, g_gpio_map[3]->irq_arg);
}
static void IRAM_ATTR gpio_isr_wrapper_4(void) {
  if (g_gpio_map[4])
    g_gpio_map[4]->irq_cb(4, g_gpio_map[4]->irq_arg);
}
static void IRAM_ATTR gpio_isr_wrapper_5(void) {
  if (g_gpio_map[5])
    g_gpio_map[5]->irq_cb(5, g_gpio_map[5]->irq_arg);
}
static void IRAM_ATTR gpio_isr_wrapper_12(void) {
  if (g_gpio_map[12])
    g_gpio_map[12]->irq_cb(12, g_gpio_map[12]->irq_arg);
}
static void IRAM_ATTR gpio_isr_wrapper_13(void) {
  if (g_gpio_map[13])
    g_gpio_map[13]->irq_cb(13, g_gpio_map[13]->irq_arg);
}
static void IRAM_ATTR gpio_isr_wrapper_14(void) {
  if (g_gpio_map[14])
    g_gpio_map[14]->irq_cb(14, g_gpio_map[14]->irq_arg);
}
static void IRAM_ATTR gpio_isr_wrapper_15(void) {
  if (g_gpio_map[15])
    g_gpio_map[15]->irq_cb(15, g_gpio_map[15]->irq_arg);
}
static void IRAM_ATTR gpio_isr_wrapper_16(void) {
  if (g_gpio_map[16])
    g_gpio_map[16]->irq_cb(16, g_gpio_map[16]->irq_arg);
}

static void (*gpio_isr_wrappers[18])(void) = {gpio_isr_wrapper_0,
                                              gpio_isr_wrapper_1,
                                              gpio_isr_wrapper_2,
                                              gpio_isr_wrapper_3,
                                              gpio_isr_wrapper_4,
                                              gpio_isr_wrapper_5,
                                              NULL,
                                              NULL,
                                              NULL,
                                              NULL,
                                              NULL,
                                              NULL,
                                              gpio_isr_wrapper_12,
                                              gpio_isr_wrapper_13,
                                              gpio_isr_wrapper_14,
                                              gpio_isr_wrapper_15,
                                              gpio_isr_wrapper_16,
                                              NULL};

static int arduino_mode(hal_gpio_mode_t mode) {
  switch (mode) {
  case HAL_GPIO_MODE_INPUT:
    return INPUT;
  case HAL_GPIO_MODE_OUTPUT:
    return OUTPUT;
  case HAL_GPIO_MODE_INPUT_PULLUP:
    return INPUT_PULLUP;
  case HAL_GPIO_MODE_INPUT_PULLDOWN:
    return INPUT_PULLDOWN_16; // Only GPIO16 supports pulldown on ESP8266
  case HAL_GPIO_MODE_OUTPUT_OPEN_DRAIN:
    return OUTPUT_OPEN_DRAIN;
  default:
    return INPUT;
  }
}

static int arduino_irq_trigger(hal_gpio_irq_t trigger) {
  switch (trigger) {
  case HAL_GPIO_IRQ_RISING:
    return RISING;
  case HAL_GPIO_IRQ_FALLING:
    return FALLING;
  case HAL_GPIO_IRQ_BOTH:
    return CHANGE;
  default:
    return 0;
  }
}

extern "C" {

hal_gpio_t *hal_gpio_open(const char *path, hal_gpio_mode_t mode) {
  int pin = -1;
  if (sscanf(path, "/dev/gpio%d", &pin) != 1 &&
      sscanf(path, "gpio%d", &pin) != 1) {
    return NULL;
  }
  if (pin < 0 || pin > 17) {
    return NULL;
  }

  hal_gpio_t *gpio = (hal_gpio_t *)calloc(1, sizeof(hal_gpio_t));
  if (!gpio)
    return NULL;

  strncpy(gpio->path, path, sizeof(gpio->path) - 1);
  gpio->mode = mode;
  gpio->pin = pin;
  gpio->irq_enabled = false;

  pinMode(pin, arduino_mode(mode));

  return gpio;
}

void hal_gpio_close(hal_gpio_t *gpio) {
  if (!gpio)
    return;
  if (gpio->irq_enabled) {
    detachInterrupt(gpio->pin);
    g_gpio_map[gpio->pin] = NULL;
  }
  free(gpio);
}

void hal_gpio_write(hal_gpio_t *gpio, bool level) {
  if (!gpio)
    return;
  digitalWrite(gpio->pin, level ? HIGH : LOW);
}

bool hal_gpio_read(const hal_gpio_t *gpio) {
  if (!gpio)
    return false;
  return digitalRead(gpio->pin);
}

void hal_gpio_toggle(hal_gpio_t *gpio) {
  if (!gpio)
    return;
  digitalWrite(gpio->pin, !digitalRead(gpio->pin));
}

int hal_gpio_set_irq(hal_gpio_t *gpio, hal_gpio_irq_t trigger,
                     hal_gpio_callback_t cb, void *arg) {
  if (!gpio || trigger == HAL_GPIO_IRQ_NONE)
    return -1;

  gpio->irq_trigger = trigger;
  gpio->irq_cb = cb;
  gpio->irq_arg = arg;

  if (gpio->irq_enabled) {
    detachInterrupt(gpio->pin);
    g_gpio_map[gpio->pin] = NULL;
  }

  if (trigger != HAL_GPIO_IRQ_NONE) {
    int arduino_trigger = arduino_irq_trigger(trigger);
    void (*isr)(void) = gpio_isr_wrappers[gpio->pin];
    if (isr) {
      g_gpio_map[gpio->pin] = gpio;
      attachInterrupt(gpio->pin, isr, arduino_trigger);
      gpio->irq_enabled = true;
    } else {
      return -1; // Pin doesn't support interrupts
    }
  }

  return 0;
}

void hal_gpio_enable_irq(hal_gpio_t *gpio) {
  if (!gpio)
    return;
  if (gpio->irq_trigger != HAL_GPIO_IRQ_NONE) {
    int arduino_trigger = arduino_irq_trigger(gpio->irq_trigger);
    void (*isr)(void) = gpio_isr_wrappers[gpio->pin];
    if (isr) {
      g_gpio_map[gpio->pin] = gpio;
      attachInterrupt(gpio->pin, isr, arduino_trigger);
      gpio->irq_enabled = true;
    }
  }
}

void hal_gpio_disable_irq(hal_gpio_t *gpio) {
  if (!gpio)
    return;
  if (gpio->irq_enabled) {
    detachInterrupt(gpio->pin);
    g_gpio_map[gpio->pin] = NULL;
    gpio->irq_enabled = false;
  }
}

int hal_gpio_get_pin_number(const hal_gpio_t *gpio) {
  return gpio ? gpio->pin : -1;
}

const char *hal_gpio_get_path(const hal_gpio_t *gpio) {
  return gpio ? gpio->path : NULL;
}

int hal_gpio_suspend(hal_gpio_t *gpio) {
  if (!gpio)
    return -1;
  if (gpio->irq_enabled) {
    detachInterrupt(gpio->pin);
    g_gpio_map[gpio->pin] = NULL;
    gpio->irq_enabled = false;
  }
  return 0;
}

int hal_gpio_resume(hal_gpio_t *gpio) {
  if (!gpio || gpio->irq_trigger == HAL_GPIO_IRQ_NONE)
    return -1;
  int arduino_trigger = arduino_irq_trigger(gpio->irq_trigger);
  void (*isr)(void) = gpio_isr_wrappers[gpio->pin];
  if (isr) {
    g_gpio_map[gpio->pin] = gpio;
    attachInterrupt(gpio->pin, isr, arduino_trigger);
    gpio->irq_enabled = true;
    return 0;
  }
  return -1;
}

int hal_gpio_set_wake(hal_gpio_t *gpio, hal_gpio_irq_t trigger) {
  (void)gpio;
  (void)trigger;
  // ESP8266 Arduino doesn't have easy wake-from-deepsleep via GPIO
  // This would require ESP-IDF APIs
  return -1;
}

void hal_gpio_clear_wake(hal_gpio_t *gpio) { (void)gpio; }

} // extern "C"
