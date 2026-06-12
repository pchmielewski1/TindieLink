#pragma once
#include "product.h"
#include <stddef.h>
#include <stdint.h>

/** Letterbox / panel behind product thumb (near-black, matches detail frame). */
constexpr uint16_t kThumbPanelBg = 0x0000;
constexpr uint16_t kSelectedRowBg = 0x1082;
constexpr uint16_t kHeaderBg = 0x0010;
constexpr uint16_t kStoreNameFg = 0xFFE0;
constexpr uint16_t kSelectionBarFg = 0x001F;
constexpr int16_t kSelectionBarW = 3;

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
void ui_header_bar_zones(const char* left, const char* center, const char* right,
    uint16_t bg = kHeaderBg, uint16_t fg = 0xFFFF, uint16_t center_fg = 0);
void ui_format_header_time_right(char* out, size_t out_len, const char* hhmm, bool fetching);
void ui_draw_list_header(int selected_index, int product_count, bool fetching, const char* hhmm);
int ui_list_visible_rows(int product_count);
int ui_list_row_height(const UiMetrics& m, int visible_rows);
void ui_footer_hint(const char* text);
