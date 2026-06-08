# Search 框架 Hook 化设计：思考与开放问题（v4）

> v4 用户反馈：
> 1. **20G 收敛进 search+Hook 了对吧？** → 是
> 2. **make_path 能否也用 search+Hook 实现？** → 可以，且自然——oracle 自己就这样写的
>
> v3 已废弃。

---

## 1. 用户两个问题的回答

### Q1：20G 是否收敛进 search + Hook？
**是。** v3 → v4 保持：
- 20G 不再是独立 search 实现，而是 `NeighborProvider` 的内部自适应分支（按 `low >= map.roof` 节点级判断）
- 这是 oracle path 现状（`search_path.cpp:187`），新框架直接沿用即可

### Q2：make_path 能否用 search + Hook 实现？
**可以，而且这样设计更优雅。**

**事实证据**（重读 oracle）：
- `oracle/search_path.cpp:39-174` 的 `make_path` 骨架：`node_search_` 队列 + `node_mark_` 访问标记 + 三类旋转邻居 + 'l'/'r'/'L'/'R'/'d'/'D' 平移邻居 + 「找到 target 时回溯」
- `oracle/search_path.cpp:177-322` 的 `search` 骨架：**完全相同的队列/标记/邻居遍历**，只是终止条件是「队列耗尽」、动作是「记录 land_point」

**唯一差别**：

| 维度 | search() | make_path() |
|---|---|---|
| 终止条件 | 队列耗尽 | 命中 target_landpoint |
| 节点访问时做什么 | 检查 land_point 并记录 | 检查 target 并构造路径 |
| 邻居生成 | 同 | 同（实际上 simple 不同，详见下一节） |
| 是否记录 parent + action | 否（不需要） | 是（需要回溯） |
| 邻居粗细 | 可粗（粒度无所谓） | 必须细（动作要被 AI 重放） |

→ **make_path = search 的一种特化**：把 search 的 `LandPointPolicy::is_land_point` 换成 `target_predicate`，把 `LandPointPolicy::record` 换成 `parent.set(node, action)`，加一个「命中 target 立即返回」的早退分支。

---

## 2. v4 设计：统一的 BFS 框架，search 与 make_path 都是它的特化

### 2.1 统一骨架

```cpp
template <
    typename NeighborProvider,    // 决定从一个 node 产生哪些邻居（含 action 标签）
    typename VisitedPolicy,       // 是否/如何去重
    typename Visitor              // 节点访问时的语义（land_point 收集 / target 命中 / 路径回溯）
>
class BfsEngine
{
    auto run(map, start_node)
    {
        Queue queue; queue.push(start_node);
        VisitedPolicy::on_visit(start_node, /*from*/nullptr, /*action*/'\0');

        while (auto node = queue.pop())
        {
            if (Visitor::on_node(node, map) == STOP) return Visitor::result();

            NeighborProvider::for_each(node, map, [&](neighbor, action) {
                if (VisitedPolicy::on_visit(neighbor, node, action))
                    queue.push(neighbor);
            });
        }
        return Visitor::result();
    }
};
```

### 2.2 search 与 make_path 的 hook 配置

| | search | make_path |
|---|---|---|
| **NeighborProvider** | 性能导向：可粗粒度（如 simple 的"左到底"） | 重放导向：必须细粒度（'l' 单步） |
| **VisitedPolicy** | `IndexedVisited`（仅 mark 进队，不存 from/action） | `ParentTracking`（mark + 记录 from + action） |
| **Visitor** | `LandPointCollector`（永不停 + 收集 land_point） | `TargetHit`（命中 target 立即停 + 回溯） |

### 2.3 关键观察：NeighborProvider 在 search 与 make_path 之间**可以不同**

oracle simple 印证：
- search 用「左到底/右到底/旋/drop」（4 类粗粒度，不需要 visited）
- make_path 用「'r'/'l' 单步 + 'z' 旋转 + 'D' drop」（细粒度，贪婪重放，不用 BFS）

新框架沿袭：

| 实现 | search NeighborProvider | search VisitedPolicy | make_path NeighborProvider | make_path VisitedPolicy |
|---|---|---|---|---|
| **simple** | `Coarse20gNeighbors` | `NoVisited`(DAG) | `RotLR1Step`(rlz D) | **`NoVisited`/特殊：贪婪重放，不走 BFS** |
| **path** | `AdaptiveNeighbors` | `IndexedVisited` | `FullActionNeighbors` | `ParentTracking` |
| **simulate** | `AdaptiveNeighbors+Bridge` | `IndexedVisited` | `FullActionNeighbors` | `ParentTracking` |
| **tag** | `FullNeighbors` (无 180/multi-LR) | `IndexedVisited` | `FullActionNeighbors` (T-piece 特化) | `ParentTracking` + 递归补旋 |

注意 simple 的 make_path 实际上是**贪婪重放**（不是 BFS），它要么：
- a. 通过把 BfsEngine 退化成"邻居图遍历器"接口适配（NoVisited + 顺序邻居），形态上勉强算 BFS
- b. 直接是独立的非 BFS 实现，不进 BfsEngine

