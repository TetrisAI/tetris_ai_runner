# 20g master 行为调研

## 目标分支
`flip-bits-clean` (HEAD `5f3937b`)

## 待迁移函数
- `search_tspin::Search::search` 的 20g 分支（src/search_tspin.cpp 460~705）
- `search_tspin::Search::search_t` 的 20g 分支（986~）
- `search_tspin::Search::make_path_20g`（710~975）

## 关键差异 — 与非 20g 的区别
1. **入口前置 drop**：`if (is_20g) node = node->drop(map);` —— spawn 进入 BFS 前先沉底。
2. **每邻居 drop**：BFS 每次取 `node = node_search_[cache_index]` 时，再做一次 `node = node->drop(map)`。注意：这里把入队的"邻居"还原成沉底后的状态，等价于"BFS 的每个节点都是 drop 后的"。
3. **landing 判据**：因 1+2，节点已经 drop 到底，所以 `!node->move_down || !node->move_down->check(map)` 等价于 `!can_move_down`，当前位置即可入 land。
4. **不再有 `low >= map.roof` 的高位捷径**：20g 走通用 BFS。
5. **邻居不显式 drop**：邻居（LR/旋等）入队时不立即 drop，因为下一轮取出时会 drop。
6. **`d`（单步下）邻居仍保留**：20g 模式下也存在 `move_down->check(map)` 邻居入队（注意：取出时还会再 drop，所以入队 `d` 与不入 `d` 集合等价；保留它是为 mark 一致性）。

## make_path_20g 与 make_path 主要差异
1. spawn 也先做 `node = node->drop(map)`。
2. BFS 邻居入队时**带 drop**：例如 LR 之后 `node_test = move_left->drop(map);` 再入队。
3. 旋转邻居：`wall_kick_node = wall_kick_node->drop(map);` 后入队。
4. BFS 中的 `assert(node == node->drop(map))`：保证队列里所有节点都已经"沉底"。
5. **没有 `d` 单步**（被 drop 自动覆盖）。
6. **末段 wall-kick 重放**：`wall_kick_node->drop(map) == node` 比较，需要把位板版的 try_kick_chain 替换为带 drop 的等价形式。

## 位板侧已有原料
- `drop_bb(BBState, usable_arr)` → master 节点（叶子直接命中 floor）。
- `usable_at_bb(r, xb, yb, usable_arr)`。
- `try_simple_move(BBState, dx, dy, &out)`。
- `rotate_no_kick(t, dir, BBState, &out)`。
- `try_kick_chain` (BBState 入口)；可加 drop 后置以适配 20g。

## Step 1 计划：MoveGenSearch::search 接管 20g（非 T）
- 在现有的位板 BFS 流程外，加 **20g 专用 run_piece_20g<T>(map, depth)**：
  1. spawn 节点构造 `BBState cur` 并把它 drop 到 floor (`drop_bb_state`：把 master 节点反推后 drop_bb 拿 floor 节点，再 state_from_node)。
  2. BFS 队列以 BBState 推进，每次取出再做 floor drop（保证沉底）。
  3. 邻居：LR、旋（不带 kick）、kick 链（cw/ccw/180）。每个邻居先认证 `usable_at_bb`，**入队时不 drop**；下一轮再 drop。
- 因 land 谓词 = "drop 后即停"，等价于 master `(!move_down || !move_down->check(map))`。
- T 块 spin 检测复用现有 `run_piece<'T', true>` 的 spin 计算逻辑，但需要 BFS 主循环按 20g 取出后再 drop。

## Step 1 落地范围
本步**只处理 search 的 20g**（非 T，先不动 T spin 路径）。`make_path_20g` 仍走 `impl_`，保持 oracle/perft 在非 20g 全绿、20g 子集（如果将来加测试）逐步替换。
现有 `oracle_diff` / `perft_movegen` 不覆盖 20g，因此 step 1 不会触发回归。

## 风险与边界
- `MoveGen<RuleSpec, T>::enumerate` 当前是非 20g 语义；20g 不能直接复用枚举器，需要专门写 BFS。
- 对于 spawn 即在地面以下导致 `drop_bb` 返回 nullptr 的边角情况，与 master `node->drop(map)` 行为对齐：master 下 spawn 自己若已无空间会立即返回；位板版 `drop_bb` 对 spawn 处不可放置直接 nullptr，需 trap-then-empty。

## Stop 条件（提交节奏）
- 1 commit：补 20g 路径，行为对齐（即使 oracle 不覆盖，但代码可读、可单元覆盖未来测试）。
- 1 commit（独立）：删 `impl_` 字段与 `search_tspin` include。
- 1 commit：迁移 `make_path_20g`。
- 1 commit：T spin 20g 单独。

按用户惯例：先做 search 20g 这一笔，提交并记录待审。
