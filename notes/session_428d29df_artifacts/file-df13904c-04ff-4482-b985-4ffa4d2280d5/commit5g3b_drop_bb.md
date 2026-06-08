# Commit 5g-3b (slice 4): drop_bb 原生 BBState 化

## Scope (single file: src/movegen_search.h)
1. 新增 drop_bb(BBState, usable_arr) 重载. 直接在 bbox 系上沿 yb 向下扫
   usable_arr[r].get(xb, yb - k), 终点回查一次 state_to_node 还原 master.
2. drop_bb_s 改成直接转发 BBState 重载, 不再开局 state_to_node.
3. 旧 TetrisNode * drop_bb 暂保留 (无 live caller, 后续与其他指针重载一并清).

## 等价性
- master cur->drop(map) 等价 "while (move_down && move_down->check(map)) cur = move_down".
- master move_down 仅 status.y -= 1, 故 master_y' = master_y - 1 等价 yb' = yb - 1
  (因 yb = my - origin.y, 平移与 origin 无关).
- 指针版 check_status_TR_dispatch -> check_T<T>(sx, sy, r) -> check_TR<T, r>(sx, sy)
  内做 (sx, sy) -> (xb, yb) = (sx + origin.x, sy - origin.y) 边界 + usable_arr[r].get.
  当 xb 已在 BBState 里时, 公式退化成"yb -= 1, 检 yb >= 0 且 usable_arr[r].get(xb, yb)".
- 终点 state_to_node(BBState{t, r, xb, yb_final}) 与原指针版 context_->get({T, sx, sy_final, r})
  落在同一节点 (state_to_node_TR 与 status_to_bbox 互逆).

## 不在本笔范围
- LR-shift 内层循环 BBState 化 (slice 5).
- try_kick_chain_to 末段 cells_key 重做 (5g-3c).
- 旧指针版 drop_bb / check_status_TR_dispatch 清扫.

## Validation 计划
- `cmake --build build -j`: 全过.
- `./oracle_diff all`: # all diffs ok, 含 mg_path_self_check_one.
- `./perft_movegen all`: exit=0.

## Constraints honored
- 单文件单提交.
- 局部变量未加 const.
- clang-format -i 须执行.
- Commit message 英文 ASCII, 描述 "MR 相对目标分支多了什么".
- 推送前等用户确认.
