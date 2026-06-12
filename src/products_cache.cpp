#include "products_cache.h"
#include <stdio.h>
#include <string.h>

void cache_init(ProductsCache* c) {
    memset(c, 0, sizeof(*c));
}

bool cache_data_changed(const ProductsCache* c, const Product* src, int n) {
    if (!c || !src) {
        return true;
    }
    if (n != c->count) {
        return true;
    }
    for (int i = 0; i < n; ++i) {
        if (memcmp(&c->items[i], &src[i], sizeof(Product)) != 0) {
            return true;
        }
    }
    return false;
}

void cache_update(ProductsCache* c, const Product* src, int n) {
    if (n > kMaxProducts) {
        n = kMaxProducts;
    }
    if (n > 0 && src) {
        memcpy(c->items, src, sizeof(Product) * n);
    }
    c->count = n;
    c->last_sync_epoch = time(nullptr);
    c->valid = n > 0;
}

const Product* cache_get(const ProductsCache* c, int index) {
    if (!c || index < 0 || index >= c->count) {
        return nullptr;
    }
    return &c->items[index];
}

void cache_format_sync_time(const ProductsCache* c, char* hhmm, size_t len) {
    if (!hhmm || len == 0) {
        return;
    }
    hhmm[0] = '\0';
    if (!c || c->last_sync_epoch <= 0) {
        snprintf(hhmm, len, "--:--");
        return;
    }
    struct tm tm_info;
    localtime_r(&c->last_sync_epoch, &tm_info);
    snprintf(hhmm, len, "%02d:%02d", tm_info.tm_hour, tm_info.tm_min);
}
