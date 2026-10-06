#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* Identity for an installed stdapp, taken from its app.json at compile time.
 * An unknown name gets version "1.0.0", author "ArdubotOS", and the name
 * itself as the description. */
const char* app_manifest_version(const char* name);
const char* app_manifest_author(const char* name);
const char* app_manifest_description(const char* name);

#ifdef __cplusplus
}
#endif
