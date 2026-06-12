#include "ui.h"

void ui_redraw(const AppContext* ctx, const ProductsCache* cache) {
    switch (ctx->screen) {
        case ScreenErrorConfig:
            ui_draw_error_config();
            break;
        case ScreenWiFiConnecting:
            ui_draw_wifi_connecting();
            break;
        case ScreenErrorWiFi:
            ui_draw_error_wifi();
            break;
        case ScreenErrorApi:
            ui_draw_error_api(ctx->last_http_code);
            break;
        case ScreenList:
            ui_draw_list(cache, ctx);
            break;
        case ScreenDetail:
            ui_draw_detail(cache, ctx);
            break;
    }
}
