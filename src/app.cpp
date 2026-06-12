#include "app.h"
#include "config.h"
#include "tindie_client.h"
#include "thumb_loader.h"
#include "ui.h"
#include "wifi_setup.h"
#include <M5StickCPlus2.h>
#include <string.h>

static unsigned long g_last_poll_ms = 0;
static bool g_thumb_busy_prev = false;
static bool g_ui_dirty = true;

static Product g_fetch_scratch[kMaxProducts];

static AppScreen g_last_screen = ScreenErrorConfig;
static int g_last_selected = -1;
static int g_last_detail = -1;
static int g_last_scroll = -1;
static bool g_last_fetching = false;
static int g_last_count = -1;

static void mark_ui_dirty() {
    g_ui_dirty = true;
}

static void clamp_selection(AppContext* ctx, ProductsCache* cache) {
    if (cache->count <= 0) {
        ctx->selected_index = 0;
        ctx->detail_index = 0;
        ctx->list_scroll_offset = 0;
        return;
    }
    if (ctx->selected_index >= cache->count) {
        ctx->selected_index = 0;
    }
    if (ctx->detail_index >= cache->count) {
        ctx->detail_index = 0;
    }
    if (ctx->selected_index < ctx->list_scroll_offset) {
        ctx->list_scroll_offset = ctx->selected_index;
    }
    if (ctx->selected_index >= ctx->list_scroll_offset + 4) {
        ctx->list_scroll_offset = ctx->selected_index - 3;
    }
    if (ctx->list_scroll_offset < 0) {
        ctx->list_scroll_offset = 0;
    }
}

static void ui_redraw_if_needed(AppContext* ctx, ProductsCache* cache) {
    const bool screen_changed = ctx->screen != g_last_screen;
    const bool selection_changed =
        ctx->selected_index != g_last_selected ||
        ctx->detail_index != g_last_detail ||
        ctx->list_scroll_offset != g_last_scroll;
    const bool data_changed = cache->count != g_last_count;

    const bool full_redraw =
        g_ui_dirty || screen_changed || selection_changed || data_changed;

    if (!full_redraw) {
        if (ctx->fetching != g_last_fetching) {
            ui_repaint_header(ctx, cache);
            g_last_fetching = ctx->fetching;
        }
        return;
    }

    ui_redraw(ctx, cache);
    g_ui_dirty = false;
    g_last_screen = ctx->screen;
    g_last_selected = ctx->selected_index;
    g_last_detail = ctx->detail_index;
    g_last_scroll = ctx->list_scroll_offset;
    g_last_fetching = ctx->fetching;
    g_last_count = cache->count;
}

static void request_detail_thumb(ProductsCache* cache, const AppContext* ctx) {
    const Product* p = cache_get(cache, ctx->detail_index);
    if (p) {
        Serial.printf("[thumb] APP request_detail id=%lu url_len=%u\n",
            (unsigned long)p->id, (unsigned)strlen(p->thumbnail_url));
        thumb_loader_request(p->id, p->thumbnail_url);
    } else {
        Serial.println("[thumb] APP request_detail SKIP no product");
    }
}

static bool app_fetch(AppContext* ctx, ProductsCache* cache, bool keep_cached_on_error) {
    if (thumb_loader_is_busy()) {
        Serial.println("[thumb] APP fetch deferred (thumb busy)");
        return keep_cached_on_error && cache->valid;
    }

    if (!wifi_ensure_connected(10)) {
        ctx->last_http_code = 0;
        if (!keep_cached_on_error || !cache->valid) {
            ctx->screen = ScreenErrorWiFi;
            mark_ui_dirty();
        }
        return false;
    }

    const bool background = keep_cached_on_error && cache->valid &&
        (ctx->screen == ScreenList || ctx->screen == ScreenDetail);

    ctx->fetching = true;
    if (background) {
        ui_repaint_header(ctx, cache);
    } else {
        mark_ui_dirty();
        ui_redraw_if_needed(ctx, cache);
    }

    TindieFetchResponse resp = tindie_fetch_all_products(g_fetch_scratch, kMaxProducts);
    ctx->fetching = false;

    if (resp.result == FetchOk) {
        const bool changed = cache_data_changed(cache, g_fetch_scratch, resp.product_count);
        cache_update(cache, g_fetch_scratch, resp.product_count);
        if (changed) {
            mark_ui_dirty();
        } else if (background) {
            ui_repaint_header(ctx, cache);
        }
        clamp_selection(ctx, cache);
        ui_redraw_if_needed(ctx, cache);
        return true;
    }

    ctx->last_http_code = resp.http_code;

    if (resp.result == FetchUnauthorized && resp.http_code == 401) {
        ctx->screen = ScreenErrorApi;
        mark_ui_dirty();
        ui_redraw_if_needed(ctx, cache);
        return false;
    }

    if (resp.result == FetchParseError) {
        if (!keep_cached_on_error || !cache->valid) {
            ctx->last_http_code = 0;
            ctx->screen = ScreenErrorApi;
            mark_ui_dirty();
        } else {
            ui_repaint_header(ctx, cache);
        }
        ui_redraw_if_needed(ctx, cache);
        return false;
    }

    if (!keep_cached_on_error || !cache->valid) {
        if (resp.result == FetchNetworkError || !wifi_is_connected()) {
            ctx->screen = ScreenErrorWiFi;
        } else {
            ctx->screen = ScreenErrorApi;
        }
        mark_ui_dirty();
    } else {
        ui_repaint_header(ctx, cache);
    }
    ui_redraw_if_needed(ctx, cache);
    return false;
}

