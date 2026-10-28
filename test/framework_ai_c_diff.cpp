// framework_ai_c_diff.cpp — C shard (ai_zzz::IO, ai_tag::old/games/enemy) 旧新数值对拍.
// 同一份 cpp 双编译：
//   AGENT_NEW → 对新 src（eval(map) + GetArgs get）
//   AGENT_OLD → 对冻结 snapshot 旧 src（旧 4 参 eval / 旧 get）
// 每 AI 跑 ≥3 config × ≥20 case，非零 Param、非空 pending、多 rules spinbonuses/allow180。
// body 逐字节 diff（MODE= 首行除外）。
#include "tetris_core.h"
#if defined(AGENT_NEW)
#include "tetris_eval_get.h"
#endif
#include "tetris_node.h"
#include "tetris_shape.h"
#include "rule_io.h"
#include "ai_zzz_io.h"
#include "ai_tag.h"

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <array>
#include <span>

using Spec = rule_io::TetrisRule::rule_spec;
using MapT = m_tetris2::TetrisMap<Spec::width, Spec::height>;
using TSpin = search_tspin::Search::TSpinType;
using AMini = search_amini::Search::AMiniType;

static MapT board_from_rows(uint32_t const *rows)
{
    MapT m{};
    for (int y = 0; y < Spec::height; ++y)
        m.set_row(y, static_cast<MapT::row_t>(MapT::row_full ^ rows[y]));
    return m;
}

static MapT make_board(int kind)
{
    uint32_t rows[40] = {};
    auto fill = [&](int y, uint32_t mask) { rows[y] = mask; };
    switch (kind)
    {
    case 0: fill(0, 0x1); break;
    case 1: fill(0, 0x3ff); fill(1, 0x3ff); fill(2, 0x3ff); fill(3, 0x3ff); break;
    case 2: fill(0, 0x3ff); fill(1, 0x3ff); fill(2, 0x3ff ^ (1u << 5)); fill(3, 0x3ff); break;
    case 3: for (int y = 0; y < 18; ++y) fill(y, 0x3ff ^ (1u << 2)); fill(18, 0x3ff); break;
    case 4: fill(0, 0x3ff); fill(1, 0x3ff); fill(2, 0x3ff ^ (1u << 0)); fill(3, 0x3ff ^ (1u << 0)); fill(4, 0x3ff); break;
    case 5: fill(36, 0x3ff); fill(37, 0x3ff); fill(38, 0x3ff); fill(39, 0x3ff); break;
    case 6: fill(0, 0x3ff ^ (1u << 9)); fill(1, 0x3ff ^ (1u << 9)); fill(2, 0x3ff); fill(3, 0x3ff); break;
    case 7: fill(0, 0x3ff); fill(1, 0x3ff); fill(2, 0x3ff); fill(3, 0x3ff); fill(4, 0x3ff); fill(5, 0x3ff ^ (1u << 3)); break;
    case 8: fill(0, 0x3ff); fill(1, 0x3ff); fill(2, 0x3ff ^ (1u << 7)); fill(3, 0x3ff); fill(4, 0x3ff); break;
    case 9: for (int y = 0; y < 24; ++y) fill(y, 0x3ff); fill(24, 0x3ff ^ (1u << 4)); break;
    }
    return board_from_rows(rows);
}

#if defined(AGENT_NEW)
template <class Details, class SpinT>
static m_tetris2::GetLanding<SpinT> make_landing(int col, int land_height, SpinT spin, char piece)
{
    using Base = m_tetris2::TetrisNodeBase<Details>;
    m_tetris2::GetLanding<SpinT> L{};
    L.piece = piece;
    L.xb = static_cast<int8_t>(col);
    L.yb = static_cast<int8_t>(land_height - Base::kHeight);
    L.w = static_cast<int8_t>(Base::kWidth);
    L.h = static_cast<int8_t>(Base::kHeight);
    for (int i = 0; i < Base::kHeight && i < (int)m_tetris2::GetLanding<SpinT>::kMaxLines; ++i)
        L.lines[i] = Base::kLines[i];
    if constexpr (!std::is_same_v<SpinT, std::monostate>)
        L.spin = spin;
    return L;
}

#endif
template <class Details, class SpinT>
static m_tetris2::TetrisNode<Details, SpinT> make_node(int col, int land_height, SpinT spin, char /*piece*/)
{
    using NB = m_tetris2::TetrisNodeBase<Details>;
    m_tetris2::TetrisNode<Details, SpinT> n;
    n.x = static_cast<int8_t>(col);
    n.y = static_cast<int8_t>(land_height - NB::kHeight + NB::kOriginY);
    if constexpr (!std::is_same_v<SpinT, std::monostate>)
        n.spin = spin;
    return n;
}

