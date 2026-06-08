# dev 分支重新调研（HEAD=4ef9e88，相对 oracle=e3b909b）

- 时间：2026-06-02 18:05+
- 仓库：`tetris_ai_runner`
- 调研分支：`dev`
- 当前 HEAD：`4ef9e88ed7c02f1a5e2b9da741d2ddecfcbc57f1`
- oracle 基线：`e3b909bf8e1df1e10231bda78c7e2227a5b0f5ab`（`origin/flip-bits`）
- 工作区状态：仅未跟踪 `notes/`
- 相对 oracle 提交数：`157`
- diff 规模：`164 files changed, 23632 insertions(+), 4452 deletions(-)`

---

## 1. 一句话结论

当前 `dev@4ef9e88` 已经**明显超出“仅做 bitboard movegen”阶段**。

它相对 oracle 的主线演进是：

1. 先把 Rule / Shape / Map / MoveGen 做成编译期 + 位板基础设施；
2. 再把 `MoveGenSearch` 的 `search / make_path / 20g` 全部改成 `BBState` 原生；
3. 继续把 hook、filtered index、spawn、piece pool 等全都从 SRS-7 / `TetrisContext` 依赖中抽离；
4. 再把旧搜索器物理归档到 `oracle/`，生产代码转到 `src/` 新策略体系；
5. 最终落成 **Searcher + Strategy + Context Mixin + Hook + bb_bfs_engine** 的可插拔搜索框架；
6. 顶层已经把 **七类 search** 统一到位板后端，`oracle/` 主要只剩对拍/归档用途。

所以，**当前位板化阶段不是“只完成 movegen”**，而是已经推进到：

- `simple / simulate / path / tag` 均有位板原生 strategy；
- `tspin / aspin / cautious` 也已通过顶层 facade 接到位板搜索体系；
- 旧 master-graph BFS 主要被挪去 `oracle/` 做对照，不再是生产主路径。

---

## 2. 相对 oracle 的总体架构变化

### 2.1 oracle 时代（基线）

`e3b909b` 时的主结构还是：

- `src/tetris_core.*`：`TetrisContext / TetrisNode / TetrisMap / TetrisNodeMark` 等旧核心；
- `src/search_*.cpp/.h`：各家搜索器直接围绕指针图、`node->index`、`land_point`、`index_filtered` 工作；
- Rule 还是运行时桥接式写法，搜索器和规则、节点图绑得比较紧。

### 2.2 当前 dev 的分层

当前 `dev@4ef9e88` 形成了 4 层结构：

1. **`core/`：旧核心数据模型保留层**
   - `core/tetris_core.h/.cpp`
   - 保留 `TetrisNode / TetrisMap / TetrisContext / TetrisEngine` 等旧上层接口；
   - 供 AI、历史调用方和桥接层继续使用。

2. **`oracle/`：旧搜索器归档层**
   - `oracle/search_path.*`
   - `oracle/search_simulate.*`
   - `oracle/search_simple.*`
   - `oracle/search_tag.*`
   - `oracle/search_tspin.*`
   - `oracle/search_aspin.*`
   - `oracle/search_cautious.*`
   - 这些文件现在更像“基线实现 / diff oracle / 行为说明书”，不再是生产主路径。

3. **`src/`：生产位板搜索框架**
   - `tetris_rule_spec.h / tetris_shape.h / tetris_map.h / tetris_simd.h / tetris_movegen.h`
   - `bb_state.h / bb_bfs_engine.h / movegen_context.h / movegen_hook.h / movegen_searcher.h / movegen_strategy.h`
   - `search_path.h / search_simulate.h / search_simple.h / search_tag.h`
   - `search_tspin.h / search_aspin.h / search_cautious.h`

4. **`tests/` + `tools/`：对拍与回归验证层**
   - `tests/*_diff.cpp` 基本覆盖七类 search；
   - `tests/_diff_harness.h` 是统一对拍骨架；
   - `tests/perft_movegen.cpp`、`tools/tag_landpoint_collision_dump.cpp`、`tools/aspin_dump.cpp` 等用于专项验证。

