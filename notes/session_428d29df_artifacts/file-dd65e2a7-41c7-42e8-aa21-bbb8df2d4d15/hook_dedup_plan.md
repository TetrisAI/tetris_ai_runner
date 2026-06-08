# Hook 与 BFS 三件套去重方案 (落盘)

> 状态：方案已落盘待用户逐一审查。当前栈位于 `flip-bits-clean`，HEAD = `92d0804`。
> 范围：`src/movegen_hook.h` 中的 5 个 SpinHook + 4 个 strategy 头 (`search_path.h`,
> `search_simulate.h`, `search_simple.h`, `search_tag.h`) 内部自带的 BFS 三件套
> (Slot / Dedup / Neighbors / Visitor)。

---

## 0. 现状全景 (Map of Things)

### 0.1 顶层 SpinHook (5 个, 全部住在 `src/movegen_hook.h`)

| Hook | active_for_piece | LandPoint | Config | Payload | 用途 |
| :--- | :--- | :--- | :--- | :--- | :--- |
| `NoHook` | 全 false | 裸 `TetrisNode const*` | 空 struct | `NoTSpinPayload` | tag/ai_tag 无 spin 路径 |
| `NoSpinHook` | 全 false | `search_tspin::NodeEx` | 空 struct | `NoTSpinPayload` | path/simulate 无 spin 路径 |
| `TSpinHook` | `T == 'T'` | `search_tspin::NodeEx` | `search_tspin::Config` | `TSpinPayload` | T-spin 全态 |
| `ASpinHook` | 全 true | `search_aspin::NodeEx` | `search_aspin::Config` | `ASpinPayload` | All-spin 系列 |
| `CautiousHook` | 全 false | `search_tspin::NodeEx` | `{bool fast_move_down}` | `NoTSpinPayload` | c2_ai (Cultris II) |

### 0.2 每 strategy 内部 BFS 三件套 (15+ 个 struct)

| strategy | 阶段 | Slot | Dedup | Neighbors | Visitor |
| :--- | :--- | :--- | :--- | :--- | :--- |
| `path` | run_piece_20g | `Run20gMarkSlot` (5B) | `Run20gDedup<T,EnableT>` | `Run20gNeighbors<T>` | `Run20gVisitor<T,EnableT>` |
| `path` | make_path_1g | (PathMark) | `detail::common::MakePath1gDedup` | `MakePath1gNeighbors` | `MakePath1gVisitor` |
| `path` | make_path_20g | (PathMark) | `MakePath20gDedup` | `MakePath20gNeighbors` | `MakePath20gVisitor` |
| `simulate` | search_1g | (PathMark) | `detail::common::MakePath1gDedup` | `Simulate1gNeighbors` | `Simulate1gVisitor` |
| `simulate` | search_20g | (PathMark) | `Simulate20gDedup` (set 二态) | `Simulate20gNeighbors` | `Simulate20gVisitor` |
| `tag` | search_1g | TagMark | `TagSearchDedup` | `TagSearch1gNeighbors` | `TagSearch1gVisitor` |
| `tag` | search_20g | TagMark | `TagSearch20gDedup` | `TagSearch20gNeighbors` | `TagSearch20gVisitor` |
| `tag` | search_t | TagMark | `TagSearchTDedup` | `TagSearchTNeighbors` | (合并入 Dedup) |
| `tag` | make_path | TagMark | `TagMakePathDedup` | `TagMakePathNeighbors` | `TagMakePathVisitor` |
| `simple` | search_drop | — | — | (无 BFS, 直接列举 spawn-row) | (drops only) |

> 注: `simple` 不走 BFS, 直接列举 `usable_arr[r]` 的位; 不参与本轮 BFS 三件套去重.

---

## 1. 审计结论 (重复模式 5 个)

### Pattern A — SpinHook 接口 noop 板样
5 个 Hook 中 `NoHook / NoSpinHook / CautiousHook` 三家完全重复实现:
- `on_init_rotations`, `on_rotate_reach`, `on_emit`, `on_search_state_init`, `check_ready`,
  `check_mini_ready` 全部空实现.
