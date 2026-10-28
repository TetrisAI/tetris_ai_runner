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

        // 纯盘面量：落块后棋盘 + Config.param/rules 的加权与派生。clear/spin/piece/all_clear/lock_out/
        // cleared_rows 依赖落点与消行，不在此停留，由 get 从 Clear/Landing/MapBefore 现场重算。
        struct Result
        {
            double value;
            int cells;
            int roof;
            // Per piece index, bit r set iff tanking r rows makes its spawn overlap the stack or the risen garbage.
            std::array<std::uint64_t, kPieceTypes> spawn_blocked;
            // Per piece index: the best line-clearing spin the board offers that piece.
            std::array<SpinSlot, kPieceTypes> spin_slots;
            // 落块消行后盘面是否为空；all_clear = clear > 0 && map_empty（get 侧与 Clear 合并）。
            bool map_empty = false;
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

        using Landing = m_tetris2::GetLanding<EvalSpinType>;
        // 引擎按此表萃取节点上下文调 get；顺序即参数顺序。
        using GetArgs = m_tetris2::type_list<
            m_tetris2::args::Eval<Result>,
            m_tetris2::args::Clear,
            m_tetris2::args::Landing<Landing>,
            m_tetris2::args::MapBefore,
            m_tetris2::args::Parent<Status>,
            m_tetris2::args::Depth,
            m_tetris2::args::Env>;

        template<class MapT>
        Result eval(MapT const &map) const;
        template<class MapT>
        Status get(Result const &eval_result, int clear, Landing const &landing, MapT const &src_map, Status const &parent, size_t depth, m_tetris2::AIEnv const &env) const;

    private:
        // init<Spec> 绑定：eval(map) 无 Spec 形参，spin_slots 扫描的模板参数在 init 期固化到此函数指针。
        using DetectSpinSlotsFn = std::array<SpinSlot, kPieceTypes> (*)(IO const *self, void const *map_raw);
        DetectSpinSlotsFn detect_spin_slots_ = nullptr;
        template<class Spec>
        static std::array<SpinSlot, kPieceTypes> detect_spin_slots(typename m_tetris2::Helpers<Spec>::map_t const &map, io_rules::Options const &rules);
        double spin_slot_value(std::array<SpinSlot, kPieceTypes> const &slots, m_tetris2::AIEnv const &env) const;

        Config const *config_ = nullptr;
        int width_ = 0;
        int height_ = 0;
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
        height_ = static_cast<int>(Spec::height);
        full_count_ = width_ * 24;
        m_tetris2::ai_rule_spec::fill_piece_index_cache<Spec>(piece_index_);
        spawn_cells_ = make_spawn_cells<Spec>();
        detect_spin_slots_ = +[](IO const *self, void const *map_raw) -> std::array<SpinSlot, kPieceTypes> {
            using MapT = typename m_tetris2::Helpers<Spec>::map_t;
            return detect_spin_slots<Spec>(*static_cast<MapT const *>(map_raw), self->config_->rules);
        };
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

    // Below the death penalty: killing and then dying is still death.
    static constexpr double kKillValue = 1e8;

    // Without charging the B2B bonus is a flat +1 whenever the chain is active, so only being active has value.
    static inline double chain_value(IO::Param const &p, io_rules::Options const &rules, int btb, double multiplier)
    {
        if (!rules.b2bcharging)
            return btb > 0 ? p.b2b * 16 : 0.0;
        double const shown = std::max(0, btb - 1);
        double const charge = io_rules::is_charged(rules, btb) ? (btb - rules.b2bcharge_at + rules.b2bcharge_base) * multiplier : 0.0;
        return p.b2b * 16 * shown + p.surge * 256 * charge;
    }

    template<class MapT>
    IO::Result IO::eval(MapT const &map) const
    {
        // 防 mismatched MapT：scanner 在 init<Spec> 期绑定，只能处理与 init 同尺寸的 MapT。
        // engine 调用方保证 init<Spec> 与本 MapT 尺寸一致；standalone 误用直接在这里炸。
        assert(width_ == MapT::width && height_ == MapT::height &&
               "IO::eval<MapT> called with a MapT whose dimensions do not match init<Spec>");
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
        // spin_slots 由 init<Spec> 固化的扫描器重算（当前 rules，不强制 8 变体重扫）。
        result.spin_slots = detect_spin_slots_(this, &map);
        result.cells = map.popcount();
        result.roof = roof;
        result.map_empty = map.none();
        return result;
    }

    template<class MapT>
    IO::Status IO::get(Result const &eval_result, int clear, Landing const &landing, MapT const &src_map, Status const &parent, size_t depth, m_tetris2::AIEnv const &env) const
    {
        constexpr int kH = MapT::height;
        constexpr std::uint32_t kRowMask = static_cast<std::uint32_t>(MapT::row_full);
        Config const &config = *config_;
        io_rules::Options const &rules = config.rules;
        Param const &p = config.param;
        Status result = parent;

        result.frame = depth == 0 ? parent.frame : parent.frame + 60 / config.pps;
        result.pending.advance(static_cast<int>(std::floor(result.frame) - std::floor(parent.frame)));
        ++result.pieces_placed;
        double const multiplier = io_rules::garbage_multiplier(rules, result.frame);

        // 旧 eval 尾部派生（顺序保留）：从 Clear/Landing/MapBefore 现场重算 cleared_rows/spin/piece/all_clear/lock_out。
        std::uint64_t cleared_rows = 0;
        if (clear > 0)
        {
            int const bottom = landing.row();
            int const h = landing.h;
            // lines[] 是 NxN 盒坐标；先找 shape_bottom（首个非零行）和 shape_left（最左 set bit）。
            // lines[] y-down (i=0 顶行)；shape_bottom = 最后一个非零行 (piece 底)，shape_left = 最左 set bit。
            int shape_bottom = -1;
            int shape_left = h;
            for (int i = 0; i < h; ++i)
                if (landing.lines[i] != 0) shape_bottom = i;
            for (int i = 0; i < h; ++i)
                for (int x = 0; x < h; ++x)
                    if ((landing.lines[i] >> x) & 1u) { shape_left = std::min(shape_left, x); break; }
            for (int i = 0; i < h; ++i)
            {
                if (landing.lines[i] == 0) continue;
                std::uint32_t const piece_row =
                    static_cast<std::uint32_t>(landing.lines[i] >> shape_left) << landing.col();
                int const board_row = bottom + (shape_bottom - i);
                if (board_row < 0 || board_row >= kH) continue;
                if ((static_cast<std::uint32_t>(src_map.occupied_row(board_row)) | piece_row) == kRowMask)
                    cleared_rows |= std::uint64_t(1) << board_row;
            }
        }
        io_rules::Spin const spin = static_cast<io_rules::Spin>(landing.spin);
        char const piece = landing.piece;
        bool const all_clear = clear > 0 && eval_result.map_empty;
        bool const lock_out = landing.row() >= 20;

        std::uint64_t const garbage_mask = parent.garbage_rows >= 64 ? ~std::uint64_t(0) : (std::uint64_t(1) << parent.garbage_rows) - 1;
        int const garbage_cleared = std::popcount(cleared_rows & garbage_mask);
        result.garbage_rows -= garbage_cleared;

        io_rules::Placement const placement{clear, spin, all_clear, garbage_cleared > 0};
        io_rules::Attack const attack = io_rules::resolve_attack(rules, {parent.btb, parent.combo}, placement, multiplier);
        io_rules::ExchangeResult const exchange = io_rules::exchange(rules, attack, result.pending, result.pieces_placed, result.lines_sent, io_rules::garbage_cap(rules, result.frame));
        result.btb = attack.counters.btb;
        result.combo = attack.counters.combo;
        result.map_rise += exchange.tanked;
        result.sent += exchange.sent;
        result.surge_sent += exchange.surge_sent;
        result.cancelled += exchange.cancelled;
        double const valued_sent = result.sent - (1 - p.surge_release) * result.surge_sent;
        int const b2b_gain = std::max(0, result.btb - parent.btb);
        result.b2b_gained += rules.b2bcharging ? b2b_gain : (parent.btb == 0 && result.btb > 0 ? 1 : 0);
        result.max_combo = std::max(result.combo, result.max_combo);

        double const waste = piece == 'I' ? p.waste_i : 0;
        switch (clear)
        {
        case 0:
            if (exchange.tanked > 0)
                result.like += waste;
            break;
        case 1:
        case 2:
        case 3:
            if (spin == io_rules::Spin::Mini)
            {
                std::array<double, 3> const mini = {p.mini_1, p.mini_2, p.mini_3};
                result.like += mini[clear - 1];
            }
            else if (spin == io_rules::Spin::Full)
            {
                std::array<double, 3> const full = {p.full_1, p.full_2, p.full_3};
                result.like += full[clear - 1];
            }
            else
            {
                std::array<double, 3> const plain = {p.clear_1, p.clear_2, p.clear_3};
                result.like += plain[clear - 1] + (clear < 3 ? waste : 0);
            }
            break;
        default:
            result.like += (result.combo + std::min(std::max(0, result.btb - 1), 5)) * (1 + result.sent + result.cancelled) * p.clear_4;
            break;
        }
        if (clear > 0 && spin != io_rules::Spin::None && garbage_cleared > 0)
            result.like += p.spin_garbage;
        if (env.hold == 'I' && clear != 4)
            result.like += p.hold_i;
        result.like += valued_sent;

        if (all_clear && !config.allow_pc)
            result.death = true;
        if (!rules.nolockout && lock_out)
            result.death = true;
        if (clear == 0 || !rules.clutch)
        {
            int const next_index = env.length > 0 ? piece_index_[static_cast<unsigned char>(*env.next)] : -1;
            std::uint64_t blocked = 0;
            for (int i = 0; i < kPieceTypes; ++i)
            {
                if (next_index < 0 || next_index == i)
                    blocked |= eval_result.spawn_blocked[i];
            }
            if (result.map_rise >= 64 || ((blocked >> result.map_rise) & 1u) != 0)
                result.death = true;
        }

        double const rate = 1. / static_cast<double>(depth + 1) + 3;
        double const fullness = std::max(0.05, (full_count_ - eval_result.cells - result.map_rise * (width_ - 1)) / static_cast<double>(full_count_));
        int const placement_attack = attack.total();
        double const offense = valued_sent * 256 * rate * p.attack +
                               result.cancelled * 256 * p.cancel +
                               (b2b_gain > 0 ? placement_attack * 16 * (22 - eval_result.roof) : 0) +
                               chain_value(p, rules, result.btb, multiplier) +
                               result.b2b_gained * 256 * p.b2b_gain +
                               result.like * 32 +
                               spin_slot_value(eval_result.spin_slots, env) * 256;

        double incoming = result.pending.total();
        if (config.opponent.known)
            incoming += p.opponent_surge * io_rules::surge_amount(rules, config.opponent.btb, multiplier);
        double const uncovered = std::max(0.0, incoming - io_rules::surge_amount(rules, result.btb, multiplier));
        result.kill = depth == 0 ? result.sent < 64 && ((config.opponent.lethal_sent >> result.sent) & 1) != 0 : parent.kill;

        result.value = eval_result.value + offense * fullness +
                       result.max_combo * (result.max_combo - 1) * (parent.btb <= result.btb ? placement_attack : 0) * p.combo -
                       uncovered * 256 * p.pending +
                       (result.kill ? kKillValue : 0) -
                       (result.death ? 999999999.0 : 0);
        return result;
    }
}
