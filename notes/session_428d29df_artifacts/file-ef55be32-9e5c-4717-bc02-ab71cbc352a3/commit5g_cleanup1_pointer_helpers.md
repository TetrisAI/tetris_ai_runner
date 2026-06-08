# Commit 5g-cleanup-1: 移除 make_path 中已无 caller 的指针版 helpers

## Scope (single file: src/movegen_search.h)
删除以下成员函数 (经过 5g-3a..5g-3c 全部 BBState 化后已无任何 caller):
- try_simple_move(TetrisNode const *, int, int)
- first_passing_kick(char, KickDir, TetrisNode const *, ...)
- first_passing_kick_dir<Dir>(char, TetrisNode const *, ...)
- try_kick_TD<T, Dir>(TetrisNode const *, ...)
- try_kick_TRD<T, R, Dir>(TetrisNode const *, ...)
- rotate_no_kick(char, KickDir, TetrisNode const *)
- rotate_no_kick_dir<Dir>(char, TetrisNode const *)
- rotate_no_kick_TD<T, Dir>(TetrisNode const *)
- rotate_no_kick_TRD<T, R, Dir>(TetrisNode const *)
- drop_bb(TetrisNode const *, ...)
- check_status_TR_dispatch (只为 drop_bb 指针版服务)

## Live, NOT touched
- BBState 版的 try_simple_move / first_passing_kick* / rotate_no_kick* / drop_bb
- try_kick_chain_TR<T, tgt, WkList>(int status_x, int status_y, usable_arr)
  叶子模板, 仍由 BBState 版 try_kick_TRD 直接调用 (传 mx, my).
- _s wrappers (try_simple_move_s/first_passing_kick_s/rotate_no_kick_s/drop_bb_s)
  仍由 BFS 主循环调用.
- node_check_bb 仍由 BFS 主循环 wk/rn/nD/nL/nR 路径调用. 后续 cleanup-2 再
  用 usable_at_bb 替换.
- cells_key_for / index_filtered_eq 等 master 反查接口, 由 BFS 命中预测使用.

## 等价性
全是 dead code 删除, 不影响任何控制流. 编译器先前已经把这些函数当作未使用
模板特化, 删除后行为无差异. 仍由 oracle_diff (含 mg_path_self_check_one) 与
perft_movegen 全量回归保护.

## Validation 计划
- `cmake --build build -j`: 全过.
- `./oracle_diff all`: # all diffs ok.
- `./perft_movegen all`: exit=0.

## Constraints honored
- 单文件单提交.
- 局部变量未加 const.
- clang-format -i 须执行.
- Commit message 英文 ASCII.
- 推送前等用户确认.
