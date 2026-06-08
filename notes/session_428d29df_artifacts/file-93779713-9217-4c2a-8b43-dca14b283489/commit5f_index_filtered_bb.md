# Commit 5f: IndexFilter -> cells-mask 等价类

## Master 语义 (tetris_core.cpp:296-321)
`IndexFilter` 把 `(node.data[0..3], node.row)` 当签名键, 同 piece 不同 (R, x, y)
若 board 上占据**完全相同**的 4 cells 就分到同一 `index_filtered`. 5f 用位板版
"绝对 cells 集合 (排序后)" 直接对比, 等价类一致.

## 4-cell 规范键
- `CellsKey { uint16_t c[4]; }` (排序), 每个 cell = `(yb+cy) * kW + (xb+cx)`.
- piece <=4 cell, 排序后唯一确定 cell 集合.
- 等价 master `IndexFilter::Less` (memcmp 整组 (data, row)).

## 实际替换点 (commit 06fb133)
- 入口 `int index = ...->index_filtered` -> `CellsKey index = cells_key_for(...)`.
- 多取一份 `CellsKey index_landpoint = cells_key_for(land_point.node)`,
  用于替换 line 148 / 222 / build_path 三处 `xxx == land_point->index_filtered`
  直对比, 不再读 master IndexFilter.
- `index_filtered_eq` 签名改 `CellsKey const &`, 内部走 `cells_key_for`.
- BFS 主循环 22 处 `index_filtered_eq(n, index)` 自动通过新签名生效.
- build_path lambda 捕获 index_landpoint, 用 `cur_key = cells_key_for(cur)`
  与 `index_landpoint` 比对替代 `cur->index_filtered != lp->index_filtered`.

## cells_key_for 派发链
- `cells_key_for(node)` (按 status.t 派发) ->
- `cells_key_T<T>(node)` (按 status.r 派发, R-out-of-range 返回空 key) ->
- `cells_key_TR<T,R>(xb, yb)` (从 shape::piece_cells<RuleSpec,T,R> 取静态 cell
  list, 投到绝对坐标, 4-元素 bubble sort).
- `status_to_bbox_T` 已在 5d 提供, 复用.

## 不在本笔范围
- BFS 队列从 `TetrisNode const*` 转 `(r, x, y)` (commit 5g 起).
- 末段 wall-kick 重放 try_kick_chain_to (5b 引入), 目标依旧是 land_point.node 指针.
- 20g + 非 SRS piece + impl_ 兜底.

## Validation 策略
- 用户负责执行: `cmake --build build && ./oracle_diff all && ./perft_movegen all`.
- oracle_diff `mg_path_self_check_one` 严格守门: 任何按键串重放后必须落在
  land_point.node, 否则 fail. cells-key 与 master IndexFilter 等价类同构,
  按键集合不变, 自检必通过.

## Constraints honored
- 单文件单提交 (src/movegen_search.h).
- 局部变量未加 const (cur_key / index / index_landpoint / k / mn 等).
- clang-format -i 已执行.
- Commit message 英文 ASCII, 描述 "MR 相对目标分支多了什么".
- 未推送, 待用户编译验证后再推.

## Next (5g)
- BFS 队列 `node_search_path_` 由 `TetrisNode const *` -> `(t,r,xb,yb)` tuple,
  最后用 context_->get 反查只在 build_path / land_point.node 比较那两步.
- 末段 wall-kick 重放走纯位板 (try_kick_chain_to 已经位板化, 但目标比较仍
  是指针 ==, 5g 一并切到 cells-key).
- impl_ 兜底改 20g 专用; 20g 单独走 commit 5h.

