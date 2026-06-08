# 移除 TetrisContext 与全组件迁移新框架计划书

本计划书旨在梳理 `TetrisContext` 在 `tetris_ai_runner` 仓库中的现状，并基于 `flip-bits` 分支已有的 `MoveGenSearch` + `kFilteredIndex` 基础设施，规划分阶段移除老旧指针网架构（`TetrisContext`）的路径。

## 1. TetrisContext 当前职责盘点

根据 `src/tetris_core.h` 定义，`TetrisContext` 目前承担以下核心职责：

### 核心接口与字段
- **节点池管理**: 
    - `node_storage_` (`deque<TetrisNode>`): 持有所有 BFS 节点。
    - `node_index_`: 基于 `TetrisBlockStatus` 的节点哈希索引。
    - `node_max()`: 返回节点总数，供 `NodeMark` 分配标记位图。
- **规则与生成**:
    - `generate(type)` / `get_generate()`: 获取各 Piece 的 spawn 节点指针。
    - `get_opertion(t, r)`: 获取 `TetrisOpertion` 指针（定义了左移、右移、旋转等链表）。
    - `get_block(t, r)`: 获取 Piece 的 cell 布局（`TetrisNodeBlockLocate`）。
- **场景属性**:
    - `width()` / `height()`: 棋盘尺寸（通常硬编码为 10x40 或 10x20）。
    - `row_mask()`: 满行掩码（10 列即 `0x3FF`）。
    - `type_max()`: 支持的方块种类上限。
- **缓存与辅助**:
    - `place_cache_`: 预计算的合法落点缓存。
    - `convert(type/index)`: 字符类型与数字索引的互转。

## 2. 新框架替代方案

新框架（`MoveGenSearch` + `RuleSpec`）已提供了以下对等能力：

- **Node 池 -> `kFilteredIndex` + `Bitboard`**:
    - 不再需要显式的 `TetrisNode` 链表。
    - `node_max()` 替代品：`movegen::kFilteredIndex<Spec, T>::size()`。
- **Spawn -> `RuleSpec::spawn`**:
    - `RuleSpec` 定义了编译期的初始位置 `(x, y, r)`，不再需要从 context 获取 `generate` 指针。
- **Operation/Logic -> `OpDesc` / `RuleSpec`**:
    - 移动与旋转逻辑由 `RuleSpec` 的 `move_left`, `rotate_cw` 等静态方法实现，或直接由位板 `MoveGen` 引擎展开。
- **Block Data -> `PieceShape` / `OpDesc::cells`**:
    - 形状信息在编译期通过模板参数确定。
- **棋盘属性 -> 模板化 `W`, `H`**:
    - `Map<W, H>` 与 `Search<Spec, W, H>` 替代了动态的 `width_` / `height_`。

## 3. 依赖图

### L0: 核心引擎 (必须最后修改)
- `tetris_core.h/cpp`: 定义了 `TetrisContext` 本身及与其耦合的 `TetrisNode` 逻辑。

### L1: 直接消费 TetrisContext API 的组件
- **Search 模块**:
    - `search_simple.cpp`: 使用 `node_max` 初始化 `node_mark_filtered_`。
    - `search_path.cpp`: 使用 `node_max` 初始化标记位图。
    - `search_simulate.cpp`: 使用 `node_max` 跟踪路径模拟。
    - `search_tag.cpp`: 使用 `generate('T')` 寻找初始节点并初始化标记位。
- **AI 评估器**:
    - `ai_zzz.cpp`: 深度依赖 `width`, `height`, `row_mask` 进行特征提取；使用 `generate` 获取初始 Piece 状态。
    - `ai.cpp` / `ai_ax.cpp`: 依赖 `type_max` 分配 `map_danger_data_`；使用 `width`/`height` 初始化内部地图。
    - `ai_misaka.cpp`: 使用 `row_mask` 和 `height` 计算高度和空洞。
    - `ai_farter.cpp`: 使用 `generate` 模拟方块下落。

### L2: 间接依赖
- **Rule 模块** (`rule_*.cpp`): `get_generate()` 返回依赖 `TetrisContext` 的函数指针。
- **TetrisEngine 模板**: 持有 `shared_ptr<TetrisContext>` 并负责其生命周期。

