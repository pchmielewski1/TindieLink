#include "thumb_jpeg.h"
#include "ui_common.h"
#include <JPEGDEC.h>
#include <M5GFX.h>
#include <Arduino.h>
#include <esp_heap_caps.h>

// JPEGDEC ~17.5 KB — must not live on the stack (avoids silent DRAW_FAIL / crash).
static JPEGDEC g_jpeg;
static lgfx::LovyanGFX* g_jpeg_panel = nullptr;

static int jpegdec_mcu_cb(JPEGDRAW* pDraw) {
    if (!g_jpeg_panel) {
        return 0;
    }
    g_jpeg_panel->pushImage(pDraw->x, pDraw->y, pDraw->iWidth, pDraw->iHeight, pDraw->pPixels);
    return 1;
}

static uint8_t* jpeg_copy_internal(const uint8_t* src, size_t len) {
    uint8_t* copy = (uint8_t*)heap_caps_malloc(len, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!copy) {
        Serial.printf("[thumb] JPEG_COPY_FAIL len=%u\n", (unsigned)len);
        return nullptr;
    }
    memcpy(copy, src, len);
    return copy;
}

static int pick_decode_scale(int iw, int ih, int box_w, int box_h) {
    static const int kScales[] = {0, JPEG_SCALE_HALF, JPEG_SCALE_QUARTER, JPEG_SCALE_EIGHTH};
    static const int kDivs[] = {1, 2, 4, 8};
    for (size_t i = 0; i < sizeof(kScales) / sizeof(kScales[0]); ++i) {
        const int div = kDivs[i];
        const int dw = (iw + div - 1) / div;
        const int dh = (ih + div - 1) / div;
        if (dw <= box_w && dh <= box_h) {
            return kScales[i];
        }
    }
    return JPEG_SCALE_EIGHTH;
}

bool thumb_jpeg_is_progressive(const uint8_t* data, size_t len) {
    for (size_t i = 0; i + 1 < len; ++i) {
        if (data[i] == 0xFF && data[i + 1] == 0xC2) {
            return true;
        }
    }
    return false;
}

bool thumb_jpeg_bake_to_sprite(lgfx::LGFX_Sprite* dst, const uint8_t* data, size_t len) {
    if (!dst || !data || len < 4) {
        return false;
    }

    const int16_t tw = dst->width();
    const int16_t th = dst->height();
    if (tw <= 0 || th <= 0) {
        return false;
    }

    uint8_t* owned = jpeg_copy_internal(data, len);
    const uint8_t* decode_ptr = owned ? owned : data;

    if (!g_jpeg.openRAM(const_cast<uint8_t*>(decode_ptr), (int)len, jpegdec_mcu_cb)) {
        Serial.printf("[thumb] JPEG_OPEN_FAIL err=%d\n", g_jpeg.getLastError());
        if (owned) {
            heap_caps_free(owned);
        }
        return false;
    }

    g_jpeg.setPixelType(RGB565_BIG_ENDIAN);

    const int iw = g_jpeg.getWidth();
    const int ih = g_jpeg.getHeight();
    const int scale = pick_decode_scale(iw, ih, tw, th);
    const int div = scale ? scale : 1;
    const int dw = (iw + div - 1) / div;
    const int dh = (ih + div - 1) / div;

    lgfx::LGFX_Sprite dec(dst);
    dec.setPsram(true);
    dec.setColorDepth(dst->getColorDepth());
    if (!dec.createSprite(dw, dh)) {
        Serial.printf("[thumb] JPEG_DEC_SPRITE_FAIL dw=%d dh=%d\n", dw, dh);
        g_jpeg.close();
        if (owned) {
            heap_caps_free(owned);
        }
        return false;
    }
    dec.fillSprite(kThumbPanelBg);

    g_jpeg_panel = static_cast<lgfx::LovyanGFX*>(&dec);
    const int dec_ok = g_jpeg.decode(0, 0, scale);
    g_jpeg_panel = nullptr;
    g_jpeg.close();

    if (owned) {
        heap_caps_free(owned);
    }

    if (!dec_ok) {
        Serial.printf("[thumb] JPEG_DECODE_FAIL err=%d scale=%d\n", g_jpeg.getLastError(), scale);
        dec.deleteSprite();
        return false;
    }

    const float z = thumb_panel_zoom(th, ih);
    const int out_w = (int)(iw * z + 0.5f);
    const int out_h = th;
    const float zx = dw > 0 ? (float)out_w / (float)dw : 1.0f;
    const float zy = dh > 0 ? (float)out_h / (float)dh : 1.0f;

    dst->fillSprite(kThumbPanelBg);
    dec.pushRotateZoom(dst, (float)tw * 0.5f, (float)th * 0.5f, 0.0f, zx, zy);
    dec.deleteSprite();
    return true;
}
