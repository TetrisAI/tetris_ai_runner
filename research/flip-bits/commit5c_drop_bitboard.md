# Commit 5c (缩范围版): drop 位板化

## Scope (single file: src/movegen_search.h)
仅替换 3 处 `cur->drop(map)` -> `drop_bb(cur, usable_arr)`. BFS 节点身份 / mark /
build_path / disable_d / index_filtered / move_* / wall_kick_* 全部不动.

## Rationale (拆分 5c)
原 5c 范围 (节点身份 + visited + drop + disable_d 一笔做) 是 ~+200/-300 行结构性
重写, context 不允许 + disable_d 三谓词没有完美位板等价 (master `node->land_point`
是 init 缓存触发条件, 位板只能保守等价). 拆成:
- 5c (本次): drop 位板化, 1:1 等价, oracle 零风险.
- 5d (后续): 节点身份切 (r,x,y) + visited 位板化 + disable_d 严格 oracle 验证.
- 5e (再后): 20g 位板版 + 删 impl_ + typedef 收口.

## Behavior equivalence proof
- master `cur->drop(map)` 走 `move_down_multi` 缓存 (src/tetris_core.cpp:174-186):
  如果 `low >= 0` 直接命中 cache, 否则 `while (move_down && move_down->check(map))`.
  cache 本身就是 init 时按相同 chain 预计算的.
- 位板版 `drop_bb`: 在 `usable_arr[r]` 上从 (status_x, status_y) y 递减直到
  `check_TR<T,R>` 不成立, 最后一个仍成立的 y 即终点.
- `usable_map<RuleSpec, T, R>` 的语义 = "(T,r) 在 bbox 基准点 (x,y) 处不越界且不
  与 board 碰撞" (src/tetris_movegen.h:108-118), 与 master `node->check(map)` 等价.
- 因此每个 (T, r, status_x, start_y) 处, drop_bb 与 master drop 命中同一终点 y.

## Validation
- `clang-format -i src/movegen_search.h`: 通过.
- 构建 oracle_diff / perft_movegen / tetris_ai / tetris_ai_runner: 全过.
- `./oracle_diff all`: `# all diffs ok`, 126/126.
- `./perft_movegen all`: exit 0, 无 fail/mismatch.

## Constraints honored
- 单文件单提交.
- 局部变量未加 `const`.
- `clang-format -i` 已执行.
- Commit message 英文, 单段, ASCII.
- 未推送.

## Next (5d)
节点身份 `TetrisNode const *` -> `(r, x, y)`, visited 切 `std::array<map_t, R>`,
`disable_d` 三谓词位板等价 + oracle 验证. 完成后 `impl_` 仅留 20g 兜底.
