#ifndef TETRIS_AI_RUNNER_MOVEGEN_STRATEGY_H_
#define TETRIS_AI_RUNNER_MOVEGEN_STRATEGY_H_

//==========================================================================
// MoveGen Strategy 契约 + 跨 strategy 共享小工具.
//
// 概念:
// "Strategy" 是一组 search 行为的纯静态 namespace-class. 每种 search
//   (path / simulate / simple / tag / ...) 都对应一个 strategy, 由
//   Searcher<Strategy, SpinHook, RuleSpec, Policy> 模板包装出运行时实例.
//
// 契约 (每个具体 strategy 必须满足):
//
//   template<class SpinHook, class RuleSpec, class Policy = void>
//   struct ConcreteStrategy
//   {
//       // 1. Strategy 自带 Context 类型别名. Context 由
//       //    m_tetris2::movegen::MoveGenContext<RuleSpec, Mixins...> 装配,
//       //    每个 strategy 只声明它需要的 mixin (见 movegen_context.h).
//       using Context = m_tetris2::movegen::MoveGenContext<
//           RuleSpec /*, ...mixins...*/>;
//
//       // 2. Strategy 持有自己的 LandPoint / Config 类型路由 (从 SpinHook
//       //    取 trait, 与 movegen_hook.h 的 Hook::LandPoint / Hook::Config
//       //    保持一致).
//       using LandPoint = typename SpinHook::LandPoint;
//       using Config    = typename SpinHook::Config;
//
//       // 3. init / search / make_path 全部为 static 方法, 第一个形参恒为
//       //    Context&. Searcher 负责持有 ctx 实例并 inline 转发. 形如:
//       //
//       //        static std::vector<LandPoint> const*
//       //        search(Context& ctx, TetrisMap const& map,
//       //
//       //        static std::vector<char>
//       //                  LandPoint const& land_point,
//       //                  TetrisMap const& map);
//
//       // 4. 行为完全由 SpinHook 与 RuleSpec 决定; 不再使用旧
//       //    Hook::is_simple_search / Hook::is_simulate_search 这类
//       //    if constexpr 路由. 上层装配代码在选 strategy 类型时就已经
//       //    决定 search 的语义, 框架运行期不再分支.
//   };
//
// 共享小工具 (本头 detail::common::*):
//   跨多个 strategy 复用的小型组件. 当前:
//   - MakePath1gDedup — 1g make_path 主循环喂 bb::run_bb_bfs 的 dedup policy,
//     path / simulate 共用.
//   - policy_or<C, Fallback> — 从 Config 提取 PolicyType, 无则回退 void.
//     供各 namespace (path / simulate / tag / ...) 的 SearchWith 透传
//     ActivePiecePolicy 到 Searcher 第四参数.
//==========================================================================

#include "bb_bfs_engine.h"
#include "bb_state.h"
#include "movegen_context.h"
#include "movegen_hook.h"
#include "tetris_core.h"

#include <array>
#include <cstdint>
#include <type_traits>

namespace m_tetris2
{
    namespace detail
    {
        //----------------------------------------------------------------------
        // policy_or<Config, Fallback>: 从 Config class 提取 PolicyType alias.
        // 若 Config 没有 PolicyType, 回退到 Fallback (默认 void).
        //
        // 用途: SearchWith<Config>::type 把该 alias 传给 Searcher 的第四参数,
        //   Searcher 再透传给 Strategy<SpinHook, RuleSpec, Policy>. 编译期零
        //   开销, 整条链路在模板实例化时固化.
        //----------------------------------------------------------------------
        template<class C, class Fallback = void, class = void>
        struct policy_or
        {
            using type = Fallback;
        };
        template<class C, class Fallback>
        struct policy_or<C, Fallback, std::void_t<typename C::PolicyType>>
        {
            using type = typename C::PolicyType;
        };
    } // namespace detail
    namespace movegen
    {
        namespace detail
        {
            namespace common
            {
                //=== 跨 strategy 共享: 1g make_path 的 BFS dedup policy =========
                // 与 bb::run_bb_bfs 的 try_admit 契约一致 (state, parent, action)
                // -> EnqueueDecision. 三态:
                //   - Skip            : 越界 / 已访问 (PathMark.set_bbox 失败).
                //   - MarkOnly        : mark 写入成功, 但本格物理不可放, 不入队.
                //   - MarkAndEnqueue  : mark 写入 + 物理可放, 入队.
                //
                // 与 master "set 在前, check 在后, 短路 &&" 完全等价 (mark 写
                // 但不入队). PathMark 与 usable_arr 由调用方持有, dedup 仅
                // 引用之.
                //
                // path 与 simulate 的 1g make_path 都消费同一份 dedup; 20g
                // path 因 drop 已隐式过 usable, 用各自 strategy 私有的 20g
                // dedup (仅 set 二态).
                template<class RuleSpec>
                struct MakePath1gDedup
                {
                    using Helpers = bb::Helpers<RuleSpec>;
                    using map_t = typename Helpers::map_t;
                    using PathMark = typename Helpers::PathMark;

                    PathMark *path_mark;
                    std::array<map_t, Helpers::kMaxR> const *usable_arr;

                    bb::EnqueueDecision try_admit(bb::BBState const &s,
                                                  bb::BBState const *parent,
                                                  char action)
                    {
                        if (s.xb < 0 || s.xb >= Helpers::kW ||
                            s.yb < 0 || s.yb >= Helpers::kH ||
                            s.r >= Helpers::kMaxR)
                            return bb::EnqueueDecision::Skip;
                        typename PathMark::PrevKey pk;
                        if (parent == nullptr)
                        {
                            //起点协议: prev = self.
                            pk = typename PathMark::PrevKey{
                                static_cast<std::uint8_t>(s.r),
                                static_cast<std::int8_t>(s.xb),
                                static_cast<std::int8_t>(s.yb)};
                        }
                        else
                        {
                            pk = typename PathMark::PrevKey{
                                static_cast<std::uint8_t>(parent->r),
                                static_cast<std::int8_t>(parent->xb),
                                static_cast<std::int8_t>(parent->yb)};
                        }
                        if (!path_mark->set_bbox(static_cast<int>(s.r),
                                                 static_cast<int>(s.xb),
                                                 static_cast<int>(s.yb),
                                                 pk,
                                                 action))
                            return bb::EnqueueDecision::Skip;
                        if (!Helpers::usable_at_bb(s.r, s.xb, s.yb, *usable_arr))
                            return bb::EnqueueDecision::MarkOnly;
                        return bb::EnqueueDecision::MarkAndEnqueue;
                    }
                };
            } // namespace common
        } // namespace detail
    } // namespace movegen
} // namespace m_tetris2

#endif // TETRIS_AI_RUNNER_MOVEGEN_STRATEGY_H_
