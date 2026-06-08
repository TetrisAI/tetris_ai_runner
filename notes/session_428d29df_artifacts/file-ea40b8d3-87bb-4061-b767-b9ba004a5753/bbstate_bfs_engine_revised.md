# 位板 BFS 抽象: 评估修订 (撤回前一份的部分论点)

> 时间: 2026-05-30  会话: flip-bits-clean
> 上一份: bbstate_bfs_engine_pushback.md (反驳整体抽象)
> 用户三点反驳:
>   1. 最终目标是"所有 search 都用位板 + 行为与 oracle 一致" → 不是 lazy 场景, 是明确长期方向
>   2. NeighborProvider/DedupPolicy 就是统合不同 search 的"必备可定制项", 差异点不是反对抽象的理由, 而是抽象的 design driver
>   3. "多一层 indirection 影响内联"是编译期事情, 模板 + 静态方法不会破坏内联
> 我的态度调整: **承认前一份的 (3) 是错的; (1)(2) 列的差异点收回作为反对论据,
> 重做为 Engine 接口的设计需求.**

---

## 收回的论点

### 论点 (3) "多一层 indirection 影响内联" — 完全收回

`template<class RuleSpec, char T, class NeighborProvider, class DedupPolicy, class Visitor>`
是编译期完全已知的: NeighborProvider::for_each / DedupPolicy::on_enqueue / Visitor::on_node
都是 `static` 方法或 functor `operator()`, GCC/Clang/MSVC 在 -O2 下都会做完整 devirtualization
+ inlining。bfs_engine.h 自己就是这么用的, search_path_node 上线没出现热路径回退。
**性能不确定性这条不成立, 收回。**

### 论点 (α/β) 列的"差异点" — 仍然是真差异, 但角色变了

前一份把"差异点比共享点多"当成反对抽象的理由, 这是错的。
**差异点正是 Engine 三件套接口必须暴露的可定制位**:

| 差异 | 抽象时的责任方 |
|---|---|
| `cover_if` 三态 (Skip / MarkOnly / MarkAndEnqueue) | DedupPolicy 返回值类型升级 |
| 1g 复合邻居 X/Z/C (shift→rotate) | NeighborProvider 内部定义 |
| 1g 多阶段 disable_d 切换 | 上层 search 类跑两次 Engine, 不归 Engine 管 |
| drop 时机 (1g 不 drop / 20g pop 后 drop) | NeighborProvider 在 sink 前自己 drop |
| 命中谓词 (cells_key vs landing) | Visitor::on_node 内决定 |
| BFS 全局上下文 (usable_arr / config / hook_state) | 升级 bfs_engine.h 的 map 参数为 Context |

bfs_engine.h 那边 `bool on_enqueue` 是因为指针图 BFS 不需要 cover_if; 位板侧需要,
**枚举返回值升级是合理扩展**。前一份说"反向污染清爽的指针 BFS engine" — 实际可以两个 engine
同存或者把指针图 engine 视为枚举返回值的退化情形 (只用 Skip / MarkAndEnqueue 两态)。

---

## 修订后的方案: 位板 BFS Engine

### 接口草案

```cpp
// bb_state.h: 纯数据 + 静态 helper
namespace m_tetris::bb {
    struct BBState { uint8_t t, r; int8_t xb, yb; };

    // 既有 helper 从 MoveGenSearch private 抽出来:
    //   build_usable_for_piece, usable_at_bb, drop_bb_state,
    //   first_passing_kick_bb, rotate_no_kick_bb,
    //   cells_key_for_state, index_filtered_eq_state,
    //   state_to_node LUT (per-instance, 由 search 类持有)
}

// bb_bfs_engine.h: 位板 BFS 引擎
namespace m_tetris::bb_bfs {
    enum class EnqueueDecision { Skip, MarkOnly, MarkAndEnqueue };
    enum class VisitResult { Continue, Stop };

    // BFS 全局上下文, 替代 bfs_engine.h 里裸 map 参数.
    template<class RuleSpec>
    struct BfsContext {
        Map<RuleSpec::width, RuleSpec::height> board;
        std::array<Map<RuleSpec::width, RuleSpec::height>, kMaxR> usable_arr;
        // 视各 search 需要再扩 (config, hook_state 等)
    };

    template<class RuleSpec, char T,
             class NeighborProvider,    // static for_each(state, ctx, sink)
             class DedupPolicy,          // EnqueueDecision on_enqueue(s, parent, action)
             class Visitor>              // VisitResult on_node(s, ctx)
    class BBStateBfsEngine {
        // queue<BBState>, drive() 主循环
        // sink 把 (child_state, action) 喂给 dedup, 按返回值入/不入队
    };
}
```