倾向 b：承认 simple make_path 是不同算法（贪婪线性回放），但与 search 的"统一框架"无关。

### 2.4 为什么不让 simple search 也走"BFS+Visitor"？

可以走，且开销可接受：
- NoVisited 编译期 inline 后无开销
- 邻居图 DAG 自带终止
- 与原 simple 实测开销基本一致

→ 收敛：**simple search 也用 BfsEngine 框架，差别仅是 hook 配置**。

---

## 3. 抽象层级回顾

```
                ┌────────────────────────────┐
                │       BfsEngine<...>       │
                │   (统一图遍历框架，编译期)   │
                └────────────────────────────┘
                              │
        ┌─────────────────────┼─────────────────────┐
        │                     │                     │
┌───────▼────────┐   ┌────────▼────────┐  ┌─────────▼──────────┐
│ search()       │   │ make_path()     │  │ (将来)其他 BFS 任务  │
│ Visitor=Collect│   │ Visitor=TargetHit│ │ 如剪枝评估、可达性  │
│ Visited=Mark   │   │ Visited=Parent  │  │ 等等                │
└────────────────┘   └─────────────────┘  └────────────────────┘
```

**带来的好处**：
1. 一处 BFS 代码，所有 BFS 任务共享（消除 oracle 中 search/make_path 重复的队列+标记代码）
2. NeighborProvider 可以在 search/make_path 之间共用（path/simulate/tag），也可以分开（simple）
3. 将来加新 BFS 任务（例如剪枝评估、邻居枚举工具）零成本

### simple make_path 的特例处理

simple 的 make_path 是贪婪线性回放（旋转→右移→左移→drop），与 BFS 无关。处理方式：
- `simple::make_path` 直接是独立函数（保留 oracle 实现风格）
- 或：包装成 PathBuilder concept 的一种实现，对外接口统一
- **倾向**：后者。所有实现都满足同一个 `PathBuilder<S>::make_path(start, target, map)` concept；内部 simple 走贪婪回放，其他走 BfsEngine + 特化 Visitor。

---

## 4. 提交给用户的开放问题（v4 收敛）

### Q1：BfsEngine 是否作为统一图遍历框架，search 和 make_path 都基于它？
- **倾向**：是。oracle 自己印证骨架同源。

### Q2：simple make_path 的「贪婪回放」是否要套进 BfsEngine？
- 选项 a：套进去（NoVisited + 特殊 Visitor）→ 接口完全统一，但语义不自然
- 选项 b：simple make_path 独立实现，但满足 PathBuilder concept → 接口统一，内部自由
- **倾向**：b

### Q3：编译期 template trait？
- **倾向**：是。

### Q4：20G 切换在 NeighborProvider 内自适应？
- **倾向**：是。沿用 oracle path 现状。

### Q5：tag 是否需要 `(index, tspin_tag)` 复合 key？
- **倾向**：先写 `tools/tag_landpoint_collision_dump.cpp` 验证再定。

### Q6：search 与 make_path 共享 NeighborProvider 还是分开？
- 共享：path / simulate / tag（这三个 oracle 中也是共享的）
- 分开：simple（粗粒度 search vs 单步 make_path）
- **倾向**：concept 层不强制，由具体实现决定（path 共享，simple 分开）

### Q7：BfsEngine 暴露的 Visitor concept 是否包含「STOP/CONTINUE」语义？
- search 永不 STOP；make_path 命中 target 即 STOP
- **倾向**：是。Visitor::on_node 返回 enum {Continue, Stop}

---

## 5. 实施阶段（v4，每阶段单 commit，oracle_diff 全程绿）

> 本质上路径就是先建 BfsEngine 框架，再让 search 和 make_path 都收敛到它上面。

- **阶段 1**：建 `BfsEngine<NeighborProvider, VisitedPolicy, Visitor>` 模板类（仅骨架，无具体 hook）
- **阶段 2**：实现 `IndexedVisited` + `LandPointCollector` Visitor + `AdaptiveNeighbors` → 迁移 path search
- **阶段 3**：实现 `ParentTracking` VisitedPolicy + `TargetHit` Visitor + `FullActionNeighbors` → 迁移 path make_path
- **阶段 4**：迁移 simulate（NeighborProvider 加桥接）
- **阶段 5**：先写 `tag_landpoint_collision_dump.cpp` 验证 Q5 → 决定 key 形态
- **阶段 6**：迁移 tag（key 决策落地 + T-piece 特化 NeighborProvider + 递归补旋 PathBuilder）
- **阶段 7**：迁移 simple search（`Coarse20gNeighbors` + `NoVisited` + `LandPointCollector::OnDrop`）
- **阶段 8**：实现 simple make_path（贪婪回放，PathBuilder concept 一员，不走 BfsEngine）
- **阶段 9**：清理 oracle search 引用，oracle 仅作对拍基准

---

## 6. 状态

- branch: `flip-bits-clean`
- HEAD: `6758178` (Phase 0 已 commit 未 push)
- 设计 v4 等待用户回应 Q1-Q7