- `RotState`, `SearchState` 定义为 empty struct.
- `LandPoint trait` (is_landpoint_none / get_last_node) 退化成同一组 noop.

`TSpinHook / ASpinHook` 之间也共享 `Config trait` (config_allow_180 / LR / d / D /
rotate_move / is_20g) 的相同实现, 仅 `TSpinHook` 多了 `last_rotate`.

📏 **冗余规模**: 大约 260 行模板 noop, 分散在 5 个 hook 内.

### Pattern B — Config trait 字面量重复
- `NoSpinHook::config_allow_180 / LR / d / D / rotate_move` 全 `return true`;
  `is_20g / last_rotate` 全 `return false`.
- `CautiousHook` 与 `NoSpinHook` 仅差一处 `config_allow_d = !cfg->fast_move_down`.
- `TSpinHook / ASpinHook` 完全同形 `cfg && cfg->xxx` 的 6 行写法各自重写.

### Pattern C — BFS Neighbors 中 L/R-multistep + drop/kick 模板
`MakePath1gNeighbors`, `MakePath20gNeighbors`, `Simulate1gNeighbors`, `Simulate20gNeighbors`
内的 L 多步、R 多步、`first_passing_kick_bb(Opp/Ccw/Cw)` 三连、d/D 落底逻辑,
4 份代码逐字符雷同, 只有 `pre_drop`/`post_drop` 顺序差别.

📏 **冗余规模**: 约 4 × 130 ≈ 520 行, 全部是简单的 BBState 平移 + kick 序列.

### Pattern D — Visitor 中 cells_key 命中谓词
`MakePath1gVisitor`, `MakePath20gVisitor`, `Simulate1gVisitor`, `Simulate20gVisitor`,
`TagMakePathVisitor` 共用同一 `check_hit` 形态:
```
k = Helpers::cells_key_for_state(s);
if (k == index) return true;
if (!landpoint_is_none && last_rotate && k == index_landpoint) return true;
```
仅 `Simulate20gVisitor` 多一层 lazy drop. 其余完全同形.

### Pattern E — Dedup 起点协议 + bbox + PathMark 写入
所有 5 个 dedup 都是相同模板:
```
if (oob) return Skip;
PrevKey pk = (parent==nullptr) ? self_as_prev(s) : from(parent);
if (!path_mark->set_bbox(...)) return Skip;
if (!usable_at_bb(...)) return MarkOnly;
return MarkAndEnqueue;
```
唯独 `Run20gDedup` 多了一组 `parent_xb/yb/r` 字段保存于自己的 `Run20gMarkSlot`.

---

## 2. 重构提案 (5 项, 按风险升序)

> 共识纪律:
> 1. 每项独立成一个 commit, 落地前先 `oracle_diff` byte-equal 验收;
> 2. 不允许触碰 `oracle/` 任何文件;
> 3. 不允许改 `ai.cpp`/`ai_*.cpp` 的实例化, 只动 `src/movegen_hook.h` 与
>    `src/search_*.h` 内部细节;
> 4. 用户编码习惯: 局部变量不加 const, 提交前格式化, 最后由用户审阅再 push.

### R.1 — `BaseSpinHook` CRTP 抽 noop (低风险)

**目标**: 消除 Pattern A. 在 `movegen_hook.h` 顶端引入 CRTP 基类:

