# Commit 5b 设计辨析: 替换 TetrisNode 指针图 → 位板原语?

## 1. 5b 原计划 (path_redesign.md / commit5a "Next step" 段)
- 把 `MoveGenSearch::make_path` 内联 BFS 里的:
  - `cur->wall_kick_opposite/ccw/cw` (TetrisNode const *[16], NULL 终止)
  - `cur->move_left / move_right / move_down`
  - `nL->rotate_opposite / rotate_ccw / rotate_cw` (是 wall_kick_*[0] union 别名)
  - `cur->drop(map)` (借助 move_down_multi 缓存)
  - `cur->check(map)` (TetrisNode 内部的 4 行 mask AND)
  - `cur->index_filtered` (visited 等价类 key)
- **替换为**:
  - `usable_map<Spec,T,R>(board)` / `shape::wk_*<Spec,T,R>` / `Map<W,H>::shifted<>` / `landable_map`
  - 位板版 visited 表 (按 path_redesign.md §5: visited key = `(canonical_r, x, y)`)

## 2. 现状路径 (commit 5a)
`MoveGenSearch::make_path` 已经把 BFS 内联进来, 但仍是
**纯指针图算法**: 节点身份是 `TetrisNode const *`, mark 表是
`TetrisNodeMark`(以 `node->index` 为 key), 输出的 path 字符序列经
`build_path` 沿 mark 反向重建.

唯一的对外耦合是 `node->index_filtered == index` 这个等价类比较 + 末段
T-spin `last->wall_kick_*` 链尝试.

## 3. 两种 5b 落地路径的取舍

### Path A: 1:1 表面替换 (cosmetic substitution)
- BFS 仍以 TetrisNode const * 为节点身份; mark 仍走 `node_mark_path_`.
- 每次"算"邻居:
  - 用 `Map<W,H>::shifted<-1,0>()` 等位板平移 + `usable_arr[r]` 做 check.
  - 用 `shape::wk_*<Spec,T,R>` 静态 kick 表枚举 (dx, dy) 候选.
  - 命中后 `context_->get({T, x', y', r'})` 反查回 TetrisNode const *,
    再喂给 `node_mark_path_.set(...)`.
- **得失评估**:
  - + 不再依赖 wall_kick_* 缓存数组 / move_* 缓存指针 / drop 缓存表.
  - + 让 BFS 的几何源头切到 RuleSpec 静态描述, 接近 "无 context"
    的目标.
  - − 每个邻居多一次 hash lookup (`context_->get` 是 chash_map),
    对比指针解引用是净劣化.
  - − **TetrisContext 依然不可移除**: visited dedup 仍要 `index_filtered`,
    末段 build_path 仍要写 TetrisNode * 链, 上层调用方仍需 TetrisNode *
    在 `play_path` 里逐步 apply key.
  - − 代码净增长: 几何换算 + RuleSpec 模板派发 + context_->get 反查
    叠在 5a 的 380 行 BFS 之上, 一笔提交难以保持单一职责.
- **结论**: Path A 是 *cosmetic* 的, 没有实质削减 TetrisContext 依赖,
  反而增加运行时常数. 与 path_redesign.md §5 "去 index_filtered" 目标
  方向一致, 但只走了一半.

### Path B: 真正的位板化 (visited key = (canonical_r, x, y))
- BFS 节点身份直接用 `(r, x_status, y_status)` 三元组.
- visited 表用 `std::array<map_t, R_count>` 一份 (按 master status 坐标
  或 bbox 坐标都行, 与 MoveGen::generate 内部表达对齐).
- 邻居生成: 完全位板, 同 `MoveGen::generate` 主循环风格.
- 末段 build_path: 由于上层调用方需要 char 序列即可, **不必**回传
  TetrisNode * 链; 但内部 mark 仍需要 (predecessor_r, predecessor_x,
  predecessor_y, key_char) 四元组以便回溯.
- T-spin land_point 末段仍需 `last_x, last_y, last_r → land_point.node`
  的 wall-kick 链; 改用 `shape::wk_*` 静态枚举即可, 不再走指针图.
