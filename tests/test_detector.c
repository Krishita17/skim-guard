/*
 * SkimGuard — host-side unit tests for the detection pipeline.
 *
 * Compiles the firmware detection logic and the SIM field sensor on the host
 * (no Flipper, no ufbt) so CI can prove the algorithm behaves. Build & run:
 *
 *   cc -DSKIMGUARD_SIM -Isrc -o /tmp/sg_test tests/test_detector.c -lm && /tmp/sg_test
 *
 * Author / sole contributor: Krishita Sanjay Choksi.
 * SPDX-License-Identifier: MIT
 */
#include <stdio.h>
#include <math.h>

#include "detection.c"
#include "field_sensor.c"

static int g_failures = 0;
static int g_checks = 0;

#define CHECK(cond, msg)                                    \
    do {                                                    \
        g_checks++;                                         \
        if(!(cond)) {                                       \
            g_failures++;                                   \
            printf("  FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); \
        }                                                   \
    } while(0)

static void feed(Detector* d, float value, int n, DetectionResult* last) {
    DetectionResult r;
    for(int i = 0; i < n; i++) detector_push(d, value, &r);
    if(last) *last = r;
}

static void test_calibration(void) {
    printf("test_calibration\n");
    Detector d;
    detector_init(&d);
    CHECK(!d.calibrated, "starts uncalibrated");
    DetectionResult r;
    feed(&d, 18.0f, SG_CALIB_SAMPLES, &r);
    CHECK(d.calibrated, "calibrated after CALIB_SAMPLES");
    CHECK(fabsf(d.baseline - 18.0f) < 1.0f, "baseline learned near quiet floor");
    CHECK(d.noise >= SG_NOISE_MIN, "noise clamped to at least NOISE_MIN");
}

static void test_absent_stays_clear(void) {
    printf("test_absent_stays_clear\n");
    Detector d;
    detector_init(&d);
    detector_force_calibration(&d, 18.0f, 6.0f);
    DetectionResult r;
    bool ever_present = false;
    for(int i = 0; i < 200; i++) {
        /* jitter around the floor, never a real signal */
        float v = 18.0f + ((i % 5) - 2) * 2.0f;
        detector_push(&d, v, &r);
        if(r.present) ever_present = true;
    }
    CHECK(!ever_present, "benign floor jitter never latches present");
}

static void test_present_and_proximity(void) {
    printf("test_present_and_proximity\n");
    Detector d;
    detector_init(&d);
    detector_force_calibration(&d, 18.0f, 6.0f);
    DetectionResult r;
    feed(&d, 700.0f, 30, &r);
    CHECK(r.present, "strong field latches present");
    CHECK(r.proximity_pct > 50.0f, "strong field gives high proximity");
    CHECK(isfinite(r.click_ms), "click cadence finite when present");
    CHECK(r.click_ms >= SG_CLICK_MS_NEAR - 1.0f, "click cadence within bounds");
}

static void test_hysteresis(void) {
    printf("test_hysteresis\n");
    Detector d;
    detector_init(&d);
    detector_force_calibration(&d, 18.0f, 6.0f);
    DetectionResult r;
    feed(&d, 400.0f, 30, &r);
    CHECK(r.present, "present after strong signal");
    /* A value whose signal sits between OFF and ON thresholds must NOT drop
     * the latch (that is the point of hysteresis). off=3.5*6=21, on=max(36,25)=36.
     * Pick a signal ~30 counts above baseline. */
    feed(&d, 48.0f, 40, &r);
    CHECK(r.present, "stays present in the hysteresis band");
    feed(&d, 18.0f, 40, &r);
    CHECK(!r.present, "drops present once signal collapses");
}

static void test_characterization(void) {
    printf("test_characterization\n");
    Detector d;
    detector_init(&d);
    detector_force_calibration(&d, 18.0f, 6.0f);
    DetectionResult r;
    feed(&d, 700.0f, SG_WINDOW_SAMPLES, &r);
    CHECK(r.characterization == SgCharSteady, "constant field -> steady");
}

static void test_sim_sensor(void) {
    printf("test_sim_sensor\n");
    CHECK(field_sensor_is_simulated(), "SIM build reports simulated");
    field_sensor_init();
    uint16_t lo = 2000, hi = 0;
    for(int i = 0; i < 300; i++) {
        uint16_t v = field_sensor_read();
        CHECK(v <= (uint16_t)SG_FULL_SCALE, "reading within full scale");
        if(v < lo) lo = v;
        if(v > hi) hi = v;
    }
    CHECK(hi > lo + 100, "sim sensor sweeps over a meaningful range");
    field_sensor_deinit();
}

int main(void) {
    printf("== SkimGuard detector tests ==\n");
    test_calibration();
    test_absent_stays_clear();
    test_present_and_proximity();
    test_hysteresis();
    test_characterization();
    test_sim_sensor();
    printf("\n%d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
