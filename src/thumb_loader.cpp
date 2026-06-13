#include "thumb_loader.h"
#include "thumb_jpeg.h"
#include "tindie_client.h"
#include "ui_common.h"
#include "config.h"
#include <M5StickCPlus2.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <esp_heap_caps.h>
#include <string.h>
#include <stdarg.h>

#ifndef THUMB_CACHE_SLOTS
#define THUMB_CACHE_SLOTS 16
#endif

#if THUMB_CACHE_SLOTS < 1 || THUMB_CACHE_SLOTS > 32
#error "THUMB_CACHE_SLOTS must be 1..32"
#endif

// Transient download cap (medium PNG ~680 KB); cache holds baked RGB565 only (~32 KB).
static const size_t kThumbDlMaxBytes = 720000;
static const size_t kThumbDlReserve = 32768;
static const size_t kThumbInternalReserve = 36000;
static const uint8_t kThumbAllocMaxFails = 3;
static const unsigned long kThumbHttpTimeoutMs = 20000;
static const size_t kThumbReadChunk = 1024;
static const size_t kThumbTickBudget = 16384;
static const unsigned long kThumbStallLogMs = 3000;
static const unsigned long kThumbRetryLogMs = 5000;

enum class ThumbFormat : uint8_t { None = 0, Png, Jpeg, Baked };

struct ThumbSlot {
    uint32_t product_id;
    uint32_t url_hash;
    unsigned long fetched_at_ms;
    ThumbFormat fmt;
    int16_t bake_w;
    int16_t bake_h;
    size_t size;
    uint8_t* data;
    char url[kThumbUrlLen];
    bool decode_ok;
};

static ThumbSlot g_slots[THUMB_CACHE_SLOTS] = {};
static bool g_session_active = false;
static WiFiClientSecure* g_tls = nullptr;
static HTTPClient* g_http = nullptr;
static uint8_t* g_dl_buf = nullptr;
static size_t g_dl_len = 0;
static size_t g_dl_cap = 0;
static uint32_t g_dl_product_id = 0;
static uint32_t g_dl_url_hash = 0;
static unsigned long g_dl_start_ms = 0;
static int g_dl_expected = -1;
static bool g_need_get = false;

static char g_pending_url[kThumbUrlLen];
static uint32_t g_pending_id = 0;
static bool g_has_pending = false;
static bool g_resolving_url = false;
static uint8_t g_alloc_fail_streak = 0;
static uint32_t g_abandon_product_id = 0;
static uint32_t g_abandon_url_hash = 0;

static unsigned long g_last_stall_log_ms = 0;
static unsigned long g_last_retry_log_ms = 0;
static uint32_t g_last_draw_log_product = 0;
static uint8_t g_last_draw_log_code = 255;

static void thumb_log(const char* step, const char* fmt, ...) {
    Serial.print("[thumb] ");
    Serial.print(step);
    Serial.print(" ");
    va_list ap;
    va_start(ap, fmt);
    char buf[160];
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    Serial.println(buf);
}

static void thumb_log_url(const char* step, const char* url) {
    if (!url || url[0] == '\0') {
        thumb_log(step, "url=(empty)");
        return;
    }
    const size_t n = strlen(url);
    if (n <= 120) {
        thumb_log(step, "url=%s", url);
    } else {
        thumb_log(step, "url=%.117s...", url);
    }
}

