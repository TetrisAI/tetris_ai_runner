# Commit 8b — init T pillar mask 去 TetrisContext + 注释清理

## 改动
1. **init T pillar mask 改 RuleSpec**:
   - `context->generate('T')` → `state_to_node(build_state_from_master<'T', 0>(3, 21))`
   - `context->width()` → `kW`
   - 等价: master 的 generate 缓存与 LUT 表都源自 `context_->get`,
     spawn 几何 (row/col/width/height) 是 R=0 的常量, 与 status (x,y)
     无关; oracle_diff 全场景通过验证。
2. **注释清理**:
   - 删除 / 改写 `movegen_search.h` 中 5b / 5g-2 / 7a-1 / 7a-2 等历史
     commit 引用, 对齐当前实现 (BFS / emit 已无 `context_->get`).

## 验证
- `oracle_diff` 全场景通过 (`# all diffs ok`).
- 剩余 `context_->get` 仅: 1g run_piece 末段契约边界 (L2273/L2289),
  init 一次性 LUT 填表 (L2340), 注释引用。

## 后续
- `build_snap` (L2161) 仍依赖 `context_->get_block`, search-once 调用,
  按 3-commit 方案不在本次替换。
