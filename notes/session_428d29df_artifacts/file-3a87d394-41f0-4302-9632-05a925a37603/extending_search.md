# 扩展 Movegen Search Strategy

> 本文档面向需要在 `tetris_ai_runner` 仓库 `flip-bits-clean` 分支之上接入
> 自定义 search 行为的第三方扩展者. 重构后 movegen 框架已经做到"中立组件
> + 可插拔 strategy", 写一个新的 search 不再需要触碰 `MoveGenSearch` 之类
> 的 god class, 只需按本文档的契约提供一个纯静态 strategy 类.

## 1. 总览: 中立化后的三层结构

```
┌──────────────────────────────────────────────────────────────────────┐
│                      上层调用方 (TetrisEngine / ai.cpp)               │
│                                                                      │
│   typedef m_tetris::movegen::Searcher<                                │
│       m_tetris::PathStrategy,           // ← 第三方可换的 strategy     │
│       m_tetris::DefaultTSpinHook,       // ← 第三方可换的 SpinHook     │
│       rule_srs::TetrisRule::rule_spec   // ← rule 由顶层选定           │
│   > MgSearch;                                                        │
└──────────────────────────────────────────────────────────────────────┘
                              │
                              ▼  调 init / search / make_path
┌──────────────────────────────────────────────────────────────────────┐
│ Searcher<Strategy, SpinHook, RuleSpec>   (src/movegen_searcher.h)     │
│                                                                      │
│   - 持单一成员 ctx_ : Strategy::Context                                │
│   - 把 init/search/make_path inline 转发到 Strategy::xxx(ctx_, ...)   │
│   - 暴露 Context / Config / LandPoint 别名供 TetrisEngine 模板嗅探    │
└──────────────────────────────────────────────────────────────────────┘
                              │
                              ▼  static 调用
┌──────────────────────────────────────────────────────────────────────┐
│ Strategy<SpinHook, RuleSpec>  (src/search_path.h / search_simulate.h │
│                                / search_simple.h / 第三方 header)     │
│                                                                      │
│   - 纯静态类, 没有非静态成员                                            │
│   - using Context = MoveGenContext<RuleSpec, ...mixins...>           │
│   - using Config / LandPoint = SpinHook::Config / SpinHook::LandPoint │
│   - static init / search / make_path 接受 Context& 作首参              │
└──────────────────────────────────────────────────────────────────────┘
                              │
                              ▼  组合
┌──────────────────────────────────────────────────────────────────────┐
│ MoveGenContext<RuleSpec, Mixins...>  (src/movegen_context.h)          │
│                                                                      │
│   - public 继承 bb::Helpers<RuleSpec> + 各 Mixin<RuleSpec[, ...]>    │
│   - 自身不持任何字段, 全部状态由 mixin 提供                            │
│   - 每个具体 strategy 只声明它需要的 mixin, 未声明的不实例化           │
└──────────────────────────────────────────────────────────────────────┘
```

三层都不依赖具体 search 行为, search 行为只活在 Strategy 类内部.
Strategy 是纯静态的, Searcher 持的是数据 (Context), 算法和数据是分开
管理的.

## 2. 命名约定

为避免文件名跟"具体 search 实现"混淆, 仓库内强制如下命名:

| 前缀 | 含义 | 示例 |
| --- | --- | --- |
| `movegen_*` | 公共 / 中立组件 | `movegen_context.h`, `movegen_searcher.h`, `movegen_strategy.h`, `movegen_hook.h` |
| `bb_*` | 位板侧基础设施 | `bb_state.h` (含 `bb::Helpers<RuleSpec>`), `bb_bfs_engine.h` |
| `search_*` | **具体** search 实现 | `search_path.h`, `search_simulate.h`, `search_simple.h`, 以及第三方扩展 |
| `rule_*` | 具体 rule 实例 | `rule_srs.h`, `rule_qq.h`, `rule_botris.h` |
| `ai_*` | 具体 AI 评估器 | `ai_zzz.cpp`, `ai_misaka.h`, `ai_tag.cpp` |

公共组件**禁止**使用 `search_*` / `ai_*` / `rule_*` 前缀; 反向, `search_*`
等前缀仅限"端到端的具体实现", 不允许放可复用的 helper.

第三方扩展应把自己的 strategy 放在 `search_<your_name>.h` 文件中, 与
官方 strategy 同形.

## 3. 行为契约

每个具体 strategy 必须满足下面的契约 (只接受 `SpinHook` 与 `RuleSpec`
两个模板参数, 不允许自带额外模板形参):

