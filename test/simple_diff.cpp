// simple_diff: SimpleStrategy + NoSpinHook vs PathStrategy + NoSpinHook 互拍.
//
// SimpleStrategy 是"裸 drop + 横扫" 的简化 search, 与 PathStrategy 在能否
// 触发 wall-kick / 大下沉等场景上有理论差异 (这是设计上允许的). 因此本
// driver 走 strict=false: 始终 exit 0, 仅 stderr 输出 stats / 差异, 让人
// 工 review 增量.
//
// 二阶段对拍协议见 _diff_harness.h. 本 driver 仅跑 phase A (集合); phase B
// 不绑 make_path 闭包 (两个 strategy 的 path 字符集本就不一样, 字节比对
// 不会通过, 也没意义).
//
// commit C4-followup: 两侧 hook 均使用 NoSpinHook, 与历史 simple_diff 行为
// 等价 (PathStrategy + NoSpin 是生产 ai 用的搜索, SimpleStrategy 只是子集
// 验证).

#include "_diff_harness.h"

#include "../src/tetris_core.h"
#include "../oracle/search_tspin.h"
#include "../src/ai_zzz.h"
#include "../src/bb_state.h"
#include "../src/movegen_searcher.h"
#include "../src/rule_srs.h"
#include "../src/search_path.h"
#include "../src/search_simple.h"
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

    using NewEngine =
        m_tetris2::TetrisEngine<rule_srs::TetrisRule, ai_zzz::TOJ, path::Search>;

    NewEngine &engine()
    {
        static NewEngine e;
        static bool prepared = []()
        { return e.prepare(10, 40); }();
        (void)prepared;
        return e;
    }

    using PathSearcher = m_tetris2::movegen::Searcher<m_tetris2::PathStrategy,
                                                      m_tetris2::NoSpinHook,
                                                      rule_srs::TetrisRule::rule_spec>;
    using SimpleSearcher = m_tetris2::movegen::Searcher<m_tetris2::SimpleStrategy,
                                                        m_tetris2::NoSpinHook,
                                                        rule_srs::TetrisRule::rule_spec>;

    PathSearcher::Config &path_config()
    {
        static PathSearcher::Config cfg{};
        return cfg;
    }

    PathSearcher &path_search_obj()
    {
        static PathSearcher s;
        static bool inited = []()
        {
            s.init(&path_config());
            return true;
        }();
        (void)inited;
        return s;
    }

    SimpleSearcher::Config &simple_config()
    {
        static SimpleSearcher::Config cfg{};
        return cfg;
    }

    SimpleSearcher &simple_search_obj()
    {
        static SimpleSearcher s;
        static bool inited = []()
        {
            s.init(&simple_config());
            return true;
        }();
        (void)inited;
        return s;
    }

    tetris_diff::CaseProbe probe(char piece, NewMap const &board,
                                 std::string const & /*board_name*/, bool /*is_20g*/)
    {
        tetris_diff::CaseProbe cp;
        m_tetris2::TetrisContext const *ctx = engine().context().get();
        if (!ctx)
            return cp;
        m_tetris2::TetrisMap map;
        tetris_diff::copy_to_oracle_map(board, map);
        auto spawn = ctx->generate(piece);
        if (!spawn || !spawn->check(map))
            return cp;

        // "oracle" 在本 driver 里 = PathStrategy (功能更全的参照),
        // "candidate" = SimpleStrategy (子集).
        auto *oracle_result = path_search_obj().search(map, spawn->status, 0);
        if (oracle_result)
        {
            std::size_t i = 0;
            for (auto const &lp : *oracle_result)
            {
                std::uint64_t ck = cells_key_to_u64(Helpers::cells_key_for_state(lp.state));
                cp.oracle_lps.push_back(LpEntry{LpKey{ck, 0, -1}, i});
                ++i;
            }
        }
        auto *candidate_result = simple_search_obj().search(map, spawn->status, 0);
        if (candidate_result)
        {
            std::size_t i = 0;
            for (auto const &lp : *candidate_result)
            {
                std::uint64_t ck = cells_key_to_u64(Helpers::cells_key_for_state(lp.state));
                cp.candidate_lps.push_back(LpEntry{LpKey{ck, 0, -1}, i});
                ++i;
            }
        }
        // make_path 闭包不绑: 两 strategy 的 path 字符集本就不同, 单 phase A 即可.
        return cp;
    }
}

int main()
{
    tetris_diff::XfailSet xfail;
    return tetris_diff::run_diff_main(
        "simple_diff",
        "OITLJSZ",
        tetris_diff::build_all_fixtures(),
        {false},
        nullptr,
        probe,
        xfail,
        /*strict=*/false);
}
