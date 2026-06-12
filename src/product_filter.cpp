#include "product_filter.h"
#include "config.h"

#ifndef LIST_SHOW_FOR_SALE
#define LIST_SHOW_FOR_SALE 1
#endif
#ifndef LIST_SHOW_SOLD_OUT
#define LIST_SHOW_SOLD_OUT 1
#endif
#ifndef LIST_SHOW_DRAFT
#define LIST_SHOW_DRAFT 0
#endif
#ifndef LIST_SHOW_RETIRED
#define LIST_SHOW_RETIRED 0
#endif
#ifndef LIST_SHOW_UNKNOWN
#define LIST_SHOW_UNKNOWN 0
#endif

bool product_list_visible(const Product& p) {
    switch (p.status) {
        case ProductForSale:
            return LIST_SHOW_FOR_SALE != 0;
        case ProductSoldOut:
            return LIST_SHOW_SOLD_OUT != 0;
        case ProductDraft:
            return LIST_SHOW_DRAFT != 0;
        case ProductRetired:
            return LIST_SHOW_RETIRED != 0;
        case ProductAwaitingApproval:
            return false;
        default:
            return LIST_SHOW_UNKNOWN != 0;
    }
}

int product_filter_list(Product* items, int count) {
    if (!items || count <= 0) {
        return 0;
    }
    int w = 0;
    for (int i = 0; i < count; ++i) {
        if (!product_list_visible(items[i])) {
            continue;
        }
        if (w != i) {
            items[w] = items[i];
        }
        ++w;
    }
    return w;
}
