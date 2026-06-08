# Search 框架 Hook 化设计：思考与开放问题

> 上下文：基于 `oracle_search_behavior_diff.md` 的事实，开始讨论新框架 search 的可扩展设计。
> 状态：**调研完成，等待用户对开放问题表态后再进入实现**。

---

## 1. 调研事实回顾（最关键差异）

| 维度 | simple | path | simulate | tag |
|---|---|---|---|---|
| 算法骨架 | 贪婪迭代（旋转→平移→drop） | 标准 BFS | BFS + 跨行掉落桥接 | BFS + T-piece 特化 |
| 1G / 20G | 仅 20G 友好 | 通过 `open(map)` 区分 | path 之上 + 桥接 | T-piece 走 `search_t` 特化 |
| 邻居动作 | 不含 1-step soft drop / 180 / multi-LR | 全 | 全 | 不含 180 / multi-LR |
| land_point key | `index_filtered` | `index_filtered` | `index_filtered` | `index_filtered`（但 land_point 含 TSpin tag） |
| make_path 策略 | 贪婪重放 | BFS 溯源 | BFS 溯源 | 递归溯源（强制补旋转） |
| 返回类型 | `vector<TetrisNode const*>` | 同左 | 同左 | `vector<TetrisNodeWithTSpinType>` |
| 外部耦合 | `node->land_point` 缓存 | `open(map)` | `node->land_point` + 桥接 | `TetrisMapSnap` + 3-corner |

**事实层面的本质结论**：
1. `simple` 与其他三者**算法骨架真的不同**（不是 BFS）。强行套 BFS 框架会牺牲 simple 的存在意义（极致性能）。
2. `path` / `simulate` / `tag` 算法骨架同源，差异集中在 3 个维度：
   - **邻居生成**（是否产生 180、multi-LR、跨行桥接）
   - **land_point 判定**（是否需要在落地时做 TSpin 标记）
   - **make_path 策略**（普通溯源 vs 递归补旋转）
3. land_point 去重 key 都是 `index_filtered`，**但 tag 的「同 index 不同 TSpin 类型」需要复合 key**——否则会丢解。

---

## 2. 我对设计方向的初步判断

### 2.1 不要做「大而全的 search」
你的判断是对的。理由：
- simple 与 BFS 类是不同算法范式，把它强塞参数化只会让 BFS 路径背着无意义分支。
- search 是开放扩展点，将来可能再加新策略（如启发式搜索、剪枝搜索）。

### 2.2 候选方案：分层 + Hook 二者结合
- **第一层（拓扑层 / Topology）**：决定算法骨架。**不可参数化**，由不同 Searcher 类型承载：
  - `GreedySearcher`（继承 simple 的迭代风格）
  - `BfsSearcher`（继承 path 的 BFS）
  - 将来可加 `AStarSearcher` 等
- **第二层（行为层 / Behavior Hook）**：在已选定拓扑下，控制具体行为。Hook 接口举例：
  - `NeighborProvider`：BfsSearcher 通过它产生邻居（基础动作 + 是否启用 180 / 桥接 / 多步 LR）
  - `LandPointPolicy`：判 land_point + 注入 metadata（如 TSpinType）
  - `LandPointKey`：复合 key（默认 `index_filtered`，tag 模式扩展为 `(index_filtered, tspin_tag)`）
  - `PathRestorer`：BFS 溯源 / 递归溯源
- **第三层（产物层 / Output Trait）**：决定 land_point 容器元素类型。
  - `TetrisNode const *` / `TetrisNodeWithTSpinType` 通过 trait 选择。

### 2.3 哪些维度天然正交、哪些耦合
- **正交**：邻居集合 ↔ land_point key ↔ make_path 风格 ↔ 输出元素类型。
- **耦合**：land_point key 必须能装下 land_point metadata（tag 的 TSpin 字段是产物，但同时也参与去重）。这是 tag 唯一打破正交性的地方。
- **耦合**：simulate 的跨行桥接需要拿到「相邻 land_point 列表」，是个**后处理**步骤而不是邻居扩展（看 `search_simulate.cpp:319` 是在 BFS 结束后做的）。所以可以做成 `PostProcessHook`，而不是混入 NeighborProvider。

