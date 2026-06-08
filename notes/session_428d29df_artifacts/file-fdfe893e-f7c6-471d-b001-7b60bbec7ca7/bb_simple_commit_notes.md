# bb simple commit notes — bitboard-native simple search without bb_bfs_engine

> 本文件描述本提交把 `search_simple_node::Search` 从 master `TetrisNode` 指针图迁
> 移到位板原生实现的设计、契约与等价性核对。**不引用** `bb::run_bb_bfs` /
> `EnqueueDecision`。

## 1. "之前的设计" 出处

simple 在 commit `b5a6ca3` 已经被重新定义为 *self-contained naive-player searcher*
（commit message 见 `git show b5a6ca3`）。该 commit 的 message 第 2 段字面如下：

> `search_simple_node` is no longer a BfsEngine specialization. It models a minimal
> player who only rotates in place, slides L/R, and hard drops. Stage 1 BFS over
> the rotation graph (clockwise / counterclockwise / opposite) yields the
> minimum-rotation prefix and the set of reachable rotations; stage 2 emits drops
> on each reachable rotation along its full L/R run, with land_point physical-position
> dedup only.

并且：

> The fast path for low >= roof is preserved. make_path emits only D and never d,
> so it returns empty when the target is not directly reachable by a hard drop
> from the rotated and translated position. The simple_node_diff harness is
> retired since the new search no longer mirrors oracle/search_simple bit for bit.

证据落在源码：

- `src/search_simple_node.h:12-19`（`search_simple_node::Search` 头文件注释，描
  述阶段 1 / 阶段 2 + "不调用 wall-kick, 不软降"）。
- `src/search_simple_node.cpp` 全文 — `collect_rotations` / `search` /
  `make_path` 三函数即上文的实现入口。
- `src/movegen_search.h::Hook trait`（之前讨论里 `is_simulate_search` 的镜像位）—
  本次新增 `is_simple_search`，编译期路由进入位板 simple 而不进 simulate / path。

> 题外话：`research/flip-bits/path_redesign.md:172` 早期把 simple 列为"不在本轮
> 范围"，因此当时也没有给 simple 写专属位板设计文档。本提交即落地 commit `b5a6ca3`
> 字面上规定的两阶段算法的位板版本，不发明新行为。

## 2. 行为契约（与 path / simulate 对照）

| 维度 | path (PathHook + NoSpinHook → bb path 三件套) | simulate (SimulateNoSpinHook → bb simulate 三件套) | simple (SimpleNoSpinHook → 本提交) |
|------|-----------------------------------------------|----------------------------------------------------|------------------------------------|
| 邻居展开 | 全键集（l/r/L/R/d/D/x/z/c）+ 可选 wall-kick / rotate-move | x/z/c/l/r/L/R/d/D，无 rotate-move；20g lazy drop | 仅旋转 (cw/ccw/opp 无 kick) → drop 后 L/R 单步 → 'D' 落地。**绝无** 'd'。 |
| BFS engine | `bb::run_bb_bfs` (NeighborProvider + DedupPolicy + Visitor) | `bb::run_bb_bfs` (Simulate1g* / Simulate20g*) | **不走** `bb::run_bb_bfs`。两阶段独立循环，自闭于 `MoveGenSearch`。 |
| Wall-kick | 全部 SRS kick | 全部 SRS kick | **禁用**。每条旋转边只取 0-kick（`rotate_no_kick_bb`）。 |
| 落点去重 | `cells_key` 等价类 | `cells_key` 等价类 | `cells_key` 等价类（"land_point physical-position dedup only"）。 |
| Spawn-row fast path | n/a | n/a | `node->land_point != nullptr && node->low >= map.roof` 时直接遍历 `node->land_point` 并 drop。与 oracle 一致。 |
| make_path 操作集 | x z c l r L R d D | x z c l r L R d D | rotation_ops（c/z/x）→ l/r → 'D'。 |

## 3. 实现细节 — 为什么不用 bfs engine

`bb::run_bb_bfs` 是一个 *unified* BFS：节点 = `BBState`，邻居展开通过 `NeighborProvider`
返回一组 `(action, child_state, kick_idx)`，结果通过 `DedupPolicy` 去重，落点通过
`Visitor.emit_landing` 记录。它适合 path / simulate 这种"邻居集很大、需要统一去
重 + 统一 emit"的算法。

simple 不是这种形态：

1. **两阶段彼此独立**。Stage 1 只在旋转图（最多 4 个节点）上做 BFS，邻居只有 3
   条边（cw/ccw/opp），节点用 `r ∈ [0,4)` 哑索引就够。Stage 2 对每个 reachable
   rotation 做完全独立的 ±x 单步推进 + drop，没有跨 rotation 的去重需求（去重发
   生在 emit 端的 `cells_key`）。

2. **没有 wall-kick / 没有 'd'**。`rotate_no_kick_bb` 在 0-kick 失败时直接返回
   `nullopt`；没有 kick chain，也就用不上 bfs engine 的 `kick_idx` 字段。

3. **make_path 的旋转前缀回溯**只关心父子链，不关心整条路径上的所有 cells_key。
   bfs engine 的 visitor / dedup 反而成了负担。

因此本提交把两阶段都写成 `MoveGenSearch` 内部的私有函数：

- `collect_rotations_bb(...)`：rotation graph BFS，输出每个 rotation 的
  `(state, parent_r, op)` 与访问顺序，与 `search_simple_node.cpp` 的
  `collect_rotations` 严格同形。
