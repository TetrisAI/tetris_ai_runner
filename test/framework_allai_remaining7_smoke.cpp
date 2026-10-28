// framework_allai_remaining7_smoke.cpp — 其余 7 AI 真实 TetrisEngine prepare+run 冒烟.
//
// 7 AI: misaka(10x22) / ai_ax(10x40 srs) / farter(10x40 srs) /
//       the_ai_games_old / the_ai_games / the_ai_games_enemy(rule_tag) / IO(rule_io+amini).
// 每个 AI 用真实 Rule/Search/AI 三元组 prepare + run 确定性落子，验证：prepare 成功；
// scored()>0（get 真实跑过）；farter / enemy 的 GetArgs 无 Eval → evals==0。
// spawn 一律用 Spec::spawn(piece,width,height)。编译: Homebrew LLVM Debug (-std=c++20 -O0 -g)。

#include "../src/tetris_core.h"
#include "../src/tetris_map.h"
#include "../src/rule_srs.h"
#include "../src/rule_tag.h"
#include "../src/rule_io.h"
#include "../src/search_path.h"
#include "../src/search_tspin.h"
#include "../src/search_tag.h"
#include "../src/search_amini.h"

#include "../src/ai_misaka.h"
#include "../src/ai_ax.h"
#include "../src/ai_farter.h"
#include "../src/ai_tag.h"
#include "../src/ai_zzz_io.h"

#include <cstdio>

namespace
{
    int g_failures = 0;
    int g_runs = 0;
    void expect(bool ok, char const *what)
    {
        if (!ok)
        {
            ++g_failures;
            std::printf("  FAIL: %s\n", what);
        }
    }

    template<class Spec>
    m_tetris2::TetrisBlockStatus spawn_of(char t)
    {
        return m_tetris2::TetrisBlockStatus(t,
                                           static_cast<int8_t>(Spec::spawn(t, Spec::width, Spec::height).first),
                                           static_cast<int8_t>(Spec::spawn(t, Spec::width, Spec::height).second),
                                           0);
    }

    // rule_tag 的 legacy spawn 返回 (W/2-2, H)，即 y=height 的顶缘缓冲区（高于界内 0..height-1），
    // 直接作为 root 会被 core 的 in-bounds 检查判为非法而 0 子节点。x 仍取 Spec::spawn，
    // y 压到界内顶行 height-1 作为合法 root（等价 pieces 已在顶部生成区）。
    template<class Spec>
    m_tetris2::TetrisBlockStatus tag_spawn(char t)
    {
        return m_tetris2::TetrisBlockStatus(t,
                                           static_cast<int8_t>(Spec::spawn(t, Spec::width, Spec::height).first),
                                           static_cast<int8_t>(Spec::height - 1),
                                           0);
    }
}

