// search_aspin.h — 双阶段头.
//
// Phase 1: search_aspin::Search POJO (enum ASpinType / struct Config /
//          struct TetrisNodeWithASpinType). 由 movegen_hook.h 在装配
//          m_tetris2::ASpinHook 时消费 (LandPoint / Config alias). 不引入
//          search_path.h 这条重链, 让 movegen_hook.h 能先于 PathStrategy
//          编完.
//
// Phase 2: 顶层 namespace aspin (DefaultConfig / SearchWith / Search) — 与
//          search_tspin / search_cautious / search_simple / ... 同形, 让
//          TetrisEngine<Rule, AI, aspin::Search> 单参装配位板后端. 这部分
//          需要 path::SearchWith / path::detail::merge / m_tetris2::
//          PathStrategy / m_tetris2::ASpinHook 全部就绪, 因此被 sentinel
//          (TETRIS_AI_RUNNER_SEARCH_PATH_TAGS_READY_) 守护; sentinel 由
//          search_path.h 末尾设置.
//
// 协调流程:
//   - 入口 = search_aspin.h: Phase 1 直出, 然后反向 #include "search_path.h"
//     把 sentinel 拉起来; search_path.h 末尾会重新 include 本头让 Phase 2
//     展开. 回到原入口时 Phase 2 已经 emit, 守护宏让二次 emit 无副作用.
//   - 入口 = search_path.h: search_path.h 内部经 movegen_hook.h 把本头先
//     拉一次 (Phase 1 emit, Phase 2 因 sentinel 未就绪而跳过); search_path
//     .h 末尾再次 include 本头, 此时 sentinel 已就绪, Phase 2 emit.
//
// 旧 search_aspin::Search 的 init / search / make_path 成员函数 (老
// search_aspin.cpp pImpl 外观) 已经废弃: 调用方应改走 aspin::Search.
#ifndef TETRIS_AI_RUNNER_SEARCH_ASPIN_PHASE1_
#define TETRIS_AI_RUNNER_SEARCH_ASPIN_PHASE1_

#include "tetris_core.h"
#include "bb_state.h"

#include <cstddef>
#include <cstring>
#include <vector>

namespace search_aspin
{
    struct Search
    {
        enum ASpinType
        {
            None,
            ASpin
        };
        struct Config
        {
            bool allow_rotate_move = false;
            bool allow_180 = true;
            bool allow_d = true;
            bool allow_D = true;
            bool allow_LR = true;
            bool is_20g = false;
        };
        struct TetrisNodeWithASpinType
        {
            TetrisNodeWithASpinType()
            {
                std::memset(this, 0, sizeof(*this));
            }
            m_tetris2::bb::BBState state{};  // 落点位板坐标 (t/r/xb/yb)
            ASpinType type;

            bool operator==(TetrisNodeWithASpinType const &other) const
            {
                return state.t == other.state.t && state.r == other.state.r &&
                       state.xb == other.state.xb && state.yb == other.state.yb &&
                       type == other.type;
            }
            bool operator==(std::nullptr_t) const
            {
                return state.t == 0;
            }
            bool operator!=(std::nullptr_t) const
            {
                return state.t != 0;
            }
        };
    };
} // namespace search_aspin

#endif // TETRIS_AI_RUNNER_SEARCH_ASPIN_PHASE1_

//=== Phase 2 协调: 仅当 sentinel 就绪时展开顶层 aspin namespace ============
//    sentinel TETRIS_AI_RUNNER_SEARCH_PATH_TAGS_READY_ 由 search_path.h 末尾设
//    置. 调用方流程:
//      - movegen_hook.h include 本头时 sentinel 未就绪, Phase 2 跳过 (只 emit
//        Phase 1 POJO);
//      - search_path.h 在自身展开完毕后再 #include "search_aspin.h", 此时
//        sentinel 就绪, Phase 2 emit 顶层 aspin namespace.
//    需要 aspin::Search 的 TU 必须 #include "search_path.h" (它会再带回本头),
//    单独 #include "search_aspin.h" 仅拿到 search_aspin::Search POJO.
#if !defined(TETRIS_AI_RUNNER_SEARCH_ASPIN_PHASE2_) && defined(TETRIS_AI_RUNNER_SEARCH_PATH_TAGS_READY_)
#define TETRIS_AI_RUNNER_SEARCH_ASPIN_PHASE2_

namespace aspin
{
    struct DefaultConfig
    {
        using HookType = m_tetris2::ASpinHook;
        //字段值与 src/search_aspin.h Config / oracle/search_aspin.h
        //  search_aspin_oracle::Search::Config 默认值同步 (运行时实际生效的
        //  仍是 SearchConfigHolder 里实例化的 ASpinHook::Config).
        static constexpr bool allow_rotate_move = false;
        static constexpr bool allow_180 = true;
        static constexpr bool allow_d = true;
        static constexpr bool allow_D = true;
        static constexpr bool allow_LR = true;
        static constexpr bool is_20g = false;
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
} // namespace aspin

#endif