```cpp
template<class Derived>
struct BaseSpinHook
{
    struct EmptyRotState {};
    struct EmptySearchState {};

    template<class MapT, std::size_t R_count>
    using RotState = EmptyRotState;
    using SearchState = EmptySearchState;

    template<char T> static constexpr bool active_for_piece = false;

    static bool is_landpoint_none(typename Derived::LandPoint const &) noexcept { return true; }
    static TetrisNode const *get_last_node(typename Derived::LandPoint const &) noexcept { return nullptr; }

    template<class Spec, char T, class MapT, std::size_t R_count>
    static void on_init_rotations(MapT const &, RotState<MapT, R_count> &) noexcept {}

    template<class MapT, std::size_t R_count>
    static void on_rotate_reach(std::uint8_t, MapT const &, RotState<MapT, R_count> &) noexcept {}

    template<class Spec, char T, class LP, class MapT, std::size_t R_count, class OA, class Fn>
    static void on_emit(MapT const &, int, RotState<MapT, R_count> const &,
                        std::array<MapT, R_count> const &,
                        std::array<MapT, R_count> const &,
                        OA const &, OA const &, Fn &&) noexcept {}

    static void on_search_state_init(SearchState &, TetrisContext const *) noexcept {}

    static bool check_ready(TetrisMap const &, TetrisNode const *, SearchState const &) noexcept { return false; }
    static bool check_mini_ready(TetrisMap const &, typename Derived::LandPoint const &) noexcept { return false; }

    static constexpr bool resolves_last_1g = false;
    static bool payload_has_last_rot(typename Derived::Payload const &) noexcept { return false; }
    static std::uint8_t payload_last_r(typename Derived::Payload const &) noexcept { return 0; }
    static int payload_last_x(typename Derived::Payload const &) noexcept { return 0; }
    static int payload_last_y(typename Derived::Payload const &) noexcept { return 0; }
};
```

`NoHook / NoSpinHook / CautiousHook` 改为 `: BaseSpinHook<XxxHook>` 派生; 只保留
`Payload`, `LandPoint`, `Config` 与差异 trait. `TSpinHook` / `ASpinHook` 保持
独立 (active_for_piece=true 路径下要写 on_emit / on_init_rotations / apply_emit_*).

**收益**: ~260 行模板 noop 砍到 ~30 行.
**风险**: 低. 全部空实现的语义不变. CRTP 只是源码级改写, 模板实例化等价.
**验收**: `oracle_diff` byte-equal (4 strategy × 1g/20g × 7 piece).

### R.2 — Config trait 表驱动 (低风险)

**目标**: 消除 Pattern B. 引入小工具:
```cpp
namespace m_tetris::detail
{
    template<bool LastRotate, bool Allow180, bool AllowLR, bool AllowD, bool AllowSmallD,
             bool AllowRotateMove, bool Is20g>
    struct StaticConfigPolicy { /* config_xxx() 一律 return constants */ };

    template<class Cfg>
    struct CfgFieldPolicy { /* config_xxx(cfg) -> cfg && cfg->xxx */ };
}
```
- `NoSpinHook` / `CautiousHook` 走 `StaticConfigPolicy` 实例化 (后者多包一层
  `cfg && !cfg->fast_move_down` 决定 `allow_d`).
- `TSpinHook` / `ASpinHook` 走 `CfgFieldPolicy<TSpinConfig>`.

**收益**: 6 × 5 = 30 行字面量函数收敛为 1 个 policy. 行为逐位等价.
**风险**: 低. 需要确认 `config_allow_d` / `is_20g` 在 cfg=nullptr 时全部 fallback
到 false (与现实现一致).
**验收**: `oracle_diff` byte-equal.

### R.3 — `BfsKitsCommon`: L/R-multistep + kick3 + d/D 套件 (中风险)

**目标**: 消除 Pattern C. 在 `movegen_strategy.h` 增 `detail::common` 命名空间:
```cpp
template<class RuleSpec>
struct BfsKitsCommon
{
    using Helpers = bb::Helpers<RuleSpec>;
    using map_t = typename Helpers::map_t;

    // L/R 多步 (post-drop=false 用于 1g/20g search; post-drop=true 用于 20g make_path).
    template<bool PostDrop, class Emit>
    static void emit_LR_multistep(char piece_t, bool allow_LR, bb::BBState const &cur,
                                  std::array<map_t, Helpers::kMaxR> const &usable,
                                  Emit emit);

    // kick3 (Opp/Ccw/Cw), 可选 PassingKick 还是 NoKick.
    template<bool WithPassingCheck, bool PostDrop, class Emit>
    static void emit_kick3(char piece_t, bool allow_180, bb::BBState const &cur,
                           std::array<map_t, Helpers::kMaxR> const &usable,
                           Emit emit);

    // d / D 落底 (1g): allow_d, allow_D 各一; disable_d 套件单出.
    template<class Emit>
    static void emit_d_D(bool allow_d, bool allow_D, bool disable_d,
                         bb::BBState const &cur,
                         std::array<map_t, Helpers::kMaxR> const &usable,
                         Emit emit);
};
```
4 个 Neighbors 改为 `BfsKitsCommon::emit_xxx(...)` 调用组合, 每家保留自己的开关
读取 (`allow_d / allow_D / allow_LR / allow_rotate_move / allow_180`).

