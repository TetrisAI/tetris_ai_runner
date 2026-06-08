# Oracle Search 实现行为差异调研报告

## TL;DR
Oracle 提供了 4 种搜索实现：`simple`（贪婪/非 BFS，高性能但路径受限）、`path`（标准 BFS，带 1G/20G 路径逻辑）、`simulate`（在 path 基础上增强了跨行重力模拟）、`tag`（专为 T-Spin 优化的 BFS，带 3-corner 判定）。核心算法骨架从 `simple` 的贪婪迭代演进到后三者的 BFS 拓扑。

---

## 1. 公开 API / 入口签名差异

| 实现 | 主入口函数 | 返回类型 | 特色结构/参数 |
|---|---|---|---|
| **simple** | `search` | `vector<TetrisNode const *> const *` | 使用 `land_point_cache_` |
| **path** | `search` | `vector<TetrisNode const *> const *` | 使用 `node_search_` (BFS 队列) |
| **simulate** | `search` | `vector<TetrisNode const *> const *` | 包含 `land_point_add_` (额外补点) |
| **tag** | `search` | `vector<TetrisNodeWithTSpinType> const *` | 返回自定义 `TetrisNodeWithTSpinType` 结构 |

**代码引用**:
- `search_simple.h:18`: `std::vector<m_tetris::TetrisNode const *> const *search(...)`
- `search_tag.h:42`: `std::vector<TetrisNodeWithTSpinType> const *search(...)`

**调用方**:
- `simple`: `demo.cpp` (Easy AI), `ai.cpp` (Dig AI), `vs.cpp`, `QQ AI` (Mode 2).
- `path`: `ai.cpp` (Default), `QQ AI` (Level 10).
- `simulate`: `QQ AI` (Mode 1).
- `tag`: `the_ai_games.cpp` (TheAIGames bot).

---

## 2. 搜索算法骨架差异

### search_simple: 贪婪迭代 (Non-BFS)
- **逻辑**: 直接尝试旋转，然后在每个旋转状态下尝试左右平移并掉落。
- **优化**: 如果 `node->land_point` 缓存可用（且在屋顶之上），则直接利用预存的落地偏移，不进行实时搜索。
- **判定**: 只看 `drop(map)` 的结果。

### search_path: 标准 BFS
- **逻辑**: 使用 `node_search_` 向量作为 BFS 队列，遍历所有可达状态。
- **重力区分**: 通过 `node->open(map)` 判断当前是否处于 1G（受限）或 20G 环境。
- **去重**: 使用 `node_mark_` 记录访问，`node_mark_filtered_` 记录落地。

### search_simulate: 增强重力模拟
- **逻辑**: 在 `path` 的 BFS 基础上，增加了一段逻辑处理水平相邻但垂直距离较大的两个节点（`search_simulate.cpp:319`）。它会尝试在这些点之间进行「桥接」，模拟在非 20G 环境下的自然下落过程。

### search_tag: T-Spin 专用 BFS
- **逻辑**:
    - 非 'T' 块：走类似 `path` 的普通 BFS。
    - 'T' 块：调用 `search_t`，使用 `TetrisMapSnap` 锁定当前地图快照进行精细的 T-Spin 判定。
- **判定**: `check_ready` 函数实现了标准的 3-corner T-Spin 判定逻辑 (`search_tag.cpp:381`)。

---

## 3. 状态扩展 / 邻居生成差异

| 动作 | search_simple | search_path | search_simulate | search_tag |
|---|---|---|---|---|
| left/right shift | Yes (loop) | Yes (1 step) | Yes (1 step) | Yes (1 step) |
| rotate (no-kick) | Yes | Yes | Yes | Yes |
| rotate with wallkick | Node-based | Yes | Yes | Yes |
| soft drop (1 step) | No | Yes ('d') | Yes ('d') | Yes ('d') |
| soft drop (to bottom) | Yes (`drop`) | Yes (`drop`) | Yes (`drop`) | Yes (`drop`) |
| 180 rotate ('x') | No | Yes | Yes | No |
| Multi-left/right ('L'/'R')| No | Yes | Yes | No |

