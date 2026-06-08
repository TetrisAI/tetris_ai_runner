# Handoff: after cleanup-3 (BFS 完全去 node_check_bb)

## Status snapshot
- Branch: `flip-bits-clean`（本地未推送）
- HEAD: `f3c2600` *Drop node_check_bb after make_path BFS migrates to usable_at_bb*
- 上三笔 commit（cleanup-3 三步）：
  1. `a205300` Surface child BBState from make_path move helpers
  2. `12e0450` Authorize make_path BFS children via usable_at_bb
  3. `f3c2600` Drop node_check_bb after make_path BFS migrates to usable_at_bb
- `oracle_diff all`: `# all diffs ok`
- `perft_movegen all`: 全部完成无差异

## Cleanup-3 收益
1. `try_simple_move` / `rotate_no_kick` 都新增了带 `BBState&` 出参的重载，调用方在 BFS 内零反查（不再 `state_from_node`）即可拿到子状态。
2. BFS 12 处 `node_check_bb(masterPtr, usable_arr)` 全部替换为 `usable_at_bb(state.r, state.xb, state.yb, usable_arr)`，与父状态在同一坐标系，去除每分支 1 次 origin 折算。
3. `node_check_bb` 函数定义已删除，归档为注释。

## 剩余 `state_from_node` 调用面
位置（皆属"边界"，下一阶段可选优化）：
| 行 | 上下文 | 处理建议 |
|----|--------|----------|
| 195 | `land_point.last` → BBState（build_path 末段入口） | land_point 出口侧重写时统一 |
| 230 | spawn 节点入队 | spawn pipeline 重写时统一 |
| 258/267/276 | `try_kick_chain` 命中后入队 | try_kick_chain 出口可加 BBState 出参 |
| 439/451 | `drop_bb` 返回 master 节点后入队 | drop_bb 改为返回 (TetrisNode*, BBState) |
| 1435 | rotate_no_kick 出参版本内部用，调用 state_from_node 把 master 反推回 BBState | 出参版只在 BFS 用一次, 可在 rotate 函数体内直接生成 BBState 省掉 state_from_node | 

> **不是 cleanup-3 范围**。继续推进按用户优先级走：5h（impl_ 兜底收紧到 20g）→ 20g 重写。

## 下一步
- **5h**：在 `impl_` 兜底入口处加 spawn shape != SRS-7 的 trap，确保非 20g 不再走 master 路径。
- **20g 路径**：5h 完成后开始把 search20g / spawn20g 迁到位板坐标。

## 提交规范提醒
- Commit 单一原子；本批次三笔已成。
- 非用户授意不主动 push；等用户口头确认。