### 各 search 套用方式

```cpp
// run_piece_20g: 已经在 MoveGenSearch 内, 重写为
struct TwentyGravityKickNeighbors {
    template<class Ctx, class Sink>
    static void for_each(BBState s, Ctx const& ctx, Sink sink) {
        // l/r/d shift  → sink 普通邻居 (不 drop, 由 visitor 在 on_node 内 drop)
        // x/z/c kick   → first_passing_kick_bb → sink (跟 master 邻居顺序一致)
    }
};
struct TwentyGravityCoverIfDedup {
    EnqueueDecision on_enqueue(BBState s, BBState parent, char action) {
        // mark[i].visited == 0 → MarkAndEnqueue
        // mark[i].action == ' ' && action != ' ' → 覆盖 mark, 返 MarkOnly (cover_if)
        // 否则 → Skip
    }
};
struct LandingEmitVisitor { /* 出队 drop, 命中 landing 时 try_emit, 总是 Continue */ };

// make_path 1g: NeighborProvider 持 6 个 config bool, 邻居顺序 x/z/c/l/r/L/R/d/D + X/Z/C
// make_path 20g: NeighborProvider 类似 20g_kick, 加 L/R; visitor 命中 cells_key 即 Stop

// 当 search_aspin / search_simple_node 改造接位板时, 同样可以选自己需要的 NeighborProvider
// 复用 — 这正是抽象的真实价值.
```

---

## 真实的设计风险 (这才是该讨论的)

### 风险 1: BFS 上下文 (Context) 字段集合不稳定

不同 search 需要的 context 字段:
- 20g run: usable_arr, allow_180, hook_state (emit 时填 spin)
- make_path 1g: usable_arr, allow_180/LR/D/d/rotate_move/disable_d
- make_path 20g: usable_arr, allow_180/LR
- search_aspin (未来): usable_arr, hook_state (any-spin 检测)
- search_tag (未来): usable_arr, last_rotate_arr

如果 Engine 模板化 `BfsContext<...>` 太狭窄会被反复扩, 太宽又会迫使所有 search 都填一堆空字段。
**建议**: Engine 不规定 Context, 各 search 自己定义 Context type, NeighborProvider/Dedup/Visitor
约定都拿同一个 `Ctx` template 参数即可。Engine 只存 `Ctx const&` 透传。

### 风险 2: state_to_node LUT 跨 search 共享还是各自持有

LUT 是 per-context (不同 RuleSpec 走不同 piece_cells), 但跟 BFS 主循环无关 (只在 emit
反查 master pointer 时用)。**LUT 不进 Engine, 留给具体 search 类**, Engine 只跑 BBState 主循环。

### 风险 3: 重构成本

当前 movegen_search.h 三处 BFS 总共 ~700 行. 抽 Engine + 重写 NeighborProvider/Dedup/Visitor
工程上是 3-4 个 commit:
1. 抽 helper 到 `bb_state.h` (零行为变化)
2. 引入 `bb_bfs_engine.h` 接口 + 把 `run_piece_20g` 改造接入 (oracle_diff 跨 piece × 场地全过)
3. `make_path` 1g 接入
4. `make_path` 20g 接入

**每步独立 oracle_diff verify**, 任何一步出回归立刻 revert。这是可控的。

---

## 修订后的态度

- 前一份 (3) "性能不确定性" — **完全收回**, 你说得对, 编译期事情。
- 前一份 (α/β) 列的"差异点" — 收回作反对论据, **改作 Engine 接口的设计需求**。
- 整体方向: **同意走 Engine 抽象**, 跟"所有 search 都用位板"长期目标一致。

唯一保留的提醒是**做的顺序**:
- 不要一次抽完三处 BFS — 风险大, oracle_diff 排查窗口长
- 按 commit 1-4 顺序走, 每步独立 verify, 失败即 revert
- helper 先抽 (commit 1), 这步零风险, 给后续 Engine 接入打基础

## 想跟你确认

- (1) 是否同意从 "commit 1: 抽 helper 到 bb_state.h" 开始, 第一步零行为变化?
- (2) Engine 接口里 `EnqueueDecision { Skip, MarkOnly, MarkAndEnqueue }` 三态枚举可接受?
  (前一份把它当成反对理由是错的, 这里它是合法 design)
- (3) Context 不在 Engine 里规定, 各 search 自定义 Ctx type, 三件套接同一个 `Ctx`,
  Engine 只透传 — 同意?

如果三个都点头, 下一回合我开 commit 1.