核心判断：**生产实现已经从“旧引擎+局部 movegen 替换”演进为“旧核心外壳 + 新位板搜索内核”**。

---

## 3. 提交历史上的主线分期

下面按 `e3b909b..4ef9e88` 的 157 个提交做主题分期。

### 阶段 A：RuleSpec + 位板基础设施 + 初代 MoveGen（提交 46e472f ~ 6cf6726）

这一段就是你之前已调研过的“早期 dev 主体”：

- `46e472f`：引入 compile-time `RuleSpec / OpDesc / WallKickList`
- `80ba299` ~ `315de50`：把各 rule 迁到编译期描述
- `6ffe243` ~ `7669ce9`：建立 `Map<W,H>`、位板 primitive、清行等基础设施
- `eb0cf22`：引入 cobra 风格 MoveGen BFS
- `a4bf720 / c9e3bb6 / 7f2d8b7 / ba9b42f`：把 T-Spin 检测逐步对齐 master 语义
- `7bdcacd` ~ `6cf6726`：通过 `oracle_diff` / `perft_movegen` 做早期对拍

此阶段结论：**先把“规则几何 + 位板落点枚举”做出来**。

### 阶段 B：MoveGenSearch 深度位板化（42923c1 ~ ebd8fcc）

这一段把搜索真正推进到 `BBState` 世界：

- `42923c1`：AI engine 先接上 `MoveGenSearch` 适配层
- `335cc1c`：make_path visited 表换成位板 `PathMark`
- `349e298 / a4620ec / d207399`：前驱、队列、邻居全面换成 `BBState`
- `08a6530 / 3cc9347 / 988c03a`：20g search / 20g make_path 接入 bbox-native BFS
- `a144b2e`：20g T BFS 里的 `cover_if` 旋转覆盖语义也搬过来
- `4616457 / 7913054 / b73a520`：逐步砍掉对 `TetrisContext`、`TetrisMapSnap` 的依赖
- `ebd8fcc`：压缩 `BBState` / mark slot 坐标到 `int8_t`

此阶段结论：**不仅 search()，连 make_path、20g、last-rotate 相关行为都改到 BBState 原生实现了**。

### 阶段 C：去 SRS-7 硬编码 + Hook / Filter / Spawn 泛化（b0d0ce6 ~ ef582c3）

这一段把“能跑”继续收束成“可泛化框架”：

- `b0d0ce6` ~ `dece250`：`RuleSpec::spawn` 进入主线
- `256aa86 / e73c860`：piece pool、LUT 轴、旋转派发从硬编码切成 RuleSpec 推导
- `20aedc2` ~ `5285fde`：`SearchHook` / `DefaultTSpinHook` / `DefaultASpinHook` 成型
- `475eca3 / 39267b5`：用 `piece_filter_index` 替代早期 `kCanonicalR`
- `3b713ce / ef582c3`：用 extreme rule / aspin dump 等做泛化回归

此阶段结论：**bitboard 搜索开始从“给 SRS-7 用的实现”升级成“按 RuleSpec/Hook 装配的框架”**。

### 阶段 D：oracle 归档 + 节点搜索过渡层（6758178 ~ 2c2c9d8）

- `6758178`：旧 oracle 搜索整体归档到 `oracle/`
- `91005aa / 8c0ab54 / f3a5b5e`：先落一个 state-agnostic 的 `bfs_engine.h`，把 node-based search 做过渡重构
- `c6c806a`：生产调用方开始只消费新的 node-based searches，oracle 保留 diff-only
- `a578cc8`：`tetris_core` 从 oracle 中移出到 `core/`
- `6b1d59c / 9be3111 / 17447b7 / d53458e / 2c2c9d8`：把 cautious/path/simulate/tag/simple 的生产消费者逐个迁到 MoveGenSearch / 新体系

