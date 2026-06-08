# Commit 5g-3b (slice 3): rotate_no_kick 原生 BBState 化

## Scope (single file: src/movegen_search.h)
1. 在 rotate_no_kick_TRD<T, R, Dir> / rotate_no_kick_TD<T, Dir> /
   rotate_no_kick_dir<Dir> / rotate_no_kick 各层加 BBState 重载.
2. BBState 重载内部仅在 rotate_no_kick_TRD<T, R, Dir>(BBState) 的叶子层用源
   rotation R 的 piece_cells.origin 把 (xb, yb) 折回 master (mx, my), 之后构造
   {T, mx, my, tgt} 调 context_->get, 与指针版完全等价.
3. rotate_no_kick_s 改成直接转发到 BBState 重载, 不再做 state_to_node 反查.
4. 旧 TetrisNode * 版本暂保留 (目前已无调用方需要它, 但保留不会引入回归; 后续
   slice 整理 dead code 时一并清).

## 等价性证明
master 旋转不改 status.x / status.y 仅改 r. 因此:
- master 路径: BBState -> state_to_node_TR<T, R> => status (mx, my, R)
            -> rotate_no_kick_TRD<T, R, Dir>(cur)
            -> get({T, mx, my, tgt}).
- 新路径    : BBState -> rotate_no_kick_TRD<T, R, Dir>(cs)
            -> 当地折出 mx = xb - origin<T,R>.x, my = yb + origin<T,R>.y
            -> get({T, mx, my, tgt}).
两端到 get 的输入完全一致, 命中同一 TetrisNode (或同样为 nullptr).

## 不在本笔范围
- drop_bb 的 BBState 化 (slice 4).
- LR-shift 内层循环的 BBState 化 (slice 5).
- try_kick_chain_to 末段重放与 land_point.node 比较 (5g-3c).

## Validation 计划
- `cmake --build build -j`: 须全过.
- `./oracle_diff all`: 必须 # all diffs ok, 含 mg_path_self_check_one.
- `./perft_movegen all`: 必须 exit=0.

## Constraints honored
- 单文件单提交.
- 局部变量未加 const.
- clang-format -i 须执行.
- Commit message 英文 ASCII, 描述 "MR 相对目标分支多了什么".
- 推送前等用户确认.
