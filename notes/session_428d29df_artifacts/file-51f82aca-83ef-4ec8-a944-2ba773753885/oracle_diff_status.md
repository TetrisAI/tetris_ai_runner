# Oracle Diff 调研进展（flip-bits）

## 当前状态
- `chash.h` 已替换为 C++20 兼容版本，整个工程可编译通过。
- `tests/oracle_diff.cpp` 与 `tests/perft_movegen.cpp` 已加入 `CMakeLists.txt` 的可执行目标。
- `oracle_diff` 现已能成功初始化老版 `TetrisEngine<rule_srs, ai_zzz::TOJ, search_tspin::Search>`。
- 修复了 `copy_to_oracle_map` 中对 `TetrisMap.row` 位极性的错误：老 `TetrisMap` 使用 `1 = 空 / 0 = 已占（含墙外）`；初始化时仅将列 0..9 置 1，越界 row 全置 0。
- 修复后 oracle 不再返回 0 落点，能跑出每个 piece 的若干结果，但与新 `MoveGen` 仍存在差异。

## 待解决：oracle 与新 MoveGen 结果不一致
跑 `./build/oracle_diff` 输出 14 个 FAIL 场景。规律性差异：

1. **y 坐标整体差 1**
   - 例：Z piece 在初始板，new 给出 `r=0 x=0 y=2`，oracle 给出 `r=0 x=0 y=3`。
   - 怀疑：老代码中 piece 的 y 表示「occupied bbox 的最低/最高位」与新代码 `Piece::y`（左下角 / pivot）定义不一致；或棋盘高度坐标方向不同（master 可能以底部为 y=0，新代码相反）。
   - 建议先把 oracle 的 `node->status.y` 含义查清：在 `search_tspin::Search` 中 y 表示什么。

2. **rotation 集合不同**
   - 对 Z/S/I 等具有 180° 对称的 piece，oracle 同时给出 `r=0` 与 `r=2`、`r=1` 与 `r=3` 的等价落点；
     新 `MoveGen` 只输出代表性的旋转（推测做了对称剪枝）。
   - 后续要么让 `MoveGen` 输出全部 4 个旋转，要么在 oracle 一侧根据 piece 对称性做归一化后再 diff。

3. **spin 字段当前都是 0**
   - 测试场景没有复杂叠加，先关注非 T-spin 落点，再上 T-spin 场景。

## 下一步计划（待确认）
1. 在 `oracle_diff.cpp` 中：
   - 对 oracle 输出的 `(r, y)` 做坐标系归一化：
     - 找到 `node->status.y` 的语义后，把 oracle 的 y 换算成「piece 占据的最低 row（按新框架坐标系）」。
     - 对 Z/S/I/O 这类有对称性的 piece，把 r 折叠到代表方向；或对新代码补齐对称旋转。
2. 修复后再次跑 `oracle_diff`，期望 14 FAIL → 0 FAIL。
3. 全量绿后将本地三笔提交（chash / movegen tspin / oracle driver）push 到远端。

## 本地未推送提交
- `7f2d8b7` Use last-rotation reach and per-rotation 3-corner mask for T-spin
- `757192b` Switch chash to allocator_traits-based rebind for C++20
- `7bdcacd` Add MoveGen oracle diff driver against original search_tspin
（外加本次刚做的 `copy_to_oracle_map` bit 极性修复，尚未 commit。）
