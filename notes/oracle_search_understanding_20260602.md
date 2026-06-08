# oracle / flip-bits search 调研记录（2026-06-02）

## 结论快照

- 本次调研以 **`origin/flip-bits` = `e3b909b`** 为 oracle 基线，不用本机本地 `flip-bits` 分支头（本机 `flip-bits` 当前是 `6cf6726`，已领先远端基线）。
- oracle 中的 `search` 不是单一实现，而是一组可插拔搜索器，由 `TetrisEngine<Rule, AI, Search>` 在编译期绑定。
- `TetrisCore` 通过 `decltype(TetrisSearch().search(...))` 推导 `LandPoint` 类型，因此 Search 不仅负责枚举落点，也决定 AI 收到的节点扩展信息结构：
  - 普通 search 返回 `vector<TetrisNode const *>`
  - spin-aware search 返回带附加标签的结构体（如 `TetrisNodeWithTSpinType` / `TetrisNodeWithASpinType`）
- `LocalContextBuilder` 会在 Search 存在 `Config` 时自动提供 `search_config()`；因此不同平台可以在不改 core 的前提下切换搜索行为参数。

## 搜索家族分层理解

### 1. 极简几何枚举：`search_simple`

- 目标：快速给出一批可直落 / 横移 / 基本旋转可达的落点。
- `search()` 优先复用 `node->land_point` 缓存；否则按“当前朝向 + 左右平移 + 逆时针轮转一圈”收集 `drop(map)`。
- `make_path()` 不是 BFS，而是按固定顺序拼路径：先补旋转，再横移，再 hard drop / soft drop。
- 适合对路径最优性要求不高、只需候选落点的场景。

### 2. 完整路径搜索：`search_path`

- 目标：枚举真实可达落点，并生成更完整的按键路径。
- `make_path()` 使用 BFS，动作包含：`x/z/c`、`l/r`、极限平移 `L/R`、下落 `d/D`。
- `search()` 也做 BFS；若已有高空 `land_point` 缓存，则从这些落点反推一些中间检查点，补齐缓存无法直接覆盖的路径可达状态。
- 对 QQ 场景，`QQTetrisSearch` 会按配置在 `simple / simulate / path` 三者之间切换。

### 3. 谨慎搜索：`search_cautious`

- 目标：更贴近 Cultris 这类“先确认真实旋转落点，再判是否能继续”的行为。
- 核心区别：旋转不是直接走 `rotate_*` 指针，而是显式遍历 `wall_kick_*`，并只采用**第一个可行踢墙结果**。
- `fast_move_down` 决定是否把向下移动压缩成直接 `D=drop`。
- `search()` / `make_path()` 都是 BFS，但动作集合更克制，强调与具体游戏输入语义对齐。

### 4. T-Spin 搜索：`search_tspin`

- 这是 oracle 中最关键的搜索器之一，服务 TOJ / SRS 侧 AI。
- 结构上分成两条线：
  1. **普通块 / 非 T 块**：仍是 BFS 枚举落点。
  2. **T 块**：走 `search_t()`，在 `TetrisMapSnap` 上做专门 BFS，额外记录“最后一步是否为旋转”“是否满足 T-Spin ready / mini ready”。
- `TetrisNodeWithTSpinType` 附带：
  - `node`：最终落点
  - `last`：最后一步前的节点
  - `type`：`None / TSpin / TSpinMini`
  - `is_last_rotate / is_ready / is_mini_ready`
- `check_ready()` 的本质：基于 T 中心附近四角占用情况做三角角点判定。初始化阶段先通过生成态与一个旋转态推导 `x_diff_ / y_diff_`，再预建 `block_data_`，避免每次重复算角点 mask。
- `check_mini_ready()` 进一步看该位置是否“ready 且已无后续旋转空间”。
- `make_path()` 与普通 BFS 不同的一点：当目标是 spin 落点时，终点索引可能要先回到 `last`，最后再补一手 `x/z/c` 才能到真正目标节点，所以它会根据 `land_point.last` 补尾动作。
- `Config` 控制的不是评估，而是**搜索动作语义**：
  - `allow_rotate_move`
  - `allow_180`
  - `allow_d` / `allow_D`
  - `allow_LR`
  - `is_20g`
  - `last_rotate`
- `is_20g` 会切换到 `make_path_20g()`，把大量状态先 `drop(map)` 后再搜索，适配高重力规则。

### 5. TAG 版 T-Spin 搜索：`search_tag`

- 可以看成 `search_tspin` 的早期 / 简化版特化：
  - 只有 `None / TSpin`，没有 mini。
  - 不支持 180、rotate-move、20G 等一组可配置行为。
  - `make_path()` 在检测到目标为 TSpin 且路径最后一步不是旋转时，会递归先找到 `last`，再手补 `z/c`。
- 用于 `the_ai_games.cpp` 的 TAG 规则 AI。

### 6. A-Spin 搜索：`search_aspin`

