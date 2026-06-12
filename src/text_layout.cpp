#include "text_layout.h"
#include <stdio.h>
#include <string.h>

void truncate_to_width(const char* src, char* dst, size_t dst_len, size_t max_chars) {
    if (!dst || dst_len == 0) {
        return;
    }
    if (!src) {
        dst[0] = '\0';
        return;
    }
    size_t n = strlen(src);
    if (n > max_chars) {
        n = max_chars;
    }
    if (n >= dst_len) {
        n = dst_len - 1;
    }
    memcpy(dst, src, n);
    dst[n] = '\0';
}

void truncate_to_width_ellipsis(const char* src, char* dst, size_t dst_len, size_t max_chars) {
    if (!dst || dst_len == 0) {
        return;
    }
    if (!src || max_chars == 0) {
        dst[0] = '\0';
        return;
    }
    const size_t n = strlen(src);
    if (n <= max_chars) {
        truncate_to_width(src, dst, dst_len, max_chars);
        return;
    }
    if (max_chars < 4 || dst_len < 4) {
        truncate_to_width(src, dst, dst_len, max_chars);
        return;
    }
    const size_t keep = max_chars - 3;
    if (keep >= dst_len) {
        truncate_to_width(src, dst, dst_len, max_chars);
        return;
    }
    memcpy(dst, src, keep);
    dst[keep] = '.';
    dst[keep + 1] = '.';
    dst[keep + 2] = '.';
    dst[keep + 3] = '\0';
}

void truncate_ssid(const char* src, char* dst, size_t dst_len, size_t max_chars) {
    truncate_to_width_ellipsis(src, dst, dst_len, max_chars);
}

static const char* list_status_short(const char* status_label) {
    if (!status_label || !status_label[0]) {
        return "Unknown";
    }
    if (strcmp(status_label, "Awaiting Admin Approval") == 0) {
        return "Pending";
    }
    return status_label;
}

void format_list_meta(char* out, size_t out_len,
    const char* status_label, int stock, int sold, size_t max_cols) {
    if (!out || out_len == 0) {
        return;
    }
    const char* status = list_status_short(status_label);
    char line[64];
    snprintf(line, sizeof(line), "%s | Stock %d | Sold %d", status, stock, sold);
    truncate_to_width_ellipsis(line, out, out_len, max_cols);
}

void format_list_title(char* out, size_t out_len, const char* title, size_t max_chars) {
    truncate_to_width_ellipsis(title, out, out_len, max_chars);
}

static const char* skip_spaces(const char* p) {
    while (p && *p == ' ') {
        ++p;
    }
    return p;
}

void wrap_title_words(const char* title, char* line1, size_t l1_len,
    char* line2, size_t l2_len, size_t max_cols) {
    if (line1 && l1_len > 0) {
        line1[0] = '\0';
    }
    if (line2 && l2_len > 0) {
        line2[0] = '\0';
    }
    if (!title || !line1 || l1_len == 0 || max_cols < 4) {
        return;
    }

    const size_t len = strlen(title);
    if (len <= max_cols) {
        truncate_to_width(title, line1, l1_len, max_cols);
        return;
    }

    size_t break_at = max_cols;
    while (break_at > 0 && title[break_at] != ' ') {
        --break_at;
    }
    if (break_at == 0) {
        truncate_to_width_ellipsis(title, line1, l1_len, max_cols);
        return;
    }

    truncate_to_width(title, line1, l1_len, break_at);
    const char* rest = skip_spaces(title + break_at);
    if (!line2 || l2_len == 0 || !rest[0]) {
        return;
    }
    truncate_to_width_ellipsis(rest, line2, l2_len, max_cols);
}

static bool take_title_line(const char* title, char* out, size_t out_len, size_t max_cols,
    const char** rest_out) {
    if (rest_out) {
        *rest_out = "";
    }
    if (!title || !out || out_len == 0 || max_cols < 4) {
        return false;
    }
    const size_t len = strlen(title);
    if (len == 0) {
        out[0] = '\0';
        return false;
    }
    if (len <= max_cols) {
        truncate_to_width(title, out, out_len, max_cols);
        if (rest_out) {
            *rest_out = title + len;
        }
        return false;
    }

    size_t break_at = max_cols;
    while (break_at > 0 && title[break_at] != ' ') {
        --break_at;
    }
    if (break_at == 0) {
        truncate_to_width_ellipsis(title, out, out_len, max_cols);
        if (rest_out) {
            *rest_out = title + len;
        }
        return false;
    }

    truncate_to_width(title, out, out_len, break_at);
    if (rest_out) {
        *rest_out = skip_spaces(title + break_at);
    }
    return true;
}

void wrap_title_3lines(const char* title, char* line1, size_t l1_len,
    char* line2, size_t l2_len, char* line3, size_t l3_len, size_t max_cols) {
    if (line1 && l1_len > 0) {
        line1[0] = '\0';
    }
    if (line2 && l2_len > 0) {
        line2[0] = '\0';
    }
    if (line3 && l3_len > 0) {
        line3[0] = '\0';
    }
    if (!title || !line1 || l1_len == 0) {
        return;
    }

    const char* rest = nullptr;
    if (!take_title_line(title, line1, l1_len, max_cols, &rest) || !rest || !rest[0]) {
        return;
    }
    if (!line2 || l2_len == 0) {
        return;
    }
    if (!take_title_line(rest, line2, l2_len, max_cols, &rest) || !rest || !rest[0]) {
        return;
    }
    if (!line3 || l3_len == 0) {
        return;
    }
    truncate_to_width_ellipsis(rest, line3, l3_len, max_cols);
}
