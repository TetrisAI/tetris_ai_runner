# Commit 8d — MoveGenSearch 脱离 TetrisMapSnap

## 背景
8c 仍持有 `block_cache_` + `build_snap_native` + 局部 `TetrisMapSnap snap`,
但 BFS 主循环根本不查 snap, snap 只服务 `check_mini_ready_native` 三连
`rotate_*->check(snap)`. snap 与 `usable_arr` 是同源位板, check(map) 与
check(snap) 对同一节点等价, 因此 snap 完全冗余.

## 改动
- `check_mini_ready_native(snap, node)` → `check_mini_ready_native(map, node)`,
  内部 `rotate_*->check(snap)` → `rotate_*->check(map)`.
- 删除 `block_cache_` 字段、`build_snap_native` 方法、init 期 7×4 拷贝循环、
  `piece_chars()` 工具、run_piece_20g 中 `TetrisMapSnap snap` 局部声明
  与对应的 `state_to_node(spawn_bb)` 反查.

## 验证
- `oracle_diff` 全场景通过 (`# all diffs ok`).
- `grep TetrisMapSnap src/movegen_search.h` 仅剩历史注释.

## 框架影响
- 全局 `TetrisMapSnap` / `TetrisNode::build_snap` / `TetrisContext::get_block`
  仍服务 `search_tag.cpp` / `search_tspin.cpp` / `search_aspin.cpp`, 不动。
