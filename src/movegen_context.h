#ifndef TETRIS_AI_RUNNER_MOVEGEN_CONTEXT_H_
#define TETRIS_AI_RUNNER_MOVEGEN_CONTEXT_H_

//==========================================================================
// MoveGenContext: Strategy 化重构里负责"装配状态"的中立 Context.
//
// 设计动机:
//   - 旧 MoveGenSearch<RuleSpec, Hook> 把所有 search-specific 字段塞在同一个
//     类里 (land_point_cache_ / ...), Hook 是哪种 strategy 通过
//     if constexpr (Hook::is_*_search) 在运行时方法体内部分支. 这种结构对外部
//     是"封闭的", 第三方扩展 search 必须改 MoveGenSearch.
//   - 新框架按 mixin 组合: 每个具体 strategy (commit 2/3 起的 search_path /
//     search_simulate / search_simple / ...) 在自己的头文件里自带:
//
//         using Context = m_tetris2::movegen::MoveGenContext<
//             RuleSpec, BfsQueueMixin<RuleSpec>>;
//
//     只声明它需要的 mixin, 别的 mixin 不会被实例化. 这样 strategy 是真正
//     纯静态类 (`static auto* search(Context&, ...)`), 与框架解耦.
//   - Searcher<Strategy, SpinHook, RuleSpec> (见 movegen_searcher.h) 持有
//     一个 ctx_ 实例, 所有 search/make_path 调用都 inline 转发.
//
// 命名空间:
//   - m_tetris2::movegen (与 src/tetris_movegen.h 内的位板生成器共享 namespace,
//     表示 "movegen 框架装配层"). 与 m_tetris2::bb 中的 piece-aware 静态
//     helper 互不混叠.
//
// commit 1 范围:
//   - 仅声明 mixin 与 MoveGenContext 模板, 不把 MoveGenSearch 的字段实际
//     迁出来. commit 2 / commit 3 的 strategy 头文件会消费这些 mixin.
//   - 不被任何 .cpp / 旧 .h include — 通过本文件内的 static_assert 自证语法.
//==========================================================================

#include "bb_state.h"

#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <vector>

namespace m_tetris2
{
    namespace movegen
    {
        //=== Mixin 1: BFS queue buffer =========================================
        // bb::run_bb_bfs(...) 需要一个 std::vector<bb::BBState> 充当 FIFO
        // 队列缓冲. 每次进入新一轮 BFS 之前 clear() 即可复用容量, 与 1g
        // make_path 的多次重试 (disable_d 翻转) 配合.
        //
        // 使用方 (commit 2 起): SearchPath / SearchSimulate 的 make_path.
        // SearchSimple 走 collect_rotations_bb 用的是局部 std::vector<uint8_t>,
        // run_piece_20g 用的是局部 std::vector<bb::BBState>, 都不入此 mixin.
        //
        // commit B (PathMark 栈化): 旧 PathMarkMixin 已删除. PathMark 由各
        //   strategy 在 BFS 入口栈分配 (clear-on-entry, 无跨调用复用诉求).
        //   ctx 不再持任何 PathMark 状态, 解锁后续多线程并发 (独立栈).
        template<class RuleSpec>
        struct BfsQueueMixin
        {
            //commit 3: 字段 node_search_path_ 由 strategy make_path 主循环直接
            //  读写 (`run_bb_bfs(..., node_search_path_)`), 必须 public.
            //  RuleSpec 模板参数仅用于和别的 mixin 保持同形签名; bb::BBState
            //  本身不依赖 RuleSpec.
            std::vector<bb::BBState> node_search_path_{};

        private:
            static_assert(static_cast<int>(RuleSpec::width) > 0,
                          "BfsQueueMixin: RuleSpec must expose static constexpr width > 0");

            //保留给将来扩展使用; 当前 strategy 直接调用 `node_search_path_.clear()`,
            //  本 helper 不在调用面.
            void reset_bfs_queue()
            {
                node_search_path_.clear();
            }
        };

        //=== MoveGenContext<RuleSpec, Mixins...> ===============================
        // 中立 Context 模板:
        //   - public 继承 bb::Helpers<RuleSpec>, 自动暴露所有 piece-aware
        //     helper / 编译期常量;
        //   - 按声明顺序 public 继承每个 Mixin<RuleSpec>;
        //   - 没有任何自身字段 / 自身方法 (Context 是纯组合容器, search
        //     state 全部由 mixin 提供);
        //   - strategy 在自己的 Context 别名里**只声明它需要的 mixin**,
        //     未声明的 mixin 占用 0 字节 (空基类优化适用).
        //
        // 例:
        //   using Context = MoveGenContext<RuleSpec,
        //                                  BfsQueueMixin<RuleSpec>,
        //                                  StateNodeLutMixin<RuleSpec>>;
        //
        // commit 2: Mixin 的模板形参从 `template<class> class Mixins...` 改为
        //   `class... Mixins` (调用方传"已实例化"的 mixin 类型). 这样 mixin
        //   既能依赖 RuleSpec 又能依赖额外参数 (e.g. SpinHook), 让 strategy
        //   把 LandPoint cache / hook_state 这种 SpinHook-依赖字段也通过
        //   strategy-specific mixin 装进 Context, 保持 "Searcher 仅持单一
        //   ctx 实体" 的契约.
        template<class RuleSpec, class... Mixins>
        struct MoveGenContext : bb::Helpers<RuleSpec>, Mixins...
        {
            using rule_spec_type = RuleSpec;
        };

        //=== sanity static_assert =============================================
        // 用 RuleSpec stub 走类型层面的实例化, 不展开 shape:: LUT.
        namespace detail
        {
            template<class RuleSpec>
            using bfs_queue_only_ctx = MoveGenContext<RuleSpec, BfsQueueMixin<RuleSpec>>;
        } // namespace detail
    } // namespace movegen
} // namespace m_tetris2

#endif // TETRIS_AI_RUNNER_MOVEGEN_CONTEXT_H_
