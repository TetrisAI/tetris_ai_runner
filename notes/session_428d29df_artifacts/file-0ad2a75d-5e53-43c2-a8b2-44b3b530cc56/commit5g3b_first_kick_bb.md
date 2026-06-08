# Commit 5g-3b (slice 2): first_passing_kick 原生 BBState 化

## Scope (single file: src/movegen_search.h)
1. 在 try_kick_TRD<T, R, Dir> / try_kick_TD<T, Dir> / first_passing_kick_dir<Dir> /
   first_passing_kick 各层级追加 BBState 重载.
2. BBState 重载内部仅在 try_kick_TRD<T, R, Dir>(BBState) 处用
   piece_cells<RuleSpec, T, R>.origin 把 (xb, yb) 折回 master (mx, my), 复用
   既有 try_kick_chain_TR<T, tgt, WkList>(mx, my, usable_arr).
3. first_passing_kick_s 改成直接转发到 BBState 重载, 不再做 state_to_node 反查.
4. 旧 TetrisNode * 版本暂保留 (try_kick_chain_to 末段重放仍用), 留给 5g-3c 清.

## 等价性
- master 公式: master_x = xb - origin.x; master_y = yb + origin.y. 这等价于
  status_to_bbox 的逆向, 与 state_to_node 内 state_to_node_TR 走的同一公式一致.
- try_kick_chain_TR<T, tgt, WkList>(mx, my, usable_arr) 内只读 mx, my, 不再依赖
  cur 指针, 故无视入口.
- 旧路径: BBState -> state_to_node -> TetrisNode -> try_kick_TD(cur->status.r) ->
         try_kick_TRD(cur->status.x, cur->status.y) -> try_kick_chain_TR(...).
  新路径: BBState -> try_kick_TD(cs.r) -> try_kick_TRD(origin reverse from cs.xb/cs.yb)
         -> try_kick_chain_TR(...).
  二者命中的 (mx, my) 完全一致, 后续逻辑相同.

## 不在本笔范围
- rotate_no_kick / drop_bb 的 BBState 化 (slice 3, 4).
- LR-shift 内层循环的 BBState 化.
- try_kick_chain_to 末段重放与 land_point.node 比较 (5g-3c).
- impl_ 兜底限至 20g (5h).

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
