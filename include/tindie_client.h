#pragma once
#include "product.h"
#include <stddef.h>

enum TindieFetchResult : uint8_t {
    FetchOk,
    FetchUnauthorized,
    FetchHttpError,
    FetchParseError,
    FetchNetworkError,
};

struct TindieFetchResponse {
    TindieFetchResult result;
    int http_code;
    int product_count;
};

TindieFetchResponse tindie_fetch_all_products(Product* out, int max_out);
void tindie_reset_store_cache();

/** Primary photo URL: images[0].sizes.medium (fit-in). List thumbnail only if API fails. */
bool tindie_resolve_primary_image_url(uint32_t product_id, const char* fallback_url, char* out,
    size_t out_len);
