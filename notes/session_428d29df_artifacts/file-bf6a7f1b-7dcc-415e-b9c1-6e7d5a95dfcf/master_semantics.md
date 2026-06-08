# master ↔ 新 MoveGen 语义对齐调研（flip-bits）

## 1. y 语义差异
- master `TetrisNode::status.y`（`src/tetris_core.cpp::create_node` 周边）：
  - 来自 piece 在 4x4 矩阵里的逻辑顶行；spawn 时 `status.y = h - Y_spawn - 1`，`h = 40`。
  - `TetrisMap` 的 y 方向**自底向上**，`row[0]` 是底层；位语义是 `1 = 空 / 0 = 已占（含墙外）`。
  - `node.row` 是 piece bbox 在 Map 中的物理起始行 —— `create_node` 通过跳过 4x4 矩阵底部空行收缩 bbox。
- 新 `Piece::y`（`src/tetris_shape.h::detail::cells_impl`，行 ~198–238）：
  - 先按 4x4 矩阵把 cells 收集起来，再用 `cx_min/cy_min` 归一化到 bbox-原点坐标系。`pivot = (1 - cx_min, H-2 - cy_min)`。
  - 因此 `Piece::y` 表示 bbox 底行的物理坐标，已丢掉原 4x4 底部空行的偏移信息。
- 二者描述同一个几何落点：换算偏移就是该 piece+rotation 在 4x4 矩阵里的「首个非空行索引」（`cy_min` 等价物）。仅依赖 piece + rotation，不依赖落点 (x, y)。

## 2. 对称 piece 的 rotation 集合差异
- master `search_tspin::Search`（`src/search_tspin.cpp`）：
  - BFS 用 `IndexFilter` 去重，键里含 `node.row`；对 Z/S/I：r=0 与 r=2 几何相同但 `status.y` / `node.row` 不同，因此**两条都会进入输出**。
  - 即 master 不做对称剪枝；4 个 rotation 都会被各自 BFS 扩展。
- 新 `MoveGen`（`src/tetris_movegen.h::generate`，行 ~127–290）：
  - 从代码看 `R_count = shape::rotation_count<Spec, T>` 是来自 `RuleSpec::ops` 中 `T` 的 `OpDesc` 数量。如果 `rule_srs` 在 RuleSpec 里就只为 Z/S/I 列了 2 个 OpDesc，新 MoveGen 自然只输出 r=0/r=1。
  - 还需直接确认 `src/tetris_rule_spec.h` + `src/rule_srs.cpp/h` 里 Z/S/I 的 ops 数量。
  - 子代理提到的 cobra `canonical_size` 剪枝是 cobra 仓库（`cobra-movegen/src/gen.hpp`）的逻辑，并非已经存在于新 MoveGen 内。

## 3. oracle_diff 现状
- 跑 `./build/oracle_diff` 14 个 FAIL，全部源自上述 (1)+(2)。
- 我们已修 `copy_to_oracle_map` 的位极性，但坐标 / 旋转语义未对齐，未 commit。

## 待用户确认的策略
- **A**：保留新框架 y 语义（bbox 底物理坐标），oracle_diff 把 master 的 `(r, status.y)` → 「几何 r + bbox 底 y」做归一化；如新 MoveGen 真有对称剪枝，去掉，让 r 全 4 个枚举。
- **B**：让新 MoveGen 的 `(r, x, y)` 元组完全等同 master 的 `status.y` 语义（保留 4x4 偏移），改动点扩散到 `cells_impl`、`Piece` 与所有下游。

## 待动手前的两个验证项
1. `rule_srs` 的 RuleSpec 中 Z/S/I 是否只声明了 2 个 OpDesc。位置：`src/tetris_rule_spec.h` + `src/rule_srs.{h,cpp}`。
2. master 对同一几何落点 r=0 / r=2 输出的 spin 标签是否一致（影响后续 T-Spin 测试对齐方式）。

## 本地未推送提交（保持不动）
- `7f2d8b7` Use last-rotation reach and per-rotation 3-corner mask for T-spin
- `757192b` Switch chash to allocator_traits-based rebind for C++20
- `7bdcacd` Add MoveGen oracle diff driver against original search_tspin
- 工作区还有未提交修改：`tests/oracle_diff.cpp::copy_to_oracle_map` 位极性修复。

## 下一步
- 等用户确认走 A / B；
- 然后核实「rule_srs Z/S/I 旋转数 + master spin 一致性」两点；
- 再改 oracle_diff（A 路径）或新 MoveGen + Piece + cells_impl（B 路径），全绿后 commit + push。
