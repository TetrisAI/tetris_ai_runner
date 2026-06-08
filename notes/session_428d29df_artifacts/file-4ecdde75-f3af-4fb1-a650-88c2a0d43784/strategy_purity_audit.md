# 审计报告: 位板 Strategy 纯净度审计

本报告针对 `src/` 下五个位板风格搜索 strategy 及其配套文件进行审计，识别其中为兼容老 master-graph (TetrisNode/TetrisContext) 或 oracle 对拍而存在的“不纯净”代码。

## 审计汇总

| 文件 | 状态 | 主要不纯净点类型 |
| :--- | :--- | :--- |
| `search_path.h` | 含有不纯净点 | Master-graph 类型引用、适配器函数、反查 LUT 依赖 |
| `search_simulate.h` | 含有不纯净点 | Master-graph 类型引用、适配器函数 |
| `search_tag.h` | 含有不纯净点 | 强依赖 Master-graph 拓扑（旋转指针）、复杂适配逻辑 |
| `search_simple.h` | 含有不纯净点 | Master-graph 类型引用、Spawn-row 快速路径依赖 |
| `search_aspin.h` | 不纯净 | 纯粹的 Master-graph 包装层接口 |
| `search_aspin.cpp` | 不纯净 | 纯粹的 Master-graph 包装层实现 |

---

## 逐文件审计详情

### 1. search_path.h

**不纯净点清单与位置**

- **include 依赖**: 
  - `tetris_core.h` (L26): 引入 master-graph 核心类型。
  - `tetris_movegen.h` (L27): 引入基于 master-graph 的 movegen。
- **ExtrasMixin 成员**: 
  - `TetrisContext const *context_ = nullptr;` (L61): 缓存 master 上下文。
- **init 适配器**:
  - `static void init(Context &ctx, TetrisContext const *context, Config const *config)` (L97): 接口对齐 master-graph。
- **search 适配器**:
  - `static std::vector<LandPoint> const * search(Context &ctx, TetrisMap const &map, TetrisNode const *node, std::size_t depth)` (L111): 签名使用 `TetrisNode`。
  - 内部逻辑调用 `node->check(map)` (L114) 和 `node->status.t` (L120, L135)。
- **make_path 适配器**:
  - `static std::vector<char> make_path(Context &ctx, TetrisNode const *node, LandPoint const &land_point, TetrisMap const &map)` (L153): 签名使用 `TetrisNode`。
- **反查与生成依赖**:
  - `ctx.state_node_lut_[i] = ctx.context_->get(st);` (L188): 在 `fill_state_lut_TR` 中将位板状态折回 master node。
  - `TetrisNode const *n = ctx.context_->get(status);` (L214): 在 `run_piece` 中依赖 master context 生成节点。
  - `TetrisNode const *sunk_node = ctx->state_to_node(ss);` (L366): 在 `Run20gVisitor::on_pop` 中通过 LUT 反查 node。
  - `bb::CellsKey k = Helpers::cells_key_for(sunk_node);` (L369): 依赖 master node 计算 key。

**源码片段示例**
```cpp
【110】            static std::vector<LandPoint> const *
【111】            search(Context &ctx, TetrisMap const &map, TetrisNode const *node, std::size_t depth)
【112】            {
【113】                ctx.land_point_cache_.clear();
【114】                if (!node || !node->check(map))
```

**可移走性评估**
- **是**。上述所有 `TetrisNode` 相关的 search/make_path 签名及 `state_node_lut_` 填充逻辑均可搬迁至 `search_path_node.{h,cpp}`。`PathStrategy` 核心算法仅需 `bb::BBState` 和 `map_t` 即可运行。目前这些代码主要为了支撑 `ai.cpp` 的直接调用。

---

### 2. search_simulate.h

**不纯净点清单与位置**

- **include 依赖**: 
  - `tetris_core.h` (L35)。
- **ExtrasMixin 成员**: 
  - `TetrisContext const *context_ = nullptr;` (L71)。
- **适配器函数**:
  - `init` (L108): 接受 `TetrisContext`。
  - `search` (L122): 接受 `TetrisNode`。
  - `make_path` (L145): 接受 `TetrisNode`。
- **内部逻辑依赖**:
  - `run_piece` (L166): `ctx.context_->get(status)`。
  - `make_path_simulate_1g_native` (L410): 接受 `TetrisNode` 并读 `node->status.t` (L414)。
  - `Helpers::cells_key_for(land_point.node)` (L422): 依赖 `LandPoint` 内的 master node 指针。

**源码片段示例**
```cpp
【121】            static std::vector<LandPoint> const *
【122】            search(Context &ctx, TetrisMap const &map, TetrisNode const *node, std::size_t depth)
【123】            {
【124】                ctx.land_point_cache_.clear();
【125】                if (!node || !node->check(map))
```

**可移走性评估**
- **是**。与 `search_path.h` 类似，这些适配器主要被 `ai.cpp` 消费，可移入 `search_simulate_node.{h,cpp}`。

---

### 3. search_tag.h

**不纯净点清单与位置**

- **include 依赖**: 
  - `tetris_core.h` (L39)。
- **ExtrasMixin 成员**: 
  - `TetrisContext const *context_ = nullptr;` (L73)。
- **核心算法强耦合 (旋转指针)**:
  - `TagSearch1gNeighbors` (L255): `TetrisNode const *cur_node = ctx->state_to_node(cur);`。
  - `cur_node->rotate_counterclockwise != nullptr` (L258, L264, L399, L405, L654, L661, L868, L873): 旋转逻辑直接读取 master node 的预建指针，而非使用位板 `Helpers`。
- **20g 种子生成与几何逻辑**:
  - `run_piece_20g_native` (L505-L564): 大量直接操作 `TetrisNode` 指针，包括 `drop()` (L509)、`status.r` (L544)、`move_left/right` (L551, L555)。
