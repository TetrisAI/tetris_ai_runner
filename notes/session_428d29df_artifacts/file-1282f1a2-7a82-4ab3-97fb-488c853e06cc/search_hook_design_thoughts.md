# Search 框架 Hook 化设计：思考与开放问题（v3）

> v3 修正点（用户反馈）：
> 1. **search 调用极频繁，make_path 调用并不频繁** → search 与 make_path 必须解耦设计；search hot path 不能为了 make_path cold path 让步
> 2. **simple 的 make_path 用 r/l 单步而非 L/R** → search 邻居图与 make_path 邻居图**本质上就不需要同源**；oracle simple 已经印证
>
> v2 已废弃（保留在 git 历史）。

---

## 1. v2 → v3 关键修正

### 错误 1：v2 Q6「search 阶段维护 parent 链」是错的

**事实**：
- search 在 AI 主循环中被调用上千上万次（每个 piece × 每个候选位置 × 每深度）
- make_path 只在选定最佳 land_point 后**调用一次**
- 让 search hot path 维护 parent 链 = 用 hot path 的开销去补贴 cold path 的浪费

**oracle 的做法**：search 阶段不维护 parent 链；make_path 时**重做一次小 BFS** 来恢复路径（`search_path.cpp:14-175`）。这是合理的：
- search 阶段：极致性能（不存 parent，用 mark.mark() 即可）
- make_path 阶段：可以接受重做 BFS 的开销（cold path）

**v3 决策**：保持 oracle 模式 — search 不存 parent；make_path 在被调用时按需重建。

### 错误 2：v2 Q4「simple 复用 L/R 多步邻居语义」是错的

**事实**：
- `oracle/search_simple.cpp:27-46` 的 make_path 输出 'r'/'l' **单步**：
  ```cpp
  while (node->status.x < land_point->status.x && node->move_right ...)
      path.push_back('r');
  ```
- 而 search 邻居（`search_simple.cpp:86-97`）是「左走到底 + 右走到底」：
  ```cpp
  while (left != nullptr && left->check(map))
      push(..., left->drop(map));
      left = left->move_left;
  ```
- **search 邻居图 ≠ make_path 邻居图**。simple 内部就是这样。

**含义**：邻居语义是 hook 子系统级别的，不是全局统一的。search 的 NeighborProvider 和 make_path 的 ActionEnumerator 各自独立。

---

## 2. v3 设计：search 与 make_path 解耦的双 Hook 体系

### 2.1 两个子系统

```
┌─────────────────────────────┐    ┌──────────────────────────────────┐
│ search subsystem (HOT)       │    │ make_path subsystem (COLD)        │
│                              │    │                                   │
│ - 极频繁调用                 │    │ - 仅最终 land_point 调用          │
│ - 编译期 trait               │    │ - 可接受 indirection / 重建        │
│ - 不存 parent                │    │ - 自带小型 BFS 重建路径            │
│ - 极致性能                   │    │ - 灵活性 > 性能                   │
└─────────────────────────────┘    └──────────────────────────────────┘
        │                                       │
        └────────共享────────┐
                             │
                    ┌────────▼────────┐
                    │  LandPoint 输出 │
                    │ (TetrisNode* 或 │
                    │ +TSpinType etc) │
                    └─────────────────┘
```

### 2.2 search hot path：仅 4 个 trait（编译期）

```cpp
template <
    typename NeighborProvider,    // 产生 BFS 邻居（含 20G 自适应）
    typename VisitedPolicy,       // IndexedVisited / NoVisited
    typename LandPointPolicy,     // 何时入 land_point + 是否注入 metadata
    typename LandPointElement     // TetrisNode* / TetrisNodeWithTSpinType
>
class Searcher
{
    auto search(map, start_node, depth)
    {
        Queue queue; queue.push(start_node);
        VisitedPolicy::mark(start_node);
        while (auto node = queue.pop())
        {
            if (LandPointPolicy::is_land_point(node, map))
                LandPointPolicy::record(land_point_cache_, node);
            NeighborProvider::for_each_neighbor(node, map,
                [&](neighbor, /* no action */) {
                    if (VisitedPolicy::should_expand(neighbor))
                        queue.push(neighbor);
                });
        }
        return land_point_cache_;
    }
};
```

注意：**邻居 lambda 不传 action**！因为不存 parent 链，action 信息只在 make_path 时才需要。

### 2.3 make_path cold path：独立 trait

```cpp
template <typename ActionEnumerator>
class PathBuilder
{
    vector<char> make_path(start, target_landpoint, map)
    {
        // 1. 跑一次小型 BFS（重建 parent 链）
        // 2. ActionEnumerator 决定哪些动作（'r'/'l'/'L'/'R'/'x'/...）
        // 3. 回溯输出 char 序列
    }
};
```

或者对 simple 这种简单情况，直接用一个**贪婪重放器**：

```cpp
class GreedyReplayer  // 不是 BFS
{
    vector<char> make_path(start, target_landpoint, map)
    {
        // 旋转匹配 → 横向单步 r/l → drop
        // 与 oracle search_simple.cpp:15-52 同款
    }
};
```

### 2.4 各 search 实现 = hook 配置

