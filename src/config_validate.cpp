#include "config.h"
#include <string.h>

#ifndef THUMB_CACHE_SLOTS
#define THUMB_CACHE_SLOTS 16
#endif
#ifndef UI_FRAME_DUMP
#define UI_FRAME_DUMP 0
#endif
#ifndef ORIENTATION_AUTO_FLIP
#define ORIENTATION_AUTO_FLIP 1
#endif
#ifndef ORIENTATION_ACCEL_AXIS
#define ORIENTATION_ACCEL_AXIS 1
#endif
#ifndef ORIENTATION_INVERT
#define ORIENTATION_INVERT 0
#endif
#ifndef ORIENTATION_DEBUG
#define ORIENTATION_DEBUG 0
#endif
#ifndef TIMEZONE_DST_AUTO
#define TIMEZONE_DST_AUTO 1
#endif
#ifndef TIMEZONE_OFFSET_SEC
#define TIMEZONE_OFFSET_SEC 0
#endif
#ifndef LIST_SHOW_FOR_SALE
#define LIST_SHOW_FOR_SALE 1
#endif
#ifndef LIST_SHOW_SOLD_OUT
#define LIST_SHOW_SOLD_OUT 1
#endif
#ifndef LIST_SHOW_DRAFT
#define LIST_SHOW_DRAFT 0
#endif
#ifndef LIST_SHOW_RETIRED
#define LIST_SHOW_RETIRED 0
#endif
#ifndef LIST_SHOW_UNKNOWN
#define LIST_SHOW_UNKNOWN 0
#endif

static bool flag01(int v) {
    return v == 0 || v == 1;
}

bool config_is_valid() {
    return strlen(WIFI_SSID) > 0
        && strlen(WIFI_PASSWORD) > 0
        && strlen(TINDIE_USERNAME) > 0
        && strlen(TINDIE_API_KEY) > 0
        && POLL_INTERVAL_SEC >= 10
        && THUMB_CACHE_SLOTS >= 1
        && THUMB_CACHE_SLOTS <= 32
        && (UI_FRAME_DUMP == 0 || UI_FRAME_DUMP == 1)
        && flag01(ORIENTATION_AUTO_FLIP)
        && ORIENTATION_ACCEL_AXIS >= 0
        && ORIENTATION_ACCEL_AXIS <= 2
        && flag01(ORIENTATION_INVERT)
        && flag01(ORIENTATION_DEBUG)
        && (TIMEZONE_DST_AUTO == 0 || TIMEZONE_DST_AUTO == 1)
        && TIMEZONE_OFFSET_SEC >= -43200
        && TIMEZONE_OFFSET_SEC <= 50400
        && flag01(LIST_SHOW_FOR_SALE)
        && flag01(LIST_SHOW_SOLD_OUT)
        && flag01(LIST_SHOW_DRAFT)
        && flag01(LIST_SHOW_RETIRED)
        && flag01(LIST_SHOW_UNKNOWN);
}
