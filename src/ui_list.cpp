#include "ui.h"
#include "ui_common.h"
#include "text_layout.h"
#include <M5StickCPlus2.h>
#include <stdio.h>

static void draw_list_row(const UiMetrics& m, int row, int row_h, bool selected,
    ProductStatus status, const char* title, const char* meta) {
    const int16_t y = m.y + m.header_h + row * row_h;
    const uint16_t bg = ui_status_bg(status, false);
    const int16_t text_x = m.x + kSelectionBarW + 3;

    ui_fill_rect(m.x, y, m.w, row_h - 1, bg);

    if (selected) {
        ui_fill_rect(m.x, y, kSelectionBarW, row_h - 1, kSelectionBarFg);
    }

    const size_t title_cols = (size_t)((m.w - kSelectionBarW - 3) / 6);
    char title_buf[48];
    format_list_title(title_buf, sizeof(title_buf), title, title_cols);

    ui_text(text_x, y + 1, title_buf, 0xFFFF, bg);
    ui_text(text_x, y + 12, meta, 0xFFFF, bg);
}

void ui_draw_list(const ProductsCache* cache, const AppContext* ctx) {
    const UiMetrics m = ui_metrics();
    ui_begin_frame(0x0000);

    char hhmm[8];
    cache_format_sync_time(cache, hhmm, sizeof(hhmm));
    ui_draw_list_header(ctx->selected_index, cache->count, ctx->fetching, hhmm);

    const int visible = ui_list_visible_rows(cache->count);
    const int row_h = ui_list_row_height(m, visible);

    char title_buf[48];
    char meta_buf[56];
    for (int row = 0; row < visible; ++row) {
        int idx = ctx->list_scroll_offset + row;
        if (idx >= cache->count) {
            break;
        }
        const Product* p = cache_get(cache, idx);
        if (!p) {
            continue;
        }
        format_list_meta(meta_buf, sizeof(meta_buf), p->status_label, p->stock, p->sold,
            (size_t)((m.w - kSelectionBarW - 3) / 6));
        draw_list_row(m, row, row_h, idx == ctx->selected_index, p->status,
            p->title, meta_buf);
    }

    ui_footer_hint("A: open | B: next");
}