### L3: 工具与测试
- `oracle_diff.cpp`: 需要 `TetrisContext` 运行 Master 路径作为对比基准。
- `perft_movegen.cpp`: 运行 Master BFS 性能测试。
- `aspin_dump.cpp`: 引擎层初始化仍需 Context。

## 4. 迁移路径分阶段

### Phase 1: 弱化工具链依赖
- **任务**: 修改 `oracle_diff`，使其在对比时不再通过 `TetrisEngine` 强行拉起 Context，而是封装一个 `MasterRunner` 专门负责老逻辑。
- **风险**: 无，仅影响测试代码。
- **验收**: `oracle_diff` 结果不变。

### Phase 2: Search Simple 迁移
- **任务**: 将 `search_simple` 迁移到新框架 `MoveGenSearch`。新增 `NoHook` 模式以支持纯 BFS 路径。
- **涉及文件**: `search_simple.cpp`, `movegen_search.h`。
- **验收**: `perft_movegen` 验证新路径正确性。

### Phase 3: 高级 Search 模块迁移
- **任务**: 迁移 `search_path`, `search_simulate`, `search_tag`。
- **挑战**: `search_path` 的回溯逻辑需要位板版本的 `make_path` 完美对齐 master。

### Phase 4: AI 评估器解耦
- **任务**: 修改 `AI::init` 接口，不再传入 `TetrisContext` 指针，改传入 `RuleSpec` 相关的描述符或 `kFilteredIndex` 大小。
- **涉及文件**: `ai_*.cpp`。
- **风险**: Piece 哈希逻辑（目前用 `TetrisNode*`）需要改为基于 `(r, x, y)` 或 `filtered_idx`。

### Phase 5: 剥离 TetrisEngine 硬依赖
- **任务**: 修改 `TetrisEngine` 模板，使其 `Context` 参数变为可选或抽象。
- **验收**: 编译通过且所有使用 `MoveGenSearch` 的实例不再分配节点池。

### Phase 6: 彻底移除
- **任务**: 删除 `TetrisContext` 类及 `tetris_core.cpp` 中相关的构建代码。

## 5. 隐性陷阱与决策点

- **NodeMark 的尺寸**: Master 的 `node_max` 是所有 Piece 节点的总和。新框架中 `kFilteredIndex` 是分 Piece 的。`NodeMark` 需支持按 `PieceType` 动态切换或预分配最大 Piece 的尺寸。
- **硬编码耦合**: `tetris_core.cpp` 中存在 `R <= 4 -> 16` 等针对 SRS 的硬编码逻辑，迁移非 SRS 规则（如 `rule_extreme`）时需确保 `RuleSpec` 能够完全接管。
- **AI 状态 Hash**: 部分 AI 将 `TetrisNode*` 作为缓存 Key。切到新框架后，必须统一使用 `LandingPos` 的标准化坐标或 `filtered_idx`，否则缓存将失效。

## 6. 当前 flip-bits 进度对此目标的贡献

最近的 commit 为移除 `TetrisContext` 打下了坚实基础：

1. **commit 4-5 (Hookify)**: 将 T-Spin/All-Spin 逻辑从 `search_tspin.cpp` 剥离到 `DefaultTSpinHook` / `DefaultASpinHook`，消除了 search 阶段对 Context 内部表的依赖。
2. **commit 6-7 (kFilteredIndex / compile-time hash)**: 提供了 `node_max` 的替代品和编译期形状查询，使得 `MoveGen` 可以在不拉起 Context 的情况下运行。
3. **commit 8 (extreme rule)**: 证明了非 SRS 规则（13x30 等）可以在新框架下正常工作，解耦了对 master SRS 硬编码的依赖。
4. **commit 9-10 (CMake & constexpr)**: 优化了编译时间，使得承载数万等价类的 `kFilteredIndex` 能够在 1 分钟内完成编译。

## 7. 推荐下一个 Commit

**建议任务**: **Phase 1 - 移除 `oracle_diff` 对 `TetrisEngine` 的非必要依赖。**

- **内容**: 在 `tests/oracle_diff.cpp` 中，不再使用 `TetrisEngine::prepare` 来间接创建 `TetrisContext`。改为直接手动 `prepare` 一个 `TetrisContext` 实例。这将为 Phase 5 剥离 Engine 依赖扫清障碍。
- **性价比**: 风险极低，改动量小，且能清晰界定 "Master 参考端" 与 "待测新框架端" 的边界。
