# Commit 5g-3a: BFS 主循环 cur 切到 BBState

## Scope (single file: src/movegen_search.h)
1. helpers 加 `_s` 后缀的 BBState 入口包装: try_simple_move_s /
   first_passing_kick_s / rotate_no_kick_s / drop_bb_s.
2. path_mark_set 加 BBState 重载, 直接吃 prev_state, 不再 status_to_bbox prev.
3. BFS 主循环把 `cur = state_to_node(node_search_path_[i])` 改 `cur_state =
   node_search_path_[i]`, 22 处 `cur` 全部用 `cur_state` 通过 _s helpers 调.
4. nL_state / nR_state 缓存避免 rotate 子分支重复 state_from_node.

## 不在本笔范围
- helpers 内部 (try_kick / try_simple_move / rotate_no_kick / drop_bb) 仍接
  TetrisNode * 操作, 那部分留给 5g-3b.
- 末段 wall-kick 重放 try_kick_chain_to + land_point.node 指针 (5g-3c).
- impl_ 兜底限到 20g (5h).

## Validation
- `cmake --build build -j`: 全过.
- `./oracle_diff all`: # all diffs ok, 含 mg_path_self_check_one.
- `./perft_movegen all`: exit=0.

## Constraints honored
- 单文件单提交.
- 局部变量未加 const.
- clang-format -i 已执行.
- Commit message 英文 ASCII, 描述 "MR 相对目标分支多了什么".
- 未推送, 待用户确认.
