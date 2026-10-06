#pragma once

/**
 * Runtime copy of an app's session. app_set_state() / app_get_state() keep
 * that copy in RAM for the life of the OS process. A later storage backend
 * can sit behind these two calls without changing apps.
 *
 * Pass the live object to app_state_bind(). Start and resume call
 * app_state_apply() (store -> live). Quit calls app_state_capture()
 * (live -> store).
 */

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define APP_STATE_BYTES 256

int app_set_state(const char* name, const void* data, size_t size);
int app_get_state(const char* name, void* data, size_t size);

int app_state_bind(const char* name, void* live, size_t size);
int app_state_capture(const char* name);
int app_state_apply(const char* name);

#ifdef __cplusplus
}
#endif