- **得失评估**:
  - + 实质上把 make_path 与 TetrisContext 的耦合点压缩到只剩
    "land_point.last (TetrisNode *) → (last_r, last_x, last_y) 解包"
    与 "返回 path 字符序列".
  - + 去除每邻居 `context_->get` 反查, 性能为 O(BFS 平摊).
  - + 与 path_redesign.md §5 完全对齐.
  - − 是结构性重写, 不再是"原样搬迁"; 单提交粒度大,
    需要新增的辅助 (predecessor 结构体, 位板 visited, 反向 origin
    换算, T-spin 末段 wall-kick try) 都要从头写.
  - − oracle_diff path_self_check 验证的是"path 能 reach
    land_point.node", 不要求字符序列与 master 字字相同; 但 BFS 序的
    任何小差异都可能让某些极端 board 的 path 退化或 reorder, 需要谨慎.

## 4. 我的判断
**Path A 应该被 skip**, 它不解决任何根本问题, 还会让 5b 提交比 5a 还胖
(380 行 → ~500 行, 还多一份与位板重叠的几何换算逻辑).

**Path B 才是 path_redesign.md 真正想要的 5b**, 但它的工作量与风险显著
高于"按既有结构替换叶子查询", 而且会把 5a 留下来的"老引擎可对照"
的回退路径破坏 (5a 还能直接用指针图作 sanity check, 5b Path B 一旦上
就完全是新代码).

我倾向以下两个选项之一:

### 选项 1 (推荐): 5b = Path B 的"位板谓词层"; 5c 才彻底重写 BFS
- 在 `MoveGenSearch` 内构建一份 `std::array<map_t, R_count> usable_arr_` /
  `landable_arr_` 缓存 (search() 期间已经构建过类似数据, 但生命周期
  局限于函数内).
- 为 make_path 引入纯位板谓词:
  - `bool can_place(r, x_status, y_status)` -> `usable_arr_[r].get(x_bbox, y_bbox)`
  - `(x', y')` = `move_left/right/down/drop` 的位板版本
  - `wk_for<dir>(r)` 枚举 RuleSpec 静态 kick 表
- BFS 主循环仍用 TetrisNode const * 作节点 (与 5a 同形), 内部把
  `cur->wall_kick_*` / `cur->move_*` / `cur->drop` / `cur->check` 全部换成
  上述谓词 + `context_->get` 反查回 TetrisNode *.
- **本质上还是 Path A, 但封装清楚, 谓词层为 5c 改成 (r, x, y) 节点身份铺路.**
- ⚠️ 我前面说 Path A "cosmetic" 的批评对这个方案仍然成立 — 性能略
  下降, 代码量上升, 且 `context_->get` 对 BFS 路径上每次邻居都要 hash
  lookup. 真正想要的清理还在 5c.

### 选项 2 (更激进): 5b 直接做 Path B
- 一笔提交 ~400 行新增, 但单一职责完整 (替换 BFS 节点身份 +
  位板 visited + 位板邻居 + 位板末段 wall-kick).
- 优点: 一步到位, 后续 TetrisContext 退场只剩 land_point.last 一个挂
  点, 容易处理.
- 缺点: 单笔很重, oracle_diff 一旦失败, 调试范围大.

## 5. 待用户决定
- ❓ **5b 走选项 1 (谓词层 + context_->get 反查) 还是选项 2 (位板 visited
  + (r,x,y) 节点身份)?**
- ❓ 若走选项 1, 5c 才做 (r, x, y) BFS 重写; 5d 移除 impl_ 老引擎兜底.
- ❓ 若走选项 2, 选项 2 即 5b 终态, 5c 只剩"删 impl_ + typedef 整理".

## 6. 当前已落地的 5a (commit fdb3092) 不受影响
- 不论选哪个选项, 5a 的内联 BFS 是 5b 的回退基准, 保持不动.
- oracle_diff 全绿, perft_movegen 全绿, 编译干净.

---
等用户回复后再开始动刀. 工作目录无未提交变更.
