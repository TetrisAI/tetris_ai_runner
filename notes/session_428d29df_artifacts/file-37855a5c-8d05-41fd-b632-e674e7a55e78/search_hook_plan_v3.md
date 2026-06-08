# Search Hook 方案 v3 (响应 2026-05-28 第二轮反驳: 正交组合)

> 状态: 等用户拍板剩余 2 个细点 (commit 拆分 + search_aspin), v3 主架构已对齐用户原话.
> 关键字: search hook orthogonal NoHook DefaultTSpinHook bridge AI Search Rule 自适应

## 0. 用户最新反驳消化

> "B. 静默退化 NoHook, AI 总是取得默认值就可以了, 我们需要确保 AI、Search、Rule 是正交的, 任何组合都可以组装起来运行"

我 v2 的"Hook 决定 NodeEx"还是错的. 正确的语义应该是:

| 组件 | 谁定义 | 谁消费 |
|---|---|---|
| `Rule::rule_spec` | Rule 作者 | 框架 (MoveGen / context) |
| `AI::TetrisNodeEx` | **AI 作者** | AI 自己的 `eval/get/iterate` |
| `Search` 输出的 LandPoint | Search 内部 | TetrisCore 通过 `decltype` 推导, 然后桥接到 `AI::TetrisNodeEx` |
| `Hook::Payload` | Hook 作者 | Search 内部填充 |

**NodeEx 是 AI 端的 record, Search 不应该决定它**. Hook 只能填 NodeEx 里它认得的字段; AI 不关心的字段, Hook 不写; AI 关心但 Hook 不会写的字段, 自动取 NodeEx ctor 的默认值. 任何组合都能编译, 都能运行, 行为差异仅在"被填了的字段"上.

## 1. v3 架构总览

### 1.1 四层正交

```
                       ╔═══════════════════════════╗
              defines  ║  AI::TetrisNodeEx (record) ║  AI 自己说了算
                       ╚════════════╤══════════════╝
                                    │
                       ┌────────────▼─────────────┐
                       │   Engine 桥接层          │   自动转 LandPoint -> AI::TetrisNodeEx
                       │   (TetrisCore in core.h) │   缺字段保持默认值
                       └────────────┬─────────────┘
                                    │
                       ┌────────────▼─────────────┐
                       │   Search (e.g. MoveGen-  │
                       │   Search<Rule, Hook>)    │   Search 内部决定 LandPoint
                       └────────────┬─────────────┘
                                    │
                       ┌────────────▼─────────────┐
                       │   Hook (NoHook /         │   Hook 决定 BFS 期间维护什么、
                       │   DefaultTSpinHook /     │   emit 时往 Payload 写什么
                       │   DefaultASpinHook /     │
                       │   用户自定义)             │
                       └────────────┬─────────────┘
                                    │
                       ┌────────────▼─────────────┐
                       │   MoveGen<Rule, T, Hook> │   纯 BFS, 零 piece 字面量,
                       │                          │   零 spin 知识
                       └──────────────────────────┘
```

### 1.2 NodeEx 的"自适应桥接"约定 (核心)

定义一个编译期 trait `bridge_node_ex<DstNodeEx, SrcLandPoint>`:

```cpp
namespace m_tetris {

// 默认: 把 src 当作 TetrisNode* 用 (依赖 Search::LandPoint::operator TetrisNode const*())
//       构造一个 fresh DstNodeEx, 不写任何扩展字段.
template<class Dst, class Src, class = void>
struct bridge_node_ex {
    static Dst convert(Src const& src) {
        return Dst(static_cast<TetrisNode const*>(src));
    }
};

// 类型完全相同: 直通, 0 拷贝.
template<class T>
struct bridge_node_ex<T, T> {
    static T const& convert(T const& src) { return src; }
};

// AI 提供了显式构造 (AI::TetrisNodeEx 有从 Search::LandPoint 的 ctor): 走 ctor.
template<class Dst, class Src>
struct bridge_node_ex<Dst, Src,
    std::enable_if_t<std::is_constructible_v<Dst, Src const&> && !std::is_same_v<Dst, Src>>> {
    static Dst convert(Src const& src) { return Dst(src); }
};

}
```

这样:
- AI typedef = `TetrisNodeWithTSpinType` + Search hook 是 `DefaultTSpinHook` (LandPoint 也是 `TetrisNodeWithTSpinType`) → **类型相等, 直通**.
- AI typedef = `TetrisNodeWithTSpinType` + Search hook 是 `NoHook` (LandPoint 是 `TetrisNode const*`) → **走 fallback ctor**, `TetrisNodeWithTSpinType(node)` 把 spin/last/flags 全置默认值. AI 拿到 `is_ready = false, type = None`, 行为退化成"任何 T 落点都不算 spin", 完全合理.
- AI typedef = `TetrisNodeWithASpinType` + Search hook 是 `DefaultTSpinHook` → 走 fallback ctor, AI 拿到 `type = None`, 等价"无任何 ASpin".
- AI typedef = `TetrisNode const*` + 任何 Search → 走 fallback (operator->/static_cast), 直接拿到指针.

**任何 (AI, Search, Rule) 组合都能编译, 都能跑, 行为差异仅在 hook 是否填了对应字段上**.

### 1.3 Hook 接口 (规范化)

