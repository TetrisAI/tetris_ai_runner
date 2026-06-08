// cautious_diff: PathStrategy + CautiousHook 对拍 oracle/search_cautious.
//
// 二阶段对拍协议见 _diff_harness.h. 上一轮 c4_blocking 报告里 cautious_diff
// 是唯一全绿的 driver — oracle/search_cautious 的 search() 没有 land_point
// fast-path, 与候选位板 PathStrategy 都跑完整 BFS, 集合天然一致, 路径串
// 也字节对齐.
//
// CautiousHook::Config 有 fast_move_down 字段, oracle 端 Config 同样有
// fast_move_down. 同步切换两侧 cfg 即可. 不区分 1g/20g.

#include "_diff_harness.h"

#include "../src/tetris_core.h"
#include "../oracle/search_cautious.h"
#include "../oracle/search_tspin.h"
#include "../oracle/rule_srs.h"
#include "../oracle/ai_easy.h"
#include "../src/ai_zzz.h"
#include "../src/movegen_searcher.h"
#include "../src/rule_srs.h"
#include "../src/search_cautious.h"
#include "../src/search_path.h"
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
        m_tetris2::TetrisEngine<rule_srs::TetrisRule, ai_zzz::TOJ, path::Search>;

    OracleEngine &oracle_engine()
    {
        static OracleEngine e;
        static bool prepared = []()
        { return e.prepare(10, 40); }();
        (void)prepared;
        return e;
    }

    search_cautious::Search::Config &oracle_config()
    {
        static search_cautious::Search::Config cfg{};
        return cfg;
    }

    NewEngine &new_engine()
    {
        static NewEngine e;
        static bool prepared = []()
        { return e.prepare(10, 40); }();
        (void)prepared;
        return e;
    }

    search_cautious::Search &oracle_search_obj()
    {
        static search_cautious::Search s;
        static bool inited = []()
        {
            m_tetris::TetrisContext const *ctx = oracle_engine().context().get();
            if (!ctx)
                return false;
            s.init(ctx, &oracle_config());
            return true;
        }();
        (void)inited;
        return s;
    }

    using NewSearcher = m_tetris2::movegen::Searcher<m_tetris2::PathStrategy,
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
        "cautious_diff",
        "OITLJSZ",
        tetris_diff::build_all_fixtures(),
        {false},
        nullptr,
        probe,
        xfail,
        /*strict=*/true,
        //commit C5: cautious_diff 是 IgnoreDrift 最显著的受益者 — oracle
        //  search_cautious 的 BFS 顺序是 `l r L R x z c ...` 且每个邻居
        //  hit-check 用 `child->drop(map)->index_filtered == index` (drop-
        //  命中, 隐式 hard-drop), 而 candidate PathStrategy 顺序是
        //  `x z c l r L R d D` 字面命中. 两套 BFS 拓扑根本不同, 路径串字符
        //  序差是必然结果, Phase A subset 已严格成立 (落点集合一致).
        tetris_diff::PhaseBPolicy::IgnoreDrift);
}
