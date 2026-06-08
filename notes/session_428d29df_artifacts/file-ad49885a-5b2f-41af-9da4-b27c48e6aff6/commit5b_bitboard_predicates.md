# Commit 5b: Replace pointer-graph hops in make_path with bitboard predicates

## Scope (single file: src/movegen_search.h)
仅替换 BFS 内的"叶子查询", 不动 BFS 节点身份 / mark / build_path 主结构.

### Replaced
- `cur->wall_kick_opposite/ccw/cw` (NULL-terminated TetrisNode* 数组)
  → `first_passing_kick(piece_t, KickDir::*, cur, usable_arr)`:
    * 顺序枚举 `shape::target_*<RuleSpec, T, R>` + `shape::wk_*<RuleSpec, T, R>`
      (slot 0 = 0-kick, slot 1..length = 显式踢墙)
    * 命中即用 `context_->get({T, x', y', target_r})` 反查 TetrisNode*.
- `cur->move_left / move_right / move_down`
  → `try_simple_move(cur, dx, dy)` = `context_->get(status with x±1 / y±1)`.
- `wk->check(map)` / `cur->move_*->check(map)`
  → `node_check_bb(n, usable_arr)` = bbox 坐标换算 + `usable_arr[r].get`.
- `nL->rotate_*` (= wall_kick_*[0])
  → `rotate_no_kick(piece_t, dir, nL)` = `context_->get` 反查 target_r 同 (x,y).

### Untouched (留给 commit 5c)
- `cur->drop(map)` — 单点操作, 5c 一起换成位板 landable.
- `cur->index_filtered` / `node_mark_path_` / `build_path` 回溯 — 节点身份切到
  `(r,x,y)` 时一并处理.
- `disable_d` 三谓词 (`node->land_point != nullptr`, `node->low >= map.roof`,
  `land_point->open(map)`) — 这三个是 master 派生量, 5c 用位板 `landable_arr` +
  roof 重新表达.
- 末段 T-spin 重放 (build_path 内): 已经走位板 (`first_passing_kick`), 但仍依赖
  `last->wall_kick_*` 逻辑的等价位板版.

### Helpers added (private)
- `enum class KickDir { Cw, Ccw, Opp }`
- `build_usable_for_piece(t, board, &out)`: 按 piece 派发到
  `movegen::usable_map<RuleSpec, T, 0..R-1>` 写入 `std::array<map_t, 4>`.
- `check_TR<T, R>(status_x, status_y, usable_arr)`: master status →
  bbox (x_bbox, y_bbox) = (status_x + origin.x, status_y - origin.y), 越界返 false.
- `try_simple_move`, `node_check_bb`, `first_passing_kick`,
  `try_kick_chain_TR`, `try_kick_chain_to`, `rotate_no_kick`.

### Why oracle 一致
- master `wall_kick_*[16]` 的填充 (src/tetris_core.cpp:467-492) 与 RuleSpec 的
  `shape::wk_*` 同源 (都从 `OpDesc::WkCW/CCW/Opp` 取数据).
- master `create()` 在 init 时只做 in-bounds (越界丢弃), 与我们 `usable_map` 的
  in-bounds AND 等价.
- 运行时 `wk->check(map)` 检查 board 碰撞, 与 `usable_map` 的 board AND 等价.
- 因此每个 piece-rotation 在每个 (x,y) 处, 我们的 "first hit" 与 master 数组扫
  描 "first non-null && check passes" 命中同一 (x', y', r').

## Validation
- `clang-format -i src/movegen_search.h`: 通过.
- `cmake --build build -j --target oracle_diff perft_movegen tetris_ai tetris_ai_runner`:
  全部成功, 仅 LTO LTRANS 串行善意提示, 无 error/warning.
- `./oracle_diff all`: `# all diffs ok`, 126/126 用例通过, 仅 `sealed_top` spawn
  处的 `failed check on map` 友好告警 (预期).
- `./perft_movegen all`: exit 0, 输出完整, 无 fail/mismatch.

## Constraints honored
- 单文件单提交.
- 局部变量未加 `const`.
- `clang-format -i` 已执行.
- 未推送.
- Commit message 英文, 单段, 描述"相对目标分支多了什么", 纯 ASCII.

## Next step (commit 5c)
- 节点身份从 `TetrisNode const *` 切到 `(r, x, y)`.
- visited 从 `node_mark_path_` 切到 `std::array<map_t, R_count>`.
- mark 表 (predecessor) 改为 (prev_r, prev_x, prev_y, key_char) 结构.
- `disable_d` 三谓词位板化.
- `cur->drop(map)` 改用 `landable_arr[r]` 或沿 y 扫.
- 完成后 `impl_` 仅留 20g 兜底, 5d 处理.
