#ifndef TETRIS_AI_RUNNER_SEARCH_CAUTIOUS_H_
#define TETRIS_AI_RUNNER_SEARCH_CAUTIOUS_H_

//==========================================================================
// 顶层 namespace cautious — 与 aspin / tspin / simple / simulate / tag /
// path 同形, 让 TetrisEngine<Rule, AI, cautious::Search> 单参装配位板后端.
// 后端走 PathStrategy + m_tetris2::CautiousHook (定义在 movegen_hook.h).
//
// 与 search_aspin.h / search_tspin.h 不同, CautiousHook 没有自家专用的
// LandPoint POJO (它用 m_tetris2::BBLandPoint，持有 State，无 TetrisNode
// 指针，与 NoSpinHook 共形 — 见 movegen_hook.h),
// 所以本头不必走双阶段守护; 直接 #include "search_path.h" 把 path 装好,
// 再 emit cautious 命名空间.
//==========================================================================

#include "search_path.h"

namespace cautious
{
    struct DefaultConfig
    {
        using HookType = m_tetris2::CautiousHook;
        // 字段语义与 CautiousHook::Config 保持一致。
        static constexpr bool fast_move_down = false;
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
} // namespace cautious

#endif // TETRIS_AI_RUNNER_SEARCH_CAUTIOUS_H_
