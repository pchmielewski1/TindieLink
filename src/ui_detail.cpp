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

static void draw_title_column(int16_t x, int16_t y, int16_t w, const char* title) {
    const int cols = title_cols_for_width(w);
    char line[24];
    char buf[24];

    truncate_to_width(title, line, sizeof(line), (size_t)cols);
    draw_info_line(x, y, w, line, 0xFFFF, 0x0000);
    y += 12;

    if (title && strlen(title) > (size_t)cols) {
        truncate_to_width(title + cols, buf, sizeof(buf), (size_t)cols);
        if (buf[0]) {
            draw_info_line(x, y, w, buf, 0xD69A, 0x0000);
            y += 12;
            if (strlen(title) > (size_t)(2 * cols)) {
                truncate_to_width(title + 2 * cols, buf, sizeof(buf), (size_t)cols - 1);
                strcat(buf, "~");
                draw_info_line(x, y, w, buf, 0x8410, 0x0000);
            }
        }
    }
    (void)y;
}

static void draw_photo_count_on_thumb(const Product* p, const UiThumbPanel& thumb) {
    if (p->image_count <= 0) {
        return;
    }
    char line[12];
    snprintf(line, sizeof(line), "%d zdj", p->image_count);
    ui_text(thumb.x + 2, thumb.y + thumb.h - 10, line, 0x4208, kThumbPanelBg);
}

static void draw_detail_body(const Product* p, const UiMetrics& m) {
    const UiThumbPanel thumb = ui_detail_thumb_panel(m);
    const int16_t info_x = m.x + thumb.w + 4;
    const int16_t info_w = m.w - thumb.w - 4;
    const int16_t body_y = m.y + m.header_h;
    const int16_t body_h = m.h - m.header_h - m.footer_h;

    ui_log_display_layout_once();
    ui_fill_rect(thumb.x, thumb.y, thumb.w, thumb.h, kThumbPanelBg);
    thumb_loader_draw(thumb.ix, thumb.iy, thumb.iw, thumb.ih, p->id);
    draw_photo_count_on_thumb(p, thumb);

    int16_t y = body_y;
    draw_title_column(info_x, y, info_w, p->title);
    y += 36;

    const uint16_t st_bg = ui_status_bg(p->status, true);
    const uint16_t st_fg = ui_status_fg(p->status);
    char status_line[28];
    snprintf(status_line, sizeof(status_line), "%s %s",
        status_to_abbrev(p->status_label), p->status_label);
    draw_info_line(info_x, y, info_w, status_line, st_fg, st_bg);
    y += 13;

    char metrics[28];
    snprintf(metrics, sizeof(metrics), "Sld:%d Stk:%d", p->sold, p->stock);
    draw_info_line(info_x, y, info_w, metrics, 0xFFE0, 0x0841);
    y += 13;

    if (p->price[0]) {
        char price_line[28];
        if (p->on_sale && p->regular_price[0] &&
            strcmp(p->price, p->regular_price) != 0) {
            snprintf(price_line, sizeof(price_line), "$%s  $%s",
                p->price, p->regular_price);
        } else {
            snprintf(price_line, sizeof(price_line), "$%s", p->price);
        }
        draw_info_line(info_x, y, info_w, price_line, 0x07E0, 0x0208);
        y += 13;
    }

    if (p->rating > 0.01f) {
        char rating_line[28];
        snprintf(rating_line, sizeof(rating_line), "ocena %.2f", p->rating);
        draw_info_line(info_x, y, info_w, rating_line, 0xFFE0, 0x0000);
        y += 13;
    }

    if (p->review_count > 0) {
        char extra[28];
        snprintf(extra, sizeof(extra), "recenzje: %d", p->review_count);
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

    char header[48];
    snprintf(header, sizeof(header), "%d/%d  #%lu  %s",
        ctx->detail_index + 1,
        cache->count,
        (unsigned long)p->id,
        hhmm);
    ui_header_bar(header, 0x0010, 0xFFE0);

    draw_detail_body(p, m);
    ui_footer_hint("A:nxt  B:list");
}
