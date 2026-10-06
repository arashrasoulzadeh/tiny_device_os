#include "app_manifest.h"

#include <string.h>

typedef struct {
    const char* name;
    const char* version;
    const char* author;
    const char* description;
} app_manifest_row_t;

static const app_manifest_row_t g_rows[] = {
#include "app_manifests.inc"
    { NULL, NULL, NULL, NULL },
};

static const app_manifest_row_t* find_row(const char* name) {
    size_t i;
    if (!name) {
        return NULL;
    }
    for (i = 0; g_rows[i].name; i++) {
        if (strcmp(g_rows[i].name, name) == 0) {
            return &g_rows[i];
        }
    }
    return NULL;
}

const char* app_manifest_version(const char* name) {
    const app_manifest_row_t* row = find_row(name);
    return row && row->version[0] ? row->version : "1.0.0";
}

const char* app_manifest_author(const char* name) {
    const app_manifest_row_t* row = find_row(name);
    return row && row->author[0] ? row->author : "ArdubotOS";
}

const char* app_manifest_description(const char* name) {
    const app_manifest_row_t* row = find_row(name);
    if (row && row->description[0]) {
        return row->description;
    }
    return name ? name : "";
}
