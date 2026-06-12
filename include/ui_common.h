#pragma once
#include "product.h"
#include <stdint.h>

/** Letterbox / panel behind product thumb (near-black, matches detail frame). */
constexpr uint16_t kThumbPanelBg = 0x0000;

struct UiMetrics {
    int16_t x;
    int16_t y;
    int16_t w;
    int16_t h;
    int16_t line_h;
    int16_t row_h;
    int16_t header_h;
    int16_t footer_h;
    int16_t cols;
};

/** Lewy panel szczegółów (pełna wysokość body, polowa szerokosc contentu). */
struct UiThumbPanel {
    int16_t x;
    int16_t y;
    int16_t w;
    int16_t h;
    int16_t ix;
    int16_t iy;
    int16_t iw;
    int16_t ih;
};

UiMetrics ui_metrics();
UiThumbPanel ui_detail_thumb_panel(const UiMetrics& m);
void ui_log_display_layout_once();
uint16_t ui_status_fg(ProductStatus status);
uint16_t ui_status_bg(ProductStatus status, bool selected);
void ui_begin_frame(uint16_t bg = 0x0000);
void ui_begin_boot_frame();
void ui_draw_logo_background();
void ui_fill_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
void ui_text(int16_t x, int16_t y, const char* text, uint16_t fg, uint16_t bg);
void ui_header_bar(const char* text, uint16_t bg = 0x18E3, uint16_t fg = 0xFFFF);
void ui_footer_hint(const char* text);
