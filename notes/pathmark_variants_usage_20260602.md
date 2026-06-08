# PathMark 家族实现与使用场景盘点（2026-06-02）

## 范围与前提

- 范围包含：当前沙盒仓库 `tetris_ai_runner`、仓库内历史调研记录、涉及 `PathMark` 的设计/实施笔记。
- 结论基于当前可读到的仓库内容。
- 需要先强调：**当前沙盒 `dev@6cf6726` 的 `src/` 并没有这些新类型的已落地代码定义**；下面盘点的是仓库内可确认存在的 **PathMark 家族实现方案 / 演进实现**。

## 总结先行

`bb::PathMark` 不是单一实现，仓库记录里出现过/规划过的主要有 4 类：

1. **`bb::PathMark`**
   - 全量 L3 档：存 `visited + prev + op`
   - 适合需要完整回溯路径、且不需要按 piece 分桶的场景

2. **`bb::PathMarkBit`**
   - L1 档：只存 `visited`
   - 适合只做 BFS 去重、不需要回溯前驱/操作的场景

3. **`bb::PathMarkPath`**
   - L3 档：存 `visited + prev + op`
   - 语义上等同“无 piece 分桶的路径回溯专用 PathMark”
   - 是把原来的通用 `bb::PathMark` 明确类型化后的版本

4. **`bb::PathMarkPathT`**
   - L3-stale 档：存 `visited + prev + op`，并且**按 piece 分桶**
   - 专门服务于 tag 的 `search_t_native`
   - 需要保留 oracle 的“同 piece 跨调用 stale 可见、跨 piece 隔离”语义

另外还有一种 **不叫 PathMark，但职责等价的变体**：

5. **`Run20gMarkSlot`**
   - 手写的 `visited + op + prev` 槽数组
   - 文档里被明确指出是“另一种 PathMark 变体”
   - 当前属于与 `bb::PathMark` 体系重复的特化实现

## 一、`bb::PathMark`：通用全量档

### 它存什么

从 `commit5d_path_mark_bb.md` 和 `path_mark_issues_tracker.md` 看，`bb::PathMark` 是最早的位板版 direct equivalent：

- key：`(r, x_bbox, y_bbox)`
- 数据：
  - `cell_ver_`
  - `cell_prev_`
  - `cell_op_`
- API：
  - `clear`
  - `set_bbox`
  - `get_bbox`
  - 后续还扩展了 `cover_if_bbox`

### 对应旧 oracle 哪一层

- 对应旧 `TetrisNodeMark` / `TetrisNodeMarkFiltered` 的**表结构本体**
- 本质是把“按 `node->index` 寻址”改成“按 `(r, xb, yb)` 寻址”

### 使用场景

适合：
- 需要保存前驱和操作
- BFS 结束后要 `build_path` 回溯按键串
- 单次调用只处理单 piece，入口 `clear()` 就够，不需要跨 piece 分桶

### 文档证据

- `commit5d_path_mark_bb.md`
- `path_mark_issues_tracker.md`
- `extending_search.md`

## 二、`bb::PathMarkBit`：最轻量 visited-only 档

### 它存什么

`path_mark_issues_tracker.md` 明确写：

- `bb::Helpers<RuleSpec>::PathMarkBit`
- 已改成 `std::uint64_t bits_[kWords] + memset clear`
- **仅承载 1 bit/cell 的已访问信息**

也就是说它只回答一个问题：

> 这个 `(r, xb, yb)` 状态来过没有？

不会记录：
- prev
- op
- 路径链

### 使用场景

最适合：
- **search 阶段的非 T BFS 去重**
- 只要防重复入队
- 不需要在 BFS 内部或结束后读取前驱

在 tag 分档计划里，明确指定给：
- `TagSearch1gDedup`
- `TagSearch20gDedup`

### 为什么需要它

因为很多 search 阶段根本不做回溯：
- 只要知道“访问过没”
- 不值得为每个 cell 带 `prev + op`

所以它是 **去重最省配版本**。

### 文档证据

- `path_mark_issues_tracker.md:15-20`
- `tag_path_mark_split_plan.md:15-16, 37, 51, 60-62`

## 三、`bb::PathMarkPath`：无 piece 分桶的路径回溯档

### 它是什么

从 `tag_path_mark_split_plan.md` 看，这是把“完整路径回溯档”显式命名出来后的类型：

- 存 `cell_ver_ / cell_prev_ / cell_op_`
- 无 piece 分桶
- 用于普通 `make_path` 场景

它和早期通用 `bb::PathMark` 的关系，本质上是：

- **能力几乎同档**
- 但职责更单一、更语义化
- 用来把“通用 PathMark”拆成更清晰的家族成员

### 使用场景

适合：
- 单次 `make_path` 调用
- 单 piece BFS
- 入口 clear 后即可
- 需要 `set` + `get` 回溯前驱链
- 不需要跨 piece 隔离

在 tag 计划中分配给：
- `TagMakePath1gDedup`
- `TagMakePath20gDedup`

### 为什么不直接继续用 `bb::PathMark`

因为设计目标已经从“一个大一统类型”变成“按需求选档”：
- 只去重 → `PathMarkBit`
- 普通路径回溯 → `PathMarkPath`
- T-search stale 特殊语义 → `PathMarkPathT`

