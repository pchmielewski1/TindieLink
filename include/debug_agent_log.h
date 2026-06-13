#pragma once
#include "config.h"
#include <Arduino.h>
#include <stdint.h>
#include <stdio.h>

#ifndef ORIENTATION_DEBUG
#define ORIENTATION_DEBUG 0
#endif

#if ORIENTATION_DEBUG

// #region agent log
/** NDJSON on Serial (prefix __DBG__). Capture: pio device monitor | python3 tools/debug_serial_ingest.py */
static inline void debug_agent_log(const char* hypothesisId, const char* location,
    const char* message, int32_t a, int32_t b, int32_t c) {
    Serial.printf(
        "__DBG__{\"hypothesisId\":\"%s\",\"location\":\"%s\","
        "\"message\":\"%s\",\"data\":{\"a\":%ld,\"b\":%ld,\"c\":%ld},\"timestamp\":%lu}\n",
        hypothesisId, location, message, (long)a, (long)b, (long)c,
        (unsigned long)millis());
}
// #endregion

#else

static inline void debug_agent_log(const char*, const char*, const char*, int32_t, int32_t,
    int32_t) {}

#endif
