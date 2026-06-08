# Search 框架 Hook 化设计：思考与开放问题（v2）

> **v2 关键修正**：用户指出 20G 也应纳入 hook 体系。重读 oracle 源码后确认，**simple 也不是独立范式**——它是 hook 配置的一个极端实例。
>
> 上一版思考（已废弃 §2）：把 simple 划为独立 GreedySearcher。错。
>
> 状态：**重新调研已完成，等待用户对开放问题表态后再进入实现**。

---

## 1. v1 → v2 的认知刷新

### 关键事实（重读 `oracle/search_path.cpp:187-271`）

```cpp
if (node->land_point != nullptr && node->low >= map.roof)
{
    // 20G 分支：land_point 缓存展开 + 跨行桥接 + 屋顶 BFS
    for (...)  push to land_point_cache_  // line 189-197
    // 跨行桥接（simulate 风格的「last_node」逻辑！）
    for (last_node 相邻横向距离 1，垂直距离 > 1)
        push 中间检查点  // line 198-226
    // 然后做 BFS，邻居判定要求 !node->open(map)
    do { ... !node->open(map) && check(map) ... } while (...)
}
else
{
    // 1G 分支：全动作 BFS（l/r/L/R/x/z/c/d/D）
    do { ... node->check(map) ... } while (...)
}
```

**这意味着**：

1. **path 内部就已经按 piece 当前状态在两套 NeighborProvider 间切换**——`low >= map.roof` 是运行时判定。
2. **search_simulate 的「跨行桥接」其实在 search_path 里就已经存在了！** 之前 v1 里说「simulate 增加跨行桥接」其实不准确——重读发现 path 里就有（line 198-226），simulate 是把这个逻辑做得更完整。
3. **search_simple 看起来"非 BFS"，其实是 hook 配置的极端形态**：
   - 邻居集合：`左走到底 + 右走到底 + 逆时针旋转 + drop` （4 类粗粒度邻居）
   - 邻居图是 DAG（旋转单向、左右走到端、drop 单向），**因此不需要 visited**
   - land_point 判定 = drop 终点
   - 20G 高空时复用 `node->land_point` 缓存（与 path 20G 分支同源）

### 修正：所有 4 个 search 实现实际上是「同一框架 + 不同 hook 配置」的实例

| Hook 维度 | simple | path (1G) | path (20G) | simulate | tag |
|---|---|---|---|---|---|
| **邻居集合** | 粗粒度 4 类 | 全细粒度 | 屋顶 BFS | 屋顶 BFS + 桥接增强 | 全细粒度（无 180/multi-LR） |
| **visited 维护** | 否（DAG） | 是 | 是 | 是 | 是 |
| **land_point 判定** | drop 终点 | `!move_down->check` | `!open && !move_down->check` | 同 path | 同 path + TSpin |
| **land_point 标记** | 无 | 无 | 无 | 无 | TSpinType |
| **make_path 风格** | 贪婪重放 | BFS 溯源 | BFS 溯源 | BFS 溯源 | 递归补旋转 |
| **20G 触发** | 总用 land_point 缓存 | 不用 | 用 land_point 缓存 | 用 land_point 缓存 | 不区分 |

> 同一份代码已经印证了"20G 是 hook 的一种"——它就是邻居生成器的一个分支。

---

## 2. v2 设计：完全统一的 BfsSearcher + Hook 体系

> 不再做「拓扑分层」。所有 search 实现都用同一套 BFS 框架，差异完全靠 hook 表达。

### 2.1 框架骨架（伪代码）

```cpp
template <
    typename NeighborProvider,    // 邻居生成
    typename VisitedPolicy,       // 是否维护 / 怎么去重
    typename LandPointPolicy,     // 何时入队 land_point + 注入 metadata
    typename LandPointKey,        // 去重 key（默认 index_filtered）
    typename PathPolicy,          // make_path 风格
    typename LandPointElement     // 输出元素类型
>
class Searcher
{
    auto search(map, start_node, depth)
    {
        Queue queue;
        queue.push(start_node);
        VisitedPolicy::mark(start_node);

        while (auto node = queue.pop())
        {
            if (LandPointPolicy::is_land_point(node, map))
            {
                LandPointPolicy::record(land_point_cache_, node);
            }
            NeighborProvider::for_each_neighbor(node, map, [&](neighbor, action) {
                if (VisitedPolicy::should_expand(neighbor))
                    queue.push(neighbor);
            });
        }
        return land_point_cache_;
    }
};
```

### 2.2 各实现 = 各 hook 配置

| 实现 | NeighborProvider | VisitedPolicy | LandPointPolicy | PathPolicy |
|---|---|---|---|---|
| `simple` | `Coarse20gNeighbors`（左到底/右到底/旋/drop） | `NoVisited`（DAG） | `OnDrop` | `GreedyReplay` |
| `path` (统一版) | `AdaptiveNeighbors`（按 `low >= roof` 切换） | `IndexedVisited` | `OnImmobile` | `BfsTrace` |
| `simulate` | `AdaptiveNeighbors` + `BridgeAugment` | `IndexedVisited` | `OnImmobile` | `BfsTrace` |
| `tag` | `FullNeighbors`（无 180/multi-LR） | `IndexedVisited` | `OnImmobileWithTSpin` | `RecursiveTraceForTSpin` |

### 2.3 20G 作为 hook：`AdaptiveNeighbors` 的实现

```cpp
struct AdaptiveNeighbors {
    void for_each_neighbor(node, map, sink) {
        if (node->land_point && node->low >= map.roof) {
            // 20G 模式：从 land_point 缓存展开 + 屋顶 BFS（要求 !open）
            roof_bfs_neighbors(node, map, sink);
        } else {
            // 1G 模式：全动作邻居
            full_neighbors(node, map, sink);
        }
    }
};
```

