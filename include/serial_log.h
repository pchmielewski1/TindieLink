#pragma once
#include <Arduino.h>

/** Wywolaj jako pierwsza rzecz w setup() — przed M5.begin. */
void serial_log_init();

void serial_log(const char* msg);
