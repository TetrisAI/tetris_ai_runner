# Movegen Neutralization — Commit 3 Notes

> 中立化 + Strategy 化重构 3-commit 计划的第 3 步, 也是收尾. commit 1 / 2
> 已经把 `MoveGenSearch<RuleSpec, Hook>` 拆成 `Searcher<Strategy, SpinHook,
> RuleSpec>` + 三个 strategy 头, ai.cpp / pso.cpp / search_aspin.cpp 全部
> 切到新调用面. 本 commit 收 API 边界, 增第三方扩展文档与最小可编译样例,
> 不改任何 strategy 算法、不改 oracle、不改引擎调用面.

## 仓库现状回顾

- Branch: `flip-bits-clean`
- HEAD: `7a29500 refactor(movegen): split search strategies into pluggable static classes with per-strategy contexts`
- 进入工作区时干净 (`git status --short` 空输出).

## 公共组件 4 个文件 public 面盘点

### 1. `src/movegen_context.h`

| 当前可见性 | 元素 | 实际消费方 | 目标可见性 | 备注 |
| --- | --- | --- | --- | --- |
| public | `PathMarkMixin::PathMark` (using) | strategy 内部 typedef | private | strategy 走 `Helpers::PathMark`, 不消费 mixin 上的 alias |
| public | `PathMarkMixin::path_mark_` | path / simulate strategy 直接读写 | **public** | mixin 字段必须可被派生 Context 直接访问 |
| public | `PathMarkMixin::reset_path_mark()` | (无) | private | 死代码, 但保留以备扩展 |
| public | `BfsQueueMixin::node_search_path_` | path / simulate strategy 直接读写 | **public** | 同上 |
| public | `BfsQueueMixin::reset_bfs_queue()` | (无) | private | 死代码, 保留 |
| public | `StateNodeLutMixin::Helpers` (using) | (无) | private | 内部别名 |
| public | `StateNodeLutMixin::kW/kH/kMaxR/kPieceCount` | (无, 直接走 `Helpers::kX`) | private | 内部常量 |
| public | `StateNodeLutMixin::kStateLutSize` | (无) | private | 内部常量 |
| public | `StateNodeLutMixin::state_node_lut_[]` | path / simple strategy 直接写 | **public** | 必须给派生 Context 看 |
| public | `StateNodeLutMixin::state_to_node()` | path / simple strategy | **public** | API |
| public | `StateNodeLutMixin::clear_state_node_lut()` | path / simple strategy init() | **public** | API |
| public | `MoveGenContext::rule_spec_type` (using) | (无) | **public** | 第三方反射用 |

mixin 字段 (`path_mark_` / `node_search_path_` / `state_node_lut_`) 由
strategy 在自己的 BFS / build_path 中直接读写, 必须保持 public. 我们把
"非字段"的内部别名/常量/未使用方法收敛到 private. struct 默认 public 的
特性下, 用一个显式 `private:` 段把它们藏起来即可.

### 2. `src/movegen_searcher.h`

| 当前可见性 | 元素 | 实际消费方 | 目标可见性 | 备注 |
| --- | --- | --- | --- | --- |
| public | `strategy_t` | (无外部) | private | 内部别名, 暴露 `Context`/`Config`/`LandPoint` 已足够 |
| public | `Context` | `tetris_core.h::TetrisHasConfig` 模板嗅探 | **public** | API |
| public | `Config` | `tetris_core.h::TetrisHasConfig` 模板嗅探 | **public** | API (TetrisEngine 强依赖此别名) |
| public | `LandPoint` | (探针走返回类型 element_traits, 但保留增强可读) | **public** | API |
| public | `init/search/make_path` | `tetris_core.h` 调用 + 测试 | **public** | API |
| public | `context()` | ai.cpp / pso.cpp 拿 TetrisContext (经 `engine.context()`); Searcher 自己的 context() 仅作 ctx_ 直读, 当前**没有外部消费方** | private | 仅作内部调试; 收敛 |
| private | `ctx_` | (内部) | private | OK |

