#include "../cultris/c2_combo_dp.h"

#include <array>
#include <cmath>
#include <cstdio>
#include <random>

namespace {
int failures = 0;
void expect(bool ok, const char* message) {
    if (!ok) { ++failures; std::printf("FAIL: %s\n", message); }
}

// Independent reference: derive line capacity from all cells present after
// this placement. Evaluate the original formula directly in double precision.
int reference(double bpm, int combo, int cells, double remaining) {
    std::array<std::array<double, 281>, 17> times;
    for (auto& row : times) row.fill(-1.0e300);
    times[combo][cells] = remaining;
    int maximum = combo;
    for (int c = combo; c <= 16; ++c) {
        for (int count = 0; count <= 280; ++count) {
            const double stored = times[c][count];
            if (stored == -1.0e300) continue;
            maximum = std::max(maximum, c);
            const double elapsed = stored - 60.0 / bpm;
            if (c > 0 && elapsed < 0.0) continue;
            const int placed_cells = count + 4;
            for (int lines = 0; lines <= 4 && lines * 10 <= placed_cells; ++lines) {
                const int result_cells = placed_cells - lines * 10;
                const int result_combo = c + (lines != 0);
                if (result_cells > 280 || result_combo > 16) continue;
                const double increment = lines == 0 ? -0.39
                    : 1.95 * (lines + 2) / 1.6 / (std::pow(c, 1.6) + 1.0) - c * 0.1;
                const double result_time = c == 0 ? increment : elapsed + increment;
                if (result_time >= 0.0)
                    times[result_combo][result_cells] = std::max(
                        times[result_combo][result_cells], result_time);
            }
        }
    }
    return maximum;
}
}

int main() {
    for (int cells = 6; cells <= 9; ++cells) {
        expect(max_reachable_combo(240, 5, cells, 0.5) == 6,
            "the current four cells can complete the first ten-cell line");
    }
    expect(max_reachable_combo(240, 5, 5, 0.5) == 5,
        "five existing cells plus four cannot clear a line before timeout");
    expect(max_reachable_combo(240, 5, 6, 0.25) == 5,
        "capacity must not bypass the negative post-clear deadline");
    expect(max_reachable_combo(240, 5, 6, 0.0) == 5,
        "expired combo must not transition merely because a line fits");
    for (int cells : {16, 17, 18, 19, 26, 27, 28, 29, 36, 37, 38, 39, 276, 280}) {
        expect(max_reachable_combo(240, 1, cells, 7.0) == reference(240, 1, cells, 7.0),
            "one-to-four line thresholds and board capacity match reference");
    }
    std::mt19937 random(1);
    int mismatches = 0;
    for (int i = 0; i < 2000; ++i) {
        const int bpm = 50 + random() % 191;
        const int combo = 1 + random() % 5;
        const int cells = random() % 281;
        const double remaining = (random() % 5001) / 1000.0;
        const int actual = max_reachable_combo(bpm, combo, cells, remaining);
        const int wanted = reference(bpm, combo, cells, remaining);
        if (actual != wanted) {
            if (mismatches++ < 5) std::printf(
                "mismatch bpm=%d combo=%d cells=%d remaining=%.3f actual=%d expected=%d\n",
                bpm, combo, cells, remaining, actual, wanted);
        }
    }
    expect(mismatches == 0, "2000 seeded legal cases match independent double formula");
    std::printf("%s c2_combo_dp_test (%d failures)\n", failures == 0 ? "PASS" : "FAIL", failures);
    return failures != 0;
}
