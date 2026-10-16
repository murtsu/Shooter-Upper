/* test_sense: drives the sensor with made-up clocks. */
#include "../src/sense.h"
#include <math.h>
#include <stdio.h>

static int fails;
#define CHECK(c, msg) do { if (!(c)) { printf("FAIL: %s\n", msg); fails++; } } while (0)

int main(void)
{
    /* 1. perfect 60 Hz clock */
    sense_reset();
    double t = 1000;
    for (int i = 0; i < 300; i++) { t += 1000.0 / 60; sense_frame(t); sense_sim_step(t); sense_update(t, true, false); }
    const SenseInfo *s = sense_info();
    CHECK(fabsf(s->fps - 60.f) < 0.5f, "fps near 60");
    CHECK(fabsf(s->sim - 60.f) < 0.5f, "sim near 60");
    CHECK(s->smooth, "perfect clock is smooth");
    CHECK(s->worst_ms < 17.f, "worst frame about 16.7 ms");
    CHECK(fabsf(s->adapt - 1.f) < 0.001f, "adaptive off keeps 1.0");

    /* 2. one slow frame is noticed */
    t += 80; sense_frame(t);
    CHECK(sense_info()->worst_ms > 79.f, "stutter shows in worst_ms");

    /* 3. a machine that only manages 40 steps a second is not smooth */
    sense_reset(); t = 0;
    for (int i = 0; i < 200; i++) { t += 25; sense_sim_step(t); }
    CHECK(!sense_info()->smooth, "40 Hz is not smooth");

    /* 4. busy player, no deaths: pressure rises, but never above the limit */
    sense_reset(); t = 0;
    for (int i = 0; i < 60 * 60; i++) {
        t += 1000.0 / 60;
        if (i % 5 == 0) sense_press(t);          /* 12 presses a second */
        sense_update(t, true, true);
    }
    CHECK(sense_info()->adapt > 1.05f, "busy player raises pressure");
    CHECK(sense_info()->adapt <= SENSE_ADAPT_MAX + 0.001f, "capped at max");
    CHECK(sense_info()->press_s > 11.f && sense_info()->press_s < 13.f, "press rate measured");

    /* 5. dying a lot gives relief */
    sense_reset(); t = 0;
    for (int i = 0; i < 60 * 60; i++) {
        t += 1000.0 / 60;
        if (i % 600 == 0) sense_death(t);        /* a death every 10 s */
        sense_update(t, true, true);
    }
    CHECK(sense_info()->adapt < 0.95f, "deaths lower pressure");
    CHECK(sense_info()->adapt >= SENSE_ADAPT_MIN - 0.001f, "floored at min");

    /* 6. when not playing, it drifts back to neutral */
    for (int i = 0; i < 60 * 60; i++) { t += 1000.0 / 60; sense_update(t, false, true); }
    CHECK(fabsf(sense_info()->adapt - 1.f) < 0.01f, "returns to 1.0 outside play");

    printf(fails ? "%d FAILED\n" : "all sense checks passed\n", fails);
    return fails ? 1 : 0;
}
