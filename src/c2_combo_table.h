#pragma once

#include <array>
#include <cmath>
#include <cstddef>

namespace ai_zzz::c2_detail
{
    // With the fastest supported planner rate (240 BPM), a fresh combo can
    // reach 16 but the most favourable next clear makes its deadline negative.
    // Pure integer combo is unbounded; this is the finite game-reachable bound
    // under the native planner's supported BPM domain.
    inline constexpr std::size_t kMaxCombo = 16;
    inline constexpr std::size_t kMaxClear = 4;

    struct ComboPrecompute
    {
        std::array<float, kMaxClear + 1> score;
        std::array<double, kMaxClear + 1> increment;
    };

    inline std::array<ComboPrecompute, kMaxCombo + 1> make_combo_table()
    {
        static constexpr std::array<std::array<float, kMaxClear + 1>, kMaxCombo + 1>
            kScores = {{
                {{0, -3000, 3000, 4000, 5000}},
                {{500, 500, 1000, 1000, 2000}},
                {{1000, 1000, 500, 500, 500}},
                {{2000, 2000, 1000, 1000, 1000}},
                {{4000, 4000, 2000, 2000, 2000}},
                {{6000, 6000, 3000, 3000, 2500}},
                {{8000, 8000, 4000, 3333, 2500}},
                {{10000, 10000, 5000, 3333, 2500}},
                {{10000, 10000, 5000, 3333, 2500}},
                {{10000, 10000, 5000, 3333, 2500}},
                {{10000, 10000, 5000, 3333, 2500}},
                {{10000, 10000, 5000, 3333, 2500}},
                {{10000, 10000, 5000, 3333, 2500}},
                {{10000, 10000, 5000, 3333, 2500}},
                {{10000, 10000, 5000, 3333, 2500}},
                {{10000, 10000, 5000, 3333, 2500}},
                {{10000, 10000, 5000, 3333, 2500}},
            }};
        static_assert(kScores.size() == kMaxCombo + 1);

        constexpr double kZeroLineIncrement = -0.39;
        constexpr double kScale = 1.95;
        constexpr double kBias = 2.0;
        constexpr double kDivisor = 1.6;
        constexpr double kExponent = 1.6;
        constexpr double kOffset = 1.0;
        constexpr double kPenalty = 0.1;

        std::array<ComboPrecompute, kMaxCombo + 1> result{};
        for (std::size_t combo = 0; combo <= kMaxCombo; ++combo)
        {
            result[combo].score = kScores[combo];
            result[combo].increment[0] = kZeroLineIncrement;
            for (std::size_t clear = 1; clear <= kMaxClear; ++clear)
            {
                result[combo].increment[clear] =
                    kScale * (static_cast<double>(clear) + kBias) / kDivisor
                    / (std::pow(static_cast<double>(combo), kExponent) + kOffset)
                    - static_cast<double>(combo) * kPenalty;
            }
        }
        return result;
    }

    // C++20 does not provide constexpr std::pow. Build this immutable table once
    // during static initialization; C2::get only performs bounded indexing.
    inline const std::array<ComboPrecompute, kMaxCombo + 1> kComboTable =
        make_combo_table();

    constexpr std::size_t clamp_combo(std::size_t combo) noexcept
    {
        return combo > kMaxCombo ? kMaxCombo : combo;
    }

    constexpr std::size_t clamp_clear(std::size_t clear) noexcept
    {
        return clear > kMaxClear ? kMaxClear : clear;
    }
}
