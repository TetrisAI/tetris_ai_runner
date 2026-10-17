#pragma once

#include "tetris_core.h"
#include "search_amini.h"
#include "io_rules.h"
#include "ai_rule_spec_utils.h"
#include "ai_zzz_io_spin_slots.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <utility>

namespace ai_zzz
{
    // TETR.IO Season 2 (Tetra League) AI. All attack and garbage arithmetic goes through io_rules.
    class IO
    {
    public:
        using EvalSpinType = search_amini::Search::AMiniType;
        static_assert(static_cast<int>(EvalSpinType::None) == static_cast<int>(io_rules::Spin::None) &&
                      static_cast<int>(EvalSpinType::Mini) == static_cast<int>(io_rules::Spin::Mini) &&
                      static_cast<int>(EvalSpinType::Full) == static_cast<int>(io_rules::Spin::Full));

        static constexpr int kMaxWidth = 10;
        static constexpr int kPieceTypes = 7;
        static constexpr std::size_t kPendingCapacity = 8;

        // Defaults: io_pso run of 2026-10-15 (12 h, 10 generations, pps 4, 10 previews), warm-started from generation 1
        // of a pps 2 / 5-preview run. Measured at pps 4 / 10 previews: 0.59 against the pre-refactor bot (768 games),
        // 0.60 against itself with the spin-slot weights zeroed (1,536 games).
        struct Param
        {
            double roof = 102.60425653876652;
            double col_trans = 509.44159257853119;
            double row_trans = 58.499579620235664;
            double hole_count = 458.62048656186698;
            double hole_line = 944.53693733827822;
            double well_depth = 125.48913965866971;
            double hole_depth = 9.7224397817965809;
            double b2b = 151.09086207135269;
            double attack = 1.7709245276742853;
            double hold_i = -0.85713194201499388;
            double waste_i = 9.0785891351183103;
            double clear_1 = 2.7779922784428548;
            double clear_2 = 6.5261632233071261;
            double clear_3 = -9.3744869806692108;
            double clear_4 = 2.977058825591949;
            double mini_1 = 1.2173095350260459;
            double mini_2 = -4.0446396874214789;
            double mini_3 = -2.3100796482700794;
            double full_1 = 13.022473712221116;
            double full_2 = 23.077924189993393;
            double full_3 = 23.422217182429904;
            double combo = 47.218957936550723;
            double ratio = 1.0027630977026101;
            double surge = 0;
            double b2b_gain = 7.863437790314511;
            double cancel = 3.4688673802342511;
            double pending = 2.8124517258007424;
            double opponent_surge = 1.419574850445857;
            double surge_release = 1;
            double spin_garbage = 11.841627759780454;
            double slot_b2b = 0.068994639972527805;
            double slot_attack = 6.177615077515096;
            double slot_decay = 0.51460052709832438;
        };

        struct ParamField
        {
            std::string_view name;
            double Param::*member;
            double default_value;
            double lower;
            double upper;
            double max_velocity;
        };
        static std::span<ParamField const> param_fields();
        // Text form: one `name = value` per line, `#` starts a comment. Names not in the text keep their value;
        // on failure `param` is left unchanged.
        static bool parse_params(std::string_view text, Param &param);
        static std::string format_params(Param const &param);
        static bool load_params(std::string const &path, Param &param);
        static bool save_params(std::string const &path, Param const &param);

