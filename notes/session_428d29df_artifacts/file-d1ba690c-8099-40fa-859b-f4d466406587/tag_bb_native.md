# tag 位板化 (search_tag.h master-graph 残留清除)

> 单一 commit. 目标分支 master, 当前分支 flip-bits-clean.
> 任务来源: `n1_n4_pending.md` 第 34-51 行 + `strategy_purity_audit.md` +
> `next_session_remove_context.md` + `hook_form_critique_round2.md` Q8.

## 改造范围 (任务 4 大类)

### 1. 邻居 emit (TagSearch{1g,20g,T}Neighbors / TagMakePathNeighbors)
原状: 任务清单写"`cur_node->rotate_counterclockwise / rotate_clockwise /
rotate_opposite` 直接指针读取". 实测 (`grep -n 'rotate_counterclockwise\|
rotate_clockwise\|rotate_opposite' src/search_tag.h`) 现版已经全部位板化:
四个 Neighbors 结构体对外只暴露 `bb::BBState cur` + `usable/inbounds_arr`,
旋转走 `Helpers::first_passing_kick_bb(piece_t, bb::KickDir::Ccw|Cw|Opp,
cur, *inbounds_arr)`. 与 search_path.h / search_simulate.h 邻居 emit 顺序
(z 先 c 后, 单步 wallkick, 平移在最后) **完全一致**.

本 commit 在这一类只清理掉文档/comment 中的 `cur_node->rotate_*` 提法残留
(comment 明确标注 "≡ master rotate_*"), 代码无修改.

### 2. run_piece_20g_native (search_tag.h L505+)
任务清单写"所有 `node->drop(map)`, `node->status.r`, `node->move_left()`,
`node->move_right()`, `ctx.context_->get(...)` 全部替换". 实测:
- 入口 `node` 已经被 `Helpers::state_from_node(node)` 转成 BBState entry, 后续
  无任何 `node->status.x/y/r` / `node->move_*` / `node->drop` 调用.
- BFS 邻居用 TagSearch20gNeighbors (与 search_path.h Run20gNeighbors 同形).
- drop seed: `Helpers::drop_bb_state(state, usable_arr)`.
- bridge probe 走 `usable_at_bb(r, xb, yb-1, *usable_arr)`.
- 对 land-point 的 master `TetrisNode const*` 反查走 `ctx.state_to_node(state)`
  (一次性预填的 state_node_lut_, 不在 BFS 热路径里 deref master 旋转链).

代码上本 commit 不改 run_piece_20g_native 主体 (它已干净), 仅复核.

### 3. search_t_native (search_tag.h L785+)
任务清单 Q8 关键项: "调 `SpinHook::check_ready(map, sunk_node, ctx.hook_state_)`
走 master 坐标系" → 改为位板等价.

#### oracle 端 check_ready 定义 (oracle/search_tag.cpp:391)
```cpp
int y   = node->status.y + y_diff_;
int row = block_data_[node->status.x];
if (y == 0)  return bitcount(map.row[1]   & row) + 2 >= 3;
int x   = node->status.x + x_diff_;
int corner_oob = (x == 0 || x == w - 1) ? 2 : 0;
return bitcount(map.row[y-1] & row) + bitcount(map.row[y+1] & row) + corner_oob >= 3;
```
本质就是 "T pivot 周围 4 corner (含越界视为占用) ≥ 3".

#### 位板等价 (movegen_hook.h:492 build_corners3)
```cpp
MapT bl = ~board.shifted<-(pvx-1), -(pvy-1)>();
MapT br = ~board.shifted<-(pvx+1), -(pvy-1)>();
MapT tl = ~board.shifted<-(pvx-1), -(pvy+1)>();
MapT tr = ~board.shifted<-(pvx+1), -(pvy+1)>();
rs.corners3_arr[R] = (bl & br & (tl | tr)) | (tl & tr & (bl | br));
```
注释 "在 init 阶段一次性算好的 ≥3 corner 占用 (含越界) 位图".
按 (xb, yb) 取位 = check_ready 同结果.

