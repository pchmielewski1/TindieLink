#pragma once
#include "app_state.h"
#include "products_cache.h"

void app_init(AppContext* ctx, ProductsCache* cache);
bool app_bootstrap(AppContext* ctx, ProductsCache* cache);
void app_tick(AppContext* ctx, ProductsCache* cache);
