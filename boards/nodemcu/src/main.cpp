/**
 * NodeMCU board entry.
 *
 * Builtin apps are not implemented here. Every device runs the apps under
 * apps/stdapps/ through the app framework (see apps/stdapps_register.c).
 * This ESP8266 image does not host that framework (no ESP8266 display
 * backend in apps/app_framework.h), so it does not draw a second launcher.
 */
#include <Arduino.h>

void setup() {
    Serial.begin(115200);
    Serial.println("[nodemcu] apps live in apps/stdapps only");
}

void loop() {}