```cpp
template<class SpinHook, class RuleSpec>
struct YourStrategy
{
    // (1) Context 类型别名; 由 MoveGenContext<RuleSpec, mixins...> 装配.
    using Context = m_tetris::movegen::MoveGenContext<
        RuleSpec /*, ...mixins... */>;

    // (2) LandPoint / Config 路由, 透传 SpinHook 的别名.
    using LandPoint = typename SpinHook::LandPoint;
    using Config    = typename SpinHook::Config;

    // (3) init: TetrisEngine prepare 阶段调用一次.
    //     第二个形参是 TetrisContext (master node 表持有方),
    //     第三个形参是 Config* (来自 SearchConfigHolder, 可能为 nullptr).
    static void init(Context& ctx,
                     m_tetris::TetrisContext const* context,
                     Config const* config);

    // (4) search: 每次 search frontier 落点采集; 返回 LandPoint vector
    //     的指针, 生命周期等同于 ctx.land_point_cache_.
    static std::vector<LandPoint> const*
    search(Context& ctx, m_tetris::TetrisMap const& map,
           m_tetris::TetrisNode const* node, std::size_t depth);

    // (5) make_path: 给定 (起点 node, 落点 land_point, 当前 map),
    //     生成 master 按键串 (l/r/L/R/d/D/x/z/c 等).
    static std::vector<char>
    make_path(Context& ctx, m_tetris::TetrisNode const* node,
              LandPoint const& land_point, m_tetris::TetrisMap const& map);
};
```

契约关键不变量:

- **三个静态方法的首参恒为 `Context&`**. Searcher 只 inline 转发, 不允许
  在静态方法外做 ctx 重置 / 切换 / 拷贝.
- **不消费 `Hook::is_simple_search` / `Hook::is_simulate_search`** 这类
  历史的 `if constexpr` 路由 bool. 选哪种 search 由上层装配 typedef 时
  绑定的 Strategy 模板特化决定, 框架运行期不再分支.
- **行为完全由 SpinHook 与 RuleSpec 决定**. 同一个 strategy 配合 NoSpinHook
  和 DefaultTSpinHook 应该有可解释的差异, 不应额外读全局开关.

## 4. Context API 速查

`MoveGenContext<RuleSpec, Mixins...>` 自动暴露下列名字, 第三方 strategy
可以通过 `ctx.xxx` 直接访问:

### 4.1 来自 `bb::Helpers<RuleSpec>` (始终可用)

`MoveGenContext` public 继承 `bb::Helpers<RuleSpec>`, 因此所有 helper 函数
都自动是 Context 成员. 也可以通过本地 typedef `using Helpers =
bb::Helpers<RuleSpec>;` 显式调用 `Helpers::xxx(...)`. 官方 strategy 全部
采用后一种写法, 第三方扩展两种均可.

| 名字 | 用途 |
| --- | --- |
| `kW` / `kH` / `kMaxR` / `kPieceCount` | 编译期维度常量 |
| `map_t` / `PathMark` / `piece_info_t` | 类型别名 |
| `piece_index(t)` / `rcount_v<T>()` | piece char 索引 / per-piece 旋转数 |
| `build_board(map)` | 把 master `TetrisMap` 折成位板 `map_t` |
| `build_usable_for_piece(t, board, out)` | 构造 (T,r) 的 usable 二维表 |
| `build_usable_T<T>(board, out)` | 同上, piece 已知时的快路径 |
| `state_from_node(node)` | master `TetrisNode*` -> `bb::BBState` |
| `status_to_bbox(node, &xb, &yb)` | master status (x,y) -> bbox 坐标 |
| `usable_at_bb(r, xb, yb, usable)` | 直接查 usable 表 |
| `drop_bb_state(state, usable)` | 沿 column 扫到地面后的新 BBState |
| `first_passing_kick_bb(t, dir, state, usable)` | wall-kick 链返回首个 fits 的 BBState |
| `rotate_no_kick_bb(t, dir, state)` | 仅做 0-kick 旋转, 失败返 nullopt |
| `cells_key_for(node)` / `cells_key_for_state(s)` | 4-cell 绝对签名, 等价 master IndexFilter |
| `index_filtered_eq(node, key)` / `index_filtered_eq_state(s, key)` | cells_key 比对 |
| `try_kick_chain_to(t, dir, last_state, target_key, usable)` | T-spin 末段 wall-kick 重放谓词 |
| `open_bb(node, board)` | 落点是否 "可以从上方直接 drop 进入" |
| `board_roof(board)` | 棋盘最高占据行 +1 |
| `spawn_min_cell_y_dispatch(spawn_node)` | spawn piece 的最低 cell y |

### 4.2 来自具体 Mixin (按声明组合)

声明 `BfsQueueMixin<RuleSpec>` 后:
- `node_search_path_` (public): `std::vector<bb::BBState>`, 喂给
  `bb::run_bb_bfs(...)` 的 FIFO 队列缓冲, 多次 BFS 复用容量.

