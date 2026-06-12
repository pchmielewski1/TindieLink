#include "app.h"
#include "config.h"
#include "products_cache.h"
#include "serial_log.h"
#include "ui.h"
#include <Arduino.h>

ProductsCache g_cache;
AppContext g_ctx;

void setup() {
    serial_log_init();
    ui_init();
    app_init(&g_ctx, &g_cache);

    if (!g_ctx.config_ok) {
        Serial.println("[TindieLink] STOP config.h niekompletny");
        ui_draw_error_config();
        return;
    }

    Serial.println("[TindieLink] bootstrap...");
    if (!app_bootstrap(&g_ctx, &g_cache)) {
        Serial.println("[TindieLink] bootstrap FAILED");
        return;
    }
    Serial.println("[TindieLink] bootstrap OK — wejdz w szczegoly produktu dla [thumb]");
}

void loop() {
    if (g_ctx.screen == ScreenErrorConfig ||
        g_ctx.screen == ScreenErrorWiFi ||
        g_ctx.screen == ScreenErrorApi) {
        delay(250);
        return;
    }
    app_tick(&g_ctx, &g_cache);
    delay(20);
}
