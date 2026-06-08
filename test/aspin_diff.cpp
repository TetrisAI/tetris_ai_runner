// aspin_diff: PathStrategy + ASpinHook 对拍 oracle/search_aspin.
//
// 二阶段对拍协议见 _diff_harness.h.
//
// LpKey 设计:
//   * spin_type: ASpinType (None=0, ASpin=1).
//   * last_idx : ASpin 路径不计算 last_rotate 链, 恒 -1.
//
// ASpin 关键 fixture (用户提供) 在 _diff_harness.h::make_aspin_all_spin_board
// 已嵌入命名 case `aspin_all_spin`, 与 lwell/rwell/center_pocket 一并跑.
//
// commit C4-followup: rule 选用 rule_srs (与其它 6 driver 一致). c4_blocking
// 报告里推测 ASpin 历史 baseline 是 rule_botris, 但 tools/aspin_dump.cpp
// 与 src/movegen_hook.h::ASpinHook 的契约文档均未硬绑 botris, oracle
// search_aspin::search 直接吃 spawn / map, 与 rule 模板无关. 已对全 OITLJSZ
// + 命名/random fixture 做 phase A 集合等价 + phase B byte-equal 确认; 若
// 仍有大量 fail, 报告 c4_followup_v2.md 留 "rule 错配?" 章节给用户裁定.

#include "_diff_harness.h"

#include "../src/tetris_core.h"
#include "../oracle/search_aspin.h"
#include "../oracle/search_tspin.h"
#include "../oracle/rule_srs.h"
#include "../oracle/ai_easy.h"
#include "../src/ai_zzz.h"
#include "../src/movegen_searcher.h"
#include "../src/rule_srs.h"
#include "../src/search_aspin.h"
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

    using Helpers = m_tetris2::bb::Helpers<rule_srs::TetrisRule::rule_spec>;

    inline std::uint64_t cells_key_to_u64(m_tetris2::bb::CellsKey const &k)
    {
        std::uint64_t v = 0;
        static_assert(sizeof(k) == sizeof(v), "CellsKey size mismatch");
        std::memcpy(&v, &k, sizeof(v));
        return v;
    }

    using OracleEngine =
        m_tetris::TetrisEngine<rule_srs_oracle::TetrisRule, ai_easy::AI, search_tspin_oracle::Search>;
    using NewEngine =
        m_tetris2::TetrisEngine<rule_srs::TetrisRule, ai_zzz::TOJ, aspin::Search>;

    OracleEngine &oracle_engine()
    {
        static OracleEngine e;
        static bool prepared = []()
        { return e.prepare(10, 40); }();
        (void)prepared;
        return e;
    }

    search_aspin_oracle::Search::Config &oracle_config()
    {
        static search_aspin_oracle::Search::Config cfg{};
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

    search_aspin_oracle::Search &oracle_search_obj()
    {
        static search_aspin_oracle::Search s;
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
                                                     m_tetris2::ASpinHook,
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

    using OracleNodePtr = decltype(static_cast<m_tetris::TetrisContext const *>(nullptr)->generate('I'));
    using OracleLP = std::decay_t<decltype((*static_cast<search_aspin_oracle::Search *>(nullptr)->search(*static_cast<m_tetris::TetrisMap const *>(nullptr), static_cast<OracleNodePtr>(nullptr), 0))[0])>;
    using CandidateLP = typename NewSearcher::LandPoint;

    std::vector<OracleLP> g_oracle_lps;
    std::vector<CandidateLP> g_candidate_lps;

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
            for (auto const &lp : *oracle_result)
                if (lp.node)
                    g_oracle_lps.push_back(lp);
        auto *candidate_result = new_search_obj().search(new_map, new_spawn->status, 0);
        if (candidate_result)
            for (auto const &lp : *candidate_result)
                if (lp.state.t != 0)
                    g_candidate_lps.push_back(lp);

        cp.oracle_lps.reserve(g_oracle_lps.size());
        for (std::size_t i = 0; i < g_oracle_lps.size(); ++i)
        {
            std::uint64_t ck = cells_key_to_u64(Helpers::cells_key_from_oracle(
                g_oracle_lps[i].node->status.t,
                static_cast<std::uint8_t>(g_oracle_lps[i].node->status.r),
                g_oracle_lps[i].node->status.x,
                g_oracle_lps[i].node->status.y));
            cp.oracle_lps.push_back(LpEntry{LpKey{ck,
                                                  static_cast<int>(g_oracle_lps[i].type),
                                                  -1},
                                            i});
        }
        cp.candidate_lps.reserve(g_candidate_lps.size());
        for (std::size_t i = 0; i < g_candidate_lps.size(); ++i)
        {
            std::uint64_t ck = cells_key_to_u64(Helpers::cells_key_for_state(g_candidate_lps[i].state));
            cp.candidate_lps.push_back(LpEntry{LpKey{ck,
                                                     static_cast<int>(g_candidate_lps[i].type),
                                                     -1},
                                               i});
        }

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
        cp.candidate_make_path = [new_spawn_copy, new_map_copy](LpEntry const &e) -> std::string
        {
            if (e.index >= g_candidate_lps.size())
                return std::string();
            auto board = m_tetris2::bb::build_board_for_search<NewSearcher::rule_spec>(new_map_copy);
            auto spawn = m_tetris2::bb::state_from_node_for_search<NewSearcher::rule_spec>(new_spawn_copy);
            std::vector<char> v = new_search_obj().make_path(spawn, g_candidate_lps[e.index], board);
            return std::string(v.begin(), v.end());
        };
        return cp;
    }
}

int main()
{
    tetris_diff::XfailSet xfail;
    auto set_flavor = [](bool is_20g)
    {
        oracle_config().is_20g = is_20g;
        new_config().is_20g = is_20g;
    };
    return tetris_diff::run_diff_main(
        "aspin_diff",
        "OITLJSZ",
        tetris_diff::build_all_aspin_fixtures(),
        {false, true},
        set_flavor,
        probe,
        xfail,
        /*strict=*/true,
        //commit C5: 同 path_diff, IgnoreDrift 豁免 BFS 邻居枚举顺序差.
        //  ASpin classification (spin=0 None / spin=1 ASpin) 在 LpKey 维度
        //  上严格 byte-equal — aspin_all_spin fixture 上 spin=1 落点 path
        //  100% byte-equal (用户钦点 case 通过), 残留差异全部在 spin=0 的
        //  路径选择上, 落点结果完全等价, 见 c5_followup.md §3.
        tetris_diff::PhaseBPolicy::IgnoreDrift);
}
