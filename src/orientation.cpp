#include "orientation.h"
#include "config.h"
#include "ui.h"
#include "debug_agent_log.h"
#include <M5Unified.h>
#include <Arduino.h>
#include <math.h>

#ifndef ORIENTATION_AUTO_FLIP
#define ORIENTATION_AUTO_FLIP 1
#endif
#ifndef ORIENTATION_ACCEL_AXIS
#define ORIENTATION_ACCEL_AXIS 1
#endif
#ifndef ORIENTATION_INVERT
#define ORIENTATION_INVERT 0
#endif
#ifndef ORIENTATION_DEBUG
#define ORIENTATION_DEBUG 0
#endif

static const unsigned long kSampleMs = 150;
static const unsigned long kStableMs = 300;
#if ORIENTATION_DEBUG
static const unsigned long kDebugLogMs = 800;
#endif
static const float kThresholdG = 0.25f;

static bool g_active = false;
static unsigned long g_last_sample_ms = 0;
#if ORIENTATION_DEBUG
static unsigned long g_last_debug_ms = 0;
#endif
static bool g_pending_upside_down = false;
static bool g_pending_valid = false;
static unsigned long g_pending_since_ms = 0;

#if ORIENTATION_DEBUG
static int32_t g_to_mg(float g) {
    return (int32_t)(g * 1000.0f);
}
#endif

/** Vertical wall mount: gravity mostly |Z|; flip 180° in plane changes Y sign. */
static float orient_pick_axis(float ax, float ay, float az, int* axis_idx) {
#if ORIENTATION_ACCEL_AXIS >= 0 && ORIENTATION_ACCEL_AXIS <= 2
    if (ORIENTATION_ACCEL_AXIS != 1) {
        float v = ay;
        *axis_idx = ORIENTATION_ACCEL_AXIS;
        if (ORIENTATION_ACCEL_AXIS == 0) {
            v = ax;
        } else if (ORIENTATION_ACCEL_AXIS == 2) {
            v = az;
        }
#if ORIENTATION_INVERT
        v = -v;
#endif
        return v;
    }
#endif
    (void)ax;
    if (fabsf(az) > 0.70f) {
        *axis_idx = 1;
        float v = ay;
#if ORIENTATION_INVERT
        v = -v;
#endif
        return v;
    }
    const float axa = fabsf(ax);
    const float aya = fabsf(ay);
    if (axa >= aya) {
        *axis_idx = 0;
#if ORIENTATION_INVERT
        return -ax;
#else
        return ax;
#endif
    }
    *axis_idx = 1;
#if ORIENTATION_INVERT
    return -ay;
#else
    return ay;
#endif
}

static bool orient_desired_upside_down(float axis_g, bool current_upside_down) {
    if (axis_g > kThresholdG) {
        return false;
    }
    if (axis_g < -kThresholdG) {
        return true;
    }
    return current_upside_down;
}

void orientation_init(void) {
#if ORIENTATION_AUTO_FLIP == 0
    g_active = false;
    Serial.println("[orient] INIT disabled (ORIENTATION_AUTO_FLIP=0)");
    return;
#endif

    if (M5.Imu.getType() == m5::imu_none) {
        g_active = false;
        Serial.println("[orient] INIT skip (no IMU)");
        return;
    }

    g_active = true;
    g_last_sample_ms = 0;
#if ORIENTATION_DEBUG
    g_last_debug_ms = 0;
#endif
    g_pending_valid = false;
    Serial.printf("[orient] INIT auto_flip=1 axis_cfg=%d invert=%d debug=%d\n",
        ORIENTATION_ACCEL_AXIS, ORIENTATION_INVERT, ORIENTATION_DEBUG);
}

bool orientation_tick(bool defer_flip) {
    if (!g_active) {
        return false;
    }

    const unsigned long now = millis();
    if (now - g_last_sample_ms < kSampleMs) {
        return false;
    }
    g_last_sample_ms = now;

    float ax = 0.0f;
    float ay = 0.0f;
    float az = 0.0f;
    if (!M5.Imu.getAccel(&ax, &ay, &az)) {
        return false;
    }

    int axis_idx = 1;
    const float axis_g = orient_pick_axis(ax, ay, az, &axis_idx);
    const bool current = ui_landscape_is_upside_down();
    const bool desired = orient_desired_upside_down(axis_g, current);

#if ORIENTATION_DEBUG
    if (now - g_last_debug_ms >= kDebugLogMs) {
        g_last_debug_ms = now;
        debug_agent_log("H4", "orientation.cpp:sample", "accel_mg",
            g_to_mg(ax), g_to_mg(ay), g_to_mg(az));
        debug_agent_log("H4", "orientation.cpp:sample", "axis_state",
            axis_idx, g_to_mg(axis_g),
            (int32_t)(desired ? 1 : 0) | ((int32_t)(current ? 1 : 0) << 1));
    }
#endif

    if (desired == current) {
        g_pending_valid = false;
        return false;
    }

    if (g_pending_valid && g_pending_upside_down == desired) {
        if (now - g_pending_since_ms < kStableMs) {
            return false;
        }
        if (defer_flip) {
            return false;
        }
        if (!ui_apply_landscape_flip(desired)) {
            return false;
        }
#if ORIENTATION_DEBUG
        Serial.printf("[orient] flip %s axis=%d g=%.2f rot=%d\n",
            desired ? "inverted" : "normal", axis_idx, axis_g, desired ? 3 : 1);
        debug_agent_log("H5", "orientation.cpp:flip", "applied",
            axis_idx, g_to_mg(axis_g), desired ? 3 : 1);
#else
        (void)axis_idx;
        (void)axis_g;
#endif
        g_pending_valid = false;
        return true;
    }

#if ORIENTATION_DEBUG
    if (!g_pending_valid || g_pending_upside_down != desired) {
        debug_agent_log("H6", "orientation.cpp:pending", "flip_pending",
            axis_idx, g_to_mg(axis_g), desired ? 1 : 0);
    }
#endif

    g_pending_upside_down = desired;
    g_pending_valid = true;
    g_pending_since_ms = now;
    return false;
}
