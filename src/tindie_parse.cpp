#include <ArduinoJson.h>
#include "product.h"
#include "tindie_parse.h"
#include <stdlib.h>
#include <string.h>

static bool str_ieq(const char* a, const char* b) {
    if (!a || !b) {
        return false;
    }
    while (*a && *b) {
        char ca = *a;
        char cb = *b;
        if (ca >= 'A' && ca <= 'Z') {
            ca = (char)(ca - 'A' + 'a');
        }
        if (cb >= 'A' && cb <= 'Z') {
            cb = (char)(cb - 'A' + 'a');
        }
        if (ca != cb) {
            return false;
        }
        ++a;
        ++b;
    }
    return *a == '\0' && *b == '\0';
}

static bool str_has_icase(const char* haystack, const char* needle) {
    if (!haystack || !needle || !needle[0]) {
        return false;
    }
    const size_t nlen = strlen(needle);
    for (const char* p = haystack; *p; ++p) {
        size_t i = 0;
        while (i < nlen && p[i]) {
            char a = p[i];
            char b = needle[i];
            if (a >= 'A' && a <= 'Z') {
                a = (char)(a - 'A' + 'a');
            }
            if (b >= 'A' && b <= 'Z') {
                b = (char)(b - 'A' + 'a');
            }
            if (a != b) {
                break;
            }
            ++i;
        }
        if (i == nlen) {
            return true;
        }
    }
    return false;
}

static bool is_awaiting_status(const char* api_status, const char* api_state) {
    if (str_ieq(api_status, "awaiting_admin_approval") ||
        str_ieq(api_status, "pending_approval") ||
        str_ieq(api_status, "awaiting_approval") ||
        str_ieq(api_state, "awaiting_admin_approval") ||
        str_ieq(api_state, "pending_approval") ||
        str_ieq(api_state, "awaiting_approval")) {
        return true;
    }
    if (str_has_icase(api_status, "awaiting") || str_has_icase(api_state, "awaiting")) {
        return true;
    }
    if (str_has_icase(api_status, "pending approval") ||
        str_has_icase(api_state, "pending approval")) {
        return true;
    }
    if (str_has_icase(api_status, "approval") || str_has_icase(api_state, "approval")) {
        if (!str_has_icase(api_status, "for sale") && !str_has_icase(api_state, "for sale")) {
            return true;
        }
    }
    return false;
}

static ProductStatus status_from_api(const char* api_status, const char* api_state, int16_t stock) {
    if (api_status && str_ieq(api_status, "retired")) {
        return ProductRetired;
    }
    if (api_state && str_ieq(api_state, "retired")) {
        return ProductRetired;
    }
    if (api_status && (str_ieq(api_status, "resubmitted") || str_ieq(api_status, "submitted"))) {
        return ProductAwaitingApproval;
    }
    if (is_awaiting_status(api_status, api_state)) {
        return ProductAwaitingApproval;
    }
    if (api_status && str_ieq(api_status, "draft")) {
        return ProductDraft;
    }
    if (api_state && str_ieq(api_state, "draft")) {
        return ProductDraft;
    }
    if (stock <= 0) {
        return ProductSoldOut;
    }
    return ProductForSale;
}

static const char* label_from_status(ProductStatus st) {
    switch (st) {
        case ProductForSale: return "For Sale";
        case ProductSoldOut: return "Sold Out";
        case ProductRetired: return "Retired";
        case ProductDraft: return "Draft";
        case ProductAwaitingApproval: return "Awaiting Admin Approval";
        default: return "Unknown";
    }
}

