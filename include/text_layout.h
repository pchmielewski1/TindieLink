#pragma once
#include <stddef.h>
#include <stdint.h>

void format_list_meta(char* out, size_t out_len,
    const char* status_label, int stock, int sold, size_t max_cols);
void format_list_title(char* out, size_t out_len, const char* title, size_t max_chars);
void wrap_title_words(const char* title, char* line1, size_t l1_len,
    char* line2, size_t l2_len, size_t max_cols);
void wrap_title_3lines(const char* title, char* line1, size_t l1_len,
    char* line2, size_t l2_len, char* line3, size_t l3_len, size_t max_cols);
void truncate_to_width(const char* src, char* dst, size_t dst_len, size_t max_chars);
void truncate_to_width_ellipsis(const char* src, char* dst, size_t dst_len, size_t max_chars);
void truncate_ssid(const char* src, char* dst, size_t dst_len, size_t max_chars);
