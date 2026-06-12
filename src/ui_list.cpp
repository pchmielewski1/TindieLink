#include "ui.h"
#include "ui_common.h"
#include "text_layout.h"
#include <M5StickCPlus2.h>
#include <stdio.h>

static void draw_list_row(const UiMetrics& m, int row, bool selected, ProductStatus status,
    const char* line) {
    const int16_t y = m.y + m.header_h + row * m.row_h;
    const uint16_t bg = ui_status_bg(status, selected);
    const uint16_t fg = ui_status_fg(status);
    ui_fill_rect(m.x, y, m.w, m.row_h - 1, bg);
    if (selected) {
        ui_fill_rect(m.x, y, 3, m.row_h - 1, 0xFFFF);
    }
    ui_text(m.x + 5, y + 2, line, fg, bg);
}

void ui_draw_list(const ProductsCache* cache, const AppContext* ctx) {
    const UiMetrics m = ui_metrics();
    ui_begin_frame(0x0000);

    char hhmm[8];
    cache_format_sync_time(cache, hhmm, sizeof(hhmm));

    char header[48];
    snprintf(header, sizeof(header), "%d prod  #%d  sync %s%s",
        cache->count,
        cache->count > 0 ? ctx->selected_index + 1 : 0,
        hhmm,
        ctx->fetching ? " *" : "");
    ui_header_bar(header, 0x0010, 0xFFE0);

    char line[48];
    for (int row = 0; row < 4; ++row) {
        int idx = ctx->list_scroll_offset + row;
        if (idx >= cache->count) {
            break;
        }
        const Product* p = cache_get(cache, idx);
        if (!p) {
            continue;
        }
        format_list_line_cols(line, sizeof(line), m.cols, idx == ctx->selected_index,
            p->status_label, p->stock, p->sold, p->id, p->title);
        draw_list_row(m, row, idx == ctx->selected_index, p->status, line);
    }

    ui_footer_hint("A:det  B:nxt");
}
