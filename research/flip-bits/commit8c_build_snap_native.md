# Commit 8c — build_snap 去 context_->get_block

## 改动
- `MoveGenSearch::Algorithm` 增 `block_cache_[piece * R]`
  (TetrisNodeBlockLocate 一次性快照), init 期从 context 拷出。
- 新增本地 `build_snap_native(node, map, snap)`, 复用 master
  `TetrisNode::build_snap` 的位运算逻辑, 但 `context->get_block` 改为
  读 `block_cache_`。
- `run_piece_20g` T 路径 `spawn_node->build_snap(map, context_, snap)`
  → `build_snap_native(spawn_node, map, snap)`。
- 同步刷新过时注释。

## 验证
- `oracle_diff` 全场景通过 (`# all diffs ok`)。

## 残余
`context_` 仅剩: 字段保存 + init 一次性 LUT/block_cache 填充
+ 1g run_piece 末段契约边界 (LandingPos→TetrisNode\*, ABI)。
BFS / emit / search-once snap 全部不再触达 context_。