此阶段结论：**仓库布局和调用方依赖关系被正式重排**。

### 阶段 E：中立 Context + Strategy/Searcher + bb_bfs_engine（6fce037 ~ 9240004）

这是当前你要求重点看的“新组件大批出现”阶段：

- `6fce037`：`search_hook.h -> movegen_hook.h`
- `7308744`：把 bitboard helper 从 god class 抽到 `bb::Helpers<RuleSpec>`
- `525282b / 59ccbb0 / 3c50f65 / f391095 / d45864c`：把 20g、1g make_path、simulate、simple 逐个换成统一/分层的实现
- `7b6003b`：引入 `MoveGenContext`、`Strategy` 契约、`Searcher` 包装器
- `7a29500`：search strategy 拆成可插拔静态类
- `8be0069`：落地 bitboard-native `TagStrategy`
- `58c846a / 4fef7c3 / 92980d2 / 3ca1d30 / 9240004`：PathMark 家族定型并栈本地化

此阶段结论：**旧 `MoveGenSearch<RuleSpec, Hook>` 神类被拆成一组中立组件**。

### 阶段 F：顶层统一 + diff 协议更新（c8d331e ~ 4ef9e88）

- `c8d331e / 9bfffa0 / 43f8c44 / 7c7e53b`：aspin/cautious/tspin 的旧 master-graph 实现继续下放到 `oracle/`
- `47b8bf8`：bitboard search instantiation 收敛为单个 SearchTag 参数
- `53d79e4`：顶层统一七类 search namespace
- `f7b9b2e`：七类 search diff 标准化
- `4ef9e88`：最新对拍协议从“完全相同搜索结果”放宽成“subset search + 路径 `Dd` 后缀规约后相等”

此阶段结论：**现在仓库关注点已经是“统一外观 + 稳定 diff 协议”，而不是单点功能打补丁**。

---

## 4. 当前最关键的新组件与职责

下面只列这轮用户点名和与之强相关的核心文件。

### 4.1 `src/bb_state.h`

这是当前位板搜索框架的**共享状态定义中心**。

#### 关键类型

1. **`bb::BBState`**
   - 字段：`t / r / xb / yb`
   - 语义：BFS 队列与 mark 表的统一身份坐标
   - 作用：用 `(piece, rotation, bbox-x, bbox-y)` 替代旧时代的 `TetrisNode*` / `node->index`

2. **`bb::CellsKey`**
   - 字段：`uint16_t c[4]`
   - 语义：piece 在棋盘上的绝对占格签名
   - 对应旧语义：`TetrisNode::index_filtered` / `TetrisNodeMarkFiltered`
   - 价值：不再依赖 pointer graph 和 `row` / `data[0..3]` memcmp 巧合

3. **`bb::Helpers<RuleSpec>::PathMark`**
   - 字段：
     - `bits_`：visited 位
     - `cell_prev_[r][cell]`：前驱 `PrevKey{r, xb, yb}`
     - `cell_op_[r][cell]`：到达动作字符
   - 语义：L3 级 mark，既能去重，也能回溯路径
   - 特点：
     - 起点协议改成 **self-prev root protocol**，不再用哨兵
     - `clear()` 直接 `memset(this)`
     - 支持 `cover_if_bbox(...)`，保留 oracle `cover_if` 语义

4. **`bb::Helpers<RuleSpec>::PathMarkBit`**
   - 字段：仅 `bits_`
   - 语义：L1 级 mark，只记 visited
   - 使用场景：Tag search 某些阶段只需要去重，不需要 prev/op 回溯

#### 关键判断

当前源码里，**PathMark 家族已经正式落地**，而且是整套搜索体系共用的基础设施，不再只是计划稿。

---

### 4.2 `src/bb_bfs_engine.h`

这是**位板世界的通用 BFS 骨架**，用来替代分散在不同 search 里的硬编码循环。

#### 关键抽象

- `NeighborProvider`
  - 负责枚举后继状态
