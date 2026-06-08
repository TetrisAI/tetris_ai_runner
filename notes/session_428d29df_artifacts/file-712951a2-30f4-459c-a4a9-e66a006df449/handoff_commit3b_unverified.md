# Commit 3b 接入 - 未验证草稿交接

## 背景
- 分支: `flip-bits-clean` (HEAD = `dcb9b85` Parameterize MoveGenSearch on RuleSpec)
- 已完成: commit 1 (last_x/y/r 占位) → 2-rot (反查 predecessor) → 3a (RuleSpec 模板化)
- 当前轮次目标: commit 3b (search 走原生位板) — **草稿已写但未推**, 因为环境无法编译, 且草稿涉及多处对位板 API/数学的未验证假设.

## 已发生的动作
- 在 `src/movegen_search.h` 中临时落了一份草稿: `search()` 走 `Map<W,H>` + `MoveGen<RuleSpec, T, kMini>` 原生路径, `make_path()` 仍委托老引擎.
- 草稿已 **撤回** (`git checkout -- src/movegen_search.h`), 完整内容保存在 `research/flip-bits/movegen_search.h.commit3b_draft`.
- 工作树当前干净, 与 `dcb9b85` 完全一致.

## 草稿中需要在动手前 **逐一核对** 的假设

### 1. `Map<W,H>` 公共 API
草稿用到了:
- `using row_t = typename map_t::row_t;`
- `constexpr row_t row_mask = map_t::row_full;`
- `out.set_row(y, r);`

需要在 `src/tetris_movegen.h` 或 `cobra-movegen` 头里确认:
- `Map` 是否暴露 `row_t` typedef?
- `row_full` 是否是 `static constexpr` 常量?
- `set_row(int, row_t)` 是否存在? 还是只能通过 `data[y] = ...` 直接写?

如果 API 名字对不上, 草稿里 `build_board` 必须改写.

### 2. piece origin 静态访问
草稿用到了:
```
shape::piece_cells<RuleSpec, T, 0>.origin.x
shape::piece_cells<RuleSpec, T, 0>.origin.y
```
需要确认:
- 是不是 `shape::piece_cells<...>` 这个 inline variable 名字?
- `.origin.x/y` 字段名? 还是 `min_x/min_y`?
- 如果 piece_cells 是 `constexpr` 数组而不是带 `origin` 的 struct, 这段就要换成手算.

### 3. spawn 坐标换算
master 的 `spawn` 是 (3, 21, r=0), 这是 master `TetrisNode.status.x/y`, 即 master "矩阵 top-left" 坐标系.
MoveGen 的 `generate(board, spawn_x, spawn_y, ...)` 期望的是 *bbox 左下* 坐标.
草稿里的换算:
```
bbox_x = master_spawn_x + origin_x_r0;
bbox_y = master_spawn_y - origin_y_r0;
```
**这个换算公式是猜的**, 必须用 oracle_diff 现有 case 反推一遍. 直接的验证方式:
- 拿 r=0 时 master 的 status (x=3, y=21) 通过 `TetrisNode::pos[0..3]` 拿到 4 个 cell 的实际 (x,y),
- 拿 MoveGen 用同样 spawn 入口跑出来的 r=0 落点的 4 个 cell 的实际 (x,y),
- 对比 1:1 一致才能确认坐标系对齐.

### 4. canonical rotation
master `TetrisBlockStatus.r` 取值 = 老规则 (具体 0/1/2/3 还是 mod 1/2/4 取决于 piece, 'I'/'S'/'Z' 在 SRS 下有合并).
新 `LandingPos.r` 是 cells 表 canonical r.
Commit 1 时已经测过 `(canonical_r → master_r)` 的对应关系吗? **没测过**. 需要在 oracle_diff 里加一条断言: 对每个 spin landing, 用 `context_->get({T, lp.x, lp.y, lp.r})` 拿到的 `TetrisNode*` 必须 == oracle 里同一 landing 的 master node. 现在只测了 `has_last_rot==1`.

### 5. 非 T piece 的 `is_check / spin flags`
master `Search::search` 非 T 分支:
```
land_point_cache.emplace_back(node);  // 隐式 ctor, flags 默认 0
land_point_cache.back().is_check = true;  // ← 是否设? 需查 search_tspin.h
```
草稿里给 `is_check = true`. 必须读 `search_tspin.h::Search::search` 非 T 分支确认是否真的置 1; 如果老引擎默认 0, 草稿要改回 0, 否则 AI eval 路径会偏.

### 6. 20g drop 等价性
草稿入口写了:
```
if (config_->is_20g) node = node->drop(map);
```
但其后 `run_piece` 完全不使用 `node` (只用 piece type 字符 + 固定 spawn). 这意味着 *无论是否 20g, 实际跑的都是从 spawn 开始的 BFS*, 与老引擎 20g (从 drop 后位置开始 BFS) **不等价**. 这是一个真正的 BUG.

正确做法: 20g 模式下, 不能再用固定 spawn, 必须把 dropped node 的 (x, y, r) 转成 bbox 坐标传给 `generate`. 若 MoveGen 接口只接受 spawn 入口, 需要扩接口或者额外写一个 `generate_from(board, x, y, r, ...)`.

## 推荐的下一轮步骤
1. **先核对** 上面 6 条假设, 改成 `research/flip-bits/handoff_commit3b_unverified_resolved.md`.
2. 把 `movegen_search.h.commit3b_draft` 拷回 `src/movegen_search.h`, 按 1~6 修订.
3. **不要** 直接跑 oracle_diff (无法编译). 改为静态对照 master `Search::search` 一行一行 review.
4. 如果坐标系/canonical r 没法静态确认, 这一 commit 不能落地, 退回到 **暂缓 3b**, 先专心做 commit 4 (make_path 重写) 或 commit 0 (扩展 oracle_diff 加严格 master_node 比对) 反向反推坐标系.

## 不可推送
- 当前轮次发生的改动只有: 这份 handoff + 草稿副本 (都在 `research/`, `.gitignore` 范围外).
- `src/` 完全干净.
- **未** 推送任何东西到 origin. 用户的规则: "待确认后再开始真正推送".
