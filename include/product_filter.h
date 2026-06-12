#pragma once
#include "product.h"

/** Keep products whose status matches LIST_SHOW_* flags in config.h. */
int product_filter_list(Product* items, int count);

bool product_list_visible(const Product& p);