- **search_t 特化逻辑**:
  - `search_t_native` (L812, L838, L842, L845): 反查 parent node 并填充 `node_ex.last`，调用 `SpinHook::check_ready(map, sunk_node, ...)`。
- **make_path 逻辑**:
  - `make_path_native` (L1035, L1039, L1047, L1050): 依赖 `node->index_filtered` 和 `last->rotate_*` 指针对比。

**源码片段示例**
```cpp
【254】                void expand(bb::BBState const &cur, Emit emit)
【255】                {
【256】                    TetrisNode const *cur_node = ctx->state_to_node(cur);
【257】                    if (cur_node == nullptr)
【258】                        return;
【259】                    if (cur_node->rotate_counterclockwise != nullptr)
```

**可移走性评估**
- **部分可移走**。适配器函数签名可移走，但 `TagStrategy` 内部对 master node 旋转指针的依赖（为了保持与 oracle 算法完全对拍）较深。若要完全去 master-graph，需在位板侧重新实现一套与 master 完全等价的旋转/踢墙选择逻辑。目前其属于“深度兼容残留”。

---

### 4. search_simple.h

**不纯净点清单与位置**

- **include 依赖**: 
  - `tetris_core.h` (L30)。
- **ExtrasMixin 成员**: 
  - `TetrisContext const *context_ = nullptr;` (L64)。
- **Spawn-row Fast Path**:
  - `if (node->land_point != nullptr && node->low >= map.roof)` (L154): 依赖 master node 预置的 `land_point` 列表（由 `TetrisContext::prepare` 生成的 piece-definition 表）。
- **适配器与反查**:
  - `search` (L127) / `make_path` (L202): 使用 `TetrisNode` 签名。
  - `emit_simple_drops_for_rotation` (L398): `ctx.state_to_node(*sunk)`。

**源码片段示例**
```cpp
【154】                if (node->land_point != nullptr && node->low >= map.roof)
【155】                {
【156】                    for (auto cit = node->land_point->begin(); cit != node->land_point->end(); ++cit)
```

**可移走性评估**
- **是**。虽然 `spawn-row` 快速路径依赖 master 表，但可以改为在位板侧通过 `bb::Helpers` 在运行时生成（或缓存在 Context 中），从而剥离对 `TetrisNode` 的直接依赖。

---

### 5. search_aspin.h / .cpp

**不纯净点清单与位置**

- **文件定位**: 这两个文件目前本质上就是为位板 Searcher 封装的 master-graph 兼容层 (Façade)。
- **接口定义 (search_aspin.h)**:
  - 全面引用 `m_tetris::TetrisNode` 和 `m_tetris::TetrisContext`。
  - `TetrisNodeWithASpinType` (L29) 结构体封装了 `TetrisNode const *node`。
- **实现细节 (search_aspin.cpp)**:
  - `Search::init` (L47), `Search::search` (L54), `Search::make_path` (L62): 均为将 master 类型转发给位板 `Backend` 的胶水函数。

**源码片段示例**
```cpp
【54】    std::vector<Search::TetrisNodeWithASpinType> const *
【55】    Search::search(TetrisMap const &map, TetrisNode const *node, size_t depth)
【56】    {
【57】        static_assert(std::is_same_v<Backend::LandPoint, TetrisNodeWithASpinType>,
【58】                      "ASpinHook::LandPoint must alias search_aspin::TetrisNodeWithASpinType");
【59】        return impl().backend.search(map, node, depth);
【60】    }
```

**可移走性评估**
- **是**。该文件整组功能即为“胶水”，可更名为 `search_aspin_node.{h,cpp}` 或在 `ai.cpp` 切换后直接删除。

---

## #include 列表与强依赖标注

| 文件 | #include 列表 (部分省略 std) | Master-graph 强依赖 |
| :--- | :--- | :--- |
| `search_path.h` | `bb_bfs_engine.h`, `bb_state.h`, `movegen_context.h`, `movegen_hook.h`, `movegen_strategy.h`, `tetris_core.h`, `tetris_movegen.h`, `tetris_rule_spec.h`, `tetris_shape.h` | **有** (`tetris_core.h`) |
| `search_simulate.h` | `bb_bfs_engine.h`, `bb_state.h`, `movegen_context.h`, `movegen_hook.h`, `movegen_strategy.h`, `tetris_core.h`, `tetris_movegen.h`, `tetris_rule_spec.h`, `tetris_shape.h` | **有** (`tetris_core.h`) |
| `search_tag.h` | `bb_bfs_engine.h`, `bb_state.h`, `movegen_context.h`, `movegen_hook.h`, `movegen_strategy.h`, `tetris_core.h`, `tetris_rule_spec.h`, `tetris_shape.h` | **有** (`tetris_core.h`) |
| `search_simple.h` | `bb_state.h`, `movegen_context.h`, `movegen_hook.h`, `movegen_strategy.h`, `tetris_core.h`, `tetris_rule_spec.h`, `tetris_shape.h` | **有** (`tetris_core.h`) |
| `search_aspin.h` | `tetris_core.h`, `vector`, `cstring` | **有** (`tetris_core.h`) |

---

## 补充说明: 算法对照注释

以下文件含有大量 oracle 算法对比说明注释（不带运行时代码），请用户根据需要决定是否保留：
- `search_path.h`: 约 3 处 (L7, L14, L93)。
- `search_simulate.h`: 约 4 处 (L8, L12, L17, L191)。
- `search_tag.h`: 约 15 处 (L5, L14, L124, L233, L290, L370, L603, L670, L821, L855, L954 等)。
- `search_simple.h`: 约 3 处 (L7, L21, L150)。
