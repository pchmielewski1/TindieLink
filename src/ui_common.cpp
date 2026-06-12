#include "ui_common.h"
#include "logo_sprite.h"
#include "tindie_client.h"
#include "text_layout.h"
#include <M5StickCPlus2.h>
#include <Arduino.h>
#include <pgmspace.h>

// Visible panel is smaller than 240x135 — ST7735 sits behind bezel; keep safe inset.
static const int16_t kMarginX = 4;
static const int16_t kMarginY = 3;

UiMetrics ui_metrics() {
    const int16_t dw = StickCP2.Display.width();
    const int16_t dh = StickCP2.Display.height();
    UiMetrics m = {};
    m.x = kMarginX;
    m.y = kMarginY;
    m.w = dw - 2 * kMarginX;
    m.h = dh - 2 * kMarginY;
    m.line_h = 11;
    m.header_h = 13;
    m.footer_h = 11;
    m.row_h = (m.h - m.header_h - m.footer_h) / 4;
    if (m.row_h < 24) {
        m.row_h = 24;
    }
    m.cols = m.w / 6;
    if (m.cols > 40) {
        m.cols = 40;
    }
    if (m.cols < 32) {
        m.cols = 32;
    }
    return m;
}

UiThumbPanel ui_detail_thumb_panel(const UiMetrics& m) {
    const int16_t body_y = m.y + m.header_h;
    const int16_t body_h = m.h - m.header_h - m.footer_h;

    UiThumbPanel p = {};
    p.x = m.x + 1;
    p.y = body_y + 1;
    p.w = m.w / 2 - 2;
    p.h = body_h - 2;

    p.ix = p.x;
    p.iy = p.y;
    p.iw = p.w;
    p.ih = p.h;
    return p;
}

void ui_log_display_layout_once() {
    static bool logged = false;
    if (logged) {
        return;
    }
    logged = true;

    const UiMetrics m = ui_metrics();
    const UiThumbPanel p = ui_detail_thumb_panel(m);
    Serial.printf("[ui] display=%dx%d rotation=1 margins=%d,%d\n",
        (int)StickCP2.Display.width(), (int)StickCP2.Display.height(),
        (int)(m.x), (int)(m.y));
    Serial.printf("[ui] content=%dx%d header=%d footer=%d body_h=%d\n",
        (int)m.w, (int)m.h, (int)m.header_h, (int)m.footer_h,
        (int)(m.h - m.header_h - m.footer_h));
    Serial.printf("[ui] thumb_panel=%dx%d at=%d,%d ratio=%.2f\n",
        (int)p.w, (int)p.h, (int)p.x, (int)p.y, (float)p.w / (float)p.h);
}

uint16_t ui_status_fg(ProductStatus status) {
    switch (status) {
        case ProductForSale:
            return 0xFFFF;
        case ProductSoldOut:
            return 0xFFFF;
        case ProductRetired:
            return 0xC618;
        case ProductDraft:
            return 0xFFE0;
        case ProductAwaitingApproval:
            return 0xFFFF;
        default:
            return 0xFFFF;
    }
}

uint16_t ui_status_bg(ProductStatus status, bool selected) {
    if (selected) {
        return kSelectedRowBg;
    }
    switch (status) {
        case ProductForSale:
            return 0x0320;
        case ProductSoldOut:
            return 0x4208;
        case ProductRetired:
            return 0x2104;
        case ProductDraft:
            return 0x000A;
        case ProductAwaitingApproval:
            return 0x2945;
        default:
            return 0x2104;
    }
}

void ui_draw_logo_background() {
    StickCP2.Display.pushImage(0, 0, kLogoSpriteW, kLogoSpriteH, kLogoSprite);
}

void ui_begin_boot_frame() {
    ui_draw_logo_background();
    StickCP2.Display.setTextSize(1);
    StickCP2.Display.setFont(&fonts::Font0);
}

