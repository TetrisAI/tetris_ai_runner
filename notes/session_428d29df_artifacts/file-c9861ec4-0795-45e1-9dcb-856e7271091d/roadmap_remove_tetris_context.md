# Roadmap: Remove TetrisContext from MoveGenSearch hot path

## 目标
把 `MoveGenSearch::search` / `make_path` 的运行期完全脱离 `TetrisContext`（最终目标是 master `TetrisContext` 仅在 `TetrisEngine` 启动期 / spawn 生成期保留），让 search 阶段所有 piece 几何均来自 `RuleSpec` 静态表 + 位板数据。性能优先：`context_->get` / `build_snap` 都是 BFS 热路径上的非位板操作，是当前最显著的开销源。

## 现状（commit 6g）残留 TetrisContext 触达点

### init 路径（一次性，非热路径）
- `init(TetrisContext const *context, ...)` — `context_` 字段保存。
- `context->generate('T')` + `context->width()` — 重建 T-spin pillar mask。
  - 替换方向：用 `RuleSpec` 静态拿 T 的 spawn cells / cw 形态 cells 即可计算 `spin_x_diff_ / spin_y_diff_ / spin_block_`。

### search / make_path 热路径
| 行 | 调用 | 用途 | 替换方向 |
|---|---|---|---|
| 1451 | `context_->get(st)` | path mark 表里 BBState ↔ TetrisNode* 还原 | `RuleSpec`-only path mark：mark 表只存 BBState，下游不再需要 TetrisNode* |
| 1590 | `context_->get(s)` | rotate_no_kick 反查 | 静态 `target_cw / target_ccw / target_opp` + bbox→status 折回，已有 helper，可直接生成 TetrisNode-free 版本 |
| 1604 | `context_->get(s)` | 同上（180） | 同上 |
| 1714~1756 | 多处 `context_->get` | first_passing_kick / try_kick_chain | 同上 |
| 1954 | `context_->get(spawn_status)` | run_piece_20g 起点 | spawn 入口仍需要 TetrisNode*（与外部 API），但 BFS 内部状态可只走 BBState |
| 2081 | `spawn_node->build_snap(map, context_, snap)` | 给 `check_mini_ready_native` 提供 snap | snap 内部用 `context->get_block`；可改为 RuleSpec 静态 cells 直接构造 snap，或干脆替换 `check(snap)` 为位板等价 |
| 2211 | `context_->get(status)` | LandingPos → TetrisNode* (1g 路径 emit) | 必须保留：emit 出口契约就是 TetrisNode* |
| 2227 | `context_->get(last_status)` | last 字段反查 | 同上 |

### 出口契约（无法消除，但可推迟）
`TetrisNodeWithTSpinType.node / .last` 字段类型是 `TetrisNode const *`，所以**emit 时**至少要做一次 `context_->get`。这是与 master AI 接口的 ABI 边界，本次重构不动。

### "热路径上的 TetrisNode*"（最值得移除的一类）
1. **path mark 表的 BBState ↔ TetrisNode* 还原**（行 1451）：mark 表存 `(visited, action, parent_state)`，回溯时不需要 `TetrisNode*`，只需要按键串。
2. **first_passing_kick / try_kick_chain 内部的 `context_->get`**（行 1590~1756）：这些 helper 需要返回 `TetrisNode const *` 以便外层做 `state_from_node` / 后续 `drop_bb`。改造方向是让它们直接返回 BBState（已有 `state_from_node` 的逆向 `bbox_to_status` helper 可用），下游 drop_bb / mark 都基于 BBState。
3. **run_piece_20g 内部的 `state_to_node`（行 1755 附近）**：用于 emit 时从 mark slot 重建 last node。可改为延迟到 emit 出口才 `context_->get` 一次。

## 路线图（建议提交序）

1. **commit 7a**：把 `init()` 的 T pillar mask 重建从 `context->generate('T')` 改成 `RuleSpec`-only 路径。一次性开销，但能完全消除 init 对 context 的"形状级"依赖（仍保留 `context_` 字段以便下游 emit 反查）。
2. **commit 7b**：把 path mark 回溯从"反查 TetrisNode*"改成"按 mark slot action 直接构造按键串"，去掉 `context_->get(st)` (行 1451)。
3. **commit 7c**：把 `first_passing_kick / try_kick_chain` 的内部接口换成返回 BBState (而非 TetrisNode*)，下游用 BBState→drop_bb 链走完。这一步会同时砍掉行 1590 / 1604 / 1714 / 1756 多处 `context_->get`。
4. **commit 7d**：在 run_piece_20g 末段 emit 时延迟构造 last node 的 TetrisNode*，让 BFS 主循环不再触达 TetrisNode*。
5. **commit 7e**：替换 `build_snap` 为 RuleSpec 静态版本（context-free），或者把 `check_mini_ready_native` 的 snap 用法改成位板等价判断。
6. **commit 7f**：emit 出口的 `context_->get(status)` 与 `context_->get(last_status)` 是契约边界，保留不动；至此 `MoveGenSearch::search` BFS 主循环完全位板化，`context_` 仅作 emit-time 反查。

## 风险点 & 验证策略
- 每个 commit 单独跑 `oracle_diff` 全场景；7c 涉及多处 helper 接口变更，需要 staged diff，可能要拆成 7c-1 / 7c-2。
- 性能基准：`perft_movegen` 跑前后对比，目标是热路径上 `context_->get` 调用次数→0。
- 7e 是最微妙的一步，`check(snap)` 在 master 是性能优化路径，去掉后需要保证 fallback 等价。
