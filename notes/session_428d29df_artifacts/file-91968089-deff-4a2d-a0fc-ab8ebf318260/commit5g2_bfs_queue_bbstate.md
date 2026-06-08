# Commit 5g-2: BFS 队列从 TetrisNode * 切换到 BBState

## Scope (single file: src/movegen_search.h)
1. 引入 `struct BBState { uint8_t t; uint8_t r; int16_t xb; int16_t yb; }` +
   `state_from_node(TetrisNode const *)` / `state_to_node(BBState const &)`.
2. `node_search_path_` 由 `std::vector<TetrisNode const *>` 切到 `std::vector<BBState>`.
3. BFS 主循环消费时通过 `state_to_node` 瞬时反查 master TetrisNode * 喂给现有
   helpers (try_simple_move/first_passing_kick/...). 反查失败 (例如 status_to_bbox
   越界) 用 continue 跳过.
4. 17 处 `node_search_path_.push_back(<TetrisNode*>)` 改 `push_back(state_from_node(...))`.

## Why 拆 5g-2 / 5g-3 两笔
本笔只把队列内的元素类型搬到位板侧, helpers 内部 (try_kick / first_passing_kick /
rotate_no_kick / drop_bb / try_simple_move / try_kick_chain_to) 仍接 TetrisNode *.
那些 helpers 的去指针化是 5g-3 的事.

`state_to_node` 走 piece_cells.origin 反偏移 (master_x = xb - origin.x,
master_y = yb + origin.y), 与 status_to_bbox 严格互逆.

## 不在本笔范围
- helpers 内部仍保留 cur->status / cur->wall_kick_*[i] 反查 (5g-3).
- 末段 wall-kick 重放仍走 try_kick_chain_to + land_point.node 指针 (5g-3).
- impl_ 兜底限到 20g (5h).

## Validation
- `cmake --build build -j`: 全过.
- `./oracle_diff all`: # all diffs ok, 含 mg_path_self_check_one 严格按键
  重放校验.
- `./perft_movegen all`: exit=0.

## Constraints honored
- 单文件单提交.
- 局部变量未加 const.
- clang-format -i 已执行.
- Commit message 英文 ASCII, 描述 "MR 相对目标分支多了什么".
- 未推送, 待用户确认.
