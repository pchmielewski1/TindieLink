#include "ui_frame_dump.h"
#include "config.h"
#include <M5StickCPlus2.h>

#ifndef UI_FRAME_DUMP
#define UI_FRAME_DUMP 0
#endif

#if UI_FRAME_DUMP

#include <mbedtls/base64.h>
#include <Arduino.h>
#include <stdlib.h>

static void serial_write_b64_chunks(const uint8_t* b64, size_t len) {
    static const size_t kChunk = 512;
    for (size_t i = 0; i < len; i += kChunk) {
        const size_t n = (len - i < kChunk) ? (len - i) : kChunk;
        Serial.write(b64 + i, n);
    }
}

void ui_frame_dump_after_redraw(void) {
    const int16_t w = StickCP2.Display.width();
    const int16_t h = StickCP2.Display.height();
    if (w <= 0 || h <= 0) {
        return;
    }

    StickCP2.Display.setTextDatum(textdatum_t::top_left);

    const size_t pixel_bytes = (size_t)w * (size_t)h * sizeof(uint16_t);
    uint16_t* pixels = static_cast<uint16_t*>(malloc(pixel_bytes));
    if (!pixels) {
        Serial.println("[screenshot] malloc failed for pixel buffer");
        return;
    }

    StickCP2.Display.readRect(0, 0, w, h, pixels);

    size_t b64_len = 0;
    const uint8_t* raw = reinterpret_cast<const uint8_t*>(pixels);
    mbedtls_base64_encode(nullptr, 0, &b64_len, raw, pixel_bytes);

    uint8_t* b64 = static_cast<uint8_t*>(malloc(b64_len + 1));
    if (!b64) {
        Serial.println("[screenshot] malloc failed for base64");
        free(pixels);
        return;
    }

    size_t written = 0;
    mbedtls_base64_encode(b64, b64_len + 1, &written, raw, pixel_bytes);
    b64[written] = '\0';
    free(pixels);

    Serial.println("---FRAMEBUFFER_START---");
    Serial.printf("%d %d RGB565\n", (int)w, (int)h);
    serial_write_b64_chunks(b64, written);
    Serial.println();
    Serial.println("---FRAMEBUFFER_END---");
    Serial.printf("[screenshot] dumped %dx%d (%u bytes base64)\n",
        (int)w, (int)h, (unsigned)written);
    Serial.flush();

    free(b64);
}

#else

void ui_frame_dump_after_redraw(void) {}

#endif
