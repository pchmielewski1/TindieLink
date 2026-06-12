#pragma once
#include <stddef.h>
#include <stdint.h>

const char* status_to_abbrev(const char* status_label);
void format_list_line(char* out, size_t out_len, bool selected,
    const char* status_label, int stock, int sold, uint32_t id, const char* title);
void format_list_line_cols(char* out, size_t out_len, int max_cols, bool selected,
    const char* status_label, int stock, int sold, uint32_t id, const char* title);
void wrap_title_2lines(const char* title, char* line1, char* line2);
void truncate_to_width(const char* src, char* dst, size_t dst_len, size_t max_chars);