- `DedupPolicy`
  - 负责准入与 mark 语义
  - 返回 `EnqueueDecision`
- `Visitor`
  - 负责命中判定、emit、是否提前终止
- `run_bb_bfs(...)`
  - 只保留“入队 / 出队 / 邻居展开 / 访问回调”的共享骨架

#### 当前角色

它是 `run_piece_20g`、`make_path 1g`、`make_path 20g`、`simulate` 等位板 BFS 逻辑的统一底座。

和 `src/bfs_engine.h` 的区别：

- `bfs_engine.h` 是**节点图过渡层**，仍然基于 `TetrisNode*`
- `bb_bfs_engine.h` 是**当前主线**，基于 `BBState`

---

### 4.3 `src/movegen_context.h`

这是**中立 Context 装配层**。

#### 关键结构

1. **`BfsQueueMixin<RuleSpec>`**
   - 字段：`std::vector<bb::BBState> node_search_path_`
   - 用途：为 `make_path` 等 BFS 提供可复用队列缓冲

2. **`StateNodeLutMixin<RuleSpec>`**
   - 字段：`TetrisNode const *state_node_lut_[...]`
   - 作用：把 `BBState -> TetrisNode*` 做静态 LUT 反查
   - 用途：桥接 emit/hook/AI 等仍以 `TetrisNode*` 为输入的历史接口

3. **`MoveGenContext<RuleSpec, Mixins...>`**
   - 本身不定义业务字段
   - 只负责组合 `bb::Helpers<RuleSpec>` 与各个 mixin

#### 核心意义

旧 `MoveGenSearch` 把所有状态塞进一个类里；现在改成：

- **状态** = Context + Mixins
- **算法** = Strategy
- **实例** = Searcher

这说明当前设计目标已经从“能工作”转向“可插拔扩展”。

---

### 4.4 `src/movegen_hook.h`

这是**旋转语义 / spin 语义 / emit 扩展层**。

#### 当前主要 hook 家族

- `NoSpinHook`
- `DefaultTSpinHook`
- `DefaultASpinHook`
- `CautiousHook`
- 以及公共基类 `BaseSpinHook<Derived>`

#### 它负责什么

1. 定义 `Payload` / `LandPoint` 类型协议
2. 在搜索过程中记录 / 计算 T-spin、A-spin、cautious 等特有语义
3. 提供 `apply_emit_1g / apply_emit_20g`
4. 提供 `config_allow_*` 之类的动作开关

#### 架构意义

以前很多 T/mini/aspin 逻辑是“写死在搜索器里”的。
现在变成：

- 搜索器只做几何可达性
- hook 决定如何解释“最后一次旋转”“ready/mini”“spin payload”

这也是 `path / tspin / aspin / cautious` 能在同一骨架上共存的关键。

---

### 4.5 `src/movegen_searcher.h`

这是**运行时包装器**。

#### 作用

把：

- `Strategy<SpinHook, RuleSpec>`
- `MoveGenContext<...>`

封成一个上层仍然好用的搜索对象。

#### 特点

- 成员核心只有一个 `ctx_`
- 所有 `search / make_path / init` 基本都是 inline 转发
- 让上层 `TetrisEngine<Rule, AI, Search>` 不需要知道内部已经拆成 Context + Strategy + Hook

可以把它理解成：**新架构对旧调用方暴露的兼容壳**。

---

### 4.6 `src/movegen_strategy.h`

这是**strategy 契约定义层**。

#### 主要内容

- 规定每个 strategy 需要提供：
  - `Context`
  - `init(...)`
  - `search(...)`
  - `make_path(...)`
- 提供跨 strategy 共享的小工具
- 包含 `detail::common::MakePath1gDedup` 这类公共 dedup 组件

#### 架构意义

这意味着当前仓库已经不再把“path/simulate/simple/tag”看成几份平行复制代码，
而是当成**同一接口下的不同策略实现**。

---

### 4.7 `src/piece_filter_index.h`

这是当前去重语义的重要补丁，容易和早期 `kCanonicalR` 混淆。

