# Cobra-Movegen (Big-Board Branch) 调研报告

## A. cobra-movegen 核心设计要点

Cobra-movegen 是一款极致性能的俄罗斯方块 Movegen 引擎，其核心设计围绕 **Bitboard 掩码并行性** 展开：

1.  **Bitboard 布局 (Vertical Row Packing)**：不同于常见的 `uint32_t[20]` 行映射，Cobra 将多行压缩进一个 `uint64_t` 中（如宽度 10 时，每个 64 位字存 6 行）。通过 GCC `vector_size` 扩展，将整个盘面定义为一个 SIMD 向量。
2.  **掩码并行碰撞检测 (Usable Map)**：一次性计算出 piece 在当前旋转下，整个盘面所有 `(x, y)` 坐标的合法性。通过对取反后的盘面进行 bitwise 移位与合并实现，彻底规避了逐位置碰撞检查。
3.  **Bitmask BFS 搜索**：搜索可达位置时，将“当前可达前沿”表示为一个 bitboard。通过对整个 bitboard 进行左/右/下移位并与 `Usable Map` 求与，实现一次性扩展整层 BFS 节点。
4.  **动态高度路由**：基于当前最高点动态选择 `Board<H>` 模板实例（如 H=6, 12, 18, 24, 48），减少不必要的位运算。
5.  **强编译期预算**：利用 `consteval` 和 `constexpr` 预处理所有方块坐标、踢墙表，甚至部分搜索路径片段。

## B. 与 tetris_ai_runner 实现对比

| 特性 | cobra-movegen (big-board) | tetris_ai_runner (flip-bits) |
| :--- | :--- | :--- |
| **盘面编码** | `uint64_t[Tn]` SIMD 向量 (垂直压缩) | `uint32_t[max_height]` (单行 32 位) |
| **方块表示** | 相对 (0,0) 的 3 个 cell 偏移 (consteval) | `TetrisNode` 结构体，含行数据与属性 |
| **碰撞检测** | `board & piece_mask` (全盘面并行) | `row & piece_row` (单行/单位置检测) |
| **搜索算法** | **Mask-Parallel BFS** (位移扩展) | **Node-based BFS** (指针网/哈希表去重) |
| **移动生成** | 直接算出所有 landable 位置的 mask | 遍历指针网中预链接的 `move_left/right` 等 |
| **内存占用** | 极低 (计算型，无大规模索引) | 较高 (预计算指针网需数百万节点/链接) |
| **SIMD 使用** | 深度依赖 (GCC vector extensions) | 基本无显式 SIMD 使用 |
| **扩展性** | 极强 (通过 Tn 支持超大盘面) | 受限于 `uint32_t` 宽度 (Max 32) |

## C. 可借鉴的技巧清单

1.  **技巧名：Bitmask BFS (Reachable Mask Expansion)**
    *   **适用场景**：在大规模搜索空间中快速找出所有可达落点。
    *   **迁移建议**：重构 `movegen` 主循环，使用 `uint32_t` 数组表示“当前可达层”的位掩码，通过行间位移模拟移动。
    *   **风险**：代码逻辑改动巨大；若无 SIMD 优化，性能可能不及预计算的指针网。
    *   **期望收益**：彻底消除指针网的预计算时间和内存开销，支持更复杂的动态地形。

2.  **技巧名：Smeared Board / Usable Map**
    *   **适用场景**：快速过滤非法位置（碰撞检测）。
    *   **迁移建议**：我们的 `build_snap` 已有雏形，可进一步优化为一次性计算整图。
    *   **风险**：低。
    *   **期望收益**：加速 `check` 函数，减少搜索分枝开销。

3.  **技巧名：Vertical Bitboard SIMD**
    *   **适用场景**：超高性能碰撞检测与消行。
    *   **迁移建议**：引入 `__m128i` 或 `__m256i` 重新定义 `TetrisMap` 的 `row` 布局。
    *   **风险**：平台依赖性增加（需 SSE/AVX 支持）。
    *   **期望收益**：消行和移动检测效率提升 2-4 倍。

## D. 100 字总结

cobra-movegen 的核心价值在于其**“位掩码并行”**思想，将搜索从“点对点”提升到了“面到面”的位移运算。虽然 `tetris_ai_runner` 的指针网在静态规则下极快，但 Cobra 的方案在**内存利用率**、**处理复杂/大盘面**及**现代 CPU 向量化利用**上具有代差优势。**强烈建议在下一代引擎中引入 Bitmask BFS。**

## E. 关键热路径代码 (MoveGen BFS 核心)

```cpp
// 摘自 cobra-movegen/src/movegen.hpp
// 此段代码展示了如何利用 bitboard 位移实现并行的 BFS 搜索
while (!done.all()) {
    auto bfs = [&]<Rotation r>{
        if (done[r]) return;
        done.set(r);
        const Rotation rc = Gen::canonical_r<p>(r);

        // 平移与软降：一次性处理整个搜索前沿
        while (true) {
            const BoardT temp = ( search[r].template shifted<-1,  0>() // 左移
                                | search[r].template shifted< 1,  0>() // 右移
                                | search[r].template shifted< 0, -1>() // 下移
                                ) & unsearched[r]; // 必须在 usable 范围内且未搜索过

            if (!temp.any()) break;
            search[r] |= temp;
            unsearched[r] ^= temp; // 更新已搜索掩码
        }

        // 旋转处理：利用预计算的 KickTable 批量对整个 search mask 进行旋转位移
        auto rotate = [&]<Gen::Direction d, const auto& kickTable>{
            // ... 省略旋转逻辑 ...
            for (size_t i = 0; i < kickSize; ++i) {
                auto kick = kickTable[r][i];
                result |= temp.shifted<kick.x, kick.y>(); // 整个 mask 整体尝试踢墙
            }
            // ...
        };
        // ...
    };
    // ...
}
```
