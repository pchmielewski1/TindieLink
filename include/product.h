#pragma once
#include <stdint.h>

static const int kMaxProducts = 64;
static const int kTitleLen = 96;
static const int kStatusLen = 24;
static const int kThumbUrlLen = 192;
static const int kPriceLen = 12;

// v2 JSON: id, title, num_in_stock, status, price, rating, thumbnail.url, reviews/images counts
// sold: enriched from GET /api/v1/order/

enum ProductStatus : uint8_t {
    ProductForSale,
    ProductSoldOut,
    ProductRetired,
    ProductDraft,
    ProductUnknown,
};

struct Product {
    uint32_t id;
    char title[kTitleLen];
    ProductStatus status;
    char status_label[kStatusLen];
    int16_t stock;
    int16_t sold;
    char price[kPriceLen];
    char regular_price[kPriceLen];
    float rating;
    bool on_sale;
    int16_t review_count;
    int16_t image_count;
    char thumbnail_url[kThumbUrlLen];
};