**关键风险点**: oracle 的邻居 emit **顺序** 决定 BFS 命中先后,
后续与 `cells_key_for_state` 命中的 path 字符串严格相关. **任何顺序变动都会
让 oracle_diff 偏离**. 必须逐 strategy 单独 commit:
1. R.3.a path 1g (含 rotate-move 嵌套, 最复杂)
2. R.3.b path 20g make_path
3. R.3.c simulate 1g
4. R.3.d simulate 20g

**收益**: 约 520 行 → 130 行 + 套件. 后续 `tag` 引入 PathMark 时也能复用.
**风险**: 中. 邻居顺序若错位会触发批量 oracle_diff 偏移.
**验收**: 每个子 commit 独立 `oracle_diff` byte-equal, 在 `flip-bits-clean` 上多 piece
(I/J/L/T/S/Z/O) × 2 mode × 5 random map 下都过.

### R.4 — `MakePathHitVisitor`: cells_key 命中谓词共享 (中风险)

**目标**: 消除 Pattern D. 在 `movegen_strategy.h` 加:
```cpp
template<class RuleSpec, bool LazyDrop>
struct MakePathHitVisitor
{
    bb::CellsKey index_key;
    bb::CellsKey index_landpoint;
    bool last_rotate;
    bool landpoint_is_none;
    std::array<map_t, Helpers::kMaxR> const *usable_arr;  // 仅 LazyDrop=true 用
    bb::BBState found{};
    bool hit = false;

    bool on_pop(bb::BBState const &) { return true; }
    bool on_admit(bb::BBState const &s, char, bb::EnqueueDecision);
};
```
- `LazyDrop=false` 给 `MakePath1gVisitor`, `Simulate1gVisitor`, `MakePath20gVisitor`,
  `TagMakePathVisitor`.
- `LazyDrop=true` 给 `Simulate20gVisitor`.

**收益**: 5 份 visitor → 1 份 + Tag 一份特化 (TagMakePath 多读 path_mark).
**风险**: 中. `MakePath20gVisitor` 区分 `index` vs `index_landpoint + last_rotate`,
`Simulate1gVisitor` 不区分 (只看 `index_landpoint`); 这两者必须保留独立行为. 用
`bool match_index_key_only` 模板参数区分.
**验收**: oracle_diff make_path 字符串 byte-equal.

### R.5 — `BboxDedup`: 起点协议 + bbox + PathMark 套件 (低风险, 但被 R.4 阻塞)

**目标**: 消除 Pattern E. 把 `detail::common::MakePath1gDedup` 扩展为可参数化:
```cpp
template<class RuleSpec, bool RequireUsable>
struct BboxDedup
{
    PathMark *path_mark;
    std::array<map_t, kMaxR> const *usable_arr;  // 仅 RequireUsable=true 用

    bb::EnqueueDecision try_admit(bb::BBState const &s,
                                  bb::BBState const *parent, char action);
};
```
- `RequireUsable=true` 替代 `MakePath1gDedup`, `Simulate1gDedup`.
- `RequireUsable=false` 替代 `MakePath20gDedup`, `Simulate20gDedup`.
- `Run20gDedup` 不动 (它要写额外的 parent slot).

