# Handoff: after 5h (impl_ 唯一兜底收窄到 20g)

## Status snapshot
- Branch: `flip-bits-clean`（本地未推送）
- HEAD: `5f3937b` *Trap non-SRS piece in non-20g MoveGenSearch path*
- 上一 commit: `f3c2600` (cleanup-3)
- `oracle_diff all`: `# all diffs ok`
- `perft_movegen all`: 170 行输出，全部完成无差异

## 5h 收益
- `MoveGenSearch::search` 与 `MoveGenSearch::make_path` 的 default / 非 SRS 兜底 `impl_.search/impl_.make_path` 删除，替换为 `assert(false, ...) + 返回空集合`。
- 上层若传入非 SRS-7 piece，调用方将立刻在 debug build 触发协议违反 trap；release 行为是稳定的"空结果"，不再隐式串到 master 路径。
- `impl_` 现在唯一现役兜底是 `is_20g == true`。20g 路径的位板重写为下一阶段的目标。

## 文件头注释已同步
更新 `// 当前实现状态` 段：明确 5h 后非 20g + 非 SRS 是上游异常。

## 下一阶段：20g 重写
**前置条件确认（5h 已达成）**：非 20g 路径下不再有任何调用会落入 `impl_`。
**目标**：把 `if (config_->is_20g)` 两处 `impl_.{search,make_path}` 也迁到位板。
**难点**：
1. `search_tspin::Search::search20g` 在 BFS 内每个邻居都强制 drop_to_floor，再以 drop 后位置作为新节点继续 BFS；
2. `make_path_20g` 配套，20g 移动序列的 `D` 与非 20g 的 `D` 语义不同（每一步 LR/旋之后强制完整 drop，`D` 是 hard-drop 真正落地）。
**计划提纲（待落细）**：
- step 1：在 `MoveGen<RuleSpec, T>` 中扩出 `enumerate_20g(...)`：BFS 每邻居生成后立刻按 `drop_bb` 沉底，邻居等价类 = 沉底后 `(r, xb, yb_floor)`。已有 `drop_bb` 完成此步骤所需的全部基础。
- step 2：`MoveGenSearch::run_piece_20g<T>` 对接（与 5h 的 SRS-7 dispatch 复用）。
- step 3：`make_path` 的 20g 分支同步——BFS 入队时把 LR/旋视为 sub-step，每一步成功后再 `drop_bb` 一次，`D` 仅在终点放置。
- step 4：删除 `search_tspin::Search::search20g/make_path_20g` 的引用，最终 `impl_` 字段可彻底移除。

## 提交规范
- 单原子提交，已成。
- 推送由用户口头确认后再发起。
