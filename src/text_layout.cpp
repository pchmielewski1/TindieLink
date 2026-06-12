#include "text_layout.h"
#include <stdio.h>
#include <string.h>

const char* status_to_abbrev(const char* s) {
    if (!s) return "???";
    if (strcmp(s, "Sold Out") == 0) return "OUT";
    if (strcmp(s, "For Sale") == 0) return "SAL";
    if (strcmp(s, "Retired") == 0) return "RET";
    if (strcmp(s, "Draft") == 0) return "DRF";
    return "???";
}

static void pad_to_cols(char* out, size_t out_len, int max_cols) {
    if (max_cols <= 0) {
        return;
    }
    size_t n = strlen(out);
    size_t target = (size_t)max_cols;
    if (target >= out_len) {
        target = out_len - 1;
    }
    while (n < target) {
        out[n++] = ' ';
    }
    out[n] = '\0';
}

void format_list_line_cols(char* out, size_t out_len, int max_cols, bool selected,
    const char* status_label, int stock, int sold, uint32_t id, const char* title) {
    if (max_cols < 24) {
        max_cols = 24;
    }
    const int title_max = max_cols - 22;
    char title_part[28];
    truncate_to_width(title, title_part, sizeof(title_part),
        title_max > 0 ? (size_t)title_max : 1);
    snprintf(out, out_len, "%c%s %d/%d #%05lu %s",
        selected ? '>' : ' ',
        status_to_abbrev(status_label), sold, stock,
        (unsigned long)id, title_part);
    pad_to_cols(out, out_len, max_cols);
}

void format_list_line(char* out, size_t out_len, bool selected,
    const char* status_label, int stock, int sold, uint32_t id, const char* title) {
    format_list_line_cols(out, out_len, 40, selected, status_label, stock, sold, id, title);
}

void truncate_to_width(const char* src, char* dst, size_t dst_len, size_t max_chars) {
    if (!src || dst_len == 0) {
        if (dst_len > 0) dst[0] = '\0';
        return;
    }
    size_t n = strlen(src);
    if (n > max_chars) n = max_chars;
    if (n >= dst_len) n = dst_len - 1;
    memcpy(dst, src, n);
    dst[n] = '\0';
}

void wrap_title_2lines(const char* title, char* line1, char* line2) {
    truncate_to_width(title, line1, 41, 40);
    if (!title || strlen(title) <= 40) {
        line2[0] = '\0';
        return;
    }
    truncate_to_width(title + 40, line2, 41, 40);
}