### 文档证据

- `tag_path_mark_split_plan.md:18-19, 41-42, 52-53, 65-67`

## 四、`bb::PathMarkPathT`：带 piece 分桶的 T-search 特化档

### 它为什么特殊

这是 PathMark 家族里最特别的一个。

`tag_path_mark_split_plan.md` 和 `path_mark_issues_tracker.md` 指出，tag 的 `search_t_native` 不是普通 BFS 去重，而是要保留 oracle 的一类特殊行为：

- **同 piece 跨 search() 调用 stale 可见**
- **跨 piece 残留必须隔离**

也就是说它不是简单的“clear 就完事”。

### 它存什么

和 `PathMarkPath` 一样，仍然是完整 L3：
- `cell_ver_`
- `cell_prev_`
- `cell_op_`

但额外多了：
- **piece 分桶**
- `set_active_piece(t)`
- `clear()` 只清当前桶
- `set/cover_if/mark/get` 都隐式作用在 active piece 桶上

### 使用场景

专门用于：
- **tag 的 `search_t_native`**
- 需要模拟 oracle T 搜索里的 stale / cover_if 语义
- 需要 emit 阶段读取 `last/op`
- 又不能让不同 piece 的旧残留互相污染

### 为什么普通 `PathMark` 不够

普通 `PathMark` 没 piece 分桶。

对于 tag T-search：
- 如果直接共用一个桶
- clear 之后虽然 version 不命中
- 但 `cell_prev/op` 的残留会以“最后一次写该 cell 的 piece”为准
- 这和 oracle 要求的“同 piece stale 可见、跨 piece 隔离”不一致

所以 `PathMarkPathT` 是 **为了复刻 oracle 的特殊历史语义** 单独拆出来的。

### 文档证据

- `tag_path_mark_split_plan.md:17, 28-31, 38-40, 52, 63-64, 82-83`
- `path_mark_issues_tracker.md:22-30, 34-36`

## 五、`Run20gMarkSlot`：不叫 PathMark，但被明确认定为 PathMark 变体

### 它是什么

`path_mark_issues_tracker.md` 明确写：

- `Run20gMarkSlot` 是 `visited + op + prev` 的手写槽数组
- 位于 `src/search_path.h:239-300`
- 被认定为“另一种 PathMark 变体”

### 为什么算 PathMark 变体

因为它承载的信息和职责与 L3 PathMark 本质一致：
- visited
- parent
- op
- 用于搜索/回溯

只是它没有走 `bb::PathMark` 的统一接口和版本清理机制，而是单独手写了一套。

### 使用场景

- `PathStrategy::run_piece_20g`
- 20g 特化路径搜索
- 当前是历史演进中留下的特化实现

### 文档里的判断

文档并不把它当“应长期保留的独立体系”，而是说：

- 它和 `bb::PathMark` 重复
- 未来应该统一进 PathMark 多档体系

所以它更像：
- **PathMark 家族之外的等价旁支**
- 而不是正式命名家族成员

### 文档证据

- `path_mark_issues_tracker.md:83-91`

## 六、按使用场景重新归类

### A. 只做 visited 去重，不需要路径回溯

用：
- **`PathMarkBit`**

典型场景：
- tag 非 T 的 1g / 20g search

---

### B. 需要回溯前驱和操作，但单次调用入口 clear 就够

用：
- **`PathMark`**（早期通用形态）
- **`PathMarkPath`**（后期更清晰的命名形态）

典型场景：
- path / simulate 的 make_path
- tag 的 make_path

---

### C. 需要回溯前驱和操作，还要保留 T-search 的 stale 特殊语义

用：
- **`PathMarkPathT`**
- 或早期尚未拆档时，由 tag 自己单独持有的 `t_mark_ : PathMark`

典型场景：
- tag 的 `search_t_native`

---

### D. 还没统一进 PathMark 家族、但功能等价的旧特化实现

用：
- **`Run20gMarkSlot`**

典型场景：
- path 20g 搜索内部特化

## 七、一个最实用的区分方法

你可以把 PathMark 家族按“信息量”理解成三档：

1. **L1：只记 visited**
   - `PathMarkBit`

2. **L3：记 visited + prev + op**
   - `PathMark`
   - `PathMarkPath`

3. **L3-stale：记 visited + prev + op + piece 分桶 / stale 语义**
   - `PathMarkPathT`

而 `Run20gMarkSlot` 是：
- 功能上接近 L3
- 但实现上是独立手写分支

## 八、最后一句结论

如果问题是“`bb::PathMark` 有哪些实现、各自用在哪”：

- **通用全量档**：`PathMark` —— 普通路径回溯场景
- **轻量 visited-only 档**：`PathMarkBit` —— 只做 search 去重
- **普通路径回溯专用档**：`PathMarkPath` —— make_path 类场景
- **T-search 特化档**：`PathMarkPathT` —— tag `search_t_native`，保 stale + piece 隔离
- **等价旁支**：`Run20gMarkSlot` —— path 20g 内部手写 L3 变体

其中最关键的区分维度不是名字，而是：

> **要不要存 prev/op、要不要保 stale、要不要按 piece 分桶。**