int main()
{
    std::printf("# framework_allai_remaining7_smoke: 7-AI real engine prepare+run\n");

    // ---- 1) misaka: 10x22 SRS 几何 + tspin Search ----
    {
        using MisakaSpec22 = m_tetris2::RuleSpec<10, 22, 4,
            rule_srs::detail::op_O1,
            rule_srs::detail::op_I1, rule_srs::detail::op_I2, rule_srs::detail::op_I3, rule_srs::detail::op_I4,
            rule_srs::detail::op_S1, rule_srs::detail::op_S2, rule_srs::detail::op_S3, rule_srs::detail::op_S4,
            rule_srs::detail::op_Z1, rule_srs::detail::op_Z2, rule_srs::detail::op_Z3, rule_srs::detail::op_Z4,
            rule_srs::detail::op_L1, rule_srs::detail::op_L2, rule_srs::detail::op_L3, rule_srs::detail::op_L4,
            rule_srs::detail::op_J1, rule_srs::detail::op_J2, rule_srs::detail::op_J3, rule_srs::detail::op_J4,
            rule_srs::detail::op_T1, rule_srs::detail::op_T2, rule_srs::detail::op_T3, rule_srs::detail::op_T4>;
        struct MisakaRule { using rule_spec = MisakaSpec22; };

        using Engine = m_tetris2::TetrisEngine<MisakaRule, ai_misaka::misaka, tspin::Search>;
        using Spec = MisakaSpec22;

        Engine eng;
        expect(eng.prepare(Spec::width, Spec::height), "misaka prepare");

        static int table[8] = {0, 0, 1, 2, 4, 8, 16, 32};
        ai_misaka::misaka::Config cfg{};
        cfg.miny_factor = 100; cfg.hole = 80; cfg.open_hole = 40; cfg.v_transitions = 20; cfg.tspin3 = 60;
        cfg.clear_efficient = 50; cfg.upcomeAtt = 25; cfg.h_factor = 15; cfg.hole_dis_factor2 = 30;
        cfg.hole_dis = 10; cfg.hole_dis_factor = 12; cfg.tspin = 45; cfg.hold_t = 70; cfg.hold_i = 60;
        cfg.clear_useless_factor = 35; cfg.dif_factor = 8; cfg.strategy_4w = 1;
        cfg.table = table; cfg.table_max = 8;
        eng.set_ai_config(cfg);

        typename Engine::map_t map{};
        eng.run(map, spawn_of<Spec>('T'), "ISZL", 4, 400);
        eng.run(map, spawn_of<Spec>('O'), "ZLST", 4, 400);
        ++g_runs;
        std::printf("  misaka: scored=%zu\n", eng.scored());
        expect(eng.scored() > 0, "misaka scored>0");
    }

    // ---- 2) ai_ax: rule_srs + path Search, 无 Config ----
    {
        using Engine = m_tetris2::TetrisEngine<rule_srs::TetrisRule, ai_ax::AI, path::Search>;
        using Spec = rule_srs::TetrisRule::rule_spec;
        Engine eng;
        expect(eng.prepare(Spec::width, Spec::height), "ai_ax prepare");
        typename Engine::map_t map{};
        eng.run(map, spawn_of<Spec>('T'), "ISZL", 4, 400);
        eng.run(map, spawn_of<Spec>('O'), "ZLST", 4, 400);
        ++g_runs;
        std::printf("  ai_ax: scored=%zu\n", eng.scored());
        expect(eng.scored() > 0, "ai_ax scored>0");
    }

    // ---- 3) farter: rule_srs + path Search, 无 Config, evals==0 ----
    {
        using Engine = m_tetris2::TetrisEngine<rule_srs::TetrisRule, ai_farteryhr::AI, path::Search>;
        using Spec = rule_srs::TetrisRule::rule_spec;
        Engine eng;
        expect(eng.prepare(Spec::width, Spec::height), "farter prepare");
        eng.provider_counters().enabled = true;
        eng.provider_counters().reset();
        typename Engine::map_t map{};
        eng.run(map, spawn_of<Spec>('T'), "ISZL", 4, 400);
        eng.run(map, spawn_of<Spec>('O'), "ZLST", 4, 400);
        auto st = eng.eval_cache_stats();
        ++g_runs;
        std::printf("  farter: scored=%zu evals=%zu\n", eng.scored(), st.evals);
        expect(eng.scored() > 0, "farter scored>0");
        expect(st.evals == 0, "farter evals==0");
        expect(eng.provider_counters().eval == 0, "farter provider.eval==0");
    }

    // ---- 4) the_ai_games_old: rule_tag + tag Search, 无 Config ----
    {
        using Engine = m_tetris2::TetrisEngine<rule_tag::TetrisRule, ai_tag::the_ai_games_old, tag::Search>;
        using Spec = rule_tag::TetrisRule::rule_spec;
        Engine eng;
        expect(eng.prepare(Spec::width, Spec::height), "games_old prepare");
        typename Engine::map_t map{};
        eng.run(map, tag_spawn<Spec>('T'), "ISZL", 4, 400);
        eng.run(map, tag_spawn<Spec>('O'), "ZLST", 4, 400);
        ++g_runs;
        std::printf("  games_old: scored=%zu\n", eng.scored());
        expect(eng.scored() > 0, "games_old scored>0");
    }

    // ---- 5) the_ai_games: rule_tag + tag Search, 14 weights all 1 ----
    {
        using Engine = m_tetris2::TetrisEngine<rule_tag::TetrisRule, ai_tag::the_ai_games, tag::Search>;
        using Spec = rule_tag::TetrisRule::rule_spec;
        Engine eng;
        expect(eng.prepare(Spec::width, Spec::height), "games prepare");
        ai_tag::the_ai_games::Config cfg{};
        cfg.map_low_width = 1; cfg.col_trans_width = 1; cfg.row_trans_width = 1; cfg.hold_count_width = 1;
        cfg.hold_focus_width = 1; cfg.well_depth_width = 1; cfg.hole_depth_width = 1; cfg.dig_clear_width = 1;
        cfg.line_clear_width = 1; cfg.tspin_clear_width = 1; cfg.tetris_clear_width = 1; cfg.tspin_build_width = 1;
        cfg.combo_add_width = 1; cfg.combo_break_minute = 1;
        eng.set_ai_config(cfg);
        typename Engine::map_t map{};
        eng.run(map, tag_spawn<Spec>('T'), "ISZL", 4, 400);
        eng.run(map, tag_spawn<Spec>('O'), "ZLST", 4, 400);
        ++g_runs;
        std::printf("  games: scored=%zu\n", eng.scored());
        expect(eng.scored() > 0, "games scored>0");
    }

    // ---- 6) the_ai_games_enemy: rule_tag + tag Search, {up_ptr,point_ptr}, evals==0 ----
    {
        using Engine = m_tetris2::TetrisEngine<rule_tag::TetrisRule, ai_tag::the_ai_games_enemy, tag::Search>;
        using Spec = rule_tag::TetrisRule::rule_spec;
        Engine eng;
        expect(eng.prepare(Spec::width, Spec::height), "games_enemy prepare");
        int up[4] = {0, 0, 0, 0};
        int point = 0;
        ai_tag::the_ai_games_enemy::Config cfg{};
        cfg.up_ptr = up;
        cfg.point_ptr = &point;
        eng.set_ai_config(cfg);
        eng.provider_counters().enabled = true;
        eng.provider_counters().reset();
        typename Engine::map_t map{};
        eng.run(map, tag_spawn<Spec>('T'), "ISZL", 4, 400);
        eng.run(map, tag_spawn<Spec>('O'), "ZLST", 4, 400);
        auto st = eng.eval_cache_stats();
        ++g_runs;
        std::printf("  games_enemy: scored=%zu evals=%zu\n", eng.scored(), st.evals);
        expect(eng.scored() > 0, "games_enemy scored>0");
        expect(st.evals == 0, "games_enemy evals==0");
        expect(eng.provider_counters().eval == 0, "games_enemy provider.eval==0");
    }

    // ---- 7) IO: rule_io + amini Search, 默认 Config(非零 param 规则) ----
    {
        using Engine = m_tetris2::TetrisEngine<rule_io::TetrisRule, ai_zzz::IO, amini::Search>;
        using Spec = rule_io::TetrisRule::rule_spec;
        Engine eng;
        expect(eng.prepare(Spec::width, Spec::height), "io prepare");
        ai_zzz::IO::Config cfg{}; // 默认 rules=tetra_league(), pps=1.5, allow_pc=true
        eng.set_ai_config(cfg);
        typename Engine::map_t map{};
        eng.run(map, spawn_of<Spec>('T'), "ISZL", 4, 400);
        eng.run(map, spawn_of<Spec>('O'), "ZLST", 4, 400);
        ++g_runs;
        std::printf("  io: scored=%zu\n", eng.scored());
        expect(eng.scored() > 0, "io scored>0");
    }

    std::printf("framework_allai_remaining7_smoke: engines=%d failures=%d\n", g_runs, g_failures);
    if (g_failures == 0)
    {
        std::printf("framework_allai_remaining7_smoke: OK\n");
        return 0;
    }
    std::printf("framework_allai_remaining7_smoke: FAIL\n");
    return 1;
}