**收益**: 4 份 dedup → 1 份 + 1 份 Run20gDedup 保留.
**风险**: 低. 起点协议与 PathMark::set_bbox 合约已统一, 行为等价.
**验收**: oracle_diff byte-equal.

---

## 3. 推荐执行顺序 (Roadmap)

| # | 提案 | 风险 | 阻塞 | 行减少估计 |
| :--- | :--- | :--- | :--- | :--- |
| 1 | **R.1** BaseSpinHook CRTP | 低 | — | ~230 |
| 2 | **R.2** Config policy 表驱动 | 低 | — | ~25 |
| 3 | **R.5** BboxDedup 模板化 | 低 | R.4 | ~80 |
| 4 | **R.4** MakePathHitVisitor 共享 | 中 | — | ~60 |
| 5 | **R.3.a** path 1g neighbors | 中 | R.4/R.5 完成后再做 | ~120 |
| 6 | **R.3.b** path 20g make_path | 中 | R.3.a | ~110 |
| 7 | **R.3.c** simulate 1g | 中 | R.3.b | ~100 |
| 8 | **R.3.d** simulate 20g | 中 | R.3.c | ~95 |

总减约 820 行, 4 strategy 头变薄约 35-40%. 每步独立 commit, 单独 oracle_diff.

---

## 4. 不在本轮范围 / 待用户决策

- **R.6 LandPoint 位板化** (高风险/远期): 把 LandPoint 主键从 `TetrisNode*` 换成
  `BBState`. 涉及下游所有 AI eval 函数与 `TetrisEngine::search` 接口. 需要建分支
  独立推进, 与本轮 hook 去重无依赖.
- **`tag` strategy 引入 PathMark** (高风险/中期): 当前 tag 仍用自家 TagMark 与
  master 旋转指针. 与本轮去重正交, 等 N.3 (`search_tag.h` 旋转去 master 化)
  落地后再合并到 `BboxDedup`.
- **`Run20gDedup` 与 `BboxDedup` 合并** (低优先): `Run20gMarkSlot` 多带
  `parent_r/xb/yb/action` 字段, 可考虑参数化 BboxDedup 的 Slot 类型. 但收益
  仅 ~30 行, 暂不纳入.

---

## 5. 用户审查 Checklist

请用户对以下条目逐一裁决, 然后我再开始 R.1:

- [ ] R.1 BaseSpinHook CRTP, 抽 5 hook noop —— **同意 / 修改 / 否决**
- [ ] R.2 Config policy 表驱动 —— **同意 / 修改 / 否决**
- [ ] R.3 BfsKitsCommon (含 4 子 commit 拆分) —— **同意 / 修改 / 否决**
- [ ] R.4 MakePathHitVisitor 模板化 —— **同意 / 修改 / 否决**
- [ ] R.5 BboxDedup 模板化 —— **同意 / 修改 / 否决**
- [ ] 推荐执行顺序 (Section 3) —— **同意 / 修改 / 否决**

每一项审查通过后, 我才开始动代码; 不会一次性合并执行.

## 6. 上下文锚点 (新会话快速恢复)

- HEAD: `92d0804 refactor(strategy): demote ExtrasMixin context_ to init-only and bitboardize tag/simple roof dispatch`
- 最近 5 次相关 commit: `92d0804 / 7c7e53b / 53d79e4 / 43f8c44 / 47b8bf8`
- 关联调研:
  - `.research/flip-bits-cleanup/strategy_purity_audit.md` — 4 strategy purity 审计
  - `.research/flip-bits-cleanup/seven_namespace_plan.md` — 7 命名空间统一历史
  - `.research/flip-bits-cleanup/next_session_remove_context.md` — TetrisContext 拆除剩余 N.1-N.4
- 关键文件:
  - `src/movegen_hook.h` (1046 行, 5 hook)
  - `src/movegen_strategy.h` (134 行, 仅 1 个共享 dedup)
  - `src/search_path.h` (1176 行)
  - `src/search_simulate.h` (655 行)
  - `src/search_tag.h` (~1100 行, 本轮暂不动)
  - `src/search_simple.h` (本轮不动)