#### 它做什么

- 对每个 `(r, xb, yb)` 按实际占格的绝对 footprint 分组
- 给等价 footprint 分配同一个 `filtered_idx`

#### 它替代了什么

- 早期 `MoveGen` 里的 `kCanonicalR + same_geometry`
- 旧 master 里的 `IndexFilter`

#### 为什么重要

早期 `kCanonicalR` 更像“按旋转等价类挑 canonical 代表”；
当前 `piece_filter_index` 才是**更直接、更一般化的 `index_filtered` 等价实现**。

---

## 5. PathMark 家族与 last-rotation：当前源码级结论

这是这轮最值得更新的地方：**当前不是“只有一个 PathMark 方案”**，而是已经有一套明确分工。

### 5.1 `PathMarkBit`：L1，仅 visited

位置：`src/bb_state.h`

- 只存一个 bitset
- `mark_bbox(r, x, y)` 成功表示首次访问
- 不记录前驱、不记录动作

当前主用法：

- `TagStrategy` 的某些 BFS 阶段只需要“到没到过这里”，不需要回溯路径
- 因此使用 `PathMarkBit`，避免用 `PathMark` 带来的大对象成本

### 5.2 `PathMark`：L3，visited + prev + op

位置：`src/bb_state.h`

- `bits_`：visited
- `cell_prev_`：前驱 `(r, xb, yb)`
- `cell_op_`：进入该状态的动作字符

当前主用法：

- `PathStrategy::make_path`
- `SimulateStrategy::make_path`
- `TagStrategy` 里需要升级/回放路径的阶段

这是当前**主线的路径回溯容器**。

### 5.3 “last-rotation” 不是独立第三个容器，而是一层语义机制

这次源码里没有看到一个独立名叫 `LastRotationMark` 的正式类型。
当前“last rotation”语义是拆开的：

1. **PathMark 的 `cell_op_` 链**
   - 记录到达状态时最后一次动作字符
   - 若末步是 `z/c/x`，即可作为“最后一次操作是旋转”的原始证据

2. **`PathMark::cover_if_bbox(...)`**
   - 在 `TagStrategy` 中用于把同一状态从 `' '` 升级成 `'z'/'c'`
   - 对应旧 oracle `TetrisNodeMark::cover_if` 那种“同一格位允许用旋转版本覆盖直落版本”的语义
   - 这是 TAG/T-Spin 路径里保留 last-rotate 语义的关键点

3. **`DefaultTSpinHook` / `DefaultASpinHook` 的 payload 计算**
   - 是否 ready / mini / aspin，不再靠单独 mark 表传递
   - 而是在 hook 内根据搜索过程中的位板信息和 emit 时机计算并附加到 `LandPoint`

### 5.4 与旧 oracle 语义的对应关系

| 旧 oracle 组件 | 当前位板对应 |
| --- | --- |
| `TetrisNodeMark` | `PathMark` |
| `TetrisNodeMark::cover_if` | `PathMark::cover_if_bbox` |
| `TetrisNodeMarkFiltered` / `index_filtered` | `CellsKey` + `piece_filter_index` |
| `node->index` | `(r, xb, yb)` / `BBState` |
| “last rotate” 由节点图路径/标记隐式承载 | `cell_op_` + `cover_if_bbox` + hook payload |

### 5.5 当前主线/兼容层判断

- **主线**：`bb_state.h` 中的 `PathMarkBit / PathMark` + `search_*.h` 中的位板 strategy
- **兼容层**：`core/tetris_core.*` 里的 `TetrisNodeMark*`、`oracle/*` 里的旧 BFS 实现

也就是说，**PathMark 家族已经不是“规划状态”，而是当前生产位板搜索的正式基础设施**。

---

## 6. 当前 search 家族的迁移状态

### 6.1 已有明确位板原生 strategy 的

