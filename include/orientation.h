#pragma once
#include <stdbool.h>

void orientation_init(void);
/** Returns true when display rotation was applied (caller should redraw). */
bool orientation_tick(bool defer_flip);
