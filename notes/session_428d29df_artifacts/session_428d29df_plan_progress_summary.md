# session_428d29df_artifacts 规划与进度整理

## 整体主线
- 该会话不是单一小任务，而是围绕 `tetris_ai_runner` 搜索/落点生成内核的架构迁移。
- 总方向：从旧的 `TetrisContext/TetrisNode/TetrisMap` 指针图体系，逐步过渡到 `RuleSpec + 位板 + MoveGenSearch/cobra 风格模板化`。
- 其中真正持续推进最多的是 **flip-bits-clean 位板增量路线**；`cobra_port`、`remove_tetris_context`、`search_hook`、`stage3 context 模板化` 更多是蓝图、阶段试点与 handoff。

## 规划路径
1. Cobra port 总路线
   - 目标：引入 cobra 风格 Board / PieceShape / PiecePos / Movegen / Search / AI / Engine 架构。
   - 分 7 个 phase：基础设施、Movegen 移植、Search 重写、AI 模板化、移除旧 Context/Node/Map、Engine 重接线、SIMD 后端。
2. 位板化现实落地路线
   - 先改 `MoveGenSearch::search` 到 native bitboard movegen。
   - 再补 `make_path` oracle/self-check。
   - 再把 `make_path` 的 1g BFS 改成 BBState/native bitboard。
   - 最后收紧 fallback，只保留 20g 兜底。
3. remove TetrisContext 路线
   - 目标：从搜索热路径中清除 `context_->get/build_snap` 等依赖，仅在 emit 边界保留必要 ABI。
4. Search Hook 正交化路线
   - 目标：把 TSpin/ASpin 等逻辑从 search 内部硬编码改成 Hook/Bridge，做到 Rule / Search / AI 可组合。

## 实际进度
### 已完成较扎实部分
- RuleSpec / bridge 基础已落地一批 commit。
- `MoveGenSearch::search` 已切到 native bitboard MoveGen。
- `make_path` 的 1g BFS 已完成大规模 BBState 迁移。
- cleanup-3 后 BFS 已不再使用 `node_check_bb`。
- fallback 已收紧到仅剩 20g。
- 上述主线有 `oracle_diff all` 与 `perft_movegen all` 验证记录。

### 部分完成 / 仍在推进
- Cobra port：只完成 Phase 1~2 地基（bitboard primitives / Map skeleton / compile-time shape lookup / RuleSpec 暴露），后续 Search/AI/Engine 尚未进入实作主战场。
- Search Hook：commit 1 已完成并验证；commit 2/3 仍在 handoff/规划中。

### 尚未完成
- 20g 位板重写：被测试覆盖缺失阻塞。
- remove TetrisContext：只有 roadmap / bookmark / next-session 拆分，没有完整落地链。
- TetrisContext 模板化（Stage 3）：存在明显设计反驳，未见实作完成证据。
- Plan C（spawn / rotation / piece count / 新 piece / 大盘）：只落了前置的一小部分（如 W 32→64），整体未完成。

## 当前遗留
- 先补 20g 回归网，再决定是否继续 20g 位板化。
- remove-context 可按 bookmark/roadmap 继续拆 commit 实施。
- Search Hook 还缺 commit 2/3。
- Cobra port 的 Phase 3~7 仍是长期蓝图。
- 是否继续模板化 `TetrisContext` 仍需再次拍板。

## 关键证据文件
- `file-0bb9bd55-5127-4179-b6d8-f1b72e8e51c5/cobra_port_execution_plan.md`
- `file-0bb9bd55-5127-4179-b6d8-f1b72e8e51c5/cobra_port_phase_progress.md`
- `file-48677ecc-8a8a-4c56-aec8-df21abb56b80/stage3_followup_tetris_context_template.md`
- `file-48677ecc-8a8a-4c56-aec8-df21abb56b80/stage3_reality_check_skip_context_template.md`
- `file-442ed940-5456-4452-ab60-c9e022d1d9fa/handoff_after_commit4.md`
- `file-5cfafe4c-9666-4ff3-abe9-014ec21b35e5/handoff_after_5g_cleanup2.md`
- `file-396d599e-92a5-4ff4-8c31-4b6bd70ebc5d/handoff_after_cleanup3.md`
- `file-39ac87a4-cbb4-41a2-9170-cef947b0978c/handoff_after_5h.md`
- `file-4ea0b906-195d-479b-8798-3a299b056b91/20g_rewrite_blocker.md`
- `file-0d77b89c-9785-4747-bf1c-0f2762150e74/roadmap_remove_tetris_context.md`
- `file-5d823725-d981-4b41-bc51-46c4a14fe49d/next_session_remove_context.md`
- `file-37855a5c-8d05-41fd-b632-e674e7a55e78/search_hook_plan_v3.md`
- `file-37855a5c-8d05-41fd-b632-e674e7a55e78/search_hook_resume_handoff.md`
- `file-2d9a7e06-0fa0-4001-b068-7fba49523709/plan_c_resume_handoff.md`
