#ifndef TETRIS_AI_RUNNER_TETRIS_ENGINE2_H_
#define TETRIS_AI_RUNNER_TETRIS_ENGINE2_H_

//==========================================================================
// TetrisEngine2<TetrisRule, TetrisAI, TetrisSearchTag>
//
//   TetrisEngine 的编译期 Policy 自动注入版本. 调用面与 TetrisEngine 完全
//   相同, 区别仅在于第三参数 TetrisSearchTag 会在实例化时被透明升级:
//
//     1. 通过 m_tetris2::DeduceSpinPolicy<TetrisAI> 推导 AI 对应的编译期
//        PolicyType (ActiveTOnlyPolicy / ActiveAllPolicy / ActiveNonePolicy).
//     2. 若 TetrisSearchTag 暴露 template<class P> using rebind_policy = ...,
//        则生成 TetrisSearchTag::rebind_policy<PolicyType> 作为实际 SearchTag,
//        让 SpinHook::active_for_piece<T, Policy> 双参版本在 Strategy 内生效.
//     3. 若 TetrisSearchTag 不支持 rebind_policy (旧式已实例化 Searcher / 自
//        定义 tag), 原样穿透 — 向后兼容零破坏.
//
//   因此对于标准 ai_zzz::tspin::AI + tspin::Search 组合:
//     TetrisEngine2<Rule, ai_zzz::tspin::AI, tspin::Search>
//   等价于:
//     TetrisEngine<Rule, ai_zzz::tspin::AI,
//                  tspin::SearchWith<>::rebind_policy<ActiveTOnlyPolicy>>
//   而历史调用面 TetrisEngine2<Rule, LegacyAI, LegacySearch> 无缝降级.
//
//   此文件必须在 movegen_hook.h include 之后才能使用:
//     movegen_hook.h 定义了 m_tetris2::DeduceSpinPolicy<AI>,
//     而 alias template 在定义点就要求该符号可见 (非依赖名, 无两段式查找).
//   通常经由 search_*.h 传递已满足此前置条件.
//==========================================================================

#include "tetris_core.h"
#include "movegen_hook.h"

namespace m_tetris2
{
    template<class TetrisRule, class TetrisAI, class TetrisSearchTag>
    using TetrisEngine2 = TetrisEngine<
        TetrisRule,
        TetrisAI,
        detail::rebind_search_policy_t<TetrisSearchTag, DeduceSpinPolicy<TetrisAI>>
    >;

    template<class TetrisRule, class TetrisAI, class TetrisSearchTag>
    using TetrisThreadEngine2 = TetrisThreadEngine<
        TetrisRule,
        TetrisAI,
        detail::rebind_search_policy_t<TetrisSearchTag, DeduceSpinPolicy<TetrisAI>>
    >;

} // namespace m_tetris2

#endif // TETRIS_AI_RUNNER_TETRIS_ENGINE2_H_
