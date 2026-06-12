#pragma once
#include <stdint.h>

enum AppScreen : uint8_t {
    ScreenErrorConfig,
    ScreenWiFiConnecting,
    ScreenErrorWiFi,
    ScreenErrorApi,
    ScreenList,
    ScreenDetail,
};

struct AppContext {
    AppScreen screen;
    int selected_index;
    int detail_index;
    int list_scroll_offset;
    bool fetching;
    bool config_ok;
    int last_http_code;
};
