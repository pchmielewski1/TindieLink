#pragma once
#include "app_state.h"
#include "products_cache.h"

void ui_init();
bool ui_apply_landscape_flip(bool upside_down);
bool ui_landscape_is_upside_down(void);
void ui_redraw(const AppContext* ctx, const ProductsCache* cache);
void ui_repaint_header(const AppContext* ctx, const ProductsCache* cache);
void ui_draw_error_config();
void ui_draw_wifi_connecting();
void ui_draw_error_wifi();
void ui_draw_error_api(int http_code);
void ui_draw_list(const ProductsCache* cache, const AppContext* ctx);
void ui_draw_detail(const ProductsCache* cache, const AppContext* ctx);