static void set_status_label(Product& p, const char* api_status, const char* api_state) {
    if (p.status == ProductAwaitingApproval &&
        api_status && str_ieq(api_status, "resubmitted")) {
        strncpy(p.status_label, "Awaiting Admin Approval", kStatusLen - 1);
        p.status_label[kStatusLen - 1] = '\0';
        return;
    }

    const char* pick = nullptr;
    if (p.status == ProductAwaitingApproval) {
        if (api_state && str_has_icase(api_state, "await")) {
            pick = api_state;
        } else if (api_status && str_has_icase(api_status, "await")) {
            pick = api_status;
        }
    } else if (api_state && api_state[0] &&
        !str_ieq(api_state, "for_sale") && !str_ieq(api_state, "draft") &&
        !str_ieq(api_state, "retired")) {
        pick = api_state;
    }

    if (pick && pick[0]) {
        strncpy(p.status_label, pick, kStatusLen - 1);
        p.status_label[kStatusLen - 1] = '\0';
        return;
    }

    const char* fallback = label_from_status(p.status);
    strncpy(p.status_label, fallback, kStatusLen - 1);
    p.status_label[kStatusLen - 1] = '\0';
}

static void fill_product(Product& p, JsonObject obj) {
    p.id = obj["id"] | 0u;
    const char* title = obj["title"] | "";
    strncpy(p.title, title, kTitleLen - 1);
    p.title[kTitleLen - 1] = '\0';

    p.stock = obj["num_in_stock"] | obj["stock"] | 0;
    p.sold = obj["num_sold"] | obj["sold"] | 0;

    const char* api_status = obj["status"] | "";
    const char* api_state = obj["state"] | "";
    p.status = status_from_api(api_status, api_state, p.stock);
    set_status_label(p, api_status, api_state);

    const char* price = obj["price"] | "";
    strncpy(p.price, price, kPriceLen - 1);
    p.price[kPriceLen - 1] = '\0';
    const char* regular = obj["regular_price"] | "";
    strncpy(p.regular_price, regular, kPriceLen - 1);
    p.regular_price[kPriceLen - 1] = '\0';

    const char* rating_str = obj["rating"] | "0";
    p.rating = (float)atof(rating_str);
    p.on_sale = obj["on_sale"] | false;

    JsonObject reviews = obj["reviews"].as<JsonObject>();
    if (!reviews.isNull()) {
        p.review_count = reviews["total_count"] | 0;
    }
    JsonObject images = obj["images"].as<JsonObject>();
    if (!images.isNull()) {
        p.image_count = images["total_count"] | 0;
    }

    JsonObject thumb = obj["thumbnail"].as<JsonObject>();
    if (!thumb.isNull()) {
        const char* url = thumb["url"] | "";
        strncpy(p.thumbnail_url, url, kThumbUrlLen - 1);
        p.thumbnail_url[kThumbUrlLen - 1] = '\0';
    } else {
        p.thumbnail_url[0] = '\0';
    }
}

static int parse_products_array(JsonArray arr, Product* out, int max_out) {
    if (arr.isNull()) {
        return -1;
    }
    int i = 0;
    for (JsonObject obj : arr) {
        if (i >= max_out) {
            break;
        }
        Product p = {};
        fill_product(p, obj);
        out[i++] = p;
    }
    return i;
}

static void apply_sold_from_orders(JsonArray orders, Product* out, int count) {
    if (orders.isNull()) {
        return;
    }
    for (JsonObject order : orders) {
        JsonArray items = order["items"].as<JsonArray>();
        if (items.isNull()) {
            continue;
        }
        for (JsonObject item : items) {
            const char* sku = item["sku"] | "";
            if (!sku[0]) {
                continue;
            }
            const uint32_t product_id = (uint32_t)strtoul(sku, nullptr, 10);
            if (product_id == 0) {
                continue;
            }
            const int16_t qty = item["quantity"] | 0;
            for (int i = 0; i < count; ++i) {
                if (out[i].id == product_id) {
                    out[i].sold = (int16_t)(out[i].sold + qty);
                    break;
                }
            }
        }
    }
}

