#include "ui.h"
#include "ui_common.h"
#include "text_layout.h"
#include "thumb_loader.h"
#include <M5StickCPlus2.h>
#include <stdio.h>
#include <string.h>

static void draw_info_line(int16_t x, int16_t y, int16_t w, const char* text,
    uint16_t fg, uint16_t bg) {
    ui_fill_rect(x, y, w, 11, bg);
    ui_text(x + 2, y + 1, text, fg, bg);
}

static int title_cols_for_width(int16_t info_w) {
    int cols = info_w / 6;
    if (cols > 22) {
        cols = 22;
    }
    if (cols < 12) {
        cols = 12;
    }
    return cols;
}

static const int16_t kDetailTitleLines = 3;
static const int16_t kDetailTitleLineH = 12;

static void draw_title_column(int16_t x, int16_t y, int16_t w, const char* title) {
    const int cols = title_cols_for_width(w);
    char line1[28];
    char line2[28];
    char line3[28];
    wrap_title_3lines(title, line1, sizeof(line1), line2, sizeof(line2), line3, sizeof(line3),
        (size_t)cols);

    draw_info_line(x, y, w, line1, 0xFFFF, 0x0000);
    y += kDetailTitleLineH;
    if (line2[0]) {
        draw_info_line(x, y, w, line2, 0xFFFF, 0x0000);
        y += kDetailTitleLineH;
    }
    if (line3[0]) {
        draw_info_line(x, y, w, line3, 0xD69A, 0x0000);
    }
}

static void draw_photo_badge_on_thumb(const Product* p, const UiThumbPanel& thumb) {
    if (p->image_count <= 0) {
        return;
    }
    char line[8];
    snprintf(line, sizeof(line), "1/%d", p->image_count);

    const int16_t badge_h = 11;
    const int16_t badge_w = (int16_t)((int)strlen(line) * 6 + 4);
    const int16_t bx = thumb.x + 2;
    const int16_t by = thumb.y + thumb.h - badge_h - 2;

    ui_fill_rect(bx, by, badge_w, badge_h, 0x0000);
    ui_text(bx + 2, by + 1, line, 0xAD55, 0x0000);
}

static void draw_detail_body(const Product* p, const UiMetrics& m) {
    const UiThumbPanel thumb = ui_detail_thumb_panel(m);
    const int16_t info_x = m.x + thumb.w + 4;
    const int16_t info_w = m.w - thumb.w - 4;
    const int16_t body_y = m.y + m.header_h;

    ui_log_display_layout_once();
    ui_fill_rect(thumb.x, thumb.y, thumb.w, thumb.h, kThumbPanelBg);
    thumb_loader_draw(thumb.ix, thumb.iy, thumb.iw, thumb.ih, p->id);
    draw_photo_badge_on_thumb(p, thumb);

    int16_t y = body_y;
    draw_title_column(info_x, y, info_w, p->title);
    y += kDetailTitleLines * kDetailTitleLineH + 2;

    const uint16_t st_bg = ui_status_bg(p->status, false);
    const uint16_t st_fg = ui_status_fg(p->status);
    const char* status = p->status_label[0] ? p->status_label : "Unknown";
    draw_info_line(info_x, y, info_w, status, st_fg, st_bg);
    y += 13;

    char metrics[40];
    snprintf(metrics, sizeof(metrics), "Sold %d | Stock %d", p->sold, p->stock);
    draw_info_line(info_x, y, info_w, metrics, 0xFFE0, 0x0841);
    y += 13;

    if (p->price[0]) {
        char price_line[32];
        if (p->on_sale && p->regular_price[0] &&
            strcmp(p->price, p->regular_price) != 0) {
            snprintf(price_line, sizeof(price_line), "$%s  was $%s",
                p->price, p->regular_price);
        } else {
            snprintf(price_line, sizeof(price_line), "$%s", p->price);
        }
        draw_info_line(info_x, y, info_w, price_line, 0x07E0, 0x0208);
        y += 13;
    }

    if (p->rating > 0.01f) {
        char rating_line[24];
        snprintf(rating_line, sizeof(rating_line), "Rating %.2f", p->rating);
        draw_info_line(info_x, y, info_w, rating_line, 0xFFE0, 0x0000);
        y += 13;
    }

    if (p->review_count > 0) {
        char extra[24];
        snprintf(extra, sizeof(extra), "Reviews %d", p->review_count);
        draw_info_line(info_x, y, info_w, extra, 0xAD55, 0x0000);
    }

    (void)y;
}

void ui_draw_detail(const ProductsCache* cache, const AppContext* ctx) {
    const Product* p = cache_get(cache, ctx->detail_index);
    if (!p) {
        ui_draw_list(cache, ctx);
        return;
    }

    const UiMetrics m = ui_metrics();
    ui_begin_frame(0x0000);

    char hhmm[8];
    cache_format_sync_time(cache, hhmm, sizeof(hhmm));

    char left[16];
    char center[16];
    char right[16];
    snprintf(left, sizeof(left), "%d of %d", ctx->detail_index + 1, cache->count);
    snprintf(center, sizeof(center), "#%lu", (unsigned long)p->id);
    ui_format_header_time_right(right, sizeof(right), hhmm, ctx->fetching);
    ui_header_bar_zones(left, center, right);

    draw_detail_body(p, m);
    ui_footer_hint("A: next | B: back");
}