1. **`PathStrategy`** — `src/search_path.h`
2. **`SimulateStrategy`** — `src/search_simulate.h`
3. **`SimpleStrategy`** — `src/search_simple.h`
4. **`TagStrategy`** — `src/search_tag.h`

这四个是“真正的 strategy 实现体”。

### 6.2 顶层 facade / 组合装配的

1. **`tspin::Search`** — `src/search_tspin.h`
   - 本质上接 `PathStrategy + DefaultTSpinHook`
2. **`aspin::Search`** — `src/search_aspin.h`
   - 本质上接 `PathStrategy + DefaultASpinHook`
3. **`cautious::Search`** — `src/search_cautious.h`
   - 本质上接 `PathStrategy + CautiousHook`

也就是说，当前“七类搜索”顶层接口已统一，但底层并不是七份独立实现，而是：

- `path` 作为几何主干
- `tspin/aspin/cautious` 通过不同 hook 派生语义
- `simple/simulate/tag` 保留各自特化 strategy

这比 oracle 时代“每个 search 自己长一棵完整实现树”更收敛。

---

## 7. 与 cobra-movegen 的对应关系

### 7.1 直接对齐 cobra 的部分

当前 dev 里，以下思路明显是 cobra 风格/同源思路：

1. **位板合法位生成**
   - `movegen::usable_map`
   - 对应 cobra 里按 piece/rotation 生成可放置位图的思路

2. **几何状态 BFS**
   - `bb::BBState` + `run_bb_bfs`
   - 对应 cobra 中围绕 `(rotation, x, y)` 做 reachability BFS

3. **规则/形状编译期化**
   - `RuleSpec` / `shape::*`
   - 对应 cobra 偏静态、按 piece/rotation 查表的实现方式

4. **几何去重与 footprint 视角**
   - `CellsKey` / `piece_filter_index`
   - 对应 cobra 那种更偏“形状/占格签名”而非指针图 index 的思维方式

### 7.2 明显是 tetris_ai_runner 为兼容 oracle 额外加的部分

这些在 cobra 里通常不存在，或者不是同样的负担：

1. **`state_node_lut_` / `state_to_node()`**
   - 目的是把位板状态反查回 `TetrisNode*`
   - 这是给历史 AI / emit / LandPoint 类型兼容用的

2. **`movegen_hook.h` 整套 hook 协议**
   - T-spin / A-spin / cautious 语义、payload、ready/mini 计算
   - 是为了兼容 oracle/AI 上层行为，而不只是做 reachability

3. **`oracle/` 目录与 diff harness**
   - 当前仓库花了大量代码在做“新旧搜索一致性 / 可接受差异”的验证
   - cobra 本身没有这个“背着旧生产协议前进”的包袱

4. **顶层 `Search` facade 与 `TetrisEngine` 接口保持**
   - 当前仓库不是独立 movegen 库，而是要嵌回完整 AI engine

### 7.3 一个更准确的定位

当前 dev 不是“直接搬 cobra”。
更准确地说是：

- **底层 reachability / bitboard / shape / BFS 思路强烈 cobra 化**；
- **上层接口、spin 语义、AI/Engine 兼容、diff 验证，则是 tetris_ai_runner 自己的桥接层**。

---

## 8. 当前位板化已经推进到哪个阶段

### 8.1 已完成的部分

可以明确算“已完成主线迁移”的有：

1. **Rule 编译期化**
   - 各 rule 已通过 `RuleSpec` 暴露几何、spawn、kick

2. **位板地图 / 形状 / MoveGen 基础设施**
   - `tetris_map.h / tetris_shape.h / tetris_movegen.h / tetris_simd.h`

3. **search() / make_path() / 20g 的 BBState-native 化**
   - 已不只是空盘落点枚举
   - 路径恢复与 20g 搜索也都已经位板化

4. **七类 search 的统一外观**
   - 顶层已经统一成 `simple / simulate / path / tag / tspin / aspin / cautious`

5. **Tag native path**
   - 这部分是这轮新进展之一，已经从旧节点搜索迁到 bitboard native

