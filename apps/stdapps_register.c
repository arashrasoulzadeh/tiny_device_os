#include "stdapps_register.h"

#include "app.h"
#include "catalog.h"

/* CMake writes this from ARDUBOT_ENABLED_APPS. A PlatformIO image that
 * does not have the header passes the same ARDUBOT_APP_*_ENABLED macros
 * on the compiler command line. Absent macro means that app is not in
 * this image. */
#if defined(__has_include)
#if __has_include("ardubot_enabled_apps.h")
#include "ardubot_enabled_apps.h"
#endif
#endif

static int install_manifest(app_manifest_t* manifest, const char* name) {
    if (!manifest || app_install_manifest(manifest, name) != 0) {
        return -1;
    }
    return 0;
}

int stdapps_install(void) {
#ifdef ARDUBOT_APP_COUNTER_ENABLED
    extern app_manifest_t* counter_app_manifest;
    if (install_manifest(counter_app_manifest, "counter") != 0) {
        return -1;
    }
#endif
#ifdef ARDUBOT_APP_INFO_ENABLED
    extern app_manifest_t* info_app_manifest;
    if (install_manifest(info_app_manifest, "info") != 0) {
        return -1;
    }
#endif
#ifdef ARDUBOT_APP_STOPWATCH_ENABLED
    extern app_manifest_t* stopwatch_app_manifest;
    if (install_manifest(stopwatch_app_manifest, "stopwatch") != 0) {
        return -1;
    }
#endif
#ifdef ARDUBOT_APP_PONG_ENABLED
    extern app_manifest_t* pong_app_manifest;
    if (install_manifest(pong_app_manifest, "pong") != 0) {
        return -1;
    }
#endif
#ifdef ARDUBOT_APP_WIDGETS_ENABLED
    extern app_manifest_t* widgets_app_manifest;
    if (install_manifest(widgets_app_manifest, "widgets") != 0) {
        return -1;
    }
#endif
#ifdef ARDUBOT_APP_POMODORO_ENABLED
    extern app_manifest_t* pomodoro_app_manifest;
    if (install_manifest(pomodoro_app_manifest, "pomodoro") != 0) {
        return -1;
    }
#endif
#ifdef ARDUBOT_APP_TASKMGR_ENABLED
    extern app_manifest_t* taskmgr_app_manifest;
    if (install_manifest(taskmgr_app_manifest, "taskmgr") != 0) {
        return -1;
    }
#endif
    /* Launcher is always built (apps/stdapps/CMakeLists.txt). settings,
     * fileman, shell, and demo stay out of the catalog even when their
     * sources are linked. */
    extern app_manifest_t* launcher_app_manifest;
    if (install_manifest(launcher_app_manifest, "launcher") != 0) {
        return -1;
    }
    if (app_kit_catalog_build("launcher") < 0) {
        return -1;
    }
    return 0;
}

const char* stdapps_start_name(void) {
#ifdef ARDUBOT_APP_POMODORO_ENABLED
    return "pomodoro";
#else
    return "launcher";
#endif
}
