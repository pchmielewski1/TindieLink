#include "config.h"
#include <string.h>

#ifndef THUMB_CACHE_TTL_SEC
#define THUMB_CACHE_TTL_SEC 1800
#endif
#ifndef THUMB_CACHE_SLOTS
#define THUMB_CACHE_SLOTS 6
#endif

bool config_is_valid() {
    return strlen(WIFI_SSID) > 0
        && strlen(WIFI_PASSWORD) > 0
        && strlen(TINDIE_USERNAME) > 0
        && strlen(TINDIE_API_KEY) > 0
        && POLL_INTERVAL_SEC >= 10
        && THUMB_CACHE_TTL_SEC >= 60
        && THUMB_CACHE_SLOTS >= 1
        && THUMB_CACHE_SLOTS <= 8;
}
