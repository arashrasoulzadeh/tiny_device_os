#include "os_boot.h"

#include "app.h"
#include "app_kit.h"
#include "boot_splash.h"
#include "clock_service.h"
#include "notify_service.h"
#include "sensor_service.h"

#define OS_BOOT_SERVICE_STEPS 4

int os_boot_load(const os_boot_args_t *args) {
  app_t *apps[APP_MAX];
  size_t app_count = 0;
  size_t i;
  int total;
  int index;
  time_t now = 0;
  time_t compiled = 0;
  os_boot_storage_fn mount = NULL;

  if (args) {
    mount = args->mount_storage;
    now = args->clock_now;
    compiled = args->clock_compiled;
  }
  if (app_list(apps, APP_MAX, &app_count) != 0) {
    app_count = 0;
  }

  total = OS_BOOT_SERVICE_STEPS + (int)app_count;
  boot_splash_open();

  index = 1;
  boot_splash_show("storage", index, total);
  if (mount) {
    (void)mount();
  }

  index++;
  boot_splash_show("clock", index, total);
  (void)clock_service_start(now, compiled);

  index++;
  boot_splash_show("notify", index, total);
  (void)notify_service_start();

  index++;
  boot_splash_show("sensors", index, total);
  (void)sensor_service_load_builtin();

  for (i = 0; i < app_count; i++) {
    const char *name = apps[i] ? apps[i]->name : "";
    index++;
    boot_splash_show(name, index, total);
    (void)app_kit_load(name);
  }

  boot_splash_close();
  return 0;
}
