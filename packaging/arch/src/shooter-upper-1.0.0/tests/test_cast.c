/* test_cast: the parade always finishes, stays on screen, and looks erratic. */
#include "../src/cast.h"
#include <math.h>
#include <stdio.h>

static int fails;
#define CHECK(c, msg) do { if (!(c)) { printf("FAIL: %s\n", msg); fails++; } } while (0)

int main(void)
{
    for (unsigned seed = 1; seed <= 25; seed++) {
        cast_start(seed * 7919u);
        int frames = 0;
        while (!cast_done() && frames < 60 * 60) { cast_step(); frames++; }
        char m[96];
        snprintf(m, sizeof m, "seed %u finishes within 60 s (took %d)", seed, frames);
        CHECK(cast_done(), m);
        CHECK(cast_locked() == cast_count(), "everyone landed");
        if (frames < 60 * 8 || frames > 60 * 30) { snprintf(m, sizeof m, "seed %u length %d frames outside 8..30 s", seed, frames); CHECK(0, m); }
    }
    printf(fails ? "%d FAILED\n" : "all cast checks passed\n", fails);
    return fails ? 1 : 0;
}
