#include "wifi_setup.h"
#include "config.h"
#include <WiFi.h>
#include <stdlib.h>
#include <time.h>

#ifndef TIMEZONE_TZ
#define TIMEZONE_TZ "UTC0"
#endif
#ifndef TIMEZONE_DST_AUTO
#define TIMEZONE_DST_AUTO 1
#endif
#ifndef TIMEZONE_OFFSET_SEC
#define TIMEZONE_OFFSET_SEC 0
#endif

bool wifi_connect_blocking(int timeout_sec) {
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    const unsigned long deadline = millis() + (unsigned long)timeout_sec * 1000UL;
    while (WiFi.status() != WL_CONNECTED && (long)(deadline - millis()) > 0) {
        delay(250);
    }
    return WiFi.status() == WL_CONNECTED;
}

bool wifi_is_connected() {
    return WiFi.status() == WL_CONNECTED;
}

bool wifi_ensure_connected(int timeout_sec) {
    if (wifi_is_connected()) {
        return true;
    }
    return wifi_connect_blocking(timeout_sec);
}

void wifi_sync_time() {
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
    for (int i = 0; i < 20; ++i) {
        time_t now = time(nullptr);
        if (now > 1700000000) {
            break;
        }
        delay(250);
    }

#if TIMEZONE_DST_AUTO
    setenv("TZ", TIMEZONE_TZ, 1);
#else
    configTime(TIMEZONE_OFFSET_SEC, 0, "pool.ntp.org", "time.nist.gov");
#endif
    tzset();
}
