#include "catalog.h"
#include "app_kit.h"

#include <stdio.h>
#include <string.h>

static app_catalog_entry_t g_catalog[APP_KIT_CATALOG_MAX];
static int g_catalog_count = 0;

void app_kit_catalog_clear(void) {
    g_catalog_count = 0;
    memset(g_catalog, 0, sizeof(g_catalog));
}

int app_kit_catalog_build(const char* exclude_name) {
    app_t* apps[APP_KIT_CATALOG_MAX];
    size_t count = 0;

    app_kit_catalog_clear();

    if (app_list(apps, APP_KIT_CATALOG_MAX, &count) != 0) {
        printf("[catalog] app_list failed\n");
        fflush(stdout);
        return -1;
    }
    printf("[catalog] app_list returned %d apps\n", (int)count);
    fflush(stdout);

    for (size_t i = 0; i < count && g_catalog_count < APP_KIT_CATALOG_MAX; i++) {
        if (!apps[i] || !apps[i]->name[0]) {
            printf("[catalog] skipping app %d: null or empty name\n", (int)i);
            fflush(stdout);
            continue;
        }
        printf("[catalog] app %d: %s\n", (int)i, apps[i]->name);
        fflush(stdout);
        if (exclude_name && strcmp(apps[i]->name, exclude_name) == 0) {
            printf("[catalog] excluding %s\n", apps[i]->name);
            fflush(stdout);
            continue;
        }
        g_catalog[g_catalog_count].name = apps[i]->name;
        g_catalog[g_catalog_count].type = apps[i]->type;
        g_catalog[g_catalog_count].icon = app_kit_get_icon(apps[i]->name);
        g_catalog_count++;
    }

    return g_catalog_count;
}

int app_kit_catalog_count(void) {
    return g_catalog_count;
}

const app_catalog_entry_t* app_kit_catalog_at(int index) {
    if (index < 0 || index >= g_catalog_count) {
        return NULL;
    }
    return &g_catalog[index];
}

const char* app_type_tag(app_type_t type) {
    switch (type) {
        case APP_TYPE_SYSTEM:
            return "[SYS]";
        case APP_TYPE_USER:
            return "[USR]";
        case APP_TYPE_GAME:
            return "[GME]";
        case APP_TYPE_TOOL:
            return "[TOL]";
        default:
            return "";
    }
}
