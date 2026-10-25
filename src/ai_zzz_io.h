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

        // Defaults: io_cmaes champion of 2026-10-25, trained under the bot client's burst cadence (own 1.5/2.5/3.5 pps
        // against opponents 1-1.5x faster, 10 previews, 32,768 positions per placement; `ratio` kept from the io_pso
        // run). It beat its predecessor by +0.093 ± 0.026 on fresh validation games; that predecessor measured +0.049
        // [+0.014, +0.084] against the previous (io_pso) defaults on held-out games with opponents up to 2x faster.
        struct Param
        {
            double roof = 141.33276285806221;
            double col_trans = 431.60084543394481;
            double row_trans = 180.77217916073553;
            double hole_count = 438.73709026110282;
            double hole_line = 750.84143373082634;
            double well_depth = 206.10824924821839;
            double hole_depth = 93.947758403702224;
            double b2b = 76.848514929866667;
            double attack = 1.7899694579753251;
            double hold_i = -9.3289075396645202;
            double waste_i = 3.2774771223894916;
            double clear_1 = 4.4820726147604475;
            double clear_2 = 8.654176982036784;
            double clear_3 = 0.041094243427085786;
            double clear_4 = 0.70002283020941292;
            double mini_1 = 2.1962073707999075;
            double mini_2 = -2.2137032769616027;
            double mini_3 = -3.8277372202728372;
            double full_1 = 17.365903846483874;
            double full_2 = 21.98479846185387;
            double full_3 = 21.588871307971374;
            double combo = 63.681806436991309;
            double ratio = 1.0027630977026101;
            double surge = 3.1214948430894642;
            double b2b_gain = 4.1893740583045203;
            double cancel = 2.0653829989317876;
            double pending = 3.1670714966640245;
            double opponent_surge = 0.32360124639051457;
            double surge_release = 0.99384395920221513;
            double spin_garbage = 13.075018476071524;
            double slot_b2b = 8.7107788002209627;
            double slot_attack = 5.4450890372858591;
            double slot_decay = 0.32059936730460409;
        };

        struct ParamField
        {
            std::string_view name;
            double Param::*member;
            double default_value;
            double lower;
            double upper;
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
            // Every field but the score accumulators (like, value, sent, surge_sent, cancelled, b2b_gained, max_combo):
            // the beam expands only the first node it pops among those at one depth that differ in them alone.
            std::uint64_t transposition_key() const;
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
        { return static_cast<std::uint32_t>(map.occupied_row(y)); };

        int col_trans = 2 * (kH - roof);
        int row_trans = 0;
        for (int y = 0; y < roof; ++y)
        {
            std::uint32_t const r = row(y);
            col_trans += (r & 1u) == 0 ? 1 : 0;
            col_trans += ((r >> kWidthM1) & 1u) == 0 ? 1 : 0;
            col_trans += std::popcount((r ^ (r << 1)) & kColMask);
            if (y != 0)
                row_trans += std::popcount(row(y - 1) ^ r);
        }
        if (roof != 0)
        {
            // 档位 A 真理：底墙 occupied(贡献 empty bits of row[0])、顶墙 empty(贡献 occupied bits of row[roof-1])。
            row_trans += std::popcount(kRowMask & ~row(0));
            row_trans += std::popcount(row(roof - 1) & kRowMask);
        }

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
                if (piece_rows[i] != 0 && (static_cast<std::uint32_t>(src_map.occupied_row(bottom + i)) | piece_rows[i]) == kRowMask)
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
