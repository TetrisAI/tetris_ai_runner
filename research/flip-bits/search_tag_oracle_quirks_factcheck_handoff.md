# SearchTag Oracle Quirks & Fact-Check Handoff

## 1. 当前 Git 状态
- **HEAD**: `8be0069` (refactor(movegen): introduce bitboard-native TagStrategy and retire search_tag_node)
- **本地未推送 Commit 列表 (最近 5 个)**:
  1. `8be0069`: refactor(movegen): introduce bitboard-native TagStrategy and retire search_tag_node
  2. `73fdf3a`: docs(movegen): finalize context API surface and document strategy plugin contract
  3. `7a29500`: refactor(movegen): split search strategies into pluggable static classes with per-strategy contexts
  4. `7b6003b`: refactor(movegen): introduce neutral context mixins, strategy contract and searcher wrapper
  5. `d45864c`: refactor(movegen): bitboard-native simple search without bb_bfs_engine

## 2. 位板化总目标重申
> 位板化的根本目的是消除 `TetrisContext` / `TetrisNode` 指针图依赖，实现真正的几何自包含搜索。
> 引用自 `research/flip-bits/roadmap_remove_tetris_context.md`: "最终状态应不再持有任何指向 `TetrisNode` 的指针，所有邻居扩展应基于 `BBState` 与 `Helpers` 计算。"

## 3. Oracle `search_tag` 反模式识别
在 `oracle/search_tag.cpp` 中识别出两处关键怪癖：

### (1) Master-node 预算旋转指针 + 单次可用性检查
- **行为**: 旋转邻居扩展直接读取 `node->rotate_clockwise` 或 `node->rotate_counterclockwise`。
- **实质**: 这等价于只尝试 `wall_kick_clockwise[0]` (通常是纯旋转)，完全忽略了 `wall_kick` 链中的后续 kick。
- **后果**: 如果某个 T-Spin 必须通过 kick 才能进入位置，`search_tag` 将无法找到该路径。

### (2) `TetrisNodeMark` 跨次搜索状态泄漏 (架构性泄漏)
- **行为**: `TetrisNodeMark` 实例作为 `Search` 类的成员持久化存在，`clear()` 仅增加 `version_` 而不擦除内存。
- **实质**: 这是一种利用 `versioning` 实现的高效清理，但在位板化背景下，它强制要求在 `Context` 中保留一个巨大的、基于 `TetrisNode::index` 索引的数组。
- **泄漏点**: `TagStrategy` 为了保证行为一致性（包括可能存在的 stale-data 读取 UB），复刻了这一设计，将 `node_mark_t_` 持久化在 `Context` 中，导致位板版依然无法脱离指针图索引。

## 4. 冲突点分析
这两处行为与位板化目标存在根本冲突：
- **依赖性**: 为了复刻上述行为，位板版必须不断在 `BBState` 与 `TetrisNode*` 之间进行转换 (`state_to_node`)，并依赖 `node->index`。
- **退步**: 这种“为了 100% 复刻 Oracle 而保留指针图依赖”的做法，使得位板化的收益仅限于性能（可能更慢，因为多了转换开销），而没能实现“消除 Context 依赖”的架构目标。

## 5. 决策选项摘要
- **选项 A (回退并重写)**: 承认当前的 `TagStrategy` 过于受制于 Oracle，回退到更早版本，重新实现一个完全不依赖 `TetrisNode` 指针、支持完整 Kick Chain 的位板版 T-Search。
- **选项 B (放弃 Byte-Equal 验收)**: 接受位板版在某些边缘路径（如 Kick 路径）上与 Oracle 行为不一致，优先保证架构纯净。
- **选项 C (维持现状)**: 接受目前的架构妥协，以换取与 Oracle 的 100% 行为同步。

## 6. 下一步
**等待用户拍板决定采用哪条路线，暂不进行代码修改。**
