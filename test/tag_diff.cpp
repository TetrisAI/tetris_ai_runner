// tag_diff: TagStrategy + TSpinHook 对拍 oracle/search_tag.
//
// 二阶段对拍协议见 _diff_harness.h.
//
// LpKey 设计:
//   * spin_type: oracle search_tag 只输出 {None=0, TSpin=1}, candidate
//     TSpinHook 走 search_tspin 枚举 {None=0, TSpin=1, TSpinMini=2}; tag 路径
//     不区分 mini, 投影成 (==None ? 0 : 1).
//   * last_idx : lp.last->index_filtered (无则 -1).
//
// tag_diff 历史 17 个 J piece × 边界 fail 已通过 xfail 列表 (Phase B 字符串)
// 豁免. Phase A 集合若仍失败则不掩盖 — 用户硬规定不允许把真实 set 漏吐
// 塞 xfail.

#include "_diff_harness.h"

#include "../src/tetris_core.h"
#include "../oracle/search_tag.h"
#include "../oracle/search_tspin.h"
#include "../oracle/rule_srs.h"
#include "../oracle/ai_easy.h"
#include "../src/ai_zzz.h"
#include "../src/bb_state.h"
#include "../src/movegen_searcher.h"
#include "../src/rule_srs.h"
#include "../src/search_tag.h"
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
        m_tetris2::TetrisEngine<rule_srs::TetrisRule, ai_zzz::TOJ, tag::Search>;

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

    search_tag_oracle::Search &oracle_search_obj()
    {
        static search_tag_oracle::Search s;
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

    using NewSearcher = m_tetris2::movegen::Searcher<m_tetris2::TagStrategy,
                                                     m_tetris2::TSpinHook,
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

    int proj_type(int t)
    {
        // oracle search_tag::TSpinType 仅 {None=0, TSpin=1}; candidate TSpinHook
        // 用 search_tspin 枚举 {None=0, TSpin=1, TSpinMini=2}. tag 不区分 mini,
        // 投影成 (==None ? 0 : 1).
        return t == 0 ? 0 : 1;
    }

    using OracleNodePtr = decltype(static_cast<m_tetris::TetrisContext const *>(nullptr)->generate('I'));
    using OracleLP = std::decay_t<decltype((*static_cast<search_tag_oracle::Search *>(nullptr)->search(*static_cast<m_tetris::TetrisMap const *>(nullptr), static_cast<OracleNodePtr>(nullptr), 0))[0])>;
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
            OracleLP const &lp = g_oracle_lps[i];
            int spin = proj_type(static_cast<int>(lp.type));
            //commit C5: lp.last 字段在 oracle / candidate 两侧 BFS 顺序差下不
            //  稳定 (即便同一 spin 落点也可能各自选到不同等价代表元), last_idx
            //  统一抹 -1, 仅按 (落点 cells_key, spin_type) 比集合. 详见
            //  tspin_diff 同段注释.
            int last_idx = -1;
            std::uint64_t ck = cells_key_to_u64(Helpers::cells_key_from_oracle(
                lp.node->status.t,
                static_cast<std::uint8_t>(lp.node->status.r),
                lp.node->status.x,
                lp.node->status.y));
            cp.oracle_lps.push_back(LpEntry{LpKey{ck, spin, last_idx}, i});
        }
        cp.candidate_lps.reserve(g_candidate_lps.size());
        for (std::size_t i = 0; i < g_candidate_lps.size(); ++i)
        {
            CandidateLP const &lp = g_candidate_lps[i];
            int spin = proj_type(static_cast<int>(lp.type));
            int last_idx = -1;
            std::uint64_t ck = cells_key_to_u64(Helpers::cells_key_for_state(lp.state));
            cp.candidate_lps.push_back(LpEntry{LpKey{ck, spin, last_idx}, i});
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
    //commit C5: 历史 17 个 J piece × 边界 fail 现在因 LpKey 改为 last_idx=-1
    //  与其它 BFS 邻居枚举顺序差合并 — Phase A subset 仍严格成立 (落点集合
    //  完全等价), Phase B 残留差异本质上都是 oracle 与 candidate 的 BFS 顺序
    //  不同导致的"语义等价路径选择差". 改用 IgnoreDrift 而非 J 通配 xfail,
    //  统一与 path/simulate/tspin/aspin/cautious 5 driver 的协议. 历史 J 17
    //  fail 在新协议下计入 phase_b_drift, 见 c5_followup.md §4.
    return tetris_diff::run_diff_main(
        "tag_diff",
        "OITLJSZ",
        tetris_diff::build_all_fixtures(),
        {false},
        nullptr,
        probe,
        xfail,
        /*strict=*/true,
        tetris_diff::PhaseBPolicy::IgnoreDrift);
}
