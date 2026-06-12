#include "serial_log.h"

void serial_log_init() {
    Serial.begin(115200);
    delay(400);
    Serial.println();
    Serial.println("[TindieLink] UART 115200 OK");
    Serial.flush();
}

void serial_log(const char* msg) {
    Serial.println(msg);
    Serial.flush();
}