**关键洞察**：`AdaptiveNeighbors` 的内部 if 是按 piece 当前状态切换，意味着**同一次 search 中，BFS 的不同节点可以走不同邻居策略**——这是 oracle path 已经验证过的设计。

### 2.4 simple 的"非 BFS"外观如何统一进 BFS 框架

simple 的关键在两点：
1. **邻居图是 DAG**（左走到底→drop、右走到底→drop、旋转一圈停在原点、drop 不可逆），所以队列即便不去重也不会死循环。
2. **没有外层 BFS 队列**——仔细看 `search_simple.cpp:82-99`，外层是 `do...while (rotate)` 即旋转一圈，内层左/右各一个 while。

把 simple 用 BFS 框架表示：
- 起始节点入队
- NeighborProvider 一次性吐出所有左到底点、右到底点、下个旋转、drop 终点
- VisitedPolicy = NoVisited（不需要 mark）
- LandPointPolicy = OnDrop（直接判定 drop 终点）
- 实测开销 ≈ 原 simple（DAG 邻居 + 无 visited，循环步数相同）

**性能担忧的处理**：
- NoVisited + 编译期 inline → 与原 simple 同等开销
- 如果 hook 抽象引入额外 indirection，**整体测试 oracle_diff 通过 + 用 perft_movegen 跑微基准**确保不退化

---

## 3. 提交给用户的开放问题（v2 重排）

### Q1：是否同意「所有 search 都用同一 BFS 框架，差异完全靠 hook」？
v1 我倾向把 simple 独立。v2 重读源码后改为：**simple 也进 hook 体系**。
- 风险：性能可能因虚函数 / 间接调用退化。
- 缓解：所有 hook 用编译期模板（trait），不引入虚函数；用 perft_movegen 微基准守门。
- **倾向**：是。统一框架 + 性能验证守门。

### Q2：hook 用编译期 template trait 还是运行时虚函数？
- 编译期 template：性能最优，每个 search 是模板特化；调用方需要选择编译期类型。
- 运行时虚函数：调用方灵活；性能损失。
- **倾向**：编译期 template。tetris_engine 已经大量使用模板，符合现有风格。

### Q3：「20G 切换」放在哪个 hook？
- a. NeighborProvider 内部按 `low >= map.roof` 自适应（参考 oracle path 现状）；
- b. 拆成两个独立 NeighborProvider，外层选择器决定用哪个；
- **倾向**：a。oracle 已验证可行，且 BFS 中可以节点级切换。

### Q4：simple 的「邻居集合粗粒度」（左到底而非 1-step）需不需要 hook 表达？
- 选项 a：把"左走到底"作为 NeighborProvider 的一种邻居类型（与 'L' 一致），simple 用这种邻居 + 不出 'l'/'r' 单步；
- 选项 b：simple 走单独的 NeighborProvider 实现；
- **倾向**：a。复用 oracle path 已有的 'L'/'R' multi-LR 邻居语义。

### Q5：land_point key 复合化（tag 需要）
- 同 v1：是否做 trait 让 tag 用 `(index_filtered, tspin_tag)` 复合 key？
- **倾向**：是。tag 现状是用 vector<TetrisNodeWithTSpinType>，重复元素的语义需要明确。
- **悬而未决**：tag 实际是否会出现「同 index 不同 tspin_type」的多个落点？需要单独写小工具确认。

### Q6：make_path 是否在 search 阶段存好 parent 链
- 同 v1：是否搜索阶段就维护 parent 链？避免 oracle 的 make_path 重做 BFS 浪费。
- **倾向**：是。tag 的递归补旋转可以在 PathPolicy 里实现，前提是 parent 链可用。

### Q7：SimpleSearcher 的"无 visited"会不会和别人的接口冲突
- VisitedPolicy = NoVisited 时，`should_expand` 永远返回 true，会造成每个邻居都入队。
- 在 simple 的 DAG 邻居图下不会成环，但需要保证 NeighborProvider 不出环（例如旋转一圈后回到起点要终止）。
- **悬而未决**：要不要给 NoVisited 加一个 max_iter 保护？

---

## 4. 实施阶段（v2，每阶段单 commit）

> 每阶段完成后 oracle_diff 必须保持绿。tag 因为 R<=4 与 oracle 完全对拍。

- **阶段 1**：抽出 `LandPointKey` trait + `LandPointPolicy` trait。先迁移 path（最普通的）。
- **阶段 2**：抽出 `NeighborProvider` trait（编译期）+ `VisitedPolicy`。完成 path 完整迁移。
- **阶段 3**：实现 `BridgeAugment` 或在 NeighborProvider 内表达 simulate 桥接。迁移 simulate。
- **阶段 4**：实现 `RecursiveTraceForTSpin` PathPolicy + `(index, tspin)` 复合 key。迁移 tag。
- **阶段 5**：实现 `Coarse20gNeighbors` + `NoVisited` + `OnDrop` + `GreedyReplay`。迁移 simple。
- **阶段 6**：所有 oracle search 已被新 hook 体系覆盖。删除 oracle 引用，oracle 仅保留作为对拍基准。

---

## 5. 需要先验证的小工具

为了拍板 Q5，建议先写个一次性 tool（不进生产）：

```
tools/tag_landpoint_collision_dump.cpp
```
扫描若干 board × piece，统计 tag 模式下「同 index_filtered 是否产生多个 tspin_type」。如果不产生，复合 key 不必要；如果产生，复合 key 必要。

---

## 6. 状态标签

- branch: `flip-bits-clean`
- HEAD: `6758178` (Phase 0 已 commit 未 push)
- 设计 v2 等待用户回应 Q1-Q7
