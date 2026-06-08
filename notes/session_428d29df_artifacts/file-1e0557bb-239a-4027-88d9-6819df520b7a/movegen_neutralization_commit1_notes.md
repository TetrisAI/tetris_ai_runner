# Movegen Neutralization — Commit 1 Notes

> 中立化 + Strategy 化重构 3-commit plan 的第 1 步。本提交只在 `src/` 加 3
> 个公共组件 header，**不改任何旧路径**，仅声明 commit 2 / commit 3 要消费的
> 框架。

## 仓库现状回顾

- Branch: `flip-bits-clean`
- HEAD: `d45864c refactor(movegen): bitboard-native simple search without bb_bfs_engine`
- 工作区进入时干净 (`git status --short` 空输出)。

## 调研产物

### 1. oracle search 的物理位置

```
oracle/
  search_path.{h,cpp}
  search_simple.{h,cpp}
  search_simulate.{h,cpp}
  search_tag.{h,cpp}
```

`oracle/` 目录已经存在 4 套 oracle 实现。`src/` 下的 search-related 头是
`search_aspin.h` / `search_path_node.h` / `search_simple_node.h` /
`search_simulate_node.h` / `search_tag_node.h` / `search_tspin.h`，都带
`_node` 后缀或属于完全无关的 spin 扫描器。**因此 commit 2 计划中的新文件
名 `src/search_path.h` / `src/search_simulate.h` / `src/search_simple.h` /
`src/search_tag.h` 与现有 `src/` 任何头都不冲突**，本 commit 不需要从其他
分支捞 oracle 文件。

### 2. `src/movegen_search.h` 字段清单与 search-specific 用法

| 字段 | 类型 | 用法分布 (path / simulate / simple / run_piece_20g) |
| --- | --- | --- |
| `context_`            | `TetrisContext const *`                                                | 全部用 (init / state_to_node / state_lut 填充 / 1g run_piece collect) |
| `config_`             | `Config const *`                                                       | 全部用 (Hook::config_*) |
| `land_point_cache_`   | `std::vector<LandPoint>`                                               | 全部用 (search 阶段输出) |
| `path_mark_`          | `Base::PathMark`                                                       | path 1g/20g make_path、simulate 1g/20g make_path；simple **不**用；run_piece 系列也**不**用 |
| `node_search_path_`   | `std::vector<bb::BBState>`                                             | path 1g/20g make_path、simulate 1g/20g make_path；run_piece_20g 用的是局部 `queue_buffer`；simple 用局部 `rot_order` |
| `hook_state_`         | `[[no_unique_address]] Hook::SearchState`                              | path/run_piece_20g (T-spin ready/mini)；NoSpin 路径下 0 字节 |
| `state_node_lut_[]`   | `TetrisNode const *[kPieceCount * kMaxR * kH * kW]`                    | run_piece_20g、make_path_20g_native（emit 段）、simple 反查；commit 1 视为多 strategy 公用基础设施 |

`state_to_node()` 静态 helper 与 `fill_state_lut_T<T>` / `fill_state_lut_TR<T,R>`
都仅依赖 `context_` + LUT。

### 3. `bb::Helpers<RuleSpec>` 现状 (摘自 `src/bb_state.h`)

`Helpers<RuleSpec>` 是 piece-aware 静态 helper / 数据结构 / 编译期常量
集合，已暴露 `kW / kH / kPieceCount / kMaxR / piece_index / rcount_v /
piece_info_t / map_t / PathMark / build_*` 等。`MoveGenContext` 直接 public
继承之，commit 2 起 strategy 通过 Context 对外暴露这些 helper。

### 4. mixin 划分（最终方案）

按"多 strategy 都会用"的尺度筛掉单 strategy 字段后，commit 1 安置 3 个
mixin：

- `PathMarkMixin<RuleSpec>`：`typename bb::Helpers<RuleSpec>::PathMark path_mark_;`
  + `void reset_path_mark()`（仅 `path_mark_.clear()`，避免 commit 2 的
  strategy 直接触达 PathMark 私有 API）。  
  **使用方**：path / simulate strategy。Simple strategy 不入。

- `BfsQueueMixin<RuleSpec>`：`std::vector<bb::BBState> node_search_path_;`
  + `void reset_bfs_queue()`。  
  **使用方**：path / simulate strategy（make_path 主循环喂 `bb::run_bb_bfs`
  的队列缓冲）。Simple / run_piece_20g 用各自的局部缓冲，本 mixin 只承载
  跨 strategy 复用的那条公共线。