- 这是 `flip-bits` 第一阶段（Botris 回灌）新增的 search。
- 服务 `rule_botris + ai_zzz::Botris/Botris_PC`。
- 设计风格明显继承 `search_tspin`：
  - 有独立 `Config`
  - 有 20G 分支
  - 有 `allow_rotate_move / allow_180 / allow_d / allow_D / allow_LR`
  - 返回 `TetrisNodeWithASpinType`
- 但 A-Spin 判定比 T-Spin 更直接：节点落稳后，如果上下左右四个方向都不可再移动，则标成 `ASpin`。
- 它没有 `last` 指针和 mini 语义，因此返回结构比 T-Spin 更轻。
- 一个实现细节：普通 `make_path()` 在 BFS 直接命中失败后，会先执行一次 `search(map, node, 0)`，利用搜索阶段写入的 `node_mark_` 再反推路径，这说明它把“可达性判定”和“路径恢复”共享了一套标记状态。

## Core 与 Search 的接口关系

### `tetris_core.h`

- `LocalContextBuilder` 会根据 `TetrisSearch` 是否定义 `Config`，自动决定是否给 `LocalContext` 注入 `search_config()`。
- `TetrisEngine` 构造 / `prepare()` 时调用 `ContextBuilder::init_search(search_, ...)`。
- `TetrisCore::LandPoint` 直接从 `TetrisSearch::search()` 返回元素类型推导，因此：
  - AI 的 `Result target` 类型
  - `run()` 最终返回的目标节点类型
  - `make_path()` 入参类型
  都跟 Search 强绑定。

## 平台绑定关系

- `QQTetrisSearch`：运行时在 `simple / simulate / path` 三者间切换。
- `search_tag::Search`：TAG / The AI Games。
- `search_tspin::Search`：TOJ / SRS 一系 AI。
- `search_cautious::Search`：C2。
- `search_aspin::Search`：Botris（`flip-bits` 新增）。

## 当前理解中的关键判断

1. **oracle 的 search 是“状态图枚举器 + 路径恢复器 + 特殊落点标注器”三合一接口**，不是单纯 BFS。
2. **Search 决定了 AI 看到的落点数据形状**，所以它其实是搜索层与评估层之间的协议边界。
3. `flip-bits` 相对 `master` 在 search 维度最显著的新东西，不是重写老 search 框架，而是**把 Botris / A-Spin 这一套接进既有可插拔接口里**。
4. `search_tspin/search_aspin/search_tag` 这三个 spin-aware 搜索都不是只在终点做后验判定，而是在搜索阶段就保留了“最后一步”“是否旋转到达”“是否 ready”这类信息，供 AI 直接消费。
5. bit semantics flip（`1=empty`）会影响 `check_ready()` 这类基于 `map.row` 位运算的逻辑，因此理解 search 时必须把它放在 `e3b909b` 新语义下看，不能混用旧分支脑内模型。

## 仍需用户确认的点

- “oracle 的 search 调研”是否以 **`origin/flip-bits` 的整套 search 家族** 为范围；还是你只想先聚焦 `search_tspin` / `search_aspin` 这类核心实现？
- 我当前倾向于把它概括为“统一接口下的多搜索器家族”，而不是“某一个 search 文件的详细逐行解读”。如果这个抽象层级对了，再写长期记忆。 

## 新确认的约束（用户补充）

- `Search` 是**有状态对象**，不能按无状态纯函数理解。
- 直接证据也和这个判断一致：各个 search 实现内部普遍持有可复用成员，如：
  - `land_point_cache_`
  - `node_search_`
  - `node_incomplete_`
  - `node_mark_` / `node_mark_filtered_`
  - `config_` / `context_`
  - 某些 search 还有 `block_data_` / `block_data_buffer_`
- `search()` / `make_path()` 调用过程中会反复读写这些成员缓存，因此**同一个 Search 实例不能被多个线程并发调用**。
- 这也解释了为什么引擎侧把 `search_` 作为 `TetrisEngine` / `TetrisThreadEngine` 的实例成员，而不是静态无状态工具函数。
- 因而在设计理解上，`AI / Rule / Search` 的“正交”只体现在接口拼装层面，**不意味着 Search 在并发语义上天然可共享**。

## 新确认的设计目标（用户补充）

- `search` 的首要目标是 **最快速地搜索落点**。
- 它属于**热路径**，性能要求最强，不允许为了接口整洁、抽象一致性或路径信息附带能力去牺牲搜索性能。
- `make_path` 的职责则是 **在目标落点已确定后做路径恢复 / 路径搜索**。
- `make_path` 不是热路径，性能优先级明显低于 `search`。
- 因而理解各个 search 实现时，应该把两件事明确拆开：
  1. `search()` —— 面向落点枚举，优先追求极致速度
  2. `make_path()` —— 面向动作恢复，允许采用更重的搜索或回溯逻辑
- 这也解释了为什么部分实现会出现：
  - `search()` 尽量使用缓存、剪枝、状态压缩、专门化 BFS / 指针网遍历
  - `make_path()` 则允许单独再跑一遍 BFS，甚至复用/重建标记状态，只要不影响热路径即可