`context()` 当前未被外部消费 (`ai.cpp / pso.cpp / oracle_diff.cpp` 拿到的
`.context()` 都是 `TetrisEngine::context()`, 不是 `Searcher::context()`).
本 commit 把 `Searcher::context()` 收敛为 private 方法, 同时保留实例
调试需要的 inline 路径(以 protected 暴露给派生).

实际操作: 把 `context()` 移到 protected (而不是 private), 让第三方继承
Searcher 写中间层时仍能读 ctx, 但顶层 API 不暴露 `ctx_`.

### 3. `src/movegen_strategy.h`

| 当前可见性 | 元素 | 实际消费方 | 目标可见性 | 备注 |
| --- | --- | --- | --- | --- |
| public | `detail::common::MakePath1gDedup` | path / simulate strategy 内部 typedef | **public** (在 `detail::common`) | 已经在 `detail::` 下, 不再下沉 |
| public | `MakePath1gDedup::path_mark` / `usable_arr` | strategy 聚合初始化 | **public** | 聚合 init 模式必须 public |
| public | `MakePath1gDedup::try_admit` | `bb::run_bb_bfs` 引擎 | **public** | API contract |

注: `detail::common::` 命名空间已隐式标注"内部". 第三方扩展文档明确
说明 `detail::*` 子命名空间是内部实现, 不享受稳定保证.

### 4. `src/bb_state.h`

| 当前可见性 | 元素 | 实际消费方 | 目标可见性 | 备注 |
| --- | --- | --- | --- | --- |
| public | `bb::BBState` / `bb::CellsKey` / `bb::KickDir` | strategy + bb_bfs_engine | **public** | API |
| public | `bb::PieceIndexInfo` / `bb::MaxRotation` | `bb::Helpers` 内部 | private (struct, 不动) | 在 `bb::` 命名空间, 但只是内部 trait 模板, 第三方不应消费. 文档化为内部. |
| public | `bb::Helpers<RuleSpec>` 全员 | strategy 大量直接消费 (`Helpers::xxx`) | **public** | API 表面 |

`Helpers` 是 piece-aware 的静态 helper 集合, 当前 strategy 都通过本地
typedef `using Helpers = bb::Helpers<RuleSpec>;` 引入命名空间. 第三方
文档建议两种等价路径: ① strategy 内 typedef Helpers 直接 `Helpers::xxx`
(与官方 strategy 同形), ② `Context::xxx` (因为 `MoveGenContext` 公开
继承 `bb::Helpers<RuleSpec>`, helper 函数自动可见). 现行 strategy 偏
①是历史延续, 算法行为 byte-equivalent 的前提下不再回流改写.

## 三个 strategy 内部 triplet 可见性

均已落在私有段:

| 文件 | 内部类型 | 当前可见性 | 备注 |
| --- | --- | --- | --- |
| `search_path.h` | `Run20gMarkSlot` / `Run20gDedup` / `Run20gNeighbors` / `Run20gVisitor` | private (line 156) | OK |
| `search_path.h` | `MakePath1gNeighbors` / `MakePath1gVisitor` / `MakePath20gDedup` / `MakePath20gNeighbors` / `MakePath20gVisitor` | private | OK |
| `search_path.h` | `path_mark_set_state_root` / `fill_state_lut_T*` | private | OK |
| `search_simulate.h` | `Simulate1gNeighbors` / `Simulate1gVisitor` / `Simulate20gNeighbors` / `Simulate20gVisitor` / `path_mark_set_state_root` | private (line 148) | OK |
| `search_simple.h` | `SimpleRotEntry` / `collect_rotations_bb` / `emit_simple_drops_for_rotation` / `current_master_x*` / `fill_state_lut_T*` | private | OK |

`PathExtrasMixin` / `SimulateExtrasMixin` / `SimpleExtrasMixin` 当前定义在
`m_tetris::movegen` 命名空间, 任何外部代码都能拿到. 它们语义上是
"strategy 私有的 mixin", 本 commit 把它们各自挪到对应 strategy 头的
`detail::` 子命名空间中, 与该 strategy 静态类绑定:

```cpp
// src/search_path.h (commit 3 起)
namespace detail
{
    namespace path
    {
        template<class SpinHook, class RuleSpec>
        struct ExtrasMixin
        {
            using Config = typename SpinHook::Config;
            using LandPoint = typename SpinHook::LandPoint;
            using SearchState = typename SpinHook::SearchState;

            TetrisContext const *context_ = nullptr;
            Config const *config_ = nullptr;
            std::vector<LandPoint> land_point_cache_{};
            [[no_unique_address]] SearchState hook_state_{};
        };
    }
}
```

Strategy 内部 `using Context = MoveGenContext<RuleSpec, ...,
detail::path::ExtrasMixin<SpinHook, RuleSpec>>;`.

## Dummy strategy 设计

`examples/dummy_strategy.h` 提供一个最小 demo: "spawn-row drop only" —
对 spawn 行的 land_point 做 drop 后 emit. 完整契约满足:

- `using Context = m_tetris::movegen::MoveGenContext<RuleSpec,
  m_tetris::movegen::StateNodeLutMixin<RuleSpec>,
  detail::dummy::ExtrasMixin<SpinHook, RuleSpec>>;`
- `using LandPoint = typename SpinHook::LandPoint;`
- `using Config = typename SpinHook::Config;`
- `static void init(Context&, TetrisContext const*, Config const*);`
- `static std::vector<LandPoint> const* search(Context&, TetrisMap const&, TetrisNode const*, std::size_t depth);`
- `static std::vector<char> make_path(Context&, TetrisNode const*, LandPoint const&, TetrisMap const&);`

`make_path` 永远返回单一 `'D'` (drop). 仅用于编译/链接验证, 不接 oracle
diff (语义不正确).

`examples/dummy_strategy_compile.cpp` 实例化:

```cpp
#include "../src/movegen_searcher.h"
#include "../src/rule_srs.h"
#include "dummy_strategy.h"

using Backend = m_tetris::movegen::Searcher<
    m_tetris::movegen::DummyStrategy,
    m_tetris::NoSpinHook,
    rule_srs::TetrisRule::rule_spec>;

extern "C" int dummy_strategy_compile_check_entry(int argc, char ** argv)
{
    Backend backend{};
    (void)backend;
    return argc;
}
```

文件不进 production library, 只挂在 `dummy_strategy_compile_check`
target 上, 用于"模板演进时编译期保活".

## CMake target 设计

新增:

```cmake
project(dummy_strategy_compile_check)
add_executable(${PROJECT_NAME}
    examples/dummy_strategy_compile.cpp
    src/random.cpp
    src/rule_srs.cpp
    src/search_tspin.cpp
)
target_include_directories(${PROJECT_NAME} PRIVATE src examples)
target_link_libraries(${PROJECT_NAME} PRIVATE tetris_core)
```

`Searcher<DummyStrategy, NoSpinHook, rule_srs::TetrisRule::rule_spec>`
实例化必须 reachable, 因此 `random.cpp` / `rule_srs.cpp` /
`search_tspin.cpp` 三个 production 单元必须 link 进来 (`tetris_core`
依赖 `random.cpp` 提供的 RNG 符号 + `search_tspin` 的命名空间符号).
`tetris_core` 已经 PRIVATE include `core src`, 但当前 target
include `examples/` 让 `dummy_strategy.h` 可达.

## 文档章节 (中文)

`docs/extending_search.md`:

1. 总览: movegen 中立化架构
2. 三层结构: Mixin / Strategy / Searcher
3. 命名约定
4. 行为契约
5. 编写第三方 SearchStrategy: 最小示例 (引用 dummy_strategy.h)
6. Context API 速查
7. 复用工具: bb::run_bb_bfs / detail::common::MakePath1gDedup / 现有 mixin

## 编译验证计划

```
cd /workspace/iris_428d29df-9165-49d0-9579-a8eee1b2f449/tetris_ai_runner/build
cmake --build . -j$(nproc)
./oracle_diff
./path_node_diff
./simulate_node_diff
./tag_node_diff
```

外加 `dummy_strategy_compile_check` 必须 link 通过 (不需要运行).