int parse_products_json(const char* json, Product* out, int max_out) {
    if (!json || !out || max_out <= 0) {
        return -1;
    }

    JsonDocument filter;
    filter["products"][0]["id"] = true;
    filter["products"][0]["title"] = true;
    filter["products"][0]["num_in_stock"] = true;
    filter["products"][0]["status"] = true;
    filter["products"][0]["state"] = true;
    filter["products"][0]["price"] = true;
    filter["products"][0]["regular_price"] = true;
    filter["products"][0]["rating"] = true;
    filter["products"][0]["on_sale"] = true;
    filter["products"][0]["thumbnail"]["url"] = true;
    filter["products"][0]["reviews"]["total_count"] = true;
    filter["products"][0]["images"]["total_count"] = true;
    filter["objects"][0]["id"] = true;
    filter["objects"][0]["title"] = true;
    filter["objects"][0]["num_in_stock"] = true;
    filter["objects"][0]["status"] = true;
    filter["objects"][0]["state"] = true;
    filter["objects"][0]["price"] = true;
    filter["objects"][0]["regular_price"] = true;
    filter["objects"][0]["rating"] = true;
    filter["objects"][0]["on_sale"] = true;
    filter["objects"][0]["thumbnail"]["url"] = true;
    filter["objects"][0]["reviews"]["total_count"] = true;
    filter["objects"][0]["images"]["total_count"] = true;

    JsonDocument doc;
    DeserializationError err =
        deserializeJson(doc, json, DeserializationOption::Filter(filter));
    if (err) {
        doc.clear();
        err = deserializeJson(doc, json);
        if (err) {
            return -1;
        }
    }

    JsonArray arr = doc["products"].as<JsonArray>();
    if (arr.isNull()) {
        arr = doc["objects"].as<JsonArray>();
    }
    return parse_products_array(arr, out, max_out);
}

void enrich_sold_from_orders_json(Product* out, int count, const char* orders_json) {
    if (!out || count <= 0 || !orders_json) {
        return;
    }

    for (int i = 0; i < count; ++i) {
        out[i].sold = 0;
    }

    JsonDocument filter;
    filter["orders"][0]["items"][0]["sku"] = true;
    filter["orders"][0]["items"][0]["quantity"] = true;
    filter["objects"][0]["items"][0]["sku"] = true;
    filter["objects"][0]["items"][0]["quantity"] = true;

    JsonDocument doc;
    DeserializationError err =
        deserializeJson(doc, orders_json, DeserializationOption::Filter(filter));
    if (err) {
        doc.clear();
        if (deserializeJson(doc, orders_json)) {
            return;
        }
    }

    JsonArray orders = doc["orders"].as<JsonArray>();
    if (orders.isNull()) {
        orders = doc["objects"].as<JsonArray>();
    }
    apply_sold_from_orders(orders, out, count);
}

#ifdef ARDUINO
int parse_products_stream(Stream& stream, Product* out, int max_out) {
    if (!out || max_out <= 0) {
        return -1;
    }

    JsonDocument filter;
    filter["products"][0]["id"] = true;
    filter["products"][0]["title"] = true;
    filter["products"][0]["num_in_stock"] = true;
    filter["products"][0]["status"] = true;
    filter["products"][0]["state"] = true;

    JsonDocument doc;
    if (deserializeJson(doc, stream, DeserializationOption::Filter(filter))) {
        return -1;
    }

    JsonArray arr = doc["products"].as<JsonArray>();
    if (arr.isNull()) {
        arr = doc["objects"].as<JsonArray>();
    }
    return parse_products_array(arr, out, max_out);
}

void enrich_sold_from_orders_stream(Product* out, int count, Stream& stream) {
    if (!out || count <= 0) {
        return;
    }

    for (int i = 0; i < count; ++i) {
        out[i].sold = 0;
    }

    JsonDocument filter;
    filter["orders"][0]["items"][0]["sku"] = true;
    filter["orders"][0]["items"][0]["quantity"] = true;

    JsonDocument doc;
    if (deserializeJson(doc, stream, DeserializationOption::Filter(filter))) {
        return;
    }

    JsonArray orders = doc["orders"].as<JsonArray>();
    if (orders.isNull()) {
        orders = doc["objects"].as<JsonArray>();
    }
    apply_sold_from_orders(orders, out, count);
}
#endif