static void thumb_log_heap(const char* step) {
    thumb_log(step, "heap int_free=%u int_largest=%u psram_free=%u psram_largest=%u",
        (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
        (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL),
        (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
        (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM));
}

static uint32_t hash_url(const char* url) {
    uint32_t h = 2166136261u;
    if (!url) {
        return 0;
    }
    for (const char* p = url; *p; ++p) {
        h ^= (uint8_t)*p;
        h *= 16777619u;
    }
    return h;
}

static void slot_clear(ThumbSlot* slot) {
    if (!slot) {
        return;
    }
    if (slot->data) {
        thumb_log("CACHE_FREE", "product_id=%lu bytes=%u",
            (unsigned long)slot->product_id, (unsigned)slot->size);
        heap_caps_free(slot->data);
    }
    slot->data = nullptr;
    slot->size = 0;
    slot->bake_w = 0;
    slot->bake_h = 0;
    slot->fmt = ThumbFormat::None;
    slot->url[0] = '\0';
    slot->decode_ok = false;
    slot->product_id = 0;
    slot->url_hash = 0;
    slot->fetched_at_ms = 0;
}

static void cache_clear_all() {
    for (int i = 0; i < THUMB_CACHE_SLOTS; ++i) {
        slot_clear(&g_slots[i]);
    }
}

static bool slot_has_baked(const ThumbSlot* slot, uint32_t product_id) {
    return slot && slot->data && slot->decode_ok && slot->product_id == product_id &&
        slot->fmt == ThumbFormat::Baked && slot->bake_w > 0 && slot->bake_h > 0;
}

static bool slot_usable(const ThumbSlot* slot, uint32_t product_id, uint32_t url_hash) {
    return slot_has_baked(slot, product_id) && slot->url_hash == url_hash;
}

static void cache_touch(ThumbSlot* slot) {
    if (slot) {
        slot->fetched_at_ms = millis();
    }
}

static ThumbSlot* cache_find(uint32_t product_id, uint32_t url_hash) {
    for (int i = 0; i < THUMB_CACHE_SLOTS; ++i) {
        if (slot_usable(&g_slots[i], product_id, url_hash)) {
            cache_touch(&g_slots[i]);
            return &g_slots[i];
        }
    }
    return nullptr;
}

static bool cache_hit(uint32_t product_id, uint32_t url_hash) {
    return cache_find(product_id, url_hash) != nullptr;
}

static ThumbSlot* cache_find_product(uint32_t product_id) {
    for (int i = 0; i < THUMB_CACHE_SLOTS; ++i) {
        ThumbSlot* slot = &g_slots[i];
        if (slot_has_baked(slot, product_id)) {
            cache_touch(slot);
            return slot;
        }
    }
    return nullptr;
}

static ThumbSlot* cache_alloc_slot(uint32_t product_id, uint32_t url_hash) {
    ThumbSlot* reuse = nullptr;
    int empty_idx = -1;
    int lru_idx = 0;
    unsigned long lru_time = ULONG_MAX;

    for (int i = 0; i < THUMB_CACHE_SLOTS; ++i) {
        ThumbSlot* slot = &g_slots[i];
        if (slot->product_id == product_id) {
            reuse = slot;
        }
        if (!slot->data && empty_idx < 0) {
            empty_idx = i;
        }
        if (slot->fetched_at_ms < lru_time) {
            lru_time = slot->fetched_at_ms;
            lru_idx = i;
        }
    }

    if (reuse) {
        slot_clear(reuse);
        return reuse;
    }
    if (empty_idx >= 0) {
        return &g_slots[empty_idx];
    }

    ThumbSlot* evict = &g_slots[lru_idx];
    thumb_log("CACHE_EVICT", "product_id=%lu for new=%lu",
        (unsigned long)evict->product_id, (unsigned long)product_id);
    slot_clear(evict);
    (void)url_hash;
    return evict;
}

static void schedule_request(uint32_t product_id, const char* url);

static bool png_dimensions(const uint8_t* data, size_t len, int32_t* out_w, int32_t* out_h) {
    if (len < 24 || data[0] != 0x89 || data[1] != 'P' || data[2] != 'N' || data[3] != 'G') {
        return false;
    }
    *out_w = (int32_t)(((uint32_t)data[16] << 24) | ((uint32_t)data[17] << 16) |
                        ((uint32_t)data[18] << 8) | (uint32_t)data[19]);
    *out_h = (int32_t)(((uint32_t)data[20] << 24) | ((uint32_t)data[21] << 16) |
                        ((uint32_t)data[22] << 8) | (uint32_t)data[23]);
    return *out_w > 0 && *out_h > 0;
}

static bool jpeg_dimensions(const uint8_t* data, size_t len, int32_t* out_w, int32_t* out_h) {
    for (size_t i = 0; i + 8 < len; ++i) {
        if (data[i] != 0xFF) {
            continue;
        }
        const uint8_t marker = data[i + 1];
        if (marker == 0xC0 || marker == 0xC1 || marker == 0xC2) {
            *out_h = (int32_t)((data[i + 5] << 8) | data[i + 6]);
            *out_w = (int32_t)((data[i + 7] << 8) | data[i + 8]);
            return *out_w > 0 && *out_h > 0;
        }
    }
    return false;
}

static bool image_dimensions(const uint8_t* data, size_t len, ThumbFormat fmt, int32_t* out_w,
    int32_t* out_h) {
    if (fmt == ThumbFormat::Png) {
        return png_dimensions(data, len, out_w, out_h);
    }
    if (fmt == ThumbFormat::Jpeg) {
        return jpeg_dimensions(data, len, out_w, out_h);
    }
    return false;
}

/** CDN fit-in uses filters:fill(fff) — replace near-white letterbox with panel black. */
static uint8_t rgb565_luma8(uint16_t c) {
    const int r = ((c >> 11) & 0x1F) * 255 / 31;
    const int g = ((c >> 5) & 0x3F) * 255 / 63;
    const int b = (c & 0x1F) * 255 / 31;
    return (uint8_t)((r * 77 + g * 150 + b * 29) >> 8);
}

static void baked_sanitize_letterbox(uint16_t* pix, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        if (rgb565_luma8(pix[i]) >= 248) {
            pix[i] = kThumbPanelBg;
        }
    }
}

static void thumb_panel_dims(int16_t* out_w, int16_t* out_h) {
    const UiMetrics m = ui_metrics();
    const UiThumbPanel p = ui_detail_thumb_panel(m);
    *out_w = p.iw;
    *out_h = p.ih;
}

static bool bake_png_to_sprite(lgfx::LGFX_Sprite* spr, const uint8_t* data, size_t len, int16_t box_h) {
    int32_t iw = 0;
    int32_t ih = 0;
    if (!image_dimensions(data, len, ThumbFormat::Png, &iw, &ih) || ih <= 0) {
        return false;
    }
    const float z = thumb_panel_zoom(box_h, (int)ih);
    const int16_t draw_w = (int16_t)(iw * z + 0.5f);
    const int16_t draw_h = box_h;
    if (draw_w <= 0 || draw_h <= 0) {
        return false;
    }
    if (!spr->createSprite(draw_w, draw_h)) {
        return false;
    }
    spr->fillSprite(kThumbPanelBg);
    StickCP2.Display.releasePngMemory();
    return spr->drawPng(data, len, 0, 0, draw_w, draw_h, 0, 0, z, z, lgfx::datum_t::top_left);
}

static bool bake_raw_to_slot(ThumbSlot* slot, uint8_t* raw, size_t raw_len, ThumbFormat fmt) {
    int16_t box_w = 0;
    int16_t box_h = 0;
    thumb_panel_dims(&box_w, &box_h);
    if (box_w <= 0 || box_h <= 0) {
        return false;
    }

    lgfx::LGFX_Sprite spr(&StickCP2.Display);
    spr.setPsram(true);
    spr.setColorDepth(StickCP2.Display.getColorDepth());

    bool rendered = false;
    if (fmt == ThumbFormat::Jpeg) {
        int32_t iw = 0;
        int32_t ih = 0;
        if (!image_dimensions(raw, raw_len, ThumbFormat::Jpeg, &iw, &ih) || ih <= 0) {
            return false;
        }
        const float z = thumb_panel_zoom(box_h, (int)ih);
        const int16_t draw_w = (int16_t)(iw * z + 0.5f);
        const int16_t draw_h = box_h;
        if (!spr.createSprite(draw_w, draw_h)) {
            thumb_log("BAKE_JPEG_SPRITE_FAIL", "draw=%dx%d", (int)draw_w, (int)draw_h);
            return false;
        }
        rendered = thumb_jpeg_bake_to_sprite(&spr, raw, raw_len);
    } else if (fmt == ThumbFormat::Png) {
        rendered = bake_png_to_sprite(&spr, raw, raw_len, box_h);
    }

    if (!rendered) {
        spr.deleteSprite();
        thumb_log("BAKE_FAIL", "product_id=%lu fmt=%s", (unsigned long)slot->product_id,
            fmt == ThumbFormat::Jpeg ? "jpeg" : "png");
        return false;
    }

    const int16_t bake_w = spr.width();
    const int16_t bake_h = spr.height();
    const size_t bytes = (size_t)bake_w * (size_t)bake_h * 2;
    uint8_t* pixels = (uint8_t*)heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!pixels) {
        pixels = (uint8_t*)heap_caps_malloc(bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    }
    if (!pixels) {
        spr.deleteSprite();
        thumb_log("BAKE_FAIL", "pixels alloc %u", (unsigned)bytes);
        return false;
    }
    memcpy(pixels, spr.getBuffer(), bytes);
    spr.deleteSprite();
    baked_sanitize_letterbox(reinterpret_cast<uint16_t*>(pixels), bytes / 2);

    slot->data = pixels;
    slot->size = bytes;
    slot->bake_w = bake_w;
    slot->bake_h = bake_h;
    slot->fmt = ThumbFormat::Baked;
    slot->decode_ok = true;

    thumb_log("BAKE_OK", "product_id=%lu baked=%dx%d bytes=%u", (unsigned long)slot->product_id,
        (int)bake_w, (int)bake_h, (unsigned)bytes);
    return true;
}

static void draw_baked(const ThumbSlot* slot, int16_t x, int16_t y, int16_t w, int16_t h) {
    const int16_t px = (int16_t)(x + (w - slot->bake_w) / 2);
    const int16_t py = (int16_t)(y + (h - slot->bake_h) / 2);

    ui_fill_rect(x, y, w, h, kThumbPanelBg);
    StickCP2.Display.setClipRect(x, y, w, h);
    StickCP2.Display.pushImage(px, py, slot->bake_w, slot->bake_h,
        reinterpret_cast<const uint16_t*>(slot->data));
    StickCP2.Display.clearClipRect();

    thumb_log("DRAW_BAKED", "box=%dx%d baked=%dx%d at=%d,%d", (int)w, (int)h, (int)slot->bake_w,
        (int)slot->bake_h, (int)px, (int)py);
}

static void draw_give_up(ThumbSlot* slot) {
    if (!slot) {
        return;
    }
    const uint32_t product_id = slot->product_id;
    const uint32_t url_hash = slot->url_hash;
    slot_clear(slot);
    g_abandon_product_id = product_id;
    g_abandon_url_hash = url_hash;
    thumb_log("DRAW_GIVE_UP", "product_id=%lu (no re-download)", (unsigned long)product_id);
}

static void release_http() {
    if (g_http) {
        thumb_log("HTTP_END", "closing client");
        g_http->end();
        delete g_http;
        g_http = nullptr;
    }
    if (g_tls) {
        g_tls->stop();
        delete g_tls;
        g_tls = nullptr;
    }
}

static void release_download_buffer() {
    if (g_dl_buf) {
        thumb_log("BUF_FREE", "released download buffer cap=%u", (unsigned)g_dl_cap);
        heap_caps_free(g_dl_buf);
        g_dl_buf = nullptr;
    }
    g_dl_len = 0;
    g_dl_cap = 0;
}

static void end_session(const char* reason) {
    if (g_session_active || g_http || g_tls || g_dl_buf) {
        thumb_log("SESSION_END", "reason=%s product_id=%lu got=%u/%d session=%d pending=%d",
            reason ? reason : "?",
            (unsigned long)g_dl_product_id,
            (unsigned)g_dl_len,
            g_dl_expected,
            (int)g_session_active,
            (int)g_has_pending);
    }
    release_http();
    release_download_buffer();
    g_session_active = false;
    g_need_get = false;
    g_dl_expected = -1;
    g_dl_product_id = 0;
    g_dl_url_hash = 0;
}

static bool alloc_download_buffer_for(size_t need_bytes) {
    thumb_log_heap("BUF_ALLOC_TRY");

    size_t need = need_bytes > 0 ? need_bytes : 65536;
    if (need > kThumbDlMaxBytes) {
        need = kThumbDlMaxBytes;
    }

    const size_t psram_largest = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);
    if (psram_largest >= need + kThumbDlReserve) {
        g_dl_buf = (uint8_t*)heap_caps_malloc(need, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (g_dl_buf) {
            g_dl_cap = need;
            g_dl_len = 0;
            thumb_log("BUF_ALLOC_OK", "where=PSRAM cap=%u need=%u", (unsigned)g_dl_cap,
                (unsigned)need_bytes);
            return true;
        }
    }

    thumb_log("BUF_ALLOC_FAIL", "psram_largest=%u need=%u (baked cache only — free raw slots)",
        (unsigned)psram_largest, (unsigned)need);
    return false;
}

static ThumbFormat detect_image_format(const uint8_t* data, size_t len) {
    if (len >= 4 && data[0] == 0x89 && data[1] == 0x50 && data[2] == 0x4E && data[3] == 0x47) {
        return ThumbFormat::Png;
    }
    if (len >= 3 && data[0] == 0xFF && data[1] == 0xD8 && data[2] == 0xFF) {
        return ThumbFormat::Jpeg;
    }
    return ThumbFormat::None;
}

static bool wifi_ready() {
    return WiFi.status() == WL_CONNECTED;
}

static void schedule_request(uint32_t product_id, const char* url) {
    g_pending_id = product_id;
    strncpy(g_pending_url, url, kThumbUrlLen - 1);
    g_pending_url[kThumbUrlLen - 1] = '\0';
    g_has_pending = true;
    g_alloc_fail_streak = 0;
    thumb_log("SCHEDULE", "product_id=%lu url_hash=%08lx",
        (unsigned long)product_id, (unsigned long)hash_url(url));
    thumb_log_url("SCHEDULE", url);
}

static bool start_session(uint32_t product_id, const char* url, uint32_t url_hash) {
    thumb_log("SESSION_START", "product_id=%lu url_hash=%08lx", (unsigned long)product_id,
        (unsigned long)url_hash);
    thumb_log_url("SESSION_START", url);
    thumb_log_heap("SESSION_START");

    end_session("restart");

    g_tls = new WiFiClientSecure();
    if (!g_tls) {
        thumb_log("TLS_FAIL", "new WiFiClientSecure failed");
        end_session("tls_new");
        return false;
    }
    g_tls->setInsecure();
    g_tls->setTimeout((uint32_t)(kThumbHttpTimeoutMs / 1000));
    thumb_log("TLS_OK", "client created setInsecure");

    g_http = new HTTPClient();
    if (!g_http) {
        thumb_log("HTTP_FAIL", "new HTTPClient failed");
        end_session("http_new");
        return false;
    }

    g_http->setReuse(false);
    g_http->setTimeout(kThumbHttpTimeoutMs);
    thumb_log("HTTP_BEGIN", "timeout_ms=%lu", kThumbHttpTimeoutMs);
    if (!g_http->begin(*g_tls, url)) {
        thumb_log("HTTP_BEGIN_FAIL", "http.begin returned false");
        end_session("http_begin");
        return false;
    }

    g_http->setUserAgent("TindieLink/1.0");
    g_dl_product_id = product_id;
    g_dl_url_hash = url_hash;
    g_dl_start_ms = millis();
    g_dl_expected = -1;
    g_need_get = true;
    g_session_active = true;
    g_last_stall_log_ms = 0;
    thumb_log("SESSION_READY", "defer GET to next tick");
    return true;
}

static void commit_to_cache() {
    thumb_log("COMMIT_TRY", "bytes=%u product_id=%lu", (unsigned)g_dl_len,
        (unsigned long)g_dl_product_id);

    if (!g_dl_buf || g_dl_len < 8) {
        thumb_log("COMMIT_FAIL", "buffer too small len=%u", (unsigned)g_dl_len);
        end_session("commit_short");
        return;
    }

    const ThumbFormat fmt = detect_image_format(g_dl_buf, g_dl_len);
    if (fmt == ThumbFormat::None) {
        thumb_log("COMMIT_FAIL", "unknown image magic=%02X%02X%02X%02X",
            g_dl_buf[0], g_dl_buf[1], g_dl_buf[2], g_dl_buf[3]);
        end_session("commit_bad_format");
        return;
    }

    ThumbSlot* slot = cache_alloc_slot(g_dl_product_id, g_dl_url_hash);
    if (!slot) {
        thumb_log("COMMIT_FAIL", "no cache slot");
        end_session("commit_no_slot");
        return;
    }

    uint8_t* raw = g_dl_buf;
    const size_t raw_len = g_dl_len;
    g_dl_buf = nullptr;
    g_dl_len = 0;
    g_dl_cap = 0;

    slot->product_id = g_dl_product_id;
    slot->url_hash = g_dl_url_hash;
    strncpy(slot->url, g_pending_url, kThumbUrlLen - 1);
    slot->url[kThumbUrlLen - 1] = '\0';

    const bool baked = bake_raw_to_slot(slot, raw, raw_len, fmt);
    heap_caps_free(raw);

    end_session("commit_ok");

    if (!baked) {
        slot_clear(slot);
        thumb_log("CACHE_STORED_FAIL", "product_id=%lu", (unsigned long)g_dl_product_id);
        return;
    }

    slot->fetched_at_ms = millis();

    thumb_log("CACHE_STORED", "product_id=%lu baked=%dx%d bytes=%u slots=%d",
        (unsigned long)slot->product_id, (int)slot->bake_w, (int)slot->bake_h,
        (unsigned)slot->size, THUMB_CACHE_SLOTS);
    g_last_draw_log_product = 0;
    g_last_draw_log_code = 255;
}

static bool body_complete() {
    if (g_dl_expected > 0) {
        return g_dl_len >= (size_t)g_dl_expected;
    }
    if (!g_http) {
        return false;
    }
    WiFiClient* stream = g_http->getStreamPtr();
    const bool connected = g_http->connected();
    const int avail = stream ? stream->available() : -1;
    return stream && !connected && avail == 0 && g_dl_len > 0;
}

static void log_body_stall() {
    const unsigned long now = millis();
    if (now - g_last_stall_log_ms < kThumbStallLogMs) {
        return;
    }
    g_last_stall_log_ms = now;

    WiFiClient* stream = g_http ? g_http->getStreamPtr() : nullptr;
    const bool connected = g_http ? g_http->connected() : false;
    const int avail = stream ? stream->available() : -1;
    const bool complete = body_complete();

    thumb_log("STALL", "elapsed_ms=%lu got=%u expect=%d connected=%d avail=%d complete=%d",
        now - g_dl_start_ms,
        (unsigned)g_dl_len,
        g_dl_expected,
        (int)connected,
        avail,
        (int)complete);
}

static void try_start_pending() {
    if (!g_has_pending) {
        return;
    }
    if (g_session_active) {
        thumb_log("START_SKIP", "already in session");
        return;
    }
    if (!wifi_ready()) {
        const unsigned long now = millis();
        if (now - g_last_retry_log_ms >= kThumbRetryLogMs) {
            g_last_retry_log_ms = now;
            thumb_log("START_WAIT_WIFI", "status=%d pending product_id=%lu",
                (int)WiFi.status(), (unsigned long)g_pending_id);
        }
        return;
    }

    const uint32_t url_hash = hash_url(g_pending_url);
    thumb_log("START_TRY", "product_id=%lu url_hash=%08lx",
        (unsigned long)g_pending_id, (unsigned long)url_hash);

    if (cache_hit(g_pending_id, url_hash)) {
        thumb_log("START_SKIP", "cache already valid");
        g_has_pending = false;
        return;
    }

    if (start_session(g_pending_id, g_pending_url, url_hash)) {
        g_has_pending = false;
        g_alloc_fail_streak = 0;
        thumb_log("START_OK", "session active, GET next tick");
    } else {
        g_alloc_fail_streak++;
        const unsigned long now = millis();
        if (g_alloc_fail_streak >= kThumbAllocMaxFails) {
            g_abandon_product_id = g_pending_id;
            g_abandon_url_hash = url_hash;
            g_has_pending = false;
            thumb_log("GIVE_UP", "brak pamieci po %u probach product_id=%lu — szary placeholder",
                (unsigned)g_alloc_fail_streak, (unsigned long)g_pending_id);
            return;
        }
        if (now - g_last_retry_log_ms >= kThumbRetryLogMs) {
            g_last_retry_log_ms = now;
            thumb_log("START_FAIL", "retry %u/%u product_id=%lu",
                (unsigned)g_alloc_fail_streak, (unsigned)kThumbAllocMaxFails,
                (unsigned long)g_pending_id);
        }
    }
}

void thumb_loader_init() {
    thumb_log("INIT", "cache_slots=%d baked_rgb565 dl_max=%u",
        THUMB_CACHE_SLOTS, (unsigned)kThumbDlMaxBytes);
    thumb_log_heap("INIT");
    cache_clear_all();
    end_session("init");
    g_has_pending = false;
    g_pending_id = 0;
    g_last_draw_log_product = 0;
    g_last_draw_log_code = 255;
}

void thumb_loader_request(uint32_t product_id, const char* thumbnail_url) {
    if (!thumbnail_url || thumbnail_url[0] == '\0') {
        thumb_log("REQUEST_SKIP", "empty thumbnail_url product_id=%lu",
            (unsigned long)product_id);
        return;
    }
    if (product_id == 0) {
        thumb_log("REQUEST_SKIP", "product_id=0");
        return;
    }

    if (cache_find_product(product_id)) {
        thumb_log("REQUEST_SKIP", "cache hit product_id=%lu (no api)", (unsigned long)product_id);
        return;
    }

    char fetch_url[kThumbUrlLen];
    g_resolving_url = true;
    const bool have_url =
        tindie_resolve_primary_image_url(product_id, thumbnail_url, fetch_url, sizeof(fetch_url));
    g_resolving_url = false;
    if (!have_url) {
        thumb_log("REQUEST_SKIP", "no image url product_id=%lu", (unsigned long)product_id);
        return;
    }

    const uint32_t url_hash = hash_url(fetch_url);

    if (g_abandon_product_id == product_id && g_abandon_url_hash == url_hash) {
        return;
    }

    if (g_has_pending && g_pending_id == product_id && hash_url(g_pending_url) == url_hash) {
        return;
    }

    if (cache_hit(product_id, url_hash)) {
        thumb_log("REQUEST_SKIP", "cache hit product_id=%lu", (unsigned long)product_id);
        return;
    }

    if (g_session_active && g_dl_product_id == product_id && g_dl_url_hash == url_hash) {
        thumb_log("REQUEST_SKIP", "download in progress product_id=%lu got=%u",
            (unsigned long)product_id, (unsigned)g_dl_len);
        return;
    }

    g_abandon_product_id = 0;
    g_abandon_url_hash = 0;
    g_alloc_fail_streak = 0;

    thumb_log("REQUEST", "product_id=%lu url_hash=%08lx session=%d pending=%d",
        (unsigned long)product_id, (unsigned long)url_hash,
        (int)g_session_active, (int)g_has_pending);
    thumb_log_url("REQUEST", fetch_url);

    schedule_request(product_id, fetch_url);
    if (g_session_active) {
        thumb_log("REQUEST", "aborting previous session for new URL");
        end_session("request_replaced");
    }
}

bool thumb_loader_is_busy() {
    return g_session_active || g_has_pending || g_resolving_url;
}

void thumb_loader_tick() {
    if (g_has_pending && !g_session_active) {
        try_start_pending();
        if (g_session_active) {
            thumb_log("TICK", "deferred after SESSION_READY");
            return;
        }
    }

    if (!g_session_active || !g_http) {
        return;
    }

    if (!wifi_ready()) {
        thumb_log("TICK_FAIL", "WiFi lost during download");
        end_session("wifi_lost");
        return;
    }

    if (millis() - g_dl_start_ms > kThumbHttpTimeoutMs) {
        thumb_log("TICK_FAIL", "timeout after %lu ms got=%u expect=%d",
            kThumbHttpTimeoutMs, (unsigned)g_dl_len, g_dl_expected);
        end_session("timeout");
        return;
    }

    if (g_need_get) {
        thumb_log("HTTP_GET", "calling GET() product_id=%lu", (unsigned long)g_dl_product_id);
        const unsigned long t0 = millis();
        const int code = g_http->GET();
        const unsigned long dt = millis() - t0;
        g_need_get = false;
        thumb_log("HTTP_GET", "code=%d duration_ms=%lu", code, dt);
        if (code != HTTP_CODE_OK) {
            const String err_body = g_http->getString();
            if (err_body.length() > 0) {
                thumb_log("HTTP_GET_FAIL", "code=%d body=%.96s", code, err_body.c_str());
            } else {
                thumb_log("HTTP_GET_FAIL", "code=%d (empty body)", code);
            }
            end_session("http_code");
            return;
        }
        g_dl_expected = g_http->getSize();
        const char* ctype = g_http->header("Content-Type").c_str();
        thumb_log("HTTP_HEADERS", "Content-Length=%d type=%s",
            g_dl_expected, ctype ? ctype : "?");

        const size_t need =
            g_dl_expected > 0 ? (size_t)g_dl_expected : (size_t)kThumbDlMaxBytes;
        if (!g_dl_buf && !alloc_download_buffer_for(need)) {
            end_session("alloc_fail");
            return;
        }
        if (g_dl_expected > 0 && (size_t)g_dl_expected > g_dl_cap) {
            thumb_log("HTTP_TOO_LARGE", "Content-Length=%d cap=%u psram_largest=%u",
                g_dl_expected, (unsigned)g_dl_cap,
                (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM));
            end_session("too_large");
            return;
        }
        if (g_dl_expected <= 0) {
            thumb_log("HTTP_HEADERS", "chunked or unknown length — completion via stream close");
        }
    }

    WiFiClient* stream = g_http->getStreamPtr();
    if (!stream) {
        thumb_log("TICK_FAIL", "getStreamPtr null");
        end_session("no_stream");
        return;
    }

    uint8_t chunk[kThumbReadChunk];
    size_t budget = kThumbTickBudget;
    size_t read_this_tick = 0;
    while (budget > 0) {
        const int avail = stream->available();
        if (avail <= 0) {
            break;
        }
        const size_t want = (size_t)avail < kThumbReadChunk ? (size_t)avail : kThumbReadChunk;
        const size_t to_read = want < budget ? want : budget;
        const int n = stream->readBytes(chunk, to_read);
        if (n <= 0) {
            thumb_log("READ", "readBytes returned %d avail=%d", n, avail);
            break;
        }
        if (g_dl_len + (size_t)n > g_dl_cap) {
            thumb_log("TICK_FAIL", "overflow len=%u + %d > cap=%u",
                (unsigned)g_dl_len, n, (unsigned)g_dl_cap);
            end_session("overflow");
            return;
        }
        memcpy(g_dl_buf + g_dl_len, chunk, (size_t)n);
        g_dl_len += (size_t)n;
        read_this_tick += (size_t)n;
        budget -= (size_t)n;
    }

    if (read_this_tick > 0) {
        thumb_log("READ", "+%u total=%u expect=%d", (unsigned)read_this_tick,
            (unsigned)g_dl_len, g_dl_expected);
    }

    if (body_complete()) {
        thumb_log("BODY_DONE", "total=%u expect=%d elapsed_ms=%lu",
            (unsigned)g_dl_len, g_dl_expected, millis() - g_dl_start_ms);
        commit_to_cache();
        return;
    }

    log_body_stall();
}

static void draw_placeholder(int16_t x, int16_t y, int16_t w, int16_t h) {
    ui_fill_rect(x, y, w, h, kThumbPanelBg);
    ui_fill_rect(x, y, w, 1, 0x4208);
    ui_fill_rect(x, y + h - 1, w, 1, 0x4208);
    ui_fill_rect(x, y, 1, h, 0x4208);
    ui_fill_rect(x + w - 1, y, 1, h, 0x4208);
}

void thumb_loader_draw(int16_t x, int16_t y, int16_t w, int16_t h, uint32_t product_id) {
    ThumbSlot* slot = cache_find_product(product_id);
    if (!slot) {
        if (g_last_draw_log_product != product_id || g_last_draw_log_code != 1) {
            g_last_draw_log_product = product_id;
            g_last_draw_log_code = 1;
            thumb_log("DRAW_PLACEHOLDER", "product_id=%lu reason=no_cache session=%d pending=%d busy=%d",
                (unsigned long)product_id,
                (int)g_session_active, (int)g_has_pending, (int)thumb_loader_is_busy());
        }
        draw_placeholder(x, y, w, h);
        return;
    }

    if (g_last_draw_log_product != product_id || g_last_draw_log_code != 10) {
        thumb_log("DRAW", "product_id=%lu baked=%dx%d at=%d,%d box=%dx%d",
            (unsigned long)product_id, (int)slot->bake_w, (int)slot->bake_h, (int)x, (int)y,
            (int)w, (int)h);
    }

    if (slot->fmt != ThumbFormat::Baked || !slot->data || slot->bake_w <= 0 || slot->bake_h <= 0) {
        thumb_log("DRAW_FAIL", "product_id=%lu reason=bad_baked", (unsigned long)product_id);
        g_last_draw_log_code = 11;
        draw_placeholder(x, y, w, h);
        draw_give_up(slot);
        return;
    }

    StickCP2.Display.startWrite();
    draw_baked(slot, x, y, w, h);
    StickCP2.Display.endWrite();

    thumb_log("DRAW_OK", "product_id=%lu", (unsigned long)product_id);
    g_last_draw_log_code = 10;
    ui_fill_rect(x, y, w, 1, 0x4208);
    ui_fill_rect(x, y + h - 1, w, 1, 0x4208);
    ui_fill_rect(x, y, 1, h, 0x4208);
    ui_fill_rect(x + w - 1, y, 1, h, 0x4208);
}
