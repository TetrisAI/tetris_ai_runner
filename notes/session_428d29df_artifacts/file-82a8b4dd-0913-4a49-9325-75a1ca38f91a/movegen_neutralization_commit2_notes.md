# Movegen Neutralization — Commit 2 Notes

> 中立化 + Strategy 化重构 3-commit 计划的第 2 步. 本提交把旧
> `MoveGenSearch<RuleSpec, Hook>` 拆成 3 个 strategy 头, 调用面切到
> `Searcher<Strategy, SpinHook, RuleSpec>`. 行为 byte-equivalent.

## 仓库现状回顾

- Branch: `flip-bits-clean`
- HEAD: `7b6003b refactor(movegen): introduce neutral context mixins, strategy contract and searcher wrapper`
- 进入工作区时干净 (`git status --short` 空输出).

## 调研结论 — `MoveGenContext` mixin 模板模板参数需要小改

commit 1 的 `MoveGenContext` 写的是
```cpp
template<class RuleSpec, template<class> class... Mixins>
struct MoveGenContext : bb::Helpers<RuleSpec>, Mixins<RuleSpec>...;
```
(只接受"接受 RuleSpec 一个模板参数"的 mixin). PathStrategy 需要把 SpinHook
依赖的字段 (`land_point_cache_<LandPoint>` / `hook_state_<SearchState>`)
打到 strategy-specific mixin 里. 这种 mixin 必须同时依赖 RuleSpec **和**
SpinHook, 不能套进 `template<class> class` 的形参签名.

最小侵入改动: 把 mixin 模板模板参数从 `template<class> class Mixins...`
改成 `class... Mixins`(直接接受已实例化类型). 这样 strategy 写
`MoveGenContext<RuleSpec, PathMarkMixin<RuleSpec>, BfsQueueMixin<RuleSpec>,
StateNodeLutMixin<RuleSpec>, PathExtrasMixin<SpinHook, RuleSpec>>` 即可,
mixin 既能依赖 RuleSpec 又能依赖 SpinHook, 调用方一致. detail::
sanity static_assert 同步改写.

## 三个 strategy 的 Context mixin 列表

- **PathStrategy**: `PathMarkMixin<RuleSpec>` + `BfsQueueMixin<RuleSpec>` +
  `StateNodeLutMixin<RuleSpec>` + `PathExtrasMixin<SpinHook, RuleSpec>`
  - PathExtrasMixin 持: `Config const* config_`, `LandPoint cache`,
    `[[no_unique_address]] SearchState hook_state_`
  - 用满 4 个 mixin: 1g make_path / 20g make_path / run_piece_20g 都用
    PathMark + BfsQueue (queue_buffer 1g 用 mixin, 20g run_piece 用 mixin
    复用); state_lut 用于 emit / fast-path / open_bb.
- **SimulateStrategy**: `PathMarkMixin<RuleSpec>` + `BfsQueueMixin<RuleSpec>` +
  `SimulateExtrasMixin<SpinHook, RuleSpec>`
  - SimulateExtras 持: `Config const* config_`, `LandPoint cache`.
    SimulateNoSpinHook 的 SearchState 是空 struct, 仍按 `[[no_unique_address]]`
    放进 mixin 保持模板对称, 0 字节占位.
  - **不**需要 `StateNodeLutMixin`: simulate make_path 命中后只走
    PathMark 链 + reverse, 不再 reverse-lookup 成 master 节点; simulate
    search 走的是 MoveGen<Hook> emit 通路 (与 path 一致), 内部已经把
    LandPoint 直接装好, 不经 state_node_lut. **保留** `BfsQueueMixin`:
    1g/20g 的 make_path 都喂 `bb::run_bb_bfs`. simulate search 路径不喂
    queue, 因此 mixin 提供的容量复用是 make_path 专属. (确认: `MoveGen`
    与 simulate make_path 是否需要 lut. `MoveGen<Hook>::generate` 内部
    通过 `context_->get(status)` 把 status -> master node, 不依赖 lut.)
  - 注意: simulate search 的 1g 路径走 MoveGen<RuleSpec, T, Hook>, 这
    本来就需要 `context_`. SimulateExtras 里需要 `TetrisContext const*
    context_` 字段供 collect lambda 用 (SimulateNoSpinHook 默认 NoSpin,
    不进 20g 路径; 但保险起见仍持 context).
  - simulate search 的 1g 路径调用 `MoveGen<RuleSpec, T, Hook>::generate`,
    MoveGen 在 collect 里 `context_->get(status)` 反查节点. context_
    当前从 mixin 拿. 与 path 同形, 不另设 mixin.
- **SimpleStrategy**: `StateNodeLutMixin<RuleSpec>` +
  `SimpleExtrasMixin<SpinHook, RuleSpec>`
  - SimpleExtras 持: `Config const* config_`, `LandPoint cache`,
    `TetrisContext const* context_` (search 的 spawn-row land_point fast-path
    需要 `node->land_point` 列表, 但那是 master node 自身链上的字段, 不
    需要 ctx). 不过 simple 走 emit 时要 state_node_lut → LandPoint 反查
    (master node 出口), 因此还是用 StateNodeLutMixin 复用 lut 的 init
    + state_to_node API.
  - 不需要 PathMarkMixin / BfsQueueMixin: simple rotation BFS 用栈/数组的
    `SimpleRotEntry[kMaxR]`, lateral run 用循环, 都是函数局部.

## `movegen_strategy.h` 公共组件中放的跨 strategy 工具

