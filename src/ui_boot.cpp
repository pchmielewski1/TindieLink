#include "ui.h"
#include "ui_common.h"
#include "config.h"
#include <M5StickCPlus2.h>
#include <stdio.h>

void ui_init() {
    Serial.println("[TindieLink] ui_init: M5.begin...");
    Serial.flush();

    auto cfg = M5.config();
    cfg.serial_baudrate = 115200;
    StickCP2.begin(cfg);

    Serial.println("[TindieLink] ui_init: M5.begin OK");
    Serial.flush();

    StickCP2.Display.setRotation(1);
    StickCP2.Display.setBrightness(80);
    ui_begin_boot_frame();
}

static void ui_error_screen(const char* title, const char* line1, const char* line2) {
    ui_begin_boot_frame();
    const UiMetrics m = ui_metrics();
    ui_header_bar(title, 0x8000, 0xFFFF);
    ui_fill_rect(m.x, m.y + m.header_h, m.w, m.line_h + 2, 0x2104);
    ui_text(m.x + 4, m.y + m.header_h + 2, line1, 0xFFFF, 0x2104);
    if (line2 && line2[0]) {
        ui_fill_rect(m.x, m.y + m.header_h + m.line_h + 6, m.w, m.line_h + 2, 0x1082);
        ui_text(m.x + 4, m.y + m.header_h + m.line_h + 8, line2, 0xAD55, 0x1082);
    }
}

void ui_draw_error_config() {
    ui_error_screen("Konfiguracja", "Uzupelnij config.h", "WiFi + Tindie API key");
}

void ui_draw_wifi_connecting() {
    ui_begin_boot_frame();
    const UiMetrics m = ui_metrics();
    ui_header_bar("WiFi", 0x0010, 0xFFE0);
    char line[48];
    snprintf(line, sizeof(line), "SSID: %s", WIFI_SSID);
    ui_text(m.x + 2, m.y + m.header_h + 4, "Laczenie...", 0xFFFF, 0x0000);
    ui_text(m.x + 2, m.y + m.header_h + 18, line, 0x07FF, 0x0000);
}

void ui_draw_error_wifi() {
    ui_error_screen("Blad WiFi", "Sprawdz SSID/haslo", "w config.h");
}

void ui_draw_error_api(int http_code) {
    char title[24];
    snprintf(title, sizeof(title), "Blad API %d", http_code);
    char line1[32];
    char line2[32];
    if (http_code == 401) {
        snprintf(line1, sizeof(line1), "Zly username");
        snprintf(line2, sizeof(line2), "lub API key");
    } else if (http_code == 0) {
        snprintf(line1, sizeof(line1), "Brak polaczenia");
        snprintf(line2, sizeof(line2), "lub blad JSON");
    } else {
        snprintf(line1, sizeof(line1), "Sprobuj ponownie");
        line2[0] = '\0';
    }
    ui_error_screen(title, line1, line2);
}
