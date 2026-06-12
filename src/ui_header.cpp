#include "ui.h"
#include "ui_common.h"
#include <M5StickCPlus2.h>
#include <stdio.h>

void ui_repaint_header(const AppContext* ctx, const ProductsCache* cache) {
    if (!cache || !ctx) {
        return;
    }

    char hhmm[8];
    cache_format_sync_time(cache, hhmm, sizeof(hhmm));
    char header[48];

    switch (ctx->screen) {
        case ScreenList:
            snprintf(header, sizeof(header), "%d prod  #%d  sync %s%s",
                cache->count,
                cache->count > 0 ? ctx->selected_index + 1 : 0,
                hhmm,
                ctx->fetching ? " *" : "");
            ui_header_bar(header, 0x0010, 0xFFE0);
            break;
        case ScreenDetail: {
            const Product* p = cache_get(cache, ctx->detail_index);
            if (!p) {
                return;
            }
            snprintf(header, sizeof(header), "%d/%d  #%lu  %s",
                ctx->detail_index + 1,
                cache->count,
                (unsigned long)p->id,
                hhmm);
            ui_header_bar(header, 0x0010, 0xFFE0);
            break;
        }
        default:
            break;
    }
}
