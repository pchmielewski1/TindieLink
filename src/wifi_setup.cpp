#include "wifi_setup.h"
#include "config.h"
#include <WiFi.h>
#include <time.h>

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
            return;
        }
        delay(250);
    }
}