static m_tetris2::AIEnv make_env(const char *next, size_t len, char node, char hold)
{
    m_tetris2::AIEnv e{};
    e.next = next; e.length = len; e.node = node; e.hold = hold; e.is_hold = (hold != ' ');
    return e;
}

struct Case
{
    int board_kind;
    int col;
    int land_height;
    int clear;
    int spin; // 0=None 1=Full/TSpin 2=Mini/TSpinMini
    int depth;
    int env_len;
    char node;
    char hold;
};

static Case const *table()
{
    static Case const t[] = {
        {0, 3, 4, 0, 0, 0, 0, ' ', ' '},
        {1, 3, 4, 0, 0, 1, 2, 'T', ' '},
        {1, 3, 4, 1, 1, 0, 3, 'I', 'T'},
        {1, 3, 4, 1, 2, 0, 1, 'T', 'I'},
        {2, 5, 4, 2, 0, 0, 2, 'T', ' '},
        {3, 2, 10, 3, 0, 2, 4, 'I', 'T'},
        {4, 8, 5, 4, 0, 0, 0, ' ', ' '},
        {5, 3, 38, 0, 0, 1, 1, 'T', ' '},
        {1, 0, 4, 1, 0, 0, 5, 'T', 'I'},
        {1, 9, 4, 0, 1, 2, 0, ' ', ' '},
        {6, 0, 4, 0, 0, 0, 2, 'I', ' '},
        {7, 3, 6, 1, 0, 1, 3, 'T', 'I'},
        {8, 5, 5, 2, 1, 0, 2, 'T', ' '},
        {9, 4, 25, 1, 0, 2, 1, 'I', 'T'},
        {2, 2, 4, 0, 2, 0, 0, ' ', ' '},
        {3, 5, 12, 0, 0, 1, 4, 'T', 'I'},
        {4, 2, 5, 4, 0, 0, 2, 'I', ' '},
        {0, 5, 6, 0, 0, 0, 0, ' ', ' '},
        {7, 6, 7, 0, 1, 2, 5, 'T', 'T'},
        {8, 1, 5, 1, 0, 1, 0, ' ', 'I'},
    };
    return t;
}
static int kNumCases() { return 20; }

static char nextbuf[32];

// ---------- IO configs ----------
static int run_io_config(int cfg_id, ai_zzz::IO::Config const &config)
{
    using DetO = m_tetris2::TetrisNodeDetails<Spec, 'O', (uint8_t)0>;
    ai_zzz::IO ai;
    ai.init<Spec>(&config);

    // Non-empty pending: 3 pending garbage chunks to exercise advance/exchange.
    ai_zzz::IO::PendingGarbage pg[3] = {{4, 0}, {2, 30}, {1, 60}};

    for (int i = 0; i < kNumCases(); ++i)
    {
        Case c = table()[i];
        MapT src = make_board(c.board_kind);
        MapT after = src;
        int row = c.land_height - 2;
        for (int dy = 0; dy < 2; ++dy)
            for (int dx = 0; dx < 2; ++dx)
            {
                int x = c.col + dx, y = row + dy;
                if (x >= 0 && x < Spec::width && y >= 0 && y < Spec::height)
                    after.set(x, y);
            }
        AMini aspin = c.spin == 0 ? AMini::None : (c.spin == 1 ? AMini::Full : AMini::Mini);
        std::memset(nextbuf, 0, sizeof(nextbuf));
        const char *seq = "TISJOLZ";
        for (int k = 0; k < c.env_len; ++k) nextbuf[k] = seq[k % 7];
        m_tetris2::AIEnv env = make_env(nextbuf, (size_t)c.env_len, c.node, c.hold);

#if defined(AGENT_OLD)
        auto lp = make_node<DetO, AMini>(c.col, c.land_height, aspin, 'O');
        auto er = ai.eval(lp, after, src, (size_t)c.clear);
        ai_zzz::IO::RootState root{};
        root.pieces_placed = 10; root.lines_sent = 5; root.btb = 1; root.combo = 1; root.frame = 60.0;
        root.garbage_rows = 3;
        root.pending = std::span<ai_zzz::IO::PendingGarbage const>(pg, 3);
        auto parent = ai.make_root_status(root, config);
        auto st = ai.get(er, (size_t)c.depth, parent, env);
        std::printf("IO cfg=%d case=%d eval.value=%.17g eval.cells=%d eval.roof=%d "
                    "get.value=%.17g get.like=%.17g get.btb=%d get.combo=%d get.death=%d get.kill=%d get.sent=%d get.cancelled=%d get.surge=%d get.maprise=%d get.frame=%.17g get.pendingtot=%.17g\n",
                    cfg_id, i, er.value, er.cells, er.roof,
                    st.value, st.like, st.btb, st.combo, (int)st.death, (int)st.kill,
                    st.sent, st.cancelled, st.surge_sent, st.map_rise, st.frame, st.pending.total());
#else
        auto landing = make_landing<DetO, AMini>(c.col, c.land_height, aspin, 'O');
        auto er = ai.eval(after);
        ai_zzz::IO::RootState root{};
        root.pieces_placed = 10; root.lines_sent = 5; root.btb = 1; root.combo = 1; root.frame = 60.0;
        root.garbage_rows = 3;
        root.pending = std::span<ai_zzz::IO::PendingGarbage const>(pg, 3);
        auto parent = ai.make_root_status(root, config);
        auto st = ai.get(er, c.clear, landing, src, parent, (size_t)c.depth, env);
        std::printf("IO cfg=%d case=%d eval.value=%.17g eval.cells=%d eval.roof=%d eval.mapempty=%d "
                    "get.value=%.17g get.like=%.17g get.btb=%d get.combo=%d get.death=%d get.kill=%d get.sent=%d get.cancelled=%d get.surge=%d get.maprise=%d get.frame=%.17g get.pendingtot=%.17g\n",
                    cfg_id, i, er.value, er.cells, er.roof, (int)er.map_empty,
                    st.value, st.like, st.btb, st.combo, (int)st.death, (int)st.kill,
                    st.sent, st.cancelled, st.surge_sent, st.map_rise, st.frame, st.pending.total());
#endif
    }
    return 0;
}