声明 `StateNodeLutMixin<RuleSpec>` 后:
- `state_node_lut_[]` (public): 维度 `[piece][r][yb][xb]` 的 LUT, init
  阶段一次性填好.
- `state_to_node(BBState)` (public): LUT 单点反查, 越界 / 未填返 nullptr.
- `clear_state_node_lut()` (public): 全表清零, 一般在 init 开头调用一次.

PathMark (BFS 期间 (r, xb, yb) 前驱 + op, 用于 `make_path` 末段
`build_path` 反向重建按键串) 不再以 mixin 形式上 ctx, 改由 strategy
在 BFS 入口栈分配 (`PathMark pm; pm.clear();`), 解锁后续多线程并发.

第三方 strategy 通常还需要一个自带的 mixin (类似官方
`detail::path::ExtrasMixin` / `detail::simulate::ExtrasMixin` /
`detail::simple::ExtrasMixin`) 来持 `TetrisContext const* context_`、
`Config const* config_`、`std::vector<LandPoint> land_point_cache_`、
`[[no_unique_address]] SearchState hook_state_` 等 SpinHook 依赖字段.
此类 mixin 应放在 strategy header 自己的 `detail::<strategy_name>::`
子命名空间里, 与 strategy 静态类绑定.

### 4.3 Searcher 暴露

`Searcher<Strategy, SpinHook, RuleSpec>` 仅 public 下列符号:

| 名字 | 类型 / 签名 | 备注 |
| --- | --- | --- |
| `Context` | `Strategy::Context` 别名 | 通常不直接消费, TetrisEngine 模板嗅探用 |
| `Config` | `Strategy::Config` 别名 | TetrisEngine `TetrisHasConfig` 探针强依赖此别名 |
| `LandPoint` | `Strategy::LandPoint` 别名 | 与 search 返回元素类型一致 |
| `init(...)` | inline 转发到 `Strategy::init(ctx_, ...)` | 形参由 strategy 自定义 |
| `search(...)` | inline 转发到 `Strategy::search(ctx_, ...)` | 同上 |
| `make_path(...)` | inline 转发到 `Strategy::make_path(ctx_, ...)` | 同上 |

`strategy_t` 别名 / `context()` 直读访问器为 protected/private — 第三方
**不应**通过这两个入口反向耦合具体 strategy 实现. 需要做中间层 (e.g.
adapter 子类) 时再继承 Searcher 拿 protected `context()`.

## 5. 编写一个新 SearchStrategy: 最小示例

仓库自带 `examples/dummy_strategy.h` 与 `examples/dummy_strategy_compile.cpp`
两个文件作为模板. CMake target `dummy_strategy_compile_check` 在每次
全量构建时实例化它, 保证文档与代码不会随 API 演进而失同步.

`DummyStrategy` 实现的最小行为是: 对 spawn 行的 `node->land_point` 列表
依次 drop 后 emit, `make_path` 永远返回单一 `'D'`. 它**不**正确 (不会
通过 oracle_diff), 只用来演示契约.

骨架 (节选, 完整版见 `examples/dummy_strategy.h`):

```cpp
namespace m_tetris
{
    namespace movegen
    {
        namespace detail
        {
            namespace dummy
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

        template<class SpinHook, class RuleSpec>
        struct DummyStrategy
        {
        public:
            using LandPoint = typename SpinHook::LandPoint;
            using Config = typename SpinHook::Config;
            using Context = MoveGenContext<RuleSpec,
                                           StateNodeLutMixin<RuleSpec>,
                                           detail::dummy::ExtrasMixin<SpinHook, RuleSpec>>;

            static void init(Context &ctx,
                             TetrisContext const *context,
                             Config const *config) { /* ... */ }

            static std::vector<LandPoint> const *
            search(Context &ctx, TetrisMap const &map,
                   TetrisNode const *node, std::size_t depth) { /* ... */ }

            static std::vector<char>
            make_path(Context &ctx, TetrisNode const *node,
                      LandPoint const &lp, TetrisMap const &map) { /* ... */ }
        };
    }

    template<class SpinHook, class RuleSpec>
    using DummyStrategy = movegen::DummyStrategy<SpinHook, RuleSpec>;
}
```

上层调用方挂钩:

```cpp
#include "movegen_searcher.h"
#include "rule_srs.h"
#include "examples/dummy_strategy.h"

using Backend = m_tetris::movegen::Searcher<
    m_tetris::DummyStrategy,
    m_tetris::NoSpinHook,
    rule_srs::TetrisRule::rule_spec>;
```

