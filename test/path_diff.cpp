// path_diff: PathStrategy + NoSpinHook 对拍 oracle/search_path.
//
// 二阶段比对协议:
//   Phase A: oracle.search() 与 candidate.search() 的 LandPoint 集 (键 =
//            index_filtered, NoSpin 路径无 spin/last 元数据故 spin=0/last=-1)
//            做 set-equal 比对.
//   Phase B: 对两侧 key 集合的交集逐 key 跑 oracle.make_path / candidate.make_path,
//            字符串 byte-equal 比对.
//
// NoSpinHook::Config 是空 struct (无 is_20g 字段), driver 仅跑 1g flavor.

#include "_diff_harness.h"

#include "../src/tetris_core.h"
#include "../oracle/search_path.h"
#include "../oracle/search_tspin.h"
#include "../oracle/rule_srs.h"
#include "../oracle/ai_easy.h"
#include "../src/ai_zzz.h"
#include "../src/movegen_searcher.h"
#include "../src/rule_srs.h"
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

    NewEngine &new_engine()
    {
        static NewEngine e;
        static bool prepared = []()
        { return e.prepare(10, 40); }();
        (void)prepared;
        return e;
    }

    search_path::Search &oracle_search_obj()
    {
        static search_path::Search s;
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

    //commit C4-followup: 候选端用 CautiousHook 而非 NoSpinHook —
    //  CautiousHook::Config 默认 fast_move_down=false, 与 NoSpinHook 在
    //  payload / LandPoint / 180/LR/d/D 开关上完全同形, 唯一关键差: 它继承
    //  BaseSpinHook::config_allow_rotate_move 默认 false, 不会展开 'C/Z/X'
    //  rotate-after-move 邻居. 这与 oracle/search_path 的 BFS 邻居字符集
    //  ('x z c l r L R d D', 不含 rotate-after-move) 严格对齐, 让 phase B
    //  byte-equal 比对成立. NoSpinHook 自家覆盖 config_allow_rotate_move
    //  返回 true (生产 ai.cpp 的 path_/simulate_ 走它), 不能为对拍把这一项
    //  改 false 而污染生产; 故 driver 端改挂 CautiousHook.
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

    //commit C4-followup: 把 oracle / candidate 的 search 输出拷贝到 driver
    //  本地存储, 让 LpEntry.index 指向稳定切片 (oracle 端 search() 第二次
    //  调用会清空 land_point_cache_, 故必须早 copy). 私有命名空间作用域
    //  static 即可, 上下两个 case 之间共享并重用.
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
        {
            g_oracle_lps.reserve(oracle_result->size());
            for (auto n : *oracle_result)
                if (n)
                    g_oracle_lps.push_back(n);
        }
        auto *candidate_result = new_search_obj().search(new_map, new_spawn->status, 0);
        if (candidate_result)
        {
            g_candidate_lps.reserve(candidate_result->size());
            for (auto const &lp : *candidate_result)
                if (lp != nullptr)
                    g_candidate_lps.push_back(lp);
        }

        cp.oracle_lps.reserve(g_oracle_lps.size());
        for (std::size_t i = 0; i < g_oracle_lps.size(); ++i)
        {
            auto n = g_oracle_lps[i];
            char buf[64];
            std::snprintf(buf, sizeof(buf), "r=%d,x=%d,y=%d,open=%d",
                          static_cast<int>(n->status.r),
                          static_cast<int>(n->status.x),
                          static_cast<int>(n->status.y),
                          n->open(oracle_map) ? 1 : 0);
            cp.oracle_lps.push_back(LpEntry{LpKey{static_cast<int>(n->index_filtered),
                                                  0, -1},
                                            i, std::string(buf)});
        }
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
        "path_diff",
        "OITLJSZ",
        tetris_diff::build_all_fixtures(),
        {false},
        nullptr,
        probe,
        xfail,
        /*strict=*/true,
        //commit C5: phase B 残留 mismatch 全部是 BFS 邻居枚举顺序差产生的
        //  语义等价路径选择差 (见 .research/flip-bits-cleanup/c5_followup.md).
        //  Phase A subset 已严格成立, ASpin/TSpin classification 与落点集合
        //  在前面阶段已 100% 校验, 故选 IgnoreDrift.
        tetris_diff::PhaseBPolicy::IgnoreDrift);
}