        // The search classifies spins exactly for four TETR.IO spin rules; the others are searched with the rule whose
        // spin positions are closest, and `approximation` names it with the difference (tetrio.js IsTSpin).
        struct SearchSpinRule
        {
            search_amini::Search::SpinRule rule;
            std::string_view approximation;
        };
        static constexpr SearchSpinRule search_spin_rule(io_rules::SpinBonuses spinbonuses)
        {
            using io_rules::SpinBonuses;
            using SpinRule = search_amini::Search::SpinRule;
            switch (spinbonuses)
            {
            case SpinBonuses::TSpins:
                return {SpinRule::TSpins, {}};
            case SpinBonuses::TSpinsPlus:
                return {SpinRule::TSpinsPlus, {}};
            case SpinBonuses::AllMini:
                return {SpinRule::AllMini, {}};
            case SpinBonuses::AllMiniPlus:
                return {SpinRule::AllMiniPlus, {}};
            case SpinBonuses::All:
                return {SpinRule::AllMini, "all-mini (the room scores immobile non-T spins as full spins)"};
            case SpinBonuses::AllPlus:
                return {SpinRule::AllMiniPlus, "all-mini+ (the room scores immobile non-T spins as full spins)"};
            case SpinBonuses::MiniOnly:
                return {SpinRule::AllMiniPlus, "all-mini+ (the room scores every spin, T included, as a mini)"};
            case SpinBonuses::Handheld:
                return {SpinRule::AllMiniPlus, "all-mini+ (the room scores S/Z/L/J corner spins, halved)"};
            case SpinBonuses::Stupid:
                return {SpinRule::AllMiniPlus, "all-mini+ (the room scores every rotation that cannot move down)"};
            case SpinBonuses::None:
                return {SpinRule::TSpins, "T-spins (the room scores no spins)"};
            }
            assert(false);
            return {SpinRule::AllMiniPlus, {}};
        }

        struct PendingGarbage
        {
            int amount;
            int frames_until_active; // relative to RootState::frame, clamped at 0
        };
        struct OpponentView
        {
            bool known = false;
            int btb = 0; // client counter
            std::uint64_t lethal_sent = 0; // bit X: sending X lines kills (IOLethal)
        };
        struct RootState
        {
            int btb = 0;
            int combo = 0;
            int pieces_placed = 0;
            int lines_sent = 0;
            int garbage_rows = 0; // contiguous garbage rows at the bottom of the board
            int frame = 0; // game frame at which the root placement locks (its scheduled execution), not the decision frame
            std::span<PendingGarbage const> pending;
        };
        struct Config
        {
            io_rules::Options rules = io_rules::Options::tetra_league();
            double pps = 1.5;
            bool allow_pc = true;
            OpponentView opponent;
            Param param;
        };

        struct Result
        {
            double value;
            std::uint64_t cleared_rows; // rows of the board before the placement that were cleared
            int clear;
            int cells;
            int roof;
            // Per piece index, bit r set iff tanking r rows makes its spawn overlap the stack or the risen garbage.
            std::array<std::uint64_t, kPieceTypes> spawn_blocked;
            // Per piece index: the best line-clearing spin the board offers that piece.
            std::array<SpinSlot, kPieceTypes> spin_slots;
            io_rules::Spin spin;
            char piece;
            bool all_clear;
            bool lock_out;
        };
        struct Status
        {
            io_rules::PendingQueue<kPendingCapacity> pending;
            double frame = 0;
            double like = 0;
            double value = 0;
            int btb = 0;
            int combo = 0;
            int pieces_placed = 0;
            int lines_sent = 0;
            int garbage_rows = 0;
            int map_rise = 0;
            int sent = 0;
            int surge_sent = 0;
            int cancelled = 0;
            int b2b_gained = 0;
            int max_combo = 0;
            bool death = false;
            bool kill = false;
            bool operator<(Status const &other) const;
        };

        static Status make_root_status(RootState const &root, Config const &config);

        struct SpawnCells
        {
            int count = 0;
            std::array<std::int8_t, 4> x{};
            std::array<std::int8_t, 4> y{};
        };
        template<class Spec>
        static consteval std::array<SpawnCells, kPieceTypes> make_spawn_cells();

        template<class Spec>
        void init(Config const *config);
        std::string ai_name() const;
        double ratio() const { return config_->param.ratio; }

        template<class Details>
        Result eval(m_tetris2::TetrisNode<Details, EvalSpinType> const &lp, typename Details::map_type const &map, typename Details::map_type const &src_map, size_t clear) const;
        Status get(Result const &eval_result, size_t depth, Status const &status, m_tetris2::AIEnv const &env) const;

    private:
        template<class Spec>
        static std::array<SpinSlot, kPieceTypes> detect_spin_slots(typename m_tetris2::Helpers<Spec>::map_t const &map, io_rules::Options const &rules);
        double spin_slot_value(std::array<SpinSlot, kPieceTypes> const &slots, m_tetris2::AIEnv const &env) const;

        Config const *config_ = nullptr;
        int width_ = 0;
        int full_count_ = 0;
        std::array<int, 256> piece_index_{};
        std::array<SpawnCells, kPieceTypes> spawn_cells_{};
    };