```cpp
struct NoHook {
    using MarkData    = std::monostate;        // BFS Mark 槽 payload, BFS 不读不写
    using RotState    = std::monostate;        // per-rotation 状态, BFS 不算
    using Payload     = std::monostate;        // emit 写出的 payload, AI 不消费
    using LandPoint   = TetrisNode const*;     // search() 返回的元素

    template<char T>
    static constexpr bool active_for_piece = false;

    // 框架根据 active_for_piece<T> 编译期决定是否调以下 hook,
    // false 时全部裁掉, 0 开销.
    static void on_init(...) {}
    static void on_rotate_reach(...) {}
    static void on_emit(...) {}
};

struct DefaultTSpinHook {
    using MarkData    = TSpinMark;             // {action, parent_r/xb/yb} (20g 路径用)
    using RotState    = TSpinRot;              // {corners3_arr, last_rotate_arr, ...}
    using Payload     = TSpinPayload;          // {spin, last_x/y/r, has_last_rot}
    using LandPoint   = TetrisNodeWithTSpinType;

    template<char T>
    static constexpr bool active_for_piece = (T == 'T');

    // T 块: 在 init 阶段根据 spawn 算 block_data_/x_diff_/y_diff_,
    // 在 expand_rotations 时点亮 last_rotate_arr,
    // 在 emit 时把 ready/mini/full 写入 Payload,
    // 在 LandPoint 构造时把 Payload 灌进 TetrisNodeWithTSpinType.
    static void on_init(...) { ... }
    static void on_rotate_reach(...) { ... }
    static void on_emit(...) { ... }
};

struct DefaultASpinHook {
    using LandPoint   = TetrisNodeWithASpinType;
    using Payload     = ASpinPayload;          // {ASpinType type}

    template<char T>
    static constexpr bool active_for_piece = true;   // 所有 piece 都做 4-direction collision

    // emit 阶段一次性查 4 方向 usable: 全阻塞则 type = ASpin.
    static void on_emit(...) { ... }
};
```

### 1.4 与 master 行为相同的保证

- ai.cpp:105-117 `MoveGenSearch<rule_toj::TetrisRule::rule_spec>` (Hook 默认 = DefaultTSpinHook) → AI typedef = TetrisNodeWithTSpinType (本来就是). 类型直通, **完全等价 master**.
- 当前 `kEnable=(T=='T')` 的字面行为 = `DefaultTSpinHook::active_for_piece<T>`, 因此 oracle_diff baseline 不变.
- search_aspin 这次先**不**接进来 (见第 3 节问题 2).

## 2. 落地 commit 计划 (与 v2 大致一致, 措辞改为 v3)

### Commit 1 (低风险): Hook 抽象骨架 + NoHook + MoveGen 模板参数切换
- `src/search_hook.h`: `struct NoHook` (空实现).
- `MoveGen<Spec, T, EnableMini=false>` → `MoveGen<Spec, T, Hook=NoHook>`.
- `LandingPos` 拆为 `LandingPos<Hook>`: 基础 (x/y/r) + `[[no_unique_address]] Hook::Payload extra`.
- 把 `if constexpr ((T=='T') && EnableMini)` 全部替换为 `if constexpr (Hook::template active_for_piece<T>)`.
- corners3 / last_rotate / emit_with_spin / target_blocked_mask / direction_open_mask 这几个 helper 暂时**保留在 MoveGen 里, 但只在 active_for_piece 时调用**, commit 2 再搬到 Hook.
- MoveGenSearch 暂时仍 `using NodeEx = TetrisNodeWithTSpinType`, 内部仍把 Hook 写死为 DefaultTSpinHook (本 commit 不改 MoveGenSearch 接口).
- 验证: oracle_diff 必须全过.

### Commit 2 (核心): DefaultTSpinHook 实体化 + MoveGenSearch 接 Hook 参数
- `src/default_tspin_hook.h`: 把 commit 1 留在 MoveGen 里的 corners3/last_rotate 等 helper 全部搬入 DefaultTSpinHook.
- MoveGenSearch 加 Hook 模板参数, 默认 = DefaultTSpinHook.
- MoveGenSearch::LandPoint = Hook::LandPoint.
- MoveGenSearch::init/spin_block_/spin_x_diff_ 搬入 DefaultTSpinHook (不再挂在 MoveGenSearch 成员上).
- 删除所有 `(T == 'T')` 字面量, 删除 `EnableMini`/`EnableT` 模板形参.
- `search_tspin::Search` 内部委托 `MoveGenSearch<Rule, DefaultTSpinHook>`.
- 验证: oracle_diff 必须全过.

### Commit 3 (兼容性): Engine 桥接层 + AI typedef 解耦
- `tetris_core.h` `TetrisCallAI<TetrisAI, LandPoint>` / `eval(...)` 路径加 `bridge_node_ex<AI::TetrisNodeEx, LandPoint>::convert(...)`.
- 不动 ai_zzz / ai_misaka / botris 的现有 typedef (它们继续 typedef 自家想要的 NodeEx; 桥接层负责适配).
- 这个 commit 实际生效场景是"未来 ai.cpp 把 MoveGenSearch 的 Hook 改成 NoHook 也不会编译失败".
- 验证: 现有所有可执行目标编译通过.

### (推迟) Commit 4: DefaultASpinHook + search_aspin 委托
- 见第 3 节问题 2, 默认不做.

## 3. 待你确认 (剩 2 项, NodeEx 语义你已经定了)

### 问题 1: commit 拆分粒度
- A. 3 个 commit (推荐, 每个独立 oracle_diff).
- B. 1 个大 commit.

### 问题 2: search_aspin 这次是否做
- A. **本次只做 commit 1+2+3** (推荐): TSpin hook 化, ASpin 暂保留旧实现. `bridge_node_ex` 已经覆盖了"未来 ASpin hook 化时也能正交组合"的能力.
- B. 加做 commit 4: ASpin 也接进来, 风险点是无 oracle baseline.

回个 (1/2 各一个字母) 我就直接开 commit 1.