- `StateNodeLutMixin<RuleSpec>`：`TetrisNode const *state_node_lut_[
  kPieceCount * kMaxR * kH * kW]{};` + `state_to_node(BBState)` 单点查表
  + `fill_state_lut_T<T>()` / `fill_state_lut_TR<T,R>()` 静态填表入口。  
  **使用方**：path 20g、simulate 20g、run_piece_20g、simple；commit 2 起
  Strategy 在 `Context::init` 里调一次 `fill_state_lut_T<T>()` 展开表格。

故意**不**进 mixin 的字段（commit 2 处理）：

- `land_point_cache_`：类型依赖 `Hook::LandPoint`，违背"mixin 只接
  RuleSpec"的契约；commit 2 放进 strategy 私有 Context 别名。
- `hook_state_`：同上理由 (`Hook::SearchState`)。
- `context_` / `config_`：跨 strategy 共用，但它们的 `Config` 类型来自
  `Hook::Config`；commit 2 通过 strategy-side `Context` 别名注入并由
  Searcher::set_config 喂入。
- `RotationGraphMixin`：rotation BFS 的 `SimpleRotEntry` 中间态仅
  `search_simple` 用，按命名约束（"仅当多个 strategy 都用才进 mixin"），
  这条不进 mixin，commit 3 留在 `search_simple.h` 私有结构里。

### 5. 命名空间选择

新公共组件落在 `m_tetris::movegen`（与 `tetris_movegen.h` 内的位板生成器
`m_tetris::movegen::MoveGen<...>` 同 namespace），把 "framework / 装配"
层级与 "bitboard 内部 helper" (`m_tetris::bb`) 分开；旧 `MoveGenSearch`
仍位于 `m_tetris`，互不影响。

### 6. Strategy 契约（`movegen_strategy.h` 起草，commit 2 / commit 3 充实）

每个具体 strategy 形如：

```cpp
template<class SpinHook, class RuleSpec>
struct SearchPath
{
    using Context = m_tetris::movegen::MoveGenContext<
        RuleSpec,
        m_tetris::movegen::PathMarkMixin,
        m_tetris::movegen::BfsQueueMixin,
        m_tetris::movegen::StateNodeLutMixin>;

    static auto const* search(Context& ctx, /*...*/);
    static std::vector<char> make_path(Context& ctx, /*...*/);
};
```

`Searcher<SearchPath, SpinHook, RuleSpec>` 持 `Strategy::Context ctx_`，
`search` / `make_path` inline 转发到 `Strategy::xxx(ctx_, ...)`。运行期
零代价（成员只 1 个 ctx 实体，方法都是 inline 模板转发）。

### 7. commit 1 安全网

由于 3 个新 header 不被任何 .cpp / .h include，编译期可见的 instantiation
只能靠 header 自带 `static_assert` 自证。本 commit 在
`movegen_context.h` 顶部加了一组 sanity static_assert：

- `MoveGenContext<RuleSpec, PathMarkMixin>` 实例化通过；
- mixin 的字段名 (`path_mark_` / `node_search_path_` / `state_node_lut_`)
  在派生 Context 上可见 (`requires` 表达式 + decltype)；
- `Searcher` 不能用裸 `MoveGenSearch` 占位（commit 1 没有 strategy）；改为
  `template static_assert` + 一个仅头文件 demo strategy：在 `movegen_strategy.h`
  里写一个 `struct DemoStrategy` 仅作语法 check，**不导出任何符号到
  `m_tetris::movegen`**（藏在匿名 namespace 或 detail::）。

## 编译验证计划

```
cd /workspace/iris_428d29df-9165-49d0-9579-a8eee1b2f449/tetris_ai_runner/build
cmake --build . -j$(nproc)
./oracle_diff
./path_node_diff
./simulate_node_diff
./tag_node_diff
```

期望：构建零警告 (新增 header 只在 commit 2 才被消费, commit 1 期间它们
通过 `static_assert` 自证语法), 4 个 diff 工具行为不变。

## 提交边界 (本 commit 仅允许)

新增：

- `src/movegen_context.h`
- `src/movegen_searcher.h`
- `src/movegen_strategy.h`
- `research/flip-bits/movegen_neutralization_commit1_notes.md`

绝不动：`src/movegen_search.h` / `src/movegen_hook.h` / `src/ai.cpp` /
`src/search_*.{h,cpp}` / `oracle/`。