    template<class Spec>
    consteval std::array<IO::SpawnCells, IO::kPieceTypes> IO::make_spawn_cells()
    {
        std::array<SpawnCells, kPieceTypes> out{};
        [&]<std::size_t... Is>(std::index_sequence<Is...>)
        {
            ([&]
             {
                 constexpr char kType = m_tetris2::ai_rule_spec::piece_type_at<Spec>(Is);
                 constexpr auto kCells = m_tetris2::shape::piece_cells<Spec, kType, 0>;
                 constexpr auto kSpawn = Spec::spawn(kType, Spec::width, Spec::height);
                 static_assert(kCells.count <= 4);
                 SpawnCells &cells = out[Is];
                 cells.count = static_cast<int>(kCells.count);
                 for (std::size_t i = 0; i < kCells.count; ++i)
                 {
                     cells.x[i] = static_cast<std::int8_t>(kSpawn.first + kCells.origin.x + kCells.cells[i].x);
                     cells.y[i] = static_cast<std::int8_t>(kSpawn.second - kCells.origin.y + kCells.cells[i].y);
                 }
             }(),
             ...);
        }(std::make_index_sequence<m_tetris2::ai_rule_spec::piece_count<Spec>()>{});
        return out;
    }

    template<class Spec>
    void IO::init(Config const *config)
    {
        static_assert(Spec::width <= kMaxWidth && Spec::height <= 64);
        static_assert(m_tetris2::ai_rule_spec::piece_count<Spec>() <= kPieceTypes);
        config_ = config;
        width_ = static_cast<int>(Spec::width);
        full_count_ = width_ * 24;
        m_tetris2::ai_rule_spec::fill_piece_index_cache<Spec>(piece_index_);
        spawn_cells_ = make_spawn_cells<Spec>();
    }

    template<class Spec>
    std::array<SpinSlot, IO::kPieceTypes> IO::detect_spin_slots(typename m_tetris2::Helpers<Spec>::map_t const &map, io_rules::Options const &rules)
    {
        using SpinRule = search_amini::Search::SpinRule;
        auto find = [&]<SpinRule Rule>()
        {
            auto const found = find_spin_slots<Spec, m_tetris2::AMiniRuleHook<Rule>>(map, rules.allow180);
            std::array<SpinSlot, kPieceTypes> slots{};
            std::copy(found.begin(), found.end(), slots.begin());
            return slots;
        };
        switch (search_spin_rule(rules.spinbonuses).rule)
        {
        case SpinRule::TSpins:
            return find.template operator()<SpinRule::TSpins>();
        case SpinRule::TSpinsPlus:
            return find.template operator()<SpinRule::TSpinsPlus>();
        case SpinRule::AllMini:
            return find.template operator()<SpinRule::AllMini>();
        case SpinRule::AllMiniPlus:
            break;
        }
        return find.template operator()<SpinRule::AllMiniPlus>();
    }