### 2.4 simple 怎么放？
两条路：
- **A. 完全独立**：simple 不参与 hook 体系，自成一格 `SimpleSearcher`，外部只通过统一接口（`Searcher` 概念）调用。优点：不污染 BFS 路径；缺点：hook 抽象只对 BFS 系生效。
- **B. 共享一部分接口**：simple 至少复用 `LandPointKey`、`PathRestorer`（虽然它的 path 是贪婪生成的）的概念。

倾向 A：**承认 simple 是不同范式**，hook 体系仅服务 BFS 家族，简单干净。

---

## 3. 提交给用户的开放问题（请逐条回应）

> 这些问题不能由我单方面决定，因为牵涉到长期架构方向。

### Q1：simple 的归属
- 选项 a：simple 完全独立，不进入新 hook 体系；
- 选项 b：simple 强行 hook 化，将 BFS 替换为 「无队列迭代器」 hook；
- **我的倾向**：a。理由：simple 的价值是性能，hook 化必然引入虚函数 / 间接调用。

### Q2：land_point 去重 key 的设计
- 选项 a：永远用 `index_filtered`，TSpin 信息只作为 metadata（接受同 index 不同 tspin 类型时只保留一个）；
- 选项 b：把 key 做成 trait/template，tag 模式下用 `(index_filtered, tspin_tag)`；
- **我的倾向**：b，因为 tag 模式确实需要保留多重落地。但需要确认 tag 的实际语义——同 index 不同 tspin_type 是否真的会发生？（需要回看 oracle）

### Q3：邻居生成的颗粒度
- 选项 a：粗粒度 hook：`vector<Neighbor> NeighborProvider::expand(node, map)`；
- 选项 b：细粒度配置：`bool enable_180`, `bool enable_multi_lr`, `bool enable_kick`；
- 选项 c：编译期 trait（template 参数）；
- **我的倾向**：c，性能敏感路径用编译期 trait，避免运行时分支。

### Q4：simulate 的跨行桥接做成什么 hook？
- 选项 a：`NeighborProvider` 的扩展（BFS 中混入桥接邻居）；
- 选项 b：`PostProcessHook`（BFS 后扫一遍 land_point 集合，补充桥接）；
- **我的倾向**：b，符合 oracle 实际实现位置（`search_simulate.cpp:319`）。

### Q5：make_path 是否要内置在 Searcher 里？
- 选项 a：Searcher 不返回路径，只返回 land_point 集合；调用方按需 `restore_path(start, end, ...)`；
- 选项 b：Searcher 返回 land_point + parent 链；调用方调用 `make_path(land_point)` 还原；
- 选项 c：Searcher 返回完整路径序列；
- **我的倾向**：b，把「搜索」与「路径还原」解耦，但搜索过程中**必须存好 parent 链**才能避免 `make_path` 重做 BFS（oracle 的做法是重做 BFS，浪费）。

### Q6：tag 的 T-Spin 判定时机
- oracle 是在 land_point 入口时调用 `check_ready` 判定；
- 新框架是延后到 land_point 收集完成统一标记，还是即时？
- **影响**：去重 key 的语义。

---

## 4. 接下来如果你同意以上方向，我建议的实施顺序

> 仅供参考，**等你回复后再开 commit**。

- 阶段 1：抽出 `LandPointKey` trait（默认 `index_filtered`，tag 用扩展 key）
- 阶段 2：抽出 `NeighborProvider` trait（编译期）
- 阶段 3：抽出 `PathRestorer`（解决 oracle 的 make_path 重做 BFS 的问题）
- 阶段 4：迁移 path → 新 BfsSearcher
- 阶段 5：迁移 simulate（仅多个 PostProcessHook）
- 阶段 6：迁移 tag（key + LandPointPolicy + 递归溯源 PathRestorer）
- 阶段 7：迁移/独立 simple

每阶段一个独立 commit，每阶段完成后 oracle_diff 必须保持绿。

---

## 5. 待恢复工作时的速查指引

- 调研事实：见 `oracle_search_behavior_diff.md`
- 当前位于 branch `flip-bits-clean`，HEAD = `6758178`（Phase 0 已 commit 未 push）
- 用户最关心的核心问题是「不要做大而全的 search」，hook 思路是否适用、要支持什么 hook
- 等待用户对 Q1-Q6 表态