6. **PathMark 栈本地化 / Context 中立化**
   - 搜索状态不再依赖单个大而全的 search 类实例字段

### 8.2 仍然没有彻底删除的旧层

当前还不能说“完全摆脱旧世界”，因为还保留了：

1. **`core/tetris_core.*`**
   - 仍然提供 `TetrisNode / TetrisMap / TetrisEngine`

2. **`state_node_lut_` 桥接**
   - 说明位板终态仍要回到 `TetrisNode*` 以兼容上层

3. **`oracle/` 全套归档搜索器**
   - 仍被 diff / regression 工具使用

4. **部分语义仍通过 hook + node_ex 适配旧 LandPoint 类型**
   - 不是完全“纯位板 API”

### 8.3 阶段判断

如果按迁移成熟度划分，我会把当前阶段判断为：

- **“位板搜索框架基本落成，旧节点图退居外壳与 oracle 参照层”**

换句话说，项目已经从：

- “movegen 实验分支”

进入到了：

- “完整 search backend 重构分支”

但还没有走到：

- “彻底删掉 `TetrisNode/TetrisContext/oracle` 历史兼容层”

---

## 9. 本轮相对上一轮（6cf6726）的最重要新增认知

如果只看你上次让我调的 `dev@6cf6726`，当时更像：

- RuleSpec + MoveGen + oracle_diff + 初步 make_path/20g 位板化

而这次 `4ef9e88` 新增的核心是：

1. **仓库结构改了**：`core/`、`oracle/`、`src/` 已经拆层
2. **框架抽象改了**：`bb_state / bb_bfs_engine / movegen_context / movegen_searcher / movegen_strategy / movegen_hook`
3. **Tag native 改了**：TAG 不再停留在旧节点搜索
4. **PathMark 家族定型了**：`PathMarkBit / PathMark / cover_if_bbox / self-prev root`
5. **顶层 search 统一了**：七类 search 已收敛到统一 facade
6. **diff 协议放宽了**：最新 `4ef9e88` 已接受 subset search 与 `Dd` 后缀规约后的路径等价

所以本轮不该再把 `dev` 只概括成“oracle 上加了一个 movegen 内核”，而应理解为：

> `dev` 正在把整个 search backend 从 master pointer-graph 体系，系统性迁到可插拔的 bitboard strategy 框架。

---

## 10. 可直接复用的结论（供后续答复用户）

### 简版结论

- 当前 `dev@4ef9e88` 相对 oracle 已不是 33 commit 的早期 movegen 试验，而是 **157 commit 的搜索框架重构线**。
- `bb_state.h / bb_bfs_engine.h / movegen_context.h / movegen_hook.h / movegen_searcher.h / movegen_strategy.h` 这批文件标志着 **MoveGenSearch 神类被拆成中立组件**。
- `PathMarkBit` 与 `PathMark` 都已在 `src/` 正式落地；“last-rotation”不再是单独容器，而是由 `cell_op_ + cover_if_bbox + hook payload` 共同承载。
- 与 cobra 的关系是：**底层算法强 cobra 化，上层接口和语义兼容明显是 tetris_ai_runner 自己的桥接层**。
- 当前位板化进度已经到 **统一七类 search 的生产后端**，旧 `oracle/` 更多是对拍与归档用途。

### 后续若要继续深挖，优先顺序建议

1. `search_tag.h`：TAG 的分阶段 PathMark / cover_if / 20g 细节
2. `movegen_hook.h`：TSpin/ASpin/Cautious 语义如何挂接到统一骨架
3. `search_tspin.h / search_aspin.h / search_cautious.h`：顶层 facade 如何复用 `PathStrategy`
4. `tests/_diff_harness.h` + 各 `tests/*_diff.cpp`：最新 diff 协议如何定义“允许的差异”

---

## 11. 本轮未做的事

- **未写长期记忆**（按本轮要求，仅写 `notes/` 临时调研）
- **未修改源码**
- **未推送任何代码**