#### 改造点
1. search_t_native 入口 (BFS 之前) 调用
   `SpinHook::on_init_rotations<RuleSpec, T, map_t, kMaxR>(board, rot_state)`
   填 corners3_arr + last_rotate_arr.
2. BFS 完成后, 在 incomplete (= grounded land-point) 循环里把
   `node_ex.is_ready = SpinHook::check_ready(map, sunk_node, ctx.hook_state_)`
   改为 `node_ex.is_ready = corners3_arr[sunk_state.r].get(xb, yb)`.
3. `node_ex.is_last_rotate / node_ex.last / node_ex.is_check` 写入语义保持
   (last_op 由 t_mark.get_bbox 取 prev+suffix).
4. **不**在本 commit 删 SpinHook::SearchState / on_search_state_init /
   check_ready / check_mini_ready (留给 N.5).

副作用: search_t_native 不再读 `ctx.hook_state_` (TSpinHook::SearchState
的 block_data_ + x_diff_ + y_diff_ + block_buffer 完全旁路).

### 4. make_path_native / make_path_none (search_tag.h L1000+)
任务清单写 "`node->index_filtered` 比对改为 `Helpers::cells_key_for_state(...) == ...`".

修改:
- `make_path_native` 入口短路: `node->index_filtered == land_point.node->index_filtered`
  → `Helpers::cells_key_for(node) == Helpers::cells_key_for(land_point.node)`.
- `make_path_none` 入口短路: 同上 (entry 用 cells_key_for, target 用 cells_key_for).
  原 entry==target 短路 (用 master index_filtered) 已早被 cells_key 等价路径
  覆盖 (`Helpers::cells_key_for_state(entry) == index_landpoint`),
  但保留 master 比对没有必要 → 一并删除.
- `last->rotate_*` 比对 (suffix 反查段, L1116/L1122) 已经走
  `Helpers::first_passing_kick_bb(last->status.t, ...)` 出来的 BBState
  cells_key 比对; 这一项现版已干净, 本 commit 不改.

cells_key 与 master IndexFilter 一一对应 (`Helpers::cells_key_for(node)` 直接
读 master node->index_filtered 的 fallback 也保留, 但热路径无需 deref).

## dedup slot 结构 (复述 search_path.h Run20gVisitor on_pop 同款)

PathMark::get_bbox(r, xb, yb) 返回 `(prev_state, suffix_char)`:
- prev_state.r/xb/yb = parent 的 (r, xb, yb)
- suffix_char = 边类型 ('z','c','x','l','r','d','D' 等)

入口 entry 的 self-prev 标记 (oracle search_tag cover_if(entry, entry, '\0', ' ')):
- prev == cur 三字段相等 → is_self_prev = true → last_node 留 nullptr.
- 否则反查 `ctx.state_to_node({piece_t, prev_r, prev_xb, prev_yb})`.

## 验收

- `cmake --build build -j` (Release) 通过, 0 warning 0 error.
- `./build/oracle_diff` byte-equal pass (TagStrategy + PathStrategy + SimulateStrategy
  共 7 piece × 1g/20g × 多 fixture).
- `./build/extreme_rule_diff` 全过.
- `path_node_diff / simulate_node_diff / tag_node_diff` 已在 `4d9086d`
  retired (源码连同 search_*_node BfsEngine 一起删除), 当前不再作为验收 target.
  对应 covers: oracle_diff + extreme_rule_diff.

## 触发的后续清理 (留给 N.5, **不在本 commit 内**)

- TSpinHook::SearchState (x_diff_ / y_diff_ / block_data_ / block_buffer_ + 10).
- TSpinHook::on_search_state_init.
- TSpinHook::check_ready / check_mini_ready (整个虚接口对).
- 与之相关的 BaseSpinHook CRTP 默认实现裁剪 (n1_n4_pending.md N.4).

本 commit 仅做 tag 内部的 master-graph 旁路, 不改 hook ABI.
