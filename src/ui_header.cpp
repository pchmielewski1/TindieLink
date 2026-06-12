#include "ui.h"
#include "config.h"
#include "ui_common.h"
#include "ui_frame_dump.h"
#include <stdio.h>

#ifndef UI_FRAME_DUMP
#define UI_FRAME_DUMP 0
#endif

void ui_repaint_header(const AppContext* ctx, const ProductsCache* cache) {
    if (!cache || !ctx) {
        return;
    }

    char hhmm[8];
    cache_format_sync_time(cache, hhmm, sizeof(hhmm));

    switch (ctx->screen) {
        case ScreenList:
            ui_draw_list_header(ctx->selected_index, cache->count, ctx->fetching, hhmm);
            break;
        case ScreenDetail: {
            const Product* p = cache_get(cache, ctx->detail_index);
            if (!p) {
                return;
            }
            char left[16];
            char center[16];
            char right[10];
            snprintf(left, sizeof(left), "%d of %d",
                ctx->detail_index + 1, cache->count);
            snprintf(center, sizeof(center), "#%lu", (unsigned long)p->id);
            ui_format_header_time_right(right, sizeof(right), hhmm, ctx->fetching);
            ui_header_bar_zones(left, center, right);
            break;
        }
        default:
            break;
    }
#if UI_FRAME_DUMP
    ui_frame_dump_after_redraw();
#endif
}
