#pragma once

#include "tetris_core.h"
#include "search_amini.h"
#include "io_rules.h"
#include "ai_rule_spec_utils.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

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

        // Defaults: io_pso run of 2026-10-14 (5.7 h, warm-started from the ZZZTOJ fork weights).
        struct Param
        {
            double roof = 201.64031562636276;
            double col_trans = 444.72788983567006;
            double row_trans = 296.43251672384974;
            double hole_count = 397.73882805061425;
            double hole_line = 551.24888660549459;
            double well_depth = 113.21002819785599;
            double hole_depth = 6.9972285469587199;
            double b2b = 118.02590775401444;
            double attack = 1.7167399013449522;
            double hold_t = 8.9781352655085147;
            double hold_i = 5.6293284154870946;
            double waste_t = -2.5804974934026754;
            double waste_i = 10.853589009080654;
            double clear_1 = 9.8457221342590611;
            double clear_2 = -0.20957165621825569;
            double clear_3 = -13.658951915651661;
            double clear_4 = -0.054761944646760874;
            double t2_slot = 1.7822381149634958;
            double t3_slot = 2.5004370689413844;
            double tspin_mini = -2.1380804346753739;
            double tspin_1 = 10.200368930833605;
            double tspin_2 = 29.876992082653906;
            double tspin_3 = 19.140936760546992;
            double combo = 4.7024903120157218;
            double ratio = 1.1889452853075886;
            double surge = 1.3195020571807554;
            double b2b_gain = 5.4766387040480176;
            double cancel = 7.800077755772266;
            double pending = 1.0138414555273567;
            double opponent_surge = 0.78052237159543703;
            double kill = 15.72307734635335;
            double spin_garbage = -1.2506027129174573;
            double allspin = 11.279518371943739;
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

        struct PendingGarbage
        {
            int amount;
            int frames_until_active; // relative to RootState::frame, clamped at 0
        };
        struct OpponentView
        {
            bool known = false;
            int free_rows = 0; // 20 - visible stack height
            int pending = 0;   // garbage queued against the opponent, active or not
            int btb = 0;       // client counter
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
            int t2_value;
            int t3_value;
            // Per piece index, bit r set iff tanking r rows makes its spawn overlap the stack or the risen garbage.
            std::array<std::uint64_t, kPieceTypes> spawn_blocked;
            io_rules::Spin spin;
            char piece;
            bool all_clear;
            bool lock_out;
        };
        struct Status
        {
            io_rules::PendingQueue<kPendingCapacity> pending;
            double frame = 0;
            double root_chain = 0;
            double like = 0;
            double value = 0;
            int btb = 0;
            int combo = 0;
            int pieces_placed = 0;
            int lines_sent = 0;
            int garbage_rows = 0;
            int map_rise = 0;
            int sent = 0;
            int cancelled = 0;
            int b2b_gained = 0;
            int max_combo = 0;
            bool death = false;
            bool operator<(Status const &other) const;
        };

        static Status make_root_status(RootState const &root, Config const &config);

        template<class Spec>
        void init(Config const *config);
        std::string ai_name() const;
        double ratio() const { return config_->param.ratio; }

        template<class Details>
        Result eval(m_tetris2::TetrisNode<Details, EvalSpinType> const &lp, typename Details::map_type const &map, typename Details::map_type const &src_map, size_t clear) const;
        Status get(Result const &eval_result, size_t depth, Status const &status, m_tetris2::AIEnv const &env) const;

    private:
        struct SpawnCells
        {
            int count = 0;
            std::array<std::int8_t, 4> x{};
            std::array<std::int8_t, 4> y{};
        };

        template<class Spec, std::size_t I = 0>
        void fill_spawn_cells();

        template<class MapT>
        static void detect_t_slots(MapT const &map, int roof, int &t2_value, int &t3_value);

        Config const *config_ = nullptr;
        int width_ = 0;
        int full_count_ = 0;
        std::array<int, 256> piece_index_{};
        std::array<SpawnCells, kPieceTypes> spawn_cells_{};
    };

    template<class Spec, std::size_t I>
    void IO::fill_spawn_cells()
    {
        if constexpr (I < m_tetris2::ai_rule_spec::piece_count<Spec>())
        {
            constexpr char kType = m_tetris2::ai_rule_spec::piece_type_at<Spec>(I);
            constexpr auto kCells = m_tetris2::shape::piece_cells<Spec, kType, 0>;
            constexpr auto kSpawn = Spec::spawn(kType, Spec::width, Spec::height);
            static_assert(kCells.count <= 4);
            SpawnCells &out = spawn_cells_[I];
            out.count = static_cast<int>(kCells.count);
            for (std::size_t i = 0; i < kCells.count; ++i)
            {
                out.x[i] = static_cast<std::int8_t>(kSpawn.first + kCells.origin.x + kCells.cells[i].x);
                out.y[i] = static_cast<std::int8_t>(kSpawn.second - kCells.origin.y + kCells.cells[i].y);
            }
            fill_spawn_cells<Spec, I + 1>();
        }
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
        fill_spawn_cells<Spec>();
    }

    template<class MapT>
    void IO::detect_t_slots(MapT const &map, int roof, int &t2_value, int &t3_value)
    {
        constexpr int kW = MapT::width;
        constexpr int kH = MapT::height;
        auto row_at = [&](int y) -> std::uint32_t
        { return y < kH ? static_cast<std::uint32_t>(map.row(y)) : 0u; };
        // TST slot, overhang on either side: 4-column masks of rows y..y+4.
        struct T3Shape
        {
            std::uint32_t floor, wall, roof_a, roof_b;
        };
        static constexpr std::array<T3Shape, 2> kShapes = {{{11, 8, 4, 12}, {13, 1, 3, 1}}};
        t2_value = 0;
        t3_value = 0;
        bool finding2 = true;
        bool finding3 = true;
        for (int y = 0; (finding2 || finding3) && y < roof - 2; ++y)
        {
            std::uint32_t const row0 = row_at(y);
            std::uint32_t const row1 = row_at(y + 1);
            std::uint32_t const row2 = row_at(y + 2);
            std::uint32_t const row3 = row_at(y + 3);
            std::uint32_t const row4 = row_at(y + 4);
            int const count0 = std::popcount(row0);
            int const count1 = std::popcount(row1);
            for (int x = 0; finding2 && x < kW - 2; ++x)
            {
                if (((row0 >> x) & 7) == 5 && ((row1 >> x) & 7) == 0 && count0 == kW - 1)
                {
                    t2_value += 1;
                    if (count1 == kW - 3)
                    {
                        t2_value += 2;
                        std::uint32_t const row2_check = (row2 >> x) & 7;
                        if (row2_check == 1 || row2_check == 4)
                            t2_value += 2;
                        finding2 = false;
                    }
                }
            }
            for (int x = 0; finding3 && x < kW - 3; ++x)
            {
                for (T3Shape const &shape : kShapes)
                {
                    if (((row0 >> x) & 15) != shape.floor || ((row1 >> x) & 15) != 9)
                        continue;
                    int value = 0;
                    if (count0 == kW - 1)
                    {
                        value += 1;
                        if (count1 == kW - 2)
                        {
                            value += 1;
                            if (((row2 >> x) & 15) == shape.floor)
                            {
                                value += 2;
                                if (std::popcount(row2) == kW - 1)
                                    value += 2;
                            }
                            std::uint32_t const row3_check = (row3 >> x) & 15;
                            if (row3_check == shape.wall || row3_check == 0)
                            {
                                value += row3_check != 0 ? 1 : 0;
                                std::uint32_t const row4_check = (row4 >> x) & 15;
                                value += row4_check == shape.roof_a || row4_check == shape.roof_b ? 1 : -2;
                            }
                            else
                            {
                                value = 0;
                            }
                        }
                    }
                    t3_value += value;
                    if (value > 3)
                        finding3 = false;
                }
            }
        }
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

        detect_t_slots(map, roof, result.t2_value, result.t3_value);

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
                for (int k = std::min(y, roof - 1); k >= 0; --k)
                {
                    if ((row(k) >> cells.x[c]) & 1u)
                        blocked |= std::uint64_t(1) << (y - k);
                }
            }
            result.spawn_blocked[i] = blocked;
        }
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