- `emit_simple_drops_for_rotation(...)`：自身 drop + ±x 单步 drop，配合调用方传
  入的 `emitted_keys` 做 cells_key 去重。
- `search_simple_native(...) / make_path_simple_native(...)`：入口。

## 4. 静态分发（与 simulate 同形态）

- `src/movegen_hook.h` 新增 `SimpleNoSpinHook`：与 `NoSpinHook` 字段完全一致，唯
  一差异 `is_simple_search = true`；同时给所有现存 hook（NoHook / NoSpinHook /
  SimulateNoSpinHook / TSpinHook / ASpinHook / CautiousHook）补 `is_simple_search
  = false` 占位（与 `is_simulate_search` 同形）。
- `src/search_simple_node.h::SimpleHook` 由 `using = NoSpinHook;` 改为
  `using = ::m_tetris::SimpleNoSpinHook;`。
- `src/movegen_search.h::search()` / `::make_path()` 在最前面新增
  `if constexpr (Hook::is_simple_search) { ... return ...; }` 分支，编译期路由。

production 调用面 `src/ai.cpp:574` 写的是 `search_simple_node::SimpleHook`，无需
修改即可自动切到位板路径。

## 5. 等价性核对清单

| 项 | 位板侧 | oracle/node 侧 (`search_simple_node.cpp`) | 等价性来源 |
|----|--------|-------------------------------------------|------------|
| 旋转图节点 | `BBState{r, xb, yb}` | `TetrisNode const *` | `state_from_node` / `state_to_node` 一一对应 |
| 旋转边 cw/ccw/opp | `rotate_no_kick_bb(piece_t, dir, state)` | `node->rotate_clockwise` / `_counterclockwise` / `_opposite` 直接指针 | `rotate_no_kick_bb` 不取任何 wall-kick 偏移，等价于 master 的 0-kick 表，且失败时返回 `nullopt`（== nullptr） |
| usable 校验 | `usable_at_bb(r, xb, yb, usable_arr)` | `neighbor->check(map)` | `build_usable_for_piece` 已在 commit 0 与 master `node->check` 等价（已被 oracle_diff / simulate_diff / tag_diff 全 18 board × 7 piece 验证） |
| 起点合法性 | `usable_at_bb` on `state_from_node(node)` | `node->check(map)` | 同上 |
| BFS 顺序 | `cw → ccw → opp` 三条边，每个节点扩一次 | `Edge[3] = {cw, ccw, opp}` 同顺序 | 字面同顺 |
| 旋转前缀回放 | `parent_r` 链反向收集 `rotation_ops` 后 `reverse` | `parent` `TetrisNode` 链反向 | parent 链同语义；op 字符 c/z/x 一致 |
| L/R 推进 | `cursor_state.xb ± 1`，`usable_at_bb` 守门 | `cursor->move_left/right`，`->check(map)` 守门 | `xb` 的 ±1 与 master `move_left/right` 等价（已在 commit5g3b 验证） |
| target_x 对齐 | 反推 `master_x = xb - origin.x`，与 `land_node->status.x` 比较 | `cursor->status.x` 比较 | `origin.x` 公式与 `state_from_node`/`status_to_bbox` 对偶 |
| drop | `drop_bb_state(state, usable_arr)` | `cursor->drop(map)` | drop helper 已被多 commit 验证 |
| 落点比对 | `cells_key_for_state(*sunk) == cells_key_for(land_node)` | `cursor->drop(map) == land_point`（指针）| cells_key 在单 piece 内是指针等价类（commit5g3c） |
| 落点去重 | `emitted_keys` 线性查重（容量 ≤ 32，N 小）| `land_point_filtered_` mark | cells_key 等价于 master `index_filtered`（commit5g3c） |
| spawn fast path | 直接遍历 `node->land_point`，每项 `state_from_node` + `drop_bb_state` | `for cit : *node->land_point ... cit->drop(map)` | `node->land_point` 是 piece-definition 表（与棋盘无关），位板化无需替换 |
| make_path 不软降 | 末尾固定 `path.push_back('D')` | `path.push_back('D')`，永不 push 'd' | 字面同 |

## 6. diff 验证结果

`/workspace/iris_428d29df-9165-49d0-9579-a8eee1b2f449/tetris_ai_runner/build` 下：

```
$ cmake --build . -j$(nproc)
... [100%] Built target tetris_ai_runner

$ ./oracle_diff           → # all diffs ok
$ ./path_node_diff        → # all path-node diffs ok  (1g + 20g)
$ ./simulate_node_diff    → # all simulate-node diffs ok  (1g + 20g)
$ ./tag_node_diff         → # all tag-node diffs ok  (1g + 20g)
```

`simple_node_diff` 的源码与构建目标在 commit `b5a6ca3` 已被同步 retire（理由也在
该 commit 的 message 里——"new search no longer mirrors oracle/search_simple bit
for bit"），因此本提交不重新引入它。production 链路 `src/ai.cpp:574`
（`MoveGenSearch<...search_simple_node::SimpleHook>`）会随主目标 `tetris_ai` /
`tetris_ai_runner` 一并被 cmake 编译，已绿。

## 7. 改动文件清单

```
src/movegen_hook.h        +135 lines  (SimpleNoSpinHook + is_simple_search = false 占位)
src/movegen_search.h      +398 lines  (search_simple_native / make_path_simple_native + helpers)
src/search_simple_node.h    +6 / -2   (SimpleHook → SimpleNoSpinHook)
```