void ui_begin_frame(uint16_t bg) {
    StickCP2.Display.fillScreen(bg);
    StickCP2.Display.setTextSize(1);
    StickCP2.Display.setFont(&fonts::Font0);
}

void ui_fill_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    StickCP2.Display.fillRect(x, y, w, h, color);
}

void ui_text(int16_t x, int16_t y, const char* text, uint16_t fg, uint16_t bg) {
    StickCP2.Display.setTextColor(fg, bg);
    StickCP2.Display.setCursor(x, y);
    StickCP2.Display.print(text);
}

void ui_header_bar(const char* text, uint16_t bg, uint16_t fg) {
    const UiMetrics m = ui_metrics();
    ui_fill_rect(m.x, m.y, m.w, m.header_h, bg);
    ui_text(m.x + 2, m.y + 2, text, fg, bg);
}

void ui_header_bar_zones(const char* left, const char* center, const char* right,
    uint16_t bg, uint16_t fg, uint16_t center_fg) {
    const UiMetrics m = ui_metrics();
    ui_fill_rect(m.x, m.y, m.w, m.header_h, bg);
    StickCP2.Display.setTextSize(1);
    StickCP2.Display.setFont(&fonts::Font0);
    StickCP2.Display.setTextColor(fg, bg);

    if (left && left[0]) {
        StickCP2.Display.setTextDatum(textdatum_t::middle_left);
        StickCP2.Display.drawString(left, m.x + 2, m.y + m.header_h / 2);
    }
    if (center && center[0]) {
        const uint16_t cfg = center_fg ? center_fg : fg;
        StickCP2.Display.setTextColor(cfg, bg);
        StickCP2.Display.setTextDatum(textdatum_t::middle_center);
        StickCP2.Display.drawString(center, m.x + m.w / 2, m.y + m.header_h / 2);
        StickCP2.Display.setTextColor(fg, bg);
    }
    if (right && right[0]) {
        StickCP2.Display.setTextDatum(textdatum_t::middle_right);
        StickCP2.Display.drawString(right, m.x + m.w - 2, m.y + m.header_h / 2);
    }
    StickCP2.Display.setTextDatum(textdatum_t::top_left);
}

void ui_format_header_time_right(char* out, size_t out_len, const char* hhmm, bool fetching) {
    if (!out || out_len == 0) {
        return;
    }
    const char* time_str = (hhmm && hhmm[0]) ? hhmm : "--:--";
    snprintf(out, out_len, "%s %c", time_str, fetching ? '*' : ' ');
}

void ui_draw_list_header(int selected_index, int product_count, bool fetching, const char* hhmm) {
    char left[20];
    char center[24];
    char right[10];
    snprintf(left, sizeof(left), "#%d  %d items",
        product_count > 0 ? selected_index + 1 : 0, product_count);
    truncate_to_width_ellipsis(tindie_store_display_name(), center, sizeof(center), 14);
    ui_format_header_time_right(right, sizeof(right), hhmm, fetching);
    ui_header_bar_zones(left, center, right, kHeaderBg, 0xFFFF, kStoreNameFg);
}

static const int kListRowSlots = 4;

int ui_list_visible_rows(int product_count) {
    if (product_count < 1) {
        return 0;
    }
    if (product_count > kListRowSlots) {
        return kListRowSlots;
    }
    return product_count;
}

int ui_list_row_height(const UiMetrics& m, int visible_rows) {
    (void)visible_rows;
    const int16_t body_h = m.h - m.header_h - m.footer_h;
    int row_h = body_h / kListRowSlots;
    if (row_h < 22) {
        row_h = 22;
    }
    return row_h;
}

void ui_footer_hint(const char* text) {
    const UiMetrics m = ui_metrics();
    const int16_t fy = m.y + m.h - m.footer_h;
    ui_fill_rect(m.x, fy, m.w, m.footer_h, 0x1082);
    ui_text(m.x + 2, fy + 2, text, 0xAD55, 0x1082);
}