| 实现 | NeighborProvider (hot) | VisitedPolicy | LandPointPolicy | PathBuilder (cold) |
|---|---|---|---|---|
| **simple** | `Coarse20gNeighbors` (左到底/右到底/旋/drop) | `NoVisited`(DAG) | `OnDrop` | **`GreedyReplayer`**(r/l 单步) |
| **path** | `AdaptiveNeighbors`(按 `low>=roof` 自适应) | `IndexedVisited` | `OnImmobile` | `BfsRebuilder`(全动作) |
| **simulate** | `AdaptiveNeighbors` (path 的 20G 分支已含桥接，加强版需 `+Bridge`) | `IndexedVisited` | `OnImmobile` | `BfsRebuilder` |
| **tag** | `FullNeighbors`(无 180/multi-LR) | `IndexedVisited` | `OnImmobileWithTSpin` | `RecursiveTSpinRebuilder` |

**关键观察**：simple 的「search 用粗粒度邻居 + make_path 用单步动作」这种组合是 oracle 已经印证可行的，新框架不必强制两者同源。

### 2.5 simple 的 NoVisited 安全性（v2 Q7 收尾）

simple search 邻居图：
- 旋转：`do { rotate = rotate->rotate_counterclockwise; } while (rotate != null && rotate != node)` — **天然有界**（一圈即停）
- 左到底/右到底：`while (left->check)` 走到地图边界即停 — **天然有界**
- drop：单向，不回头 — **天然有界**

**结论**：NoVisited 不需要 max_iter 保护，邻居生成器自身有界。
（如果将来加新的 hook 配置，由该 hook 自行保证有界即可。）

---

## 3. 提交给用户的开放问题（v3 收敛）

### Q1：search 与 make_path 是否解耦为两套独立 hook？
- **倾向**：是。理由见 §1。
- 影响：search 不存 parent；make_path 重建。

### Q2：hook 用编译期 template trait？
- **倾向**：是。tetris_engine 现状已大量使用模板，统一风格。
- search hot path 必须编译期；make_path cold path 也用编译期但可放宽。

### Q3：20G 切换放在 NeighborProvider 内自适应？
- **倾向**：是。沿用 oracle path 的 `low >= map.roof` 节点级判断。

### Q4：simple 的「左到底/右到底」是 search 邻居的形态，与 make_path 用 r/l 解耦
- 不再争论"是否复用 L/R 邻居"。**search 用粗粒度邻居，make_path 用细粒度动作**，各走各的。
- **倾向**：是。

### Q5：tag 是否需要 `(index_filtered, tspin_tag)` 复合 key？
- **悬而未决**：先写 `tools/tag_landpoint_collision_dump.cpp` 扫一批 board 验证：
  - 若同 index 不会出现多个 tspin_type → 复合 key 不必要，单 key + metadata 即可
  - 否则必要
- **倾向**：先验证再决定，不预设。

### Q6：make_path 的 BFS 重建会不会成为问题？
- 与 oracle 一致，oracle 在生产中跑了多年没问题。
- 仅在选定最佳 land_point 后调用一次，开销可忽略。
- **倾向**：保持 oracle 模式，不优化。

### Q7：simple 的 GreedyReplayer 与 BFS 框架的关系
- 它**不是** PathBuilder 的 BFS 实例，是一个完全独立的 cold path 实现。
- 要不要把 GreedyReplayer 也包装成 PathBuilder concept 的另一种实现，让接口统一？
  - 选项 a：是，外层调用方只看到 PathBuilder 接口；内部 GreedyReplayer 不做 BFS，直接重放
  - 选项 b：否，GreedyReplayer 是独立类，simple search 直接调用
- **倾向**：a。即 PathBuilder 是个 concept，GreedyReplayer / BfsRebuilder / RecursiveTSpinRebuilder 都满足这个 concept；内部实现自由。

---

## 4. 实施阶段（v3，每阶段单 commit，oracle_diff 全程绿）

> 与 v2 实施阶段一致，但每阶段的工作量更小（不再涉及 search 阶段的 parent 链改造）。

- **阶段 1**：抽出 `LandPointKey` + `LandPointPolicy` trait → 迁移 path 的 search 部分
- **阶段 2**：抽出 `NeighborProvider` + `VisitedPolicy` → path search 完整迁移
- **阶段 3**：抽出 `PathBuilder` concept + `BfsRebuilder` → path make_path 迁移
- **阶段 4**：迁移 simulate（NeighborProvider 的 20G 桥接增强）
- **阶段 5**：写 `tag_landpoint_collision_dump` 验证 Q5 → 决定 key 形态
- **阶段 6**：迁移 tag（key 决策落地 + `RecursiveTSpinRebuilder`）
- **阶段 7**：实现 `Coarse20gNeighbors` + `NoVisited` + `OnDrop` + `GreedyReplayer` → 迁移 simple
- **阶段 8**：清理 oracle search 引用，oracle 仅作为对拍基准

---

## 5. 状态

- branch: `flip-bits-clean`
- HEAD: `6758178` (Phase 0 已 commit 未 push)
- 设计 v3 等待用户回应 Q1-Q7
