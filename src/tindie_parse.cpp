#include <ArduinoJson.h>
#include "product.h"
#include "tindie_parse.h"
#include <stdlib.h>
#include <string.h>

static ProductStatus status_from_api(const char* api_status, int16_t stock) {
    if (api_status && strcmp(api_status, "retired") == 0) return ProductRetired;
    if (api_status && strcmp(api_status, "draft") == 0) return ProductDraft;
    if (stock <= 0) return ProductSoldOut;
    return ProductForSale;
}

static const char* label_from_status(ProductStatus st) {
    switch (st) {
        case ProductForSale: return "For Sale";
        case ProductSoldOut: return "Sold Out";
        case ProductRetired: return "Retired";
        case ProductDraft: return "Draft";
        default: return "Unknown";
    }
}

static void fill_product(Product& p, JsonObject obj) {
    p.id = obj["id"] | 0u;
    const char* title = obj["title"] | "";
    strncpy(p.title, title, kTitleLen - 1);
    p.title[kTitleLen - 1] = '\0';

    p.stock = obj["num_in_stock"] | obj["stock"] | 0;
    p.sold = obj["num_sold"] | obj["sold"] | 0;

    const char* api_status = obj["status"] | obj["state"] | "";
    p.status = status_from_api(api_status, p.stock);
    strncpy(p.status_label, label_from_status(p.status), kStatusLen - 1);
    p.status_label[kStatusLen - 1] = '\0';

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
