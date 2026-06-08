// simulate_diff: SimulateStrategy + (Cautious-form NoSpin) 对拍 oracle/search_simulate.
//
// 二阶段对拍协议见 _diff_harness.h. simulate 与 path 的差只在 search 阶段
// (oracle simulate 的 BFS 起点在 spawn.drop(map), make_path 起点也在 spawn).
//
// commit C4-followup: 候选端用 CautiousHook 而非 NoSpinHook —
//   CautiousHook::Config 默认 fast_move_down=false 时, 与 NoSpinHook 在
//   payload / LandPoint / 180/LR/d/D 开关上完全同形, 但
//   config_allow_rotate_move 继承 BaseSpinHook 默认 false (NoSpinHook 自家
//   覆盖为 true), 因此不会展开 'C/Z/X' rotate-after-move 邻居. 这与
//   oracle/search_simulate 的 BFS 邻居字符集 ('x z c l r L R d D') 一致,
//   让 phase B byte-equal 比对成立. 不动 NoSpinHook 是因为生产 ai.cpp 的
//   simulate_/path_ 走它, 那边历史 allow_rotate_move=true 是产线行为.

#include "_diff_harness.h"

#include "../src/tetris_core.h"
#include "../oracle/search_simulate.h"
#include "../oracle/search_tspin.h"
#include "../oracle/rule_srs.h"
#include "../oracle/ai_easy.h"
#include "../src/ai_zzz.h"
#include "../src/movegen_searcher.h"
#include "../src/rule_srs.h"
#include "../src/search_simulate.h"
#include "../src/search_tspin.h"

#include <cstdio>
#include <string>
#include <vector>

namespace
{
    using NewMap = tetris_diff::NewMap;
    using LpEntry = tetris_diff::LpEntry;
    using LpKey = tetris_diff::LpKey;

    using OracleEngine =
        m_tetris::TetrisEngine<rule_srs_oracle::TetrisRule, ai_easy::AI, search_tspin_oracle::Search>;
    using NewEngine =
        m_tetris2::TetrisEngine<rule_srs::TetrisRule, ai_zzz::TOJ, simulate::Search>;

    OracleEngine &oracle_engine()
    {
        static OracleEngine e;
        static bool prepared = []()
        { return e.prepare(10, 40); }();
        (void)prepared;
        return e;
    }

    NewEngine &new_engine()
    {
        static NewEngine e;
        static bool prepared = []()
        { return e.prepare(10, 40); }();
        (void)prepared;
        return e;
    }

    search_simulate::Search &oracle_search_obj()
    {
        static search_simulate::Search s;
        static bool inited = []()
        {
            m_tetris::TetrisContext const *ctx = oracle_engine().context().get();
            if (!ctx)
                return false;
            s.init(ctx);
            return true;
        }();
        (void)inited;
        return s;
    }

    using NewSearcher = m_tetris2::movegen::Searcher<m_tetris2::SimulateStrategy,
                                                     m_tetris2::CautiousHook,
                                                     rule_srs::TetrisRule::rule_spec>;

    NewSearcher::Config &new_config()
    {
        static NewSearcher::Config cfg{};
        return cfg;
    }

    NewSearcher &new_search_obj()
    {
        static NewSearcher s;
        static bool inited = []()
        {
            s.init(&new_config());
            return true;
        }();
        (void)inited;
        return s;
    }

    auto g_oracle_lps = std::vector<decltype(static_cast<m_tetris::TetrisContext const *>(nullptr)->generate('I'))>();
    std::vector<m_tetris2::BBLandPoint> g_candidate_lps;

    tetris_diff::CaseProbe probe(char piece, NewMap const &board,
                                 std::string const & /*board_name*/, bool /*is_20g*/)
    {
        tetris_diff::CaseProbe cp;
        m_tetris::TetrisContext const *oracle_ctx = oracle_engine().context().get();
        m_tetris2::TetrisContext const *new_ctx = new_engine().context().get();
        if (!oracle_ctx || !new_ctx)
            return cp;
        m_tetris::TetrisMap oracle_map;
        m_tetris2::TetrisMap new_map;
        tetris_diff::copy_to_oracle_map(board, oracle_map);
        tetris_diff::copy_to_oracle_map(board, new_map);
        auto oracle_spawn = oracle_ctx->generate(piece);
        auto new_spawn = new_ctx->generate(piece);
        if (!oracle_spawn || !new_spawn || !oracle_spawn->check(oracle_map) || !new_spawn->check(new_map))
            return cp;

        g_oracle_lps.clear();
        g_candidate_lps.clear();
        auto *oracle_result = oracle_search_obj().search(oracle_map, oracle_spawn, 0);
        if (oracle_result)
            for (auto n : *oracle_result)
                if (n)
                    g_oracle_lps.push_back(n);
        auto *candidate_result = new_search_obj().search(new_map, new_spawn->status, 0);
        if (candidate_result)
            for (auto const &lp : *candidate_result)
                if (lp != nullptr)
                    g_candidate_lps.push_back(lp);

        cp.oracle_lps.reserve(g_oracle_lps.size());
        for (std::size_t i = 0; i < g_oracle_lps.size(); ++i)
            cp.oracle_lps.push_back(LpEntry{LpKey{static_cast<int>(g_oracle_lps[i]->index_filtered),
                                                  0, -1},
                                            i});
        // TODO: rewrite candidate_lps construction using BBLandPoint / CellsKey
        // (Helpers<rule_spec>::cells_key_for_state) once LpKey migrates off int index_filtered.

        m_tetris::TetrisMap oracle_map_copy = oracle_map;
        m_tetris2::TetrisMap new_map_copy = new_map;
        auto oracle_spawn_copy = oracle_spawn;
        auto new_spawn_copy = new_spawn;
        cp.oracle_make_path = [oracle_spawn_copy, oracle_map_copy](LpEntry const &e) -> std::string
        {
            if (e.index >= g_oracle_lps.size())
                return std::string();
            std::vector<char> v = oracle_search_obj().make_path(oracle_spawn_copy, g_oracle_lps[e.index], oracle_map_copy);
            return std::string(v.begin(), v.end());
        };
        // TODO: rewrite using g_candidate_lps[e.index] (BBLandPoint) directly.
        cp.candidate_make_path = [new_spawn_copy, new_map_copy](LpEntry const & /*e*/) -> std::string
        {
            (void)new_spawn_copy;
            (void)new_map_copy;
            return std::string();
        };
        return cp;
    }
}

int main()
{
    tetris_diff::XfailSet xfail;
    return tetris_diff::run_diff_main(
        "simulate_diff",
        "OITLJSZ",
        tetris_diff::build_all_fixtures(),
        {false},
        nullptr,
        probe,
        xfail,
        /*strict=*/true,
        //commit C5: 同 path_diff, IgnoreDrift 豁免 BFS 邻居枚举顺序差.
        tetris_diff::PhaseBPolicy::IgnoreDrift);
}