`Searcher` 实例化后即可作为 `TetrisEngine` 的第三模板参数装配. TetrisEngine
内的 `TetrisHasConfig` 探针会通过 `Backend::Config` 别名识别有无 search
config; `LandPoint` 走 `element_traits<decltype(search())>::Element` 推断,
不需要单独 alias 也能装配, 但保留 `Backend::LandPoint` 让上层代码可读.

## 6. 复用资源: Mixin / BFS 引擎 / 共享 dedup

写新 strategy 时, 优先复用下列已有组件:

### 6.1 Mixin 组合

| Mixin | 字段 / API | 何时使用 |
| --- | --- | --- |
| `m_tetris::movegen::BfsQueueMixin<RuleSpec>` | `node_search_path_` | 需要喂 `bb::run_bb_bfs` 的 FIFO 缓冲时 |
| `m_tetris::movegen::StateNodeLutMixin<RuleSpec>` | `state_node_lut_[]`, `state_to_node`, `clear_state_node_lut` | 需要把 `bb::BBState` 折回 master `TetrisNode*` 时 |

`PathMark` (BFS 命中后回放按键串) 不再走 mixin, 改由 strategy 在
BFS 入口栈分配 (`PathMark pm; pm.clear();`), 不再上 ctx.

需要 `TetrisContext*` / `Config*` / `LandPoint cache` 这种 SpinHook 依赖
字段时, 自己写一个 strategy 私有 mixin (放在 `detail::<your>::` 子命名
空间), 不要污染公共组件.

### 6.2 通用 BFS 引擎: `bb::run_bb_bfs`

声明位于 `src/bb_bfs_engine.h`. 调用契约:

```cpp
m_tetris::bb::run_bb_bfs(
    start_state,        // BBState, 起点
    neighbors,          // 提供 expand(state, emit) 的对象
    dedup,              // 提供 try_admit(state, parent_or_null, action) 的对象
    visitor,            // 提供 on_pop(state) / on_admit(state, action, decision) 的对象
    queue_buffer);      // std::vector<BBState>&, 调用方持有
```

`dedup.try_admit` 返回 `EnqueueDecision::{Skip|MarkOnly|MarkAndEnqueue}`
三态, 支持 cover_if 语义 (后到的更优 mark 覆盖先到的弱 mark, 但不再扩展).
邻居展开时通过 `emit` lambda 第三参数可指定 parent_override, 用于
"以中间状态作为前驱"的二级扩展 (如 1g rotate_move 的 X/Z/C).

### 6.3 共享 dedup: `detail::common::MakePath1gDedup<RuleSpec>`

定义在 `src/movegen_strategy.h`. 与 `bb::run_bb_bfs` 的 try_admit 契约一致,
封装 1g make_path 的 BFS 准入 (set_bbox + usable_at_bb 三态), `path` 与
`simulate` strategy 的 1g make_path 共用同一份 dedup. 使用方法:

```cpp
typename m_tetris::Helpers<RuleSpec>::PathMark pm;
pm.clear();
typename m_tetris::movegen::detail::common::MakePath1gDedup<RuleSpec> dedup{
    /*path_mark =*/ &pm,
    /*usable_arr =*/ &usable_arr};
```

不带 `usable` 校验的二态 dedup (例如 20g make_path) 应该 strategy 自己写,
官方 path strategy 用的是 `MakePath20gDedup`, 模板可参考.

## 7. 自检 & 防漂移

`docs/extending_search.md` (本文件)、`examples/dummy_strategy.h`、
`examples/dummy_strategy_compile.cpp` 与 CMake target
`dummy_strategy_compile_check` 形成一个闭环: 任何破坏 strategy 公共契约
的修改 (改 mixin 字段名、改 Searcher 暴露面、改静态方法签名) 都会让
`dummy_strategy_compile_check` 在全量构建时编译失败. 维护者改动公共
组件后, **必须**先确认本 target 仍能 link 通过.

## 8. 不变量与禁区

第三方扩展者请保持下列不变量:

- 不要为 `MoveGenContext` 添加自身字段; 所有状态都通过 mixin 持有.
- 不要在 strategy 静态方法外搬运 `Context`; Searcher 持的是单一 ctx 实体,
  跨 search 调用复用容量.
- 不要重新引入 `is_*_search` 这类 `Hook` 上的 trait bool; 选 strategy 是
  上层 typedef 的事, 不是 hook 的事.
- 不要把 strategy 的私有 BFS 三件套 (Neighbors / Dedup / Visitor) 移到
  公共组件里, 即便外形相似. 复用价值低于命名空间污染.
- 不要修改 `oracle/` 下任何文件; oracle 是行为 baseline, 由 `*_diff` 工具
  消费, 改了就失去 byte-equivalent 验证手段.
