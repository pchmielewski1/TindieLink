#pragma once
#include <stdint.h>

void thumb_loader_init();
void thumb_loader_request(uint32_t product_id, const char* thumbnail_url);
void thumb_loader_tick();
bool thumb_loader_is_busy();
void thumb_loader_draw(int16_t x, int16_t y, int16_t w, int16_t h, uint32_t product_id);
