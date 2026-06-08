# Oracle Diff 调研进展（flip-bits）

## 当前状态
- `chash.h` 已替换为 C++20 兼容版本，整个工程可编译通过。
- `tests/oracle_diff.cpp` 与 `tests/perft_movegen.cpp` 已加入 `CMakeLists.txt`。
- `oracle_diff` 已能正确初始化老 `TetrisEngine` 并 dump 双方完整集合到
  `research/flip-bits/dumps/<board>_<piece>_{oracle,new}.txt`。
- 偏移公式 `master_x = new_x - cx_min, master_y = new_y + H-1-cy_min` 已落地（commit 46d783c），
  emit 阶段直接转换坐标。

## 当前 ./build/oracle_diff 结果（commit d45f612 之后）
- `[ok]` empty/tst × {O, L, J}
- `[FAIL]` empty/tst × {I, S, Z}：oracle 17 / new 34，差的全是 r=1/r=2/r=3 这类对称等价 rotation
- `[FAIL]` empty/tst × {T}：oracle 34 / new 34，**数量一致 spin 字段不同**——
  master 在空盘对 r=1/2/3 全部落点标 `spin=2`，新 MoveGen 因 last-rotate 限制全标 `spin=0`。

## 待做（进入"提交 B"前的两条调研）
1. **ZSI 对称去重的真实 key**：master 似乎对几何相同的 r=0/r=2、r=1/r=3 做了归一化，但
   不是单纯按 (r, x, y) —— r=0 与 r=2 的 (master_x, master_y) 不撞却仍只输出 r=0。
   需要去 `src/tetris_core.cpp::IndexFilter` 看 BFS 真实 key（猜测是 piece 落地后实际占据
   cells 的 bitmask 或 row 索引）。
2. **空盘 T 全 spin=2**：master `search_tspin::Search` 对空盘 T 全部 r=1/2/3 落点标 spin=2，
   说明它的 spin 判定不要求"最后一步是旋转"，或者其 corner 判定阈值与 cobra/新 MoveGen 不同。
   需要去 `src/search_tspin.cpp` 找到 `is_ready / is_mini_ready` 设值处。

## 偏移修复后的本地提交线（最新在前）
- `d45f612` Fix oracle_diff TetrisMap polarity and add per-piece landing dumps
- `46d783c` Emit MoveGen landings in master TetrisNode status coordinates
- `7bdcacd` Add MoveGen oracle diff driver against original search_tspin
- `757192b` Switch chash to allocator_traits-based rebind for C++20
- `7f2d8b7` Use last-rotation reach and per-rotation 3-corner mask for T-spin

## 下一步计划
- 完成上述两条调研，把结论补到 master_semantics.md。
- 提交 B1：复刻 master 去重，让 ZSI 输出从 34 → 17。
- 提交 B2：复刻 master T-spin 判定，让 T 全绿。
- 全绿后再统一 push，等用户最终确认。

