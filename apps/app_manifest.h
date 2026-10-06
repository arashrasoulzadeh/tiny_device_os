#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* Identity and screen chrome for an installed stdapp, taken from its app.json
 * at compile time. An unknown name gets version "1.0.0", author "ArdubotOS",
 * the name itself as the description and title, and an empty help line. */
const char* app_manifest_version(const char* name);
const char* app_manifest_author(const char* name);
const char* app_manifest_description(const char* name);
const char* app_manifest_title(const char* name);
const char* app_manifest_help(const char* name);

#ifdef __cplusplus
}
#endif
