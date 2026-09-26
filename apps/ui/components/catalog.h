#pragma once

#include "app.h"

#ifdef __cplusplus
extern "C" {
#endif

#define APP_KIT_CATALOG_MAX 16

typedef struct {
    const char* name;
    app_type_t type;
} app_catalog_entry_t;

/** Snapshot launchable apps (skips `exclude_name`, typically "launcher"). */
int app_kit_catalog_build(const char* exclude_name);
void app_kit_catalog_clear(void);
int app_kit_catalog_count(void);
const app_catalog_entry_t* app_kit_catalog_at(int index);

/** Short tag for menu columns, e.g. "[TOL]". */
const char* app_type_tag(app_type_t type);

#ifdef __cplusplus
}
#endif
