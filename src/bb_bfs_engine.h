#ifndef TETRIS_AI_RUNNER_BB_BFS_ENGINE_H_
#define TETRIS_AI_RUNNER_BB_BFS_ENGINE_H_

//==========================================================================
// 通用位板 BFS 引擎.
//
// 设计动机:
//   位板侧的 BFS 主循环 (run_piece_20g / make_path 1g / make_path 20g) 在
//   主循环骨架上完全一致 (FIFO 队列 + 准入 mark + 邻居展开 + 命中谓词),
//   只是各自的"邻居函数"、"准入策略"、"命中/采样行为"不同. 把骨架抽出来,
//   让三处 BFS 共享一份循环, 各自只描述差异:
//
//     - NeighborProvider: 描述"一个状态有哪些后继" (Run20gNeighbors,
//       MakePath1gNeighbors, MakePath20gNeighbors, ...);
//     - DedupPolicy: 描述"准入 mark 的语义" (是否覆盖已访问标记 / 是否记录
//       前驱与按键). 三态返回支持 1g make_path 的 cover_if 语义.
//     - Visitor: 出队回调, 负责"是否到达终点 / 是否需要 emit / 是否需要
//       中止 BFS".
//
// 引擎本身只做以下三件事:
//   1. 按 dedup.try_admit 的返回决定起点是否入队.
//   2. 主循环 FIFO 弹出 cur, 调 vis.on_pop(cur); 返回 false 立即终止.
//   3. 调 np.expand(cur, emit), 把每一对 (child, action) 喂回 dedup;
//      若返回 MarkAndEnqueue 则入队.
//
// 引擎全部模板化, 无虚函数, 无热路径上的动态分配. 队列容器开放外部传入,
// 调用方可跨多次 run 复用同一份缓冲.
//==========================================================================

#include <cstddef>
#include <cstdint>
#include <vector>

namespace m_tetris2
{
    namespace bb
    {
        //准入返回三态.
        //  Skip            : 不入队也不更新 mark (已存在等价更新或不可达).
        //  MarkOnly        : 更新 mark / parent / action, 但不入队 (cover_if
        //                    模式: 后到的 "更优" 标记覆盖先到的弱标记, 路径可
        //                    沿其回溯, 但不再扩展). 当前 commit 2 (run_piece_20g)
        //                    的 kick cover_if 命中即走此分支; 后续 commit 3
        //                    1g make_path 的 cover_if 也复用同一语义.
        //  MarkAndEnqueue  : 首次访问 (或语义上等价于"首次"), 写 mark 并入队.
        enum class EnqueueDecision : std::uint8_t
        {
            Skip,
            MarkOnly,
            MarkAndEnqueue,
        };

        //通用位板 BFS 入口 (外部缓冲版).
        //
        // 模板契约:
        //   DedupPolicy::try_admit(state, parent_or_null, action)
        //     -> EnqueueDecision; parent_or_null == nullptr 表示 "起点准入".
        //   NeighborProvider::expand(state, emit) — 对每个候选后继
        //     调用 emit(child, action[, parent_override]); 引擎把 (child, action)
        //     转交 dedup, 依据返回值决定是否入队. emit 的返回值是 dedup 的决策,
        //     供 NeighborProvider 决定是否进一步 fan out (e.g. 1g rotate_move 的
        //     X/Z/C 仅在 nL/nR 实际入队时才展开).
        //   Visitor::on_pop(state) -> bool; 返回 false 立刻退出整个 BFS,
        //     不再处理队列中剩余项. 主要用于 "出队后惰性命中" 模式 (commit 2
        //     的 run_piece_20g visitor).
        //   Visitor::on_admit(state, action, decision) -> bool; dedup 准入
        //     (Skip 之外) 即触发, 返回 false 立刻退出整个 BFS. 用于 "入队即检
        //     命中" 模式 — master 1g make_path 的每一处 set+check_index 命中
        //     即返回的语义在引擎侧就是 on_admit 命中后回 false.
        //   parent_override (emit 第三参数, 默认 nullptr) 允许 NeighborProvider
        //     用一个 "中间状态" 作为 parent 而非默认的当前出队 cur. 1g
        //     rotate_move 把 nL/nR 作为 X/Z/C 的 parent 就靠这个机制.
        //
        // 队列容器由调用方持有 (queue_buffer); 引擎进入时 clear, 内部按 FIFO
        // 顺序追加. 这样允许调用方跨多次 run 复用 std::vector 容量.
        template<class State, class NeighborProvider, class DedupPolicy, class Visitor>
        inline void run_bb_bfs(State const &start,
                               NeighborProvider &np,
                               DedupPolicy &dedup,
                               Visitor &vis,
                               std::vector<State> &queue_buffer)
        {
            queue_buffer.clear();
            EnqueueDecision admit = dedup.try_admit(start, static_cast<State const *>(nullptr), static_cast<char>(0));
            if (admit == EnqueueDecision::Skip)
            {
                //起点直接被 dedup 拒绝 (越界 / 异常). 不可能展开任何邻居.
                return;
            }
            if (!vis.on_admit(start, static_cast<char>(0), admit))
            {
                //起点入队即命中 (visitor 在 on_admit 里识别). 退出.
                return;
            }
            if (admit == EnqueueDecision::MarkAndEnqueue)
            {
                queue_buffer.push_back(start);
            }
            else
            {
                //起点 MarkOnly: 当前调用方都在起点用 MarkAndEnqueue, MarkOnly
                //  在起点的语义退化为"什么都不做". 引擎在此直接结束.
                return;
            }
            std::size_t head = 0;
            while (head < queue_buffer.size())
            {
                State cur = queue_buffer[head++];
                if (!vis.on_pop(cur))
                {
                    return;
                }
                bool stop = false;
                auto emit = [&](State const &child, char action,
                                State const *parent_override = nullptr) -> EnqueueDecision
                {
                    if (stop)
                        return EnqueueDecision::Skip;
                    State const *parent = parent_override ? parent_override : &cur;
                    EnqueueDecision d = dedup.try_admit(child, parent, action);
                    if (d == EnqueueDecision::Skip)
                        return d;
                    if (!vis.on_admit(child, action, d))
                    {
                        stop = true;
                        return d;
                    }
                    if (d == EnqueueDecision::MarkAndEnqueue)
                    {
                        queue_buffer.push_back(child);
                    }
                    return d;
                };
                np.expand(cur, emit);
                if (stop)
                {
                    return;
                }
            }
        }

        //自带队列缓冲的便捷版本; 适合一次性使用、不关心容量复用的调用点.
        template<class State, class NeighborProvider, class DedupPolicy, class Visitor>
        inline void run_bb_bfs(State const &start,
                               NeighborProvider &np,
                               DedupPolicy &dedup,
                               Visitor &vis)
        {
            std::vector<State> queue_buffer;
            queue_buffer.reserve(64);
            run_bb_bfs(start, np, dedup, vis, queue_buffer);
        }
    } // namespace bb
} // namespace m_tetris2

#endif // TETRIS_AI_RUNNER_BB_BFS_ENGINE_H_