int main()
{
#if defined(AGENT_OLD)
    std::printf("MODE=OLD\n");
#else
    std::printf("MODE=NEW\n");
#endif

    using DetT = m_tetris2::TetrisNodeDetails<Spec, 'T', (uint8_t)0>;
    using DetO = m_tetris2::TetrisNodeDetails<Spec, 'O', (uint8_t)0>;

    // ---------- IO: 3 configs, non-zero Param, varied rules ----------
    {
        ai_zzz::IO::Config cfgA; // non-zero defaults from C++ aggregate
        cfgA.param.roof = 1.5;
        cfgA.param.col_trans = 2.0;
        cfgA.param.row_trans = 1.0;
        cfgA.param.hole_count = 3.0;
        cfgA.param.hole_line = 4.0;
        cfgA.param.well_depth = 1.0;
        cfgA.param.hole_depth = 2.0;
        cfgA.param.clear_4 = 1.0;
        cfgA.param.combo = 0.5;
        cfgA.param.pending = 0.25;
        run_io_config(0, cfgA);

        ai_zzz::IO::Config cfgB;
        cfgB.rules.b2bcharging = true;
        cfgB.rules.spinbonuses = io_rules::SpinBonuses::AllMiniPlus;
        cfgB.rules.allow180 = true;
        cfgB.param.roof = 2.0;
        cfgB.param.hole_count = 5.0;
        cfgB.param.b2b = 1.0;
        cfgB.param.surge = 1.0;
        cfgB.param.combo = 1.0;
        run_io_config(1, cfgB);

        ai_zzz::IO::Config cfgC;
        cfgC.rules.spinbonuses = io_rules::SpinBonuses::Handheld;
        cfgC.rules.nolockout = true;
        cfgC.rules.clutch = false;
        cfgC.param.roof = 0.5;
        cfgC.param.hole_line = 10.0;
        cfgC.param.clear_1 = 1.0;
        cfgC.param.mini_1 = 0.5;
        run_io_config(2, cfgC);
    }

    // ---------- tag old: 3 configs (only one Param-free AI, but vary parent State) ----------
    {
        ai_tag::the_ai_games_old ai;
        ai.init<Spec>();
        for (int cfg = 0; cfg < 3; ++cfg)
        {
            for (int i = 0; i < kNumCases(); ++i)
            {
                Case c = table()[i];
                MapT map = make_board(c.board_kind);
#if defined(AGENT_OLD)
                auto lp = make_node<DetO, std::monostate>(c.col, c.land_height, std::monostate{}, 'O');
                auto er = ai.eval(lp, map, map, (size_t)c.clear);
                ai_tag::the_ai_games_old::Status parent{};
                parent.combo = (size_t)(cfg == 0 ? 1 : cfg == 1 ? 0 : 3);
                parent.up = cfg;
                parent.land_point = 0.25 * cfg;
                parent.value = 1.0 + cfg;
                auto st = ai.get(er, (size_t)c.depth, parent);
                std::printf("Told cfg=%d case=%d eval.map=%.17g eval.tilt=%d eval.full=%d eval.count=%d eval.lowy=%d eval.roof=%d "
                            "get.combo=%zu get.up=%d get.landpoint=%.17g get.value=%.17g\n",
                            cfg, i, er.map, er.tilt, er.full, er.count, er.low_y, er.roof,
                            st.combo, st.up, st.land_point, st.value);
#else
                auto landing = make_landing<DetO, std::monostate>(c.col, c.land_height, std::monostate{}, 'O');
                auto er = ai.eval(map);
                ai_tag::the_ai_games_old::Status parent{};
                parent.combo = (size_t)(cfg == 0 ? 1 : cfg == 1 ? 0 : 3);
                parent.up = cfg;
                parent.land_point = 0.25 * cfg;
                parent.value = 1.0 + cfg;
                auto st = ai.get(er, landing, c.clear, parent, (size_t)c.depth);
                std::printf("Told cfg=%d case=%d eval.map=%.17g eval.tilt=%d eval.full=%d eval.count=%d eval.lowy=%d eval.roof=%d "
                            "get.combo=%zu get.up=%d get.landpoint=%.17g get.value=%.17g\n",
                            cfg, i, er.map, er.tilt, er.full, er.count, er.low_y, er.roof,
                            st.combo, st.up, st.land_point, st.value);
#endif
            }
        }
    }

    // ---------- tag games: 3 configs ----------
    {
        for (int cfg = 0; cfg < 3; ++cfg)
        {
            ai_tag::the_ai_games::Config g{};
            if (cfg == 0)
            {
                g.map_low_width = 1.0; g.col_trans_width = 2.0; g.row_trans_width = 3.0;
                g.hold_count_width = 4.0; g.hold_focus_width = 5.0; g.well_depth_width = 6.0;
                g.hole_depth_width = 7.0; g.dig_clear_width = 8.0; g.line_clear_width = 9.0;
                g.tspin_clear_width = 10.0; g.tetris_clear_width = 11.0; g.tspin_build_width = 12.0;
                g.combo_add_width = 13.0; g.combo_break_minute = 14.0;
            }
            else if (cfg == 1)
            {
                g.map_low_width = 0.5; g.col_trans_width = 1.0; g.row_trans_width = 0.25;
                g.hold_count_width = 2.0; g.hold_focus_width = 0.5; g.well_depth_width = 1.5;
                g.hole_depth_width = 0.75; g.dig_clear_width = 0.2; g.line_clear_width = 4.0;
                g.tspin_clear_width = 8.0; g.tetris_clear_width = 2.0; g.tspin_build_width = 3.0;
                g.combo_add_width = 1.0; g.combo_break_minute = 0.5;
            }
            else
            {
                g.map_low_width = 3.0; g.col_trans_width = 0.5; g.row_trans_width = 1.5;
                g.hold_count_width = 0.1; g.hold_focus_width = 2.0; g.well_depth_width = 0.5;
                g.hole_depth_width = 1.0; g.dig_clear_width = 3.0; g.line_clear_width = 1.0;
                g.tspin_clear_width = 1.0; g.tetris_clear_width = 5.0; g.tspin_build_width = 0.5;
                g.combo_add_width = 0.25; g.combo_break_minute = 3.0;
            }
            ai_tag::the_ai_games ai;
            ai.init<Spec>(&g);

            for (int i = 0; i < kNumCases(); ++i)
            {
                Case c = table()[i];
                MapT map = make_board(c.board_kind);
                TSpin tspin = c.spin == 0 ? TSpin::None : (c.spin == 1 ? TSpin::TSpin : TSpin::TSpinMini);
#if defined(AGENT_OLD)
                auto lp = make_node<DetT, TSpin>(c.col, c.land_height, tspin, 'T');
                auto er = ai.eval(lp, map, map, (size_t)c.clear);
                ai_tag::the_ai_games::Status parent{};
                parent.max_combo = cfg; parent.combo = (size_t)(cfg == 1 ? 0 : 1);
                parent.max_attack = 1.0 * cfg; parent.attack = 0.5;
                parent.up[0] = cfg; parent.up[1] = cfg + 1; parent.up[2] = cfg + 2; parent.up[3] = cfg + 3;
                parent.land_point = 0.25; parent.value = 2.0;
                auto st = ai.get(er, (size_t)c.depth, parent);
                std::printf("Tgames cfg=%d case=%d eval.map=%.17g eval.maplow=%d eval.tbuild=%d eval.roof=%d "
                            "get.combo=%zu get.attack=%.17g get.maxattack=%.17g get.value=%.17g\n",
                            cfg, i, er.map, er.map_low, er.tbuild, er.roof,
                            st.combo, st.attack, st.max_attack, st.value);
#else
                auto landing = make_landing<DetT, TSpin>(c.col, c.land_height, tspin, 'T');
                auto er = ai.eval(map);
                ai_tag::the_ai_games::Status parent{};
                parent.max_combo = cfg; parent.combo = (size_t)(cfg == 1 ? 0 : 1);
                parent.max_attack = 1.0 * cfg; parent.attack = 0.5;
                parent.up[0] = cfg; parent.up[1] = cfg + 1; parent.up[2] = cfg + 2; parent.up[3] = cfg + 3;
                parent.land_point = 0.25; parent.value = 2.0;
                auto st = ai.get(er, landing, c.clear, parent, (size_t)c.depth);
                std::printf("Tgames cfg=%d case=%d eval.map=%.17g eval.maplow=%d eval.tbuild=%d eval.roof=%d "
                            "get.combo=%zu get.attack=%.17g get.maxattack=%.17g get.value=%.17g\n",
                            cfg, i, er.map, er.map_low, er.tbuild, er.roof,
                            st.combo, st.attack, st.max_attack, st.value);
#endif
            }
        }
    }

    // ---------- tag enemy: 3 configs (different upstream point_ptr to test write-through threshold) ----------
    {
        for (int cfg = 0; cfg < 3; ++cfg)
        {
            int up_out[4] = {-99, -99, -99, -99};
            int point_out = (cfg == 0 ? 0 : cfg == 1 ? 10 : 50);
            ai_tag::the_ai_games_enemy::Config cfg_e{up_out, &point_out};
            ai_tag::the_ai_games_enemy ai;
            ai.init<Spec>(&cfg_e);

            for (int i = 0; i < kNumCases(); ++i)
            {
                Case c = table()[i];
                MapT map = make_board(c.board_kind);
                TSpin tspin = c.spin == 0 ? TSpin::None : (c.spin == 1 ? TSpin::TSpin : TSpin::TSpinMini);
#if defined(AGENT_OLD)
                auto lp = make_node<DetT, TSpin>(c.col, c.land_height, tspin, 'T');
                auto er = ai.eval(lp, map, map, (size_t)c.clear);
                ai_tag::the_ai_games_enemy::Status parent{};
                parent.up[0] = cfg; parent.up[1] = cfg + 1; parent.up[2] = cfg + 2; parent.up[3] = cfg + 3;
                parent.point = 5; parent.combo = 1;
                auto st = ai.get(er, (size_t)c.depth, parent);
                std::printf("Tenemy cfg=%d case=%d "
                            "get.up0=%d up1=%d up2=%d up3=%d get.point=%d get.combo=%d extpoint=%d extup=%d,%d,%d,%d\n",
                            cfg, i,
                            st.up[0], st.up[1], st.up[2], st.up[3], st.point, st.combo,
                            point_out, up_out[0], up_out[1], up_out[2], up_out[3]);
#else
                auto landing = make_landing<DetT, TSpin>(c.col, c.land_height, tspin, 'T');
                (void)ai.eval(map);
                ai_tag::the_ai_games_enemy::Status parent{};
                parent.up[0] = cfg; parent.up[1] = cfg + 1; parent.up[2] = cfg + 2; parent.up[3] = cfg + 3;
                parent.point = 5; parent.combo = 1;
                auto st = ai.get(c.clear, landing, parent, (size_t)c.depth);
                std::printf("Tenemy cfg=%d case=%d "
                            "get.up0=%d up1=%d up2=%d up3=%d get.point=%d get.combo=%d extpoint=%d extup=%d,%d,%d,%d\n",
                            cfg, i,
                            st.up[0], st.up[1], st.up[2], st.up[3], st.point, st.combo,
                            point_out, up_out[0], up_out[1], up_out[2], up_out[3]);
#endif
            }
        }
    }

    return 0;
}
