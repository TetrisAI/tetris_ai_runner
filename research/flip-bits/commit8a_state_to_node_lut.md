# Commit 8a — state_to_node 静态查表 LUT

## 背景
`state_to_node` 是 emit 段把 BBState (t, r, xb, yb) 还原成 `TetrisNode*`
的唯一路径, 之前实现是 bbox -> master 坐标反推 + `context_->get`
（哈希）查询, 每个落点 17~34 次。

## 改动
- `MoveGenSearch::Algorithm` 持有静态数组
  `state_node_lut_[piece * R * H * W]`。
- `init()` 阶段经由 `fill_state_lut_T<piece>` 模板展开, 复用旧的
  bbox -> master 反推公式 + `context_->get` 一次性填表。
- `state_to_node` 改成 piece_index + (r, yb, xb) 数组解引;
  删除 `state_to_node_TR / state_to_node_T / state_to_node_dispatch`
  这三层模板分发。
- 头部补全 `<algorithm>` `<cstring>` `<limits>`.

## 验证
- `oracle_diff` 全场景通过 (`# all diffs ok`).
- `grep "context_->get" src/movegen_search.h` 仅剩 1g run_piece
  末段契约边界 (L2290 / L2306) + init 一次性填表点 + 历史注释。
