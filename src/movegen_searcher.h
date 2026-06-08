#ifndef TETRIS_AI_RUNNER_MOVEGEN_SEARCHER_H_
#define TETRIS_AI_RUNNER_MOVEGEN_SEARCHER_H_

//==========================================================================
// Searcher<Strategy, SpinHook, RuleSpec>: Strategy 化重构里的运行时包装.
//
// 设计目标:
//   - 取代旧 MoveGenSearch<RuleSpec, Hook> 的"实例". MoveGenSearch 把
//     Context 状态 + search 算法塞在同一个类里; 新模型把状态 (Context)
//     与算法 (Strategy) 拆开, Searcher 仅做"持 ctx + inline 转发".
//   - 运行期零代价: 成员只 1 个 ctx_ 实体 (空基类优化让未声明的 mixin
//     不占字节), 所有方法都是 inline 模板转发到
//     `Strategy<SpinHook, RuleSpec>::xxx(ctx_, ...)`.
//   - 上层 (TetrisEngine 类、ai_*.cpp 的 typedef 链) 只看 Searcher;
//     选什么 search 就用什么 Strategy 模板特化, 不再依赖
//     Hook::is_simple_search / Hook::is_simulate_search 这类 if constexpr
//     bool flag.
//
// 命名空间: m_tetris2::movegen.
//==========================================================================

#include "movegen_context.h"
#include "movegen_hook.h"
#include "movegen_strategy.h"
#include "tetris_core.h"

#include <cstddef>
#include <utility>

namespace m_tetris2
{
    namespace movegen
    {
        //--------------------------------------------------------------------
        // Searcher<Strategy, SpinHook, RuleSpec, Policy>
        //
        //   - Strategy 是 template<class SpinHook, class RuleSpec, class Policy>
        //     的纯静态类, 满足 movegen_strategy.h 的契约 (暴露 Context 别名 +
        //     static search(Context&, ...) + static make_path(Context&, ...)).
        //   - Policy: 编译期 ActivePiecePolicy tag, 决定哪些 piece 激活 spin
        //     路径. 默认 void 表示不干预 Hook 的历史行为 (旧代码零破坏).
        //   - Searcher 自身只持一个 ctx_ 实体. 所有 user-facing 方法都
        //     forward 到 strategy_t::xxx(ctx_, args...). forward 走 perfect
        //     forwarding + auto -> decltype(auto), 不限定签名.
        //   - 暴露 Config / LandPoint 别名: TetrisEngine 通过
        //     TetrisHasConfig<TetrisSearch> 探针决定是否绑 search_config_,
        //     该探针只查 `Config` 嵌套类型存在与否; element_traits 通过
        //     search() 返回类型推 LandPoint, 这里仍以单点 alias 增强可读.
        //--------------------------------------------------------------------
        template<template<class, class, class> class Strategy,
                 class SpinHook,
                 class RuleSpec,
                 class Policy = void>
        class Searcher
        {
        private:
            //commit 3: strategy_t 是内部别名, 第三方调用方应通过下面的
            //  Context / Config / LandPoint 访问. 把 strategy_t 收敛到 private
            //  避免下游用 `Searcher::strategy_t` 反向耦合具体实现.
            using strategy_t = Strategy<SpinHook, RuleSpec, Policy>;

        public:
            using Context = typename strategy_t::Context;
            using Config = typename strategy_t::Config;
            using LandPoint = typename strategy_t::LandPoint;
            using rule_spec = RuleSpec;

            //=== 转发: init ====================================================
            //  TetrisEngine 在 prepare 时通过 TetrisCallInit 探针调
            //  search.init(local_context->search_config()).
            //  search_config() 在 SearchConfigHolder 里实例化为 Config*,
            //  与 strategy_t::init 第二参数对齐.
            template<class... Args>
            inline auto init(Args &&...args)
                -> decltype(strategy_t::init(std::declval<Context &>(),
                                             std::forward<Args>(args)...))
            {
                return strategy_t::init(ctx_, std::forward<Args>(args)...);
            }

            //=== 转发: search ==================================================
            // spawn 直接从 TetrisBlockStatus 构建 BBState，走纯位板路径，
            // 不查 oracle 指针图。
            inline auto search(TetrisMap const &map, TetrisBlockStatus const &status, std::size_t depth)
            {
                bb::BBState spawn = bb::Helpers<RuleSpec>::state_from_status(
                    status.t,
                    static_cast<std::uint8_t>(status.r),
                    status.x,
                    status.y);
                return strategy_t::search(ctx_, map, spawn, depth);
            }

            template<class... Args>
            inline auto search(Args &&...args)
                -> decltype(strategy_t::search(std::declval<Context &>(),
                                               std::forward<Args>(args)...))
            {
                return strategy_t::search(ctx_, std::forward<Args>(args)...);
            }

            //=== 转发: search_eval ============================================
            // Phase 2 推式接口. EvalCallback 须满足:
            //   template<char T, uint8_t R> void operator()(LandPoint const &)
            // 转发方式与 search() 相同 (ctx_ 隐含注入).
            template<class EvalCallback>
            inline void search_eval(Map<RuleSpec::width, RuleSpec::height> const &board,
                                    bb::BBState const &spawn,
                                    std::size_t depth, EvalCallback &on_land)
            {
                strategy_t::template search_eval<EvalCallback>(ctx_, board, spawn, depth, on_land);
            }

            //=== 转发: make_path ===============================================
            template<class S = strategy_t>
            inline auto make_path(bb::BBState const &spawn,
                                  LandPoint const &land_point,
                                  Map<RuleSpec::width, RuleSpec::height> const &board)
                -> decltype(S::make_path(std::declval<Context &>(),
                                         spawn, land_point, board))
            {
                return S::make_path(ctx_, spawn, land_point, board);
            }

        protected:
            //=== 内部 ctx 直接读访问 ==========================================
            //commit 3: 只对派生类暴露 (中间层 / 适配器子类可能需要), 不再
            //  直接 public — 顶层 API 不暴露 ctx_ 实体.
            inline Context &context() noexcept
            {
                return ctx_;
            }

            inline Context const &context() const noexcept
            {
                return ctx_;
            }

        private:
            Context ctx_{};
        };
    } // namespace movegen
} // namespace m_tetris2

#endif // TETRIS_AI_RUNNER_MOVEGEN_SEARCHER_H_
