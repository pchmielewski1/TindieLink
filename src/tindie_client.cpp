#include "tindie_client.h"
#include "tindie_parse.h"
#include "config.h"
#include <ArduinoJson.h>
#include <esp_heap_caps.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

static const char* kHost = "https://www.tindie.com";
static const char* kUserAgent = "M5StickC-tindie/1.0";
static const int kHttpTimeoutMs = 20000;
static const int kHttpRetries = 3;

static String g_store_id;
static bool g_store_id_resolved = false;

void tindie_reset_store_cache() {
    g_store_id = "";
    g_store_id_resolved = false;
}

static void build_auth_header(char* out, size_t out_len) {
    snprintf(out, out_len, "ApiKey %s:%s", TINDIE_USERNAME, TINDIE_API_KEY);
}

static bool http_open_stream(const String& url, WiFiClientSecure& client, HTTPClient& http, int* http_code) {
    char auth[160];
    build_auth_header(auth, sizeof(auth));

    for (int attempt = 0; attempt < kHttpRetries; ++attempt) {
        client.setInsecure();
        client.setTimeout(kHttpTimeoutMs / 1000);
        client.stop();

        http.setReuse(false);
        http.setTimeout(kHttpTimeoutMs);
        if (!http.begin(client, url)) {
            delay(200);
            continue;
        }

        http.setUserAgent(kUserAgent);
        http.addHeader("Authorization", auth);
        *http_code = http.GET();
        if (*http_code == 200) {
            return true;
        }

        http.end();
        client.stop();

        if (*http_code == 401) {
            return false;
        }
        delay(300 * (attempt + 1));
    }

    *http_code = 0;
    return false;
}

static void http_close_stream(WiFiClientSecure& client, HTTPClient& http) {
    http.end();
    client.stop();
}

static bool http_get_body(const String& url, int* http_code, String* body) {
    WiFiClientSecure client;
    HTTPClient http;
    if (!http_open_stream(url, client, http, http_code)) {
        return *http_code != 0;
    }

    *body = http.getString();
    http_close_stream(client, http);
    return *http_code == 200;
}

static int fetch_products_body(const String& url, int* http_code, Product* out, int max_out) {
    String body;
    if (!http_get_body(url, http_code, &body)) {
        return -1;
    }
    const int n = parse_products_json(body.c_str(), out, max_out);
    body = "";
    return n;
}

static bool resolve_store_id(int* http_code) {
    if (g_store_id_resolved && g_store_id.length() > 0) {
        return true;
    }

    String url = String(kHost) + "/api/v1/product/?format=json&store_username=" +
                 TINDIE_USERNAME + "&limit=1";
    String body;
    if (!http_get_body(url, http_code, &body)) {
        return false;
    }

    int id_start = body.indexOf("\"id\":");
    if (id_start < 0) {
        return false;
    }
    id_start += 5;
    while (id_start < (int)body.length() && (body[id_start] == ' ' || body[id_start] == ':')) {
        id_start++;
    }
    int id_end = id_start;
    while (id_end < (int)body.length() && body[id_end] >= '0' && body[id_end] <= '9') {
        id_end++;
    }
    if (id_end <= id_start) {
        return false;
    }

    const String product_id = body.substring(id_start, id_end);
    body = "";

    url = String(kHost) + "/api/v2/products/" + product_id + "/?format=json";
    if (!http_get_body(url, http_code, &body)) {
        return false;
    }

    const int store_pos = body.indexOf("\"store\"");
    if (store_pos < 0) {
        return false;
    }
    int id_pos = body.indexOf("\"id\":", store_pos);
    if (id_pos < 0) {
        return false;
    }
    id_pos += 5;
    while (id_pos < (int)body.length() &&
           (body[id_pos] == ' ' || body[id_pos] == ':' || body[id_pos] == '"')) {
        id_pos++;
    }
    id_end = id_pos;
    while (id_end < (int)body.length() && body[id_end] >= '0' && body[id_end] <= '9') {
        id_end++;
    }
    if (id_end <= id_pos) {
        return false;
    }

    g_store_id = body.substring(id_pos, id_end);
    g_store_id_resolved = g_store_id.length() > 0;
    return g_store_id_resolved;
}

static void enrich_sold_counts(Product* out, int count) {
    int code = 0;
    String body;
    const String url = String(kHost) + "/api/v1/order/?format=json&limit=100";
    if (!http_get_body(url, &code, &body)) {
        return;
    }
    enrich_sold_from_orders_json(out, count, body.c_str());
    body = "";
}