- **`detail::common::MakePath1gDedup<RuleSpec>`**: path 与 simulate 的 1g
  make_path 共享同一份 dedup (set_bbox + usable_at_bb 三态). 拆成模板供
  simulate / path 各自的 make_path 1g 主循环消费.
- 其它 BFS 三件套是 strategy 私有 (Run20g* / Path20g* / Path1gNeighbors /
  Path1gVisitor / Simulate1gNeighbors / Simulate1gVisitor / Simulate20g*)
  — 邻居规则、命中谓词差异大, 没有跨 strategy 复用价值, 留在 strategy
  私有 namespace 里.
- 删除 `detail::DemoStrategy`(commit 1 占位): 实际 strategy 取代它.

## strategy-specific mixin 字段细节

```cpp
template<class SpinHook, class RuleSpec>
struct PathExtrasMixin
{
    using Config = typename SpinHook::Config;
    using LandPoint = typename SpinHook::LandPoint;
    using SearchState = typename SpinHook::SearchState;

    TetrisContext const *context_ = nullptr;
    Config const *config_ = nullptr;
    std::vector<LandPoint> land_point_cache_{};
    [[no_unique_address]] SearchState hook_state_{};
};

// Simulate / Simple 同形 (Simulate 不用 SearchState, 但仍按对称放空 struct
// 的 [[no_unique_address]]; Simple 同样).
```

对应 strategy 静态方法签名:

```cpp
static void init(Context& ctx, TetrisContext const* context, Config const* config);
static std::vector<LandPoint> const* search(Context& ctx, TetrisMap const&, TetrisNode const*, std::size_t depth);
static std::vector<char> make_path(Context& ctx, TetrisNode const*, LandPoint const&, TetrisMap const&);
```

`Searcher` 已暴露 `context()` 访问器, init / search / make_path 通过
strategy 静态方法消费 `ctx`.

## ai.cpp 调用面变更

- `m_tetris::TetrisEngine<...>` 第 3 模板形参从
  `m_tetris::MoveGenSearch<rule_xxx::TetrisRule::rule_spec[, Hook]>` 改为
  `m_tetris::movegen::Searcher<XxxStrategy, SpinHook, rule_xxx::TetrisRule::rule_spec>`.
  (注: TetrisEngine 通过 `TetrisHasConfig<TetrisSearch>` 自动接 Config trait,
  所以 Searcher 必须暴露 `Config` 别名给上层探针; 同时
  `LandPoint` 也由 `element_traits<decltype(search())>` 推断, 走 strategy
  内的 LandPoint 即可, 不需要单独 alias.)
- 直接转换映射:
  - `MoveGenSearch<rule_st, search_path_node::PathHook>` →
    `Searcher<PathStrategy, search_path_node::PathHook, rule_st::TetrisRule::rule_spec>`
  - `MoveGenSearch<rule_toj>` (默认 DefaultTSpinHook) →
    `Searcher<PathStrategy, m_tetris::DefaultTSpinHook, rule_toj::TetrisRule::rule_spec>`
  - `MoveGenSearch<rule_qq, search_simple_node::SimpleHook>` →
    `Searcher<SimpleStrategy, search_simple_node::SimpleHook, rule_qq::TetrisRule::rule_spec>`
    — 注意 SimpleHook 已退化为占位; commit 删 SimpleNoSpinHook 后, AI 端
    typedef 改为直接传 NoSpinHook (with SimpleStrategy 自带行为).
  - 同理 SimulateHook / SimulateNoSpinHook 删除后, AI 端 typedef 直接传
    NoSpinHook (with SimulateStrategy).
- `tests/oracle_diff.cpp` 内的 `MgSearch = m_tetris::MoveGenSearch<...>` 同步
  改为 `Searcher<PathStrategy, DefaultTSpinHook, rule_srs::...>`.
- `src/search_aspin.cpp` 内的 `Backend = MoveGenSearch<..., DefaultASpinHook>`
  改为 `Searcher<PathStrategy, DefaultASpinHook, ...>`.
- `src/ppt_pso.cpp` / `src/pso.cpp` 同步.

## `movegen_hook.h` 瘦身

- 删 `is_simulate_search` / `is_simple_search` 两 trait 常量(所有 hook).
- 删 `SimulateNoSpinHook` / `SimpleNoSpinHook` 两类型(用 NoSpinHook + 选
  Strategy 模板代替).
- 其它 hook (NoHook / NoSpinHook / TSpinHook / ASpinHook / CautiousHook)
  保留不动, 仅删 search-routing 相关 trait.
- search_path_node.h / search_simulate_node.h / search_simple_node.h 内的
  `using PathHook / SimulateHook / SimpleHook = ...` 全部改为
  `using XxxHook = ::m_tetris::NoSpinHook` (不考虑兼容; 文档说明).

## 删除文件

- `src/movegen_search.h`: 整个删除; 在 `src/ai.cpp` / `src/search_aspin.cpp` /
  `src/ppt_pso.cpp` / `src/pso.cpp` / `tests/oracle_diff.cpp` 改 include
  为 `src/movegen_searcher.h` + 三个 strategy 头.

## 行为不变契约

- 所有 BFS 三件套**算法搬迁**, 邻居顺序、kick 顺序、命中谓词、PathMark
  写入语义、cover_if 升级语义全部保持.
- `oracle_diff` / `path_node_diff` / `simulate_node_diff` /
  `tag_node_diff` 必须 ok.

## 编译验证计划

```
cd /workspace/iris_428d29df-9165-49d0-9579-a8eee1b2f449/tetris_ai_runner/build
cmake --build . -j$(nproc)
./oracle_diff
./path_node_diff
./simulate_node_diff
./tag_node_diff
```