**关键代码**:
- `search_path.cpp:48, 60, 72`: 分别处理 `rotate_opposite` (x), `rotate_counterclockwise` (z), `rotate_clockwise` (c)。
- `search_simple.cpp:86, 92`: 简单的 `while` 循环左右平移。

---

## 4. 路径还原 (make_path) 差异

- **search_simple**: **贪婪生成** (`search_simple.cpp:15`)。按固定顺序（旋转 -> 右移 -> 左移 -> 下移 -> 掉落）生成路径。不保证是最短路径，可能在复杂地形失效。
- **search_path / search_simulate**: **BFS 溯源**。在 `search` 过程中不记录路径，但在 `make_path` 时重新运行一个小型的 BFS 并利用 `node_mark_` 存储的 `(parent, action)` 链条还原出动作序列。
- **search_tag**: **带递归的溯源** (`search_tag.cpp:48`)。特别处理 T-Spin 路径。如果寻路结果的最后一步不是旋转但结果是 T-Spin，会递归寻找旋转前的路径并强制补上旋转动作。

---

## 5. land_point 判定 / 去重 key

- **Deduplication Key**: 四个实现均统一使用 `node->index_filtered`。
- **判定准则**:
    - `simple`: 只有 `drop(map)` 的终点是 land_point。
    - `path/simulate/tag`: 任何不能再执行 `move_down` 的状态均视为潜在落地。
    - `tag`: 额外将具有 `TSpin` 标记且满足 `check_ready` 的状态标记为 land_point。

---

## 6. 与外部的耦合点

- **TetrisNode 预建结构**: 全部强耦合 `TetrisNode` 的邻居指针（如 `move_left`, `rotate_clockwise` 等）。
- **T-Spin 逻辑**: `search_tag` 深度耦合了 `TetrisMapSnap` 和自定义的 3-corner 规则判定。
- **缓存依赖**: `simple` 和 `simulate` 会尝试读取 `node->land_point`（预生成的落地点列表）来加速搜索。

---

## 行为差异总表

| 维度 | simple | path | simulate | tag |
|---|---|---|---|---|
| **算法类型** | 贪婪迭代 | 标准 BFS | 模拟 BFS | 增强 BFS (T-Piece 特化) |
| **1G/20G 支持** | 仅 20G 效果好 | 全支持 | 增强 1G 模拟 | 侧重 T-Spin 1G |
| **路径还原** | 贪婪生成 | BFS 溯源 | BFS 溯源 | 递归溯源 (T-Spin 特化) |
| **T-Spin 判定** | 无 | 无 | 无 | 3-corner 规则 |
| **180 旋转** | ❌ | ✅ | ✅ | ❌ |
| **运行效率** | 极高 | 中 | 中 | 中 |

---

## 为新框架 hook 化的初步建议清单（含问题）

### 1. 核心 Hook 回调点
- `bool is_valid_expansion(node, action)`: 模拟 `open(map)` 逻辑，处理 1G/20G 限制。
- `bool is_land_point(node)`: 统一 `simple` 和 `path` 的落地定义差异。
- `LandPointData on_land_point(node)`: 用于 `tag` 模式下的 T-Spin 标记注入。

### 2. 状态存储与 Key (Traits)
- 落地点的 Key 是否永远只需要 `index_filtered`？
- **问题**: 是否需要支持多维 Key（如 `(index, tspin_type)`），以允许在同一个位置存储不同类型的落地？

### 3. 搜索拓扑的可插拔性
- `simple` 的非 BFS 逻辑证明了有些场景不需要队列。
- **问题**: 新框架的 `Searcher` 是否应该是一个模板类，允许在 BFS 队列和简单迭代器之间切换？

### 4. 路径还原的正交化
- 目前 `make_path` 在各实现中都有重复的 BFS 代码。
- **建议**: 将「BFS 寻路器」抽离成独立组件，各 Search 实现只负责提供「可达性判定」和「动作权重」。

### 5. 重力模拟的解耦
- `simulate` 中的跨行检测逻辑。
- **建议**: 这种「虚构邻居」是否可以通过一个 `NeighborProvider` Hook 注入，而不是硬编码在搜索主循环中？