bool tindie_resolve_primary_image_url(uint32_t product_id, const char* fallback_url, char* out,
    size_t out_len) {
    if (!out || out_len < 32 || product_id == 0) {
        return false;
    }
    out[0] = '\0';

    if (WiFi.status() != WL_CONNECTED) {
        if (fallback_url && fallback_url[0]) {
            strncpy(out, fallback_url, out_len - 1);
            out[out_len - 1] = '\0';
            Serial.printf("[thumb] IMAGE_URL product_id=%lu source=thumbnail_smart (offline)\n",
                (unsigned long)product_id);
            return true;
        }
        return false;
    }

    int code = 0;
    const String url =
        String(kHost) + "/api/v2/products/" + String(product_id) + "/images/?format=json";
    String body;
    if (!http_get_body(url, &code, &body) || code != 200) {
        body = "";
        if (fallback_url && fallback_url[0]) {
            strncpy(out, fallback_url, out_len - 1);
            out[out_len - 1] = '\0';
            Serial.printf(
                "[thumb] IMAGE_URL product_id=%lu source=thumbnail_smart (api http=%d)\n",
                (unsigned long)product_id, code);
            return true;
        }
        return false;
    }

    JsonDocument filter;
    filter["images"][0]["sizes"]["medium"]["url"] = true;
    filter["images"][0]["url"] = true;

    JsonDocument doc;
    const DeserializationError err =
        deserializeJson(doc, body.c_str(), DeserializationOption::Filter(filter));
    body = "";
    if (err) {
        if (fallback_url && fallback_url[0]) {
            strncpy(out, fallback_url, out_len - 1);
            out[out_len - 1] = '\0';
            Serial.printf(
                "[thumb] IMAGE_URL product_id=%lu source=thumbnail_smart (parse)\n",
                (unsigned long)product_id);
            return true;
        }
        return false;
    }

    JsonArray images = doc["images"].as<JsonArray>();
    if (images.isNull() || images.size() == 0) {
        if (fallback_url && fallback_url[0]) {
            strncpy(out, fallback_url, out_len - 1);
            out[out_len - 1] = '\0';
            Serial.printf(
                "[thumb] IMAGE_URL product_id=%lu source=thumbnail_smart (no images)\n",
                (unsigned long)product_id);
            return true;
        }
        return false;
    }

    JsonObject first = images[0].as<JsonObject>();
    const char* pick = first["sizes"]["medium"]["url"] | "";
    const char* source = "medium_fitin";
    if (!pick[0]) {
        pick = first["url"] | "";
        source = "image_url";
    }
    if (!pick[0]) {
        if (fallback_url && fallback_url[0]) {
            strncpy(out, fallback_url, out_len - 1);
            out[out_len - 1] = '\0';
            Serial.printf(
                "[thumb] IMAGE_URL product_id=%lu source=thumbnail_smart (empty)\n",
                (unsigned long)product_id);
            return true;
        }
        return false;
    }

    strncpy(out, pick, out_len - 1);
    out[out_len - 1] = '\0';
    Serial.printf("[thumb] IMAGE_URL product_id=%lu source=%s len=%u\n",
        (unsigned long)product_id, source, (unsigned)strlen(out));
    return true;
}

TindieFetchResponse tindie_fetch_all_products(Product* out, int max_out) {
    TindieFetchResponse resp = {};
    if (WiFi.status() != WL_CONNECTED) {
        resp.result = FetchNetworkError;
        resp.http_code = 0;
        return resp;
    }

    int code = 0;
    if (!resolve_store_id(&code)) {
        resp.http_code = code;
        if (code == 401) {
            resp.result = FetchUnauthorized;
        } else if (code == 0) {
            resp.result = FetchNetworkError;
        } else {
            resp.result = FetchHttpError;
        }
        return resp;
    }

    const String url = String(kHost) + "/api/v2/products/?format=json&limit=50&store=" + g_store_id;
    const int n = fetch_products_body(url, &code, out, max_out);
    resp.http_code = code;

    if (code == 401) {
        resp.result = FetchUnauthorized;
        return resp;
    }
    if (code == 0) {
        resp.result = FetchNetworkError;
        return resp;
    }
    if (code != 200) {
        resp.result = FetchHttpError;
        return resp;
    }

    if (n < 0) {
        resp.result = FetchParseError;
        return resp;
    }

    enrich_sold_counts(out, n);

    resp.product_count = n;
    resp.result = FetchOk;
    return resp;
}