    template<class Details>
    IO::Result IO::eval(m_tetris2::TetrisNode<Details, EvalSpinType> const &lp, typename Details::map_type const &map, typename Details::map_type const &src_map, size_t clear) const
    {
        using MapT = typename Details::map_type;
        using Spec = typename Details::spec_type;
        constexpr int kW = MapT::width;
        constexpr int kH = MapT::height;
        constexpr std::uint32_t kRowMask = static_cast<std::uint32_t>(MapT::row_full);
        constexpr std::uint32_t kColMask = kRowMask & ~1u;
        constexpr int kWidthM1 = kW - 1;

        Result result{};
        int const roof = map.none() ? 0 : map.max_y() + 1;
        auto row = [&](int y)
        { return static_cast<std::uint32_t>(map.row(y)); };

        int col_trans = 2 * (kH - roof);
        int row_trans = roof == kH ? 0 : kW;
        for (int y = 0; y < roof; ++y)
        {
            std::uint32_t const r = row(y);
            col_trans += (r & 1u) == 0 ? 1 : 0;
            col_trans += ((r >> kWidthM1) & 1u) == 0 ? 1 : 0;
            col_trans += std::popcount((r ^ (r << 1)) & kColMask);
            if (y != 0)
                row_trans += std::popcount(row(y - 1) ^ r);
        }
        row_trans += std::popcount(kRowMask & ~row(0));
        if (roof != 0)
            row_trans += std::popcount(roof == kH ? kRowMask & ~row(roof - 1) : row(roof - 1));

        int hole_count = 0;
        int hole_line = 0;
        int hole_depth = 0;
        int well_depth = 0;
        std::array<int, kW> hole_num{};
        std::array<int, kW> well_num{};
        std::array<int, kH> clear_width{};
        // Bit 63 - y set iff cell (x, y) is occupied, so `>> (63 - y)` puts board(x, y - r) at bit r.
        std::array<std::uint64_t, kW> flipped_columns{};
        int hole_rows = 0;
        std::uint32_t cover = 0;
        for (int y = roof - 1; y >= 0; --y)
        {
            std::uint32_t const r = row(y);
            cover |= r;
            std::uint32_t const line_hole = cover ^ r;
            if (line_hole != 0)
            {
                ++hole_line;
                hole_count += std::popcount(line_hole);
                for (int hy = y + 1; hy < roof; ++hy)
                {
                    std::uint32_t const check = line_hole & row(hy);
                    if (check == 0)
                        break;
                    clear_width[hole_rows] += (hy + 1) * std::popcount(check);
                }
                ++hole_rows;
            }
            for (int x = 0; x < kW; ++x)
            {
                flipped_columns[x] |= std::uint64_t((r >> x) & 1u) << (63 - y);
                if ((line_hole >> x) & 1u)
                    hole_depth += ++hole_num[x];
                else
                    hole_num[x] = 0;
                std::uint32_t const left = x == 0 ? 1u : (cover >> (x - 1)) & 1u;
                std::uint32_t const right = x == kWidthM1 ? 1u : (cover >> (x + 1)) & 1u;
                if (((cover >> x) & 1u) == 0 && left != 0 && right != 0)
                    well_depth += ++well_num[x];
            }
        }

        auto const &p = config_->param;
        result.value = 0. - roof * p.roof - col_trans * p.col_trans - row_trans * p.row_trans - hole_count * p.hole_count - hole_line * p.hole_line - well_depth * p.well_depth - hole_depth * p.hole_depth;
        double rate = 32;
        for (int i = 0; i < hole_rows; ++i, rate *= 0.25)
            result.value -= clear_width[i] * rate;

        // Tanking r rows moves board(x, y - r) into spawn cell (x, y). For r > y the cell holds a garbage row; hole
        // columns are unknown to the search, so such a cell counts as blocked.
        for (int i = 0; i < kPieceTypes; ++i)
        {
            SpawnCells const &cells = spawn_cells_[i];
            std::uint64_t blocked = 0;
            for (int c = 0; c < cells.count; ++c)
            {
                int const y = cells.y[c];
                blocked |= ~((std::uint64_t(2) << y) - 1);
                blocked |= flipped_columns[cells.x[c]] >> (63 - y);
            }
            result.spawn_blocked[i] = blocked;
        }
        result.spin_slots = detect_spin_slots<Spec>(map, config_->rules);
        result.cells = map.popcount();
        result.roof = roof;
        result.clear = static_cast<int>(clear);
        result.cleared_rows = 0;
        if (clear > 0)
        {
            constexpr auto kCells = m_tetris2::shape::piece_cells<Spec, Details::t_value, Details::r_value>;
            std::array<std::uint32_t, 4> piece_rows{};
            int const bottom = lp.row();
            for (std::size_t i = 0; i < kCells.count; ++i)
                piece_rows[kCells.cells[i].y] |= 1u << (lp.col() + kCells.cells[i].x);
            for (int i = 0; i < 4 && bottom + i < kH; ++i)
            {
                if (piece_rows[i] != 0 && (static_cast<std::uint32_t>(src_map.row(bottom + i)) | piece_rows[i]) == kRowMask)
                    result.cleared_rows |= std::uint64_t(1) << (bottom + i);
            }
        }
        result.spin = static_cast<io_rules::Spin>(lp.spin);
        result.piece = Details::t_value;
        result.all_clear = clear > 0 && map.none();
        result.lock_out = lp.row() >= 20;
        return result;
    }
}