void app_init(AppContext* ctx, ProductsCache* cache) {
    thumb_loader_init();
    cache_init(cache);
    ctx->screen = ScreenWiFiConnecting;
    ctx->selected_index = 0;
    ctx->detail_index = 0;
    ctx->list_scroll_offset = 0;
    ctx->fetching = false;
    ctx->config_ok = config_is_valid();
    ctx->last_http_code = 0;
    g_last_poll_ms = millis();
    mark_ui_dirty();
}

void app_tick(AppContext* ctx, ProductsCache* cache) {
    StickCP2.update();
    thumb_loader_tick();

    const bool thumb_busy = thumb_loader_is_busy();
    if (g_thumb_busy_prev && !thumb_busy && ctx->screen == ScreenDetail) {
        Serial.println("[thumb] APP ui_redraw after download finished");
        mark_ui_dirty();
    }
    g_thumb_busy_prev = thumb_busy;

    const unsigned long now = millis();

    if (ctx->screen == ScreenList || ctx->screen == ScreenDetail) {
        if (now - g_last_poll_ms >= (unsigned long)POLL_INTERVAL_SEC * 1000UL) {
            if (!thumb_loader_is_busy()) {
                app_fetch(ctx, cache, true);
                g_last_poll_ms = now;
            }
        }
    }

    if (StickCP2.BtnA.wasPressed() && ctx->screen == ScreenList && cache->count > 0) {
        ctx->detail_index = ctx->selected_index;
        ctx->screen = ScreenDetail;
        request_detail_thumb(cache, ctx);
        mark_ui_dirty();
    } else if (StickCP2.BtnA.wasPressed() && ctx->screen == ScreenDetail && cache->count > 0) {
        ctx->detail_index = (ctx->detail_index + 1) % cache->count;
        request_detail_thumb(cache, ctx);
        mark_ui_dirty();
    }

    if (StickCP2.BtnB.wasPressed() && ctx->screen == ScreenList && cache->count > 0) {
        ctx->selected_index = (ctx->selected_index + 1) % cache->count;
        clamp_selection(ctx, cache);
        mark_ui_dirty();
    } else if (StickCP2.BtnB.wasPressed() && ctx->screen == ScreenDetail) {
        ctx->selected_index = ctx->detail_index;
        ctx->screen = ScreenList;
        clamp_selection(ctx, cache);
        mark_ui_dirty();
    }

    ui_redraw_if_needed(ctx, cache);
}

bool app_bootstrap(AppContext* ctx, ProductsCache* cache) {
    if (!ctx->config_ok) {
        ctx->screen = ScreenErrorConfig;
        mark_ui_dirty();
        ui_redraw_if_needed(ctx, cache);
        return false;
    }

    mark_ui_dirty();
    ui_draw_wifi_connecting();
    if (!wifi_connect_blocking(20)) {
        ctx->screen = ScreenErrorWiFi;
        mark_ui_dirty();
        ui_redraw_if_needed(ctx, cache);
        return false;
    }

    wifi_sync_time();

    if (!app_fetch(ctx, cache, false)) {
        ui_redraw_if_needed(ctx, cache);
        return false;
    }

    ctx->screen = ScreenList;
    mark_ui_dirty();
    ui_redraw_if_needed(ctx, cache);
    return true;
}
