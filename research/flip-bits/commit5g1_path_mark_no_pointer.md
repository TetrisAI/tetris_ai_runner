# Commit 5g-1: PathMark.cell_prev_ 由 TetrisNode * 切到 (r, xb, yb)

## Scope (single file: src/movegen_search.h)
1. `PathMark` 内部把 `TetrisNode const *cell_prev_` 改为
   `PrevKey { uint8_t r; int16_t xb; int16_t yb; bool has(); }`,
   起点哨兵 r=0xFF.
2. `path_mark_set` 在塞入 prev 之前用 status_to_bbox 折出 (pr, pxb, pyb).
3. `path_mark_get_bb(r, xb, yb)` 替代旧 `path_mark_get(TetrisNode*)`,
   返回 `(PrevKey, char)`. build_path 的 lambda 用 PrevKey 链回溯,
   不再持有 cur 指针.
4. PathMark 定义提前到 `private:` 段开头, 让 path_mark_set/path_mark_get_bb
   能用 `decltype(path_mark_.get_bbox(...))` 推导返回类型.

## 等价性
- 旧 PathMark 命中等价于 (visited[r][bbox]=ver), 这部分不变.
- 旧 get 返回 (TetrisNode const *prev, char op), 新 get 返回 (PrevKey, op).
  两者持有的 prev 都唯一定位上一步的 (r, x_bbox, y_bbox), 只是后者直接是
  键, 不需要再走 status_to_bbox 反查. build_path 的链表语义不变.
- 起点终止: 旧 prev=nullptr -> while loop break; 新 PrevKey.has()==false
  -> break. 一一对应.

## 不在本笔范围
- BFS 队列 `node_search_path_` 仍是 TetrisNode 指针 (5g-2).
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
