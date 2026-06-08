// search_tspin.h — 双阶段头.
//
// Phase 1: search_tspin::Search POJO (enum TSpinType / struct Config /
//          struct TetrisNodeWithTSpinType). 由 movegen_hook.h 在装配
//          m_tetris2::TSpinHook / CautiousHook 时消费 (LandPoint / Config
//          alias). 不引入 search_path.h.
//          master-graph BFS 实现已归档到 oracle/search_tspin.{h,cpp}
//          (namespace search_tspin_oracle), 仅供 oracle_diff /
//          tag_landpoint_collision_dump 等对照工具使用; 生产路径 (位板侧)
//          全走顶层 namespace tspin::Search.
//
// Phase 2: 顶层 namespace tspin (DefaultConfig / SearchWith / Search) — 与
//          aspin / cautious / simple / ... 同形, 让
//          TetrisEngine<Rule, AI, tspin::Search> 单参装配位板后端. 这部分
//          需要 path::SearchWith / path::detail::merge / m_tetris2::
//          PathStrategy / m_tetris2::TSpinHook 全部就绪, 因此被 sentinel
//          (TETRIS_AI_RUNNER_SEARCH_PATH_TAGS_READY_) 守护; sentinel 由
//          search_path.h 末尾设置. 协调流程见 search_aspin.h 同段注释.
#ifndef TETRIS_AI_RUNNER_SEARCH_TSPIN_PHASE1_
#define TETRIS_AI_RUNNER_SEARCH_TSPIN_PHASE1_

#include "tetris_core.h"
#include "bb_state.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace search_tspin
{
    // POJO-only facade. master-graph BFS (init/search/make_path 三方法及内部
    // helper) 已迁出到 oracle/search_tspin.{h,cpp}. 保留 class Search 作为
    // 类型 namespace, 让 ai_zzz.h / ai_misaka.h / ai_tag.h / movegen_hook.h
    // 等依赖方继续以 search_tspin::Search::TSpinType /
    // search_tspin::Search::TetrisNodeWithTSpinType / search_tspin::Search::
    // Config 的旧路径解析.
    class Search
    {
    public:
        enum TSpinType
        {
            None,
            TSpin,
            TSpinMini
        };
        struct Config
        {
            bool allow_rotate_move = false;
            bool allow_180 = true;
            bool allow_d = true;
            bool allow_D = true;
            bool allow_LR = true;
            bool is_20g = false;
            bool last_rotate = false;
        };
        struct TetrisNodeWithTSpinType
        {
            TetrisNodeWithTSpinType()
            {
                std::memset(this, 0, sizeof(*this));
            }
            m_tetris2::bb::BBState state{};  // 落点位板坐标 (t/r/xb/yb)
            m_tetris2::bb::BBState last{};   // last-rotate 前驱位置（无前驱时 t==0）
            TSpinType type;
            union
            {
                struct
                {
                    bool is_check;
                    bool is_last_rotate;
                    bool is_ready;
                    bool is_mini_ready;
                };
                uint32_t flags;
            };
            // 兼容接口：以 state.t==0 作为"无效/nullptr"哨兵
            bool operator==(std::nullptr_t) const
            {
                return state.t == 0;
            }
            bool operator!=(std::nullptr_t) const
            {
                return state.t != 0;
            }
            bool operator==(TetrisNodeWithTSpinType const &other) const
            {
                return state.t == other.state.t && state.r == other.state.r &&
                       state.xb == other.state.xb && state.yb == other.state.yb &&
                       type == other.type && flags == other.flags;
            }
        };
    };
} // namespace search_tspin

#endif // TETRIS_AI_RUNNER_SEARCH_TSPIN_PHASE1_

//=== Phase 2 协调: 仅当 sentinel 就绪时展开顶层 tspin namespace ============
//    sentinel TETRIS_AI_RUNNER_SEARCH_PATH_TAGS_READY_ 由 search_path.h 末尾设
//    置. 流程同 search_aspin.h Phase 2 注释.
#if !defined(TETRIS_AI_RUNNER_SEARCH_TSPIN_PHASE2_) && defined(TETRIS_AI_RUNNER_SEARCH_PATH_TAGS_READY_)
#define TETRIS_AI_RUNNER_SEARCH_TSPIN_PHASE2_

namespace tspin
{
    struct DefaultConfig
    {
        using HookType = m_tetris2::TSpinHook;
        //字段名与 oracle/search_tspin Config (= src/search_tspin.h
        //  search_tspin::Search::Config) 同步; 默认值同 master-graph BFS
        //  原版 (allow_rotate_move=false, last_rotate=false, allow_180=true,
        //  允许 d/D/LR, 非 20g). 字段仅作 trait 形态装饰; 运行时实际生效的
        //  仍是 SearchConfigHolder 里实例化的 TSpinHook::Config 实例.
        static constexpr bool allow_rotate_move = false;
        static constexpr bool allow_180 = true;
        static constexpr bool allow_d = true;
        static constexpr bool allow_D = true;
        static constexpr bool allow_LR = true;
        static constexpr bool is_20g = false;
        static constexpr bool last_rotate = false;
    };

    template<class Config = DefaultConfig>
    struct SearchWith
    {
        template<class RuleType>
        using type = m_tetris2::movegen::Searcher<
            m_tetris2::PathStrategy,
            typename path::detail::merge<DefaultConfig, Config>::HookType,
            typename RuleType::rule_spec,
            typename path::detail::merge<DefaultConfig, Config>::PolicyType>;

        template<class NewPolicy>
        struct PolicyOverrideConfig : Config { using PolicyType = NewPolicy; };

        template<class NewPolicy>
        using rebind_policy = SearchWith<PolicyOverrideConfig<NewPolicy>>;
    };

    using Search = SearchWith<>;
} // namespace tspin

#endif
