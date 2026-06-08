# Commit 5g-cleanup-2: 移除 _s wrappers, BFS 主循环直调 BBState 重载

## Scope (single file: src/movegen_search.h)
1. 删除 try_simple_move_s / first_passing_kick_s / rotate_no_kick_s / drop_bb_s
   四个 `_s` wrapper (5g-3a 引入的过渡转发, 5g-3b 后已退化为单行直调).
2. BFS 主循环 16 处调用从 `xxx_s(cur_state/nL_state/nR_state, ...)` 改成
   `xxx(cur_state/nL_state/nR_state, ...)` 直接命中 BBState 重载.
3. node_check_bb 暂保留 (BFS 多分支仍依赖, 切换 usable_at_bb 涉及多点 BBState
   传播, 留给后续 cleanup).

## 等价性
所有 `_s` wrapper 都仅是 1:1 转发 BBState 入口 helper, 删除等价于 inline 它们.
control flow 与命中节点完全一致.

## Validation
- `cmake --build build -j`: clean.
- `./oracle_diff all`: # all diffs ok (含 mg_path_self_check_one).
- `./perft_movegen all`: exit=0.

## Constraints honored
- 单文件单提交.
- 局部变量未加 const.
- clang-format -i 已执行.
- Commit message 英文 ASCII.
- 推送前等用户确认.
