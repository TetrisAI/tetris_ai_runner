#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

inline constexpr int kC2BoardWidth = 10;
inline constexpr int kC2BoardHeight = 28;
inline constexpr int kC2MaxCells = kC2BoardWidth * kC2BoardHeight;
inline constexpr int kC2MaxCombo = 16;

namespace c2_combo_detail {

constexpr double pow_4(double value) noexcept {
    const double square = value * value;
    return square * square;
}

constexpr double pow_8(double value) noexcept {
    const double fourth = pow_4(value);
    return fourth * fourth;
}

constexpr double fifth_root(double value) noexcept {
    if (value <= 0.0) return 0.0;
    double root = value;
    for (int i = 0; i < 80; ++i) {
        root = (4.0 * root + value / pow_4(root)) / 5.0;
    }
    return root;
}

constexpr double pow_1_6(double value) noexcept {
    // 1.6 = 8/5; the fixed-iteration Newton root keeps the table
    // C++17 constant-evaluable without relying on non-constexpr std::pow.
    return pow_8(fifth_root(value));
}

constexpr auto make_delta_table() noexcept {
    std::array<std::array<double, 5>, kC2MaxCombo + 1> table{};
    for (int combo = 0; combo <= kC2MaxCombo; ++combo) {
        table[combo][0] = -0.39;
        for (int k = 1; k <= 4; ++k) {
            table[combo][k] =
                1.95 * ((static_cast<double>(k) + 2.0) / 1.6 /
                (pow_1_6(static_cast<double>(combo)) + 1.0))
                - static_cast<double>(combo) * 0.1;
        }
    }
    return table;
}

}  // namespace c2_combo_detail

inline constexpr auto kDeltaTable = c2_combo_detail::make_delta_table();

static_assert(kDeltaTable[0][0] == -0.39);
static_assert(kDeltaTable[1][4] > 3.55 && kDeltaTable[1][4] < 3.56);
static_assert(kDeltaTable[6][1] > -0.41 && kDeltaTable[6][1] < -0.40);

inline int max_reachable_combo(double bpm, int current_combo,
                                int cells, double remaining_s) {
    if (!std::isfinite(bpm) || bpm <= 0.0) bpm = 60.0;
    bpm = std::clamp(bpm, 1.0, 240.0);
    current_combo = std::clamp(current_combo, 0, kC2MaxCombo);
    cells = std::clamp(cells, 0, kC2MaxCells);
    if (!std::isfinite(remaining_s)) remaining_s = 0.0;
    remaining_s = std::clamp(remaining_s, 0.0, 3600.0);

    constexpr float kDead = -std::numeric_limits<float>::infinity();
    std::array<std::array<float, kC2MaxCells + 1>,
                kC2MaxCombo + 1> reach{};
    for (auto& row : reach) row.fill(kDead);
    reach[current_combo][cells] = static_cast<float>(remaining_s);

    const float piece_s = static_cast<float>(60.0 / bpm);
    int best = -1;
    for (int combo = current_combo; combo <= kC2MaxCombo; ++combo) {
        for (int ce = 0; ce <= kC2MaxCells; ++ce) {
            const float stored_rem = reach[combo][ce];
            if (stored_rem == kDead) continue;
            best = std::max(best, combo);

            const float rem = stored_rem - piece_s;
            if (combo > 0 && rem < 0.0f) continue;
            // A placement adds four cells before any full rows are cleared.
            const int max_k = std::min((ce + 4) / kC2BoardWidth, 4);
            for (int k = 0; k <= max_k; ++k) {
                const int new_ce = ce + 4 - k * kC2BoardWidth;
                if (new_ce < 0 || new_ce > kC2MaxCells) continue;
                const float delta =
                    static_cast<float>(kDeltaTable[combo][k]);
                const float new_rem = combo == 0 ? delta : rem + delta;
                if (new_rem < 0.0f) continue;
                const int new_combo = k > 0 ? combo + 1 : combo;
                if (new_combo > kC2MaxCombo) continue;
                float& slot = reach[new_combo][new_ce];
                if (new_rem > slot) slot = new_rem;
            }
        }
    }
    return best < 0 ? 0 : best;
}
