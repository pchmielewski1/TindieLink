#pragma once
#include "product.h"

int parse_products_json(const char* json, Product* out, int max_out);
void enrich_sold_from_orders_json(Product* out, int count, const char* orders_json);

#ifdef ARDUINO
#include <Stream.h>
int parse_products_stream(Stream& stream, Product* out, int max_out);
void enrich_sold_from_orders_stream(Product* out, int count, Stream& stream);
#endif
