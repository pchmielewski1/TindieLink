#pragma once
#include "product.h"
#include <stdbool.h>
#include <time.h>

struct ProductsCache {
    Product items[kMaxProducts];
    int count;
    time_t last_sync_epoch;
    bool valid;
};

void cache_init(ProductsCache* c);
void cache_update(ProductsCache* c, const Product* src, int n);
bool cache_data_changed(const ProductsCache* c, const Product* src, int n);
const Product* cache_get(const ProductsCache* c, int index);
void cache_format_sync_time(const ProductsCache* c, char* hhmm, size_t len);
