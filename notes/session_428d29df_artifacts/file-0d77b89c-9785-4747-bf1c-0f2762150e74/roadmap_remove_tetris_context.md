# Roadmap: Remove TetrisContext from MoveGenSearch hot path

## 目标
把 `MoveGenSearch::search` / `make_path` 的运行期完全脱离 `TetrisContext`（最终目标：master `TetrisContext` 仅在 `TetrisEngine` 启动期 / spawn 生成期保留），让 search 阶段所有 piece 几何均来自 `RuleSpec` 静态表 + 位板数据。**性能优先**：`context_->get` / `build_snap` 是 BFS 热路径上仅存的非位板操作，这是最显著的开销源。

## 现状（commit 6g）残留 TetrisContext 触达点

### init 路径（一次性，**非热路径**，性能收益 ~0）
- `init(TetrisContext const *context, ...)` — `context_` 字段保存。
- `context->generate('T')` + `context->width()` — 重建 T-spin pillar mask。
  - 替换方向：用 `RuleSpec` 静态拿 T 的 spawn cells / cw 形态 cells 即可计算 `spin_x_diff_ / spin_y_diff_ / spin_block_`。
  - **优先级降级**：init 一次性跑完，热路径无影响。先跳过，等 BFS 主循环位板化完成再回头清理。

### search / make_path **热路径**（每次决策都跑，性能优先级最高）
| 行 | 调用 | 用途 | 替换方向 | 估算热度 |
|---|---|---|---|---|
| 1451 | `context_->get(st)` | path mark 表 BBState ↔ TetrisNode* 还原 | mark 表只存 BBState + action，回溯按键串不需要 TetrisNode* | 1g make_path 每条路径 O(L) 次 |
| 1590 / 1604 | `context_->get(s)` | rotate_no_kick 反查 | 静态 `target_cw / target_ccw / target_opp` + bbox→status 折回 helper 已具备，可直接生成 BBState-only 版本 | 中 |
| 1714~1756 | 多处 `context_->get` | first_passing_kick / try_kick_chain | 把这些 helper 改为返回 BBState（取代 TetrisNode\*），下游用 BBState→drop_bb 链 | **极高**（BFS 每节点 ×3 kick 方向） |
| 1954 | `context_->get(spawn_status)` | run_piece_20g 起点 | 与外部 API spawn TetrisNode\* 一次绑定即可 | 1 次 / search call |
| 2081 | `spawn_node->build_snap(map, context_, snap)` | 给 `check_mini_ready_native` 提供 snap | snap 内部用 `context->get_block`；可用 RuleSpec 静态 cells 直接构造 snap，或把 `check(snap)` 替换为位板等价 | 1 次 / search call |
| 2211 | `context_->get(status)` | LandingPos → TetrisNode* (1g 路径 emit) | **必须保留**：emit 出口契约就是 TetrisNode* | emit 次数（小） |
| 2227 | `context_->get(last_status)` | last 字段反查 | 同上 | emit 次数（小） |

### 出口契约（无法消除，且无热路径影响）
`TetrisNodeWithTSpinType.node / .last` 字段类型是 `TetrisNode const *`，emit 时至少要做一次 `context_->get`。这是与 master AI 接口的 ABI 边界，本次重构不动。emit 次数等于落点集合大小（典型 17~34），不是性能瓶颈。

## 调整后的提交序（按热路径影响 + 独立性排序）

### 第一阶段：消除 BFS 内部的 `context_->get`（性能直接收益）
1. **commit 7a (was 7c)**：first_passing_kick / try_kick_chain helper 接口改为返回 BBState，取代 TetrisNode*。下游 drop_bb / mark / `check_ready_native` 已经只需要 BBState 或 sunk_node（emit-时 TetrisNode）。这一步直接砍掉行 1590 / 1604 / 1714 / 1756 多处 `context_->get`。**预期热路径加速最显著**。
   - 风险：multiple helper 接口同时切换；建议拆 7a-1 (rotate_no_kick) / 7a-2 (try_kick_chain) 两个子提交。
2. **commit 7b**：path mark 回溯不再 `context_->get(st)`（行 1451）。改成存 `(parent_state, action)` 序列，`build_path` 直接用 mark slot 的 action 字符。
3. **commit 7c**：run_piece_20g emit 末段延迟构造 last node TetrisNode\*——把 `state_to_node(parent_state)` 移出 BFS 主循环，只在 emit 时（landing 命中）才反查一次。
4. **commit 7d**：替换 `build_snap` 为 RuleSpec 静态版本（context-free），或把 `check_mini_ready_native` 的 snap 用法改成位板等价判断（`(node->rotate_*)->check(snap)` → 位板等价的 cells overlap 判断）。

### 第二阶段：清理非热路径（代码整洁度）
5. **commit 7e**：init 中的 T pillar mask 重建从 `context->generate('T')` 改为 `RuleSpec`-only。
6. **commit 7f**：（如有需要）emit 出口的 `context_->get` 是契约边界，保留不动；至此 `MoveGenSearch::search` BFS 主循环完全位板化，`context_` 仅作 emit-time 反查。

## 风险点 & 验证策略
- 每个 commit 单独跑 `oracle_diff` 全场景。
- 7a 涉及多处 helper 接口变更，必须 staged：先加 BBState 重载，再切调用方，再删旧 TetrisNode\* 重载，至少 3 个子提交。
- 性能基准：`perft_movegen` 跑前后对比，目标是 BFS 内部 `context_->get` 调用次数→0。
- 7d 是最微妙的一步，`check(snap)` 是 master 性能优化路径，去掉后需要保证 fallback 等价（建议跑性能对比同时跑全场景行为对比）。

## 下一步入口
**下次会话从 commit 7a-1 开始**：在 `rotate_no_kick_TRD` 旁新增一个直接返回 BBState 的 sibling helper，先让 first_passing_kick 内部用 BBState 走完整链路，下游再切。
