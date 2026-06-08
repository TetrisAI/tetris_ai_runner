# Search 框架 Hook 化最终设计（v4-final）

> 状态：用户已拍板所有问题，进入实施阶段。

---

## 用户拍板要点

| Q | 决策 |
|---|---|
| Q1 | BfsEngine 作为统一图遍历框架，search 和 make_path 都基于它 |
| Q2 | simple make_path 保持独立函数（不套进 BfsEngine） |
| Q3 | hook 用编译期 template trait |
| Q4 | 20G 收敛进 NeighborProvider 内自适应（节点级 `low >= map.roof` 判定） |
| **Q5** | **去重方式（key/dedup policy）作为 hook，由各 Search 自己定，不预先规定 tag 是否复合 key** |
| **Q6** | **search 与 make_path 的 NeighborProvider 各自独立**：search 目标是性能 + 不遗漏落点；make_path 邻居由各 Search 自行决策 |
| **Q7** | **Visitor 必须支持 STOP**（make_path 命中 target 必须立刻退出） |

---

## v4-final 框架结构

### BfsEngine 模板签名

```cpp
template <
    typename NeighborProvider,    // 产生邻居（带 action 标签）
    typename DedupPolicy,         // hook：去重策略，由各 Search 决定
    typename Visitor              // 访问回调：返回 Continue/Stop
>
class BfsEngine
{
    template <typename Queue>
    void run(map, start, queue) {
        DedupPolicy::on_enqueue(start, /*from*/nullptr, /*action*/'\0');
        queue.push(start);
        while (!queue.empty()) {
            auto node = queue.front(); queue.pop();
            if (Visitor::on_node(node, map) == VisitResult::Stop) return;
            NeighborProvider::for_each(node, map, [&](neighbor, action) {
                if (DedupPolicy::on_enqueue(neighbor, node, action))
                    queue.push(neighbor);
            });
        }
    }
};
```

### 各 search 实现的 hook 配置（v4-final 矩阵）

| 实现 | search NeighborProvider | search DedupPolicy | search Visitor | make_path 实现 |
|---|---|---|---|---|
| **simple** | `Coarse20gNeighbors`（左到底/右到底/旋/drop） | `NoDedup`(DAG) | `LandPointCollector::OnDrop` | **独立函数（贪婪线性回放）** |
| **path** | `AdaptiveNeighbors`（按 `low>=roof` 自适应） | `IndexedDedup` | `LandPointCollector::OnImmobile` | BfsEngine + `FullActionNeighbors` + `ParentTrackingDedup` + `TargetHitVisitor` |
| **simulate** | `AdaptiveNeighbors+Bridge` | `IndexedDedup` | `LandPointCollector::OnImmobile` | 同 path |
| **tag** | `FullNeighbors`（无 180/multi-LR） | **由 tag 自行决定（可能复合 key）** | `LandPointCollector::OnImmobileWithTSpin` | BfsEngine + 同上 + 递归补旋后处理 |

### 关键 concept 定义

**1. NeighborProvider concept**
```cpp
template <typename T>
concept NeighborProvider = requires(T t, TetrisNode const* node, TetrisMap const& map, auto sink) {
    T::for_each(node, map, sink);   // sink(neighbor, action_char)
};
```

**2. DedupPolicy concept**
```cpp
template <typename T>
concept DedupPolicy = requires(T t, TetrisNode const* node, TetrisNode const* from, char action) {
    { t.on_enqueue(node, from, action) } -> std::convertible_to<bool>;  // true 表示首次入队
    t.clear();
};
```

具体实现：
- `NoDedup`：永远返回 true（simple 用）
- `IndexedDedup`：按 `node->index` 标记，首次返回 true（path/simulate 用）
- `ParentTrackingDedup`：在 IndexedDedup 基础上记录 (from, action)，供 make_path 回溯
- `TagDedup`：tag 自行决定（待 `tag_landpoint_collision_dump.cpp` 验证后定）

**3. Visitor concept**
```cpp
enum class VisitResult { Continue, Stop };
template <typename T>
concept Visitor = requires(T t, TetrisNode const* node, TetrisMap const& map) {
    { t.on_node(node, map) } -> std::same_as<VisitResult>;
};
```

具体实现：
- `LandPointCollector<Predicate>`：永不 Stop，匹配 Predicate 时记录到 cache
- `TargetHitVisitor`：命中 target_index_filtered 时 Stop

---

## 实施阶段（每阶段单 commit，oracle_diff 全程绿）

### 阶段 1（next）：建 BfsEngine 骨架
- 文件：`src/bfs_engine.h`（仅模板与 concept 定义，无具体 hook 实现）
- 不改动任何现有 search
- 新增到 CMake 但暂时不被任何 target 链接（或链接但不调用）
- oracle_diff 不会受影响（未触碰）

### 阶段 2：迁移 path::search → BfsEngine
- 实现 `IndexedDedup` + `LandPointCollector::OnImmobile` + `AdaptiveNeighbors`
- 新框架版本 search 与 oracle path 行为完全等价
- oracle_diff 必须绿

### 阶段 3：迁移 path::make_path → BfsEngine
- 实现 `ParentTrackingDedup` + `TargetHitVisitor` + `FullActionNeighbors`
- oracle_diff 必须绿

### 阶段 4：迁移 simulate
- 在 NeighborProvider 中加桥接增强
- oracle_diff 必须绿

### 阶段 5：写 `tools/tag_landpoint_collision_dump.cpp`
- 一次性调研工具，验证 tag 是否真的需要复合 key
- 拍板 TagDedup 形态

### 阶段 6：迁移 tag
- 落地 TagDedup + 递归补旋 PathPolicy
- oracle_diff 必须绿（注意 R<=4 部分对拍）

### 阶段 7：迁移 simple search
- `Coarse20gNeighbors` + `NoDedup` + `LandPointCollector::OnDrop`
- simple make_path 保持独立函数（贪婪回放）
- oracle_diff 必须绿

### 阶段 8：清理 oracle search 引用
- 生产代码不再依赖 `oracle/search_*`
- oracle 仅作为对拍基准

---

## 状态

- branch: `flip-bits-clean`
- HEAD: `6758178` (Phase 0 已 commit 未 push)
- v4-final 已拍板，**进入阶段 1：建 BfsEngine 骨架**
