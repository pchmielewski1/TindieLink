#pragma once

#include <cstddef>
#include <cstdint>
#include <lgfx/v1/LGFX_Sprite.hpp>

/**
 * Scale to fill panel height (Tindie fit-in is 3:2 landscape canvas; portrait products
 * need full 103px height, horizontal overflow is center-cropped).
 */
inline float thumb_panel_zoom(int box_h, int img_h) {
    return (float)box_h / (float)img_h;
}

/** Render JPEG into an existing sprite (bake_w x bake_h). */
bool thumb_jpeg_bake_to_sprite(lgfx::LGFX_Sprite* dst, const uint8_t* data, size_t len);

bool thumb_jpeg_is_progressive(const uint8_t* data, size_t len);
