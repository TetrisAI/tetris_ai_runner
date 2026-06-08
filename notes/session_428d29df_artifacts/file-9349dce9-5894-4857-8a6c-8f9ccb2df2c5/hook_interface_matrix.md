# Hook 接口矩阵审计 (落盘)

> 状态：方案落盘待用户逐一审查。HEAD = `92d0804`。
> 范围：`src/movegen_hook.h` 5 个 Hook × **完整接口面** (类型族 / trait / 算法 / 1g / 20g / make_path 全部覆盖).
> 目的：把 Hook 暴露的每一个接口、每一处被 strategy 消费的位置都摆成矩阵，
> 对每一格回答两个问题:
> 1. 该 hook 上的该接口是否合理 (语义自洽 / 是否冗余 noop / 接口参数是否臃肿)？
> 2. 是否有多余的需求 (实现存在但调用方从不消费 / 调用方消费但实现总是常量)？

---

## 0. Hook 接口面清单 (28 个接口, 6 大类)

### 0.1 类型族 (6 个)
| # | 接口 | 性质 | 由谁消费 |
| :-- | :--- | :--- | :--- |
| T1 | `LandPoint` | typedef | strategy `LandPoint = SpinHook::LandPoint`, ai eval, ctx.land_point_cache_ |
| T2 | `Config` | typedef | strategy `Config = SpinHook::Config`, TetrisEngine status_config_ |
| T3 | `Payload` | typedef | `LandingPosT<Hook>::extra` 字段 |
| T4 | `RotState<MapT, R_count>` | typedef | `MoveGen::generate` 局部 `rot_state` (BFS per-piece) |
| T5 | `SearchState` | typedef | strategy 持有 `ctx.hook_state_` |
| T6 | `active_for_piece<T>` | constexpr bool | strategy 1g/20g pop, MoveGen kCheckTSpin |

### 0.2 LandPoint trait (2 个)
| # | 接口 | 由谁消费 |
| :-- | :--- | :--- |
| L1 | `is_landpoint_none(lp)` | path/simulate/tag make_path: 区分 None / spin 路径 |
| L2 | `get_last_node(lp)` | path/simulate/tag make_path: 取 last_rotate 谱系 |

### 0.3 Config trait (7 个)
| # | 接口 | 由谁消费 |
| :-- | :--- | :--- |
| C1 | `config_last_rotate(cfg)` | path/simulate/tag make_path: 是否启用 last_rotate 命中谓词 |
| C2 | `config_allow_180(cfg)` | path/simulate/tag run/make_path: 邻居是否枚举 180° |
| C3 | `config_allow_LR(cfg)` | path/tag make_path: 邻居是否枚举 L/R 多步 |
| C4 | `config_allow_d(cfg)` | path 1g make_path: 邻居是否展开 'd' |
| C5 | `config_allow_D(cfg)` | path 1g make_path: 邻居是否展开 'D' 落底 |
| C6 | `config_allow_rotate_move(cfg)` | path 1g make_path: 是否枚举 rotate-after-shift |
| C7 | `config_is_20g(cfg)` | path/simulate/tag/simple search: 选 1g vs 20g BFS 入口 |

### 0.4 Payload trait (5 个)
| # | 接口 | 由谁消费 |
| :-- | :--- | :--- |
| P1 | `resolves_last_1g` (constexpr) | path/simulate 1g pop: 是否要从 payload 折回 last_node |
| P2 | `payload_has_last_rot(p)` | (P1=true 路径下) 检查本 emit 是否携带 last 节点 |
| P3 | `payload_last_r(p)` | (P2=true) 折回用的旋转位 |
| P4 | `payload_last_x(p)` | (P2=true) 折回用的 x |
| P5 | `payload_last_y(p)` | (P2=true) 折回用的 y |

### 0.5 算法回调 (8 个)
| # | 接口 | 由谁消费 |
| :-- | :--- | :--- |
| A1 | `on_init_rotations<Spec,T>(board, rs)` | MoveGen::generate 一次性建 corners3 等 |
| A2 | `on_rotate_reach(dst_r, reached, rs)` | MoveGen::expand_rotations 累积 last_rotate_arr |
| A3 | `on_emit<Spec,T,LP>(landings, r, rs, search, usable, ox, oy, fn)` | MoveGen::generate 落点输出 |
| A4 | `on_search_state_init(state, ctx)` | strategy::init 一次性建 SearchState (TSpinHook x_diff/y_diff/block_buffer) |
| A5 | `check_ready(map, node, state)` | tag search_t_native: T-spin ready 判定 |
| A6 | `check_mini_ready(map, lp)` | TSpinHook::apply_emit_20g 内自调; tag make_path 间接消费 |
| A7 | `apply_emit_1g<MapT,R>(lp, payload, depth, cfg, last_node, usable, r, xb, yb)` | path/simulate 1g pop |
| A8 | `apply_emit_20g<MapT,R>(lp, last_node, action, depth, cfg, map, state, sunk_node, usable, r, xb, yb)` | path 20g run_piece_20g pop |

---

## 1. 完整矩阵 (5 hook × 28 接口)

> 标记: ✅ = 真正消费（实现非空 / constexpr 真值）; ⛔ = 空实现 / 常量退化;
>       ⚠ = 形态存在但与已有重复; — = 不适用（调用路径被 active_for_piece=false 裁掉）.

### 1.1 类型族
| 接口 | NoHook | NoSpinHook | TSpinHook | ASpinHook | CautiousHook |
| :--- | :--- | :--- | :--- | :--- | :--- |
| T1 LandPoint | `TetrisNode*` | `tspin::NodeEx` | `tspin::NodeEx` | `aspin::NodeEx` | `tspin::NodeEx` |
| T2 Config | `{}` | `{}` | `tspin::Config` | `aspin::Config` | `{fast_move_down}` |
| T3 Payload | `NoTSpinPayload` | `NoTSpinPayload` | `TSpinPayload` | `ASpinPayload` | `NoTSpinPayload` |
| T4 RotState | Empty | Empty | `{last_rotate_arr, corners3_arr}` | Empty | Empty |
| T5 SearchState | Empty | Empty | `{x_diff, y_diff, block_buffer[52], block*}` | Empty | Empty |
| T6 active_for_piece<T> | false | false | `T=='T'` | true | false |

### 1.2 LandPoint trait
| 接口 | NoHook | NoSpinHook | TSpinHook | ASpinHook | CautiousHook |
| :--- | :--- | :--- | :--- | :--- | :--- |
| L1 is_landpoint_none | (无定义⚠) | ✅ true const | ✅ `lp.type==None` | ✅ `lp.type==None` | ✅ true const |
| L2 get_last_node | (无定义⚠) | ✅ nullptr const | ✅ `lp.last` | ✅ nullptr const | ✅ nullptr const |

### 1.3 Config trait
| 接口 | NoHook | NoSpinHook | TSpinHook | ASpinHook | CautiousHook |
| :--- | :--- | :--- | :--- | :--- | :--- |
| C1 last_rotate | (无定义⚠) | ⛔ false const | ✅ `cfg->last_rotate` | ⛔ false const | ⛔ false const |
| C2 allow_180 | (无定义⚠) | ⛔ true const | ✅ `cfg->allow_180` | ✅ `cfg->allow_180` | ⛔ true const |
| C3 allow_LR | (无定义⚠) | ⛔ true const | ✅ `cfg->allow_LR` | ✅ `cfg->allow_LR` | ⛔ true const |
| C4 allow_d | (无定义⚠) | ⛔ true const | ✅ `cfg->allow_d` | ✅ `cfg->allow_d` | ✅ `!cfg->fast_move_down` |
| C5 allow_D | (无定义⚠) | ⛔ true const | ✅ `cfg->allow_D` | ✅ `cfg->allow_D` | ⛔ true const |
| C6 allow_rotate_move | (无定义⚠) | ⛔ true const | ✅ `cfg->allow_rotate_move` | ✅ `cfg->allow_rotate_move` | ⛔ false const |
| C7 is_20g | (无定义⚠) | ⛔ false const | ✅ `cfg->is_20g` | ✅ `cfg->is_20g` | ⛔ false const |

### 1.4 Payload trait
| 接口 | NoHook | NoSpinHook | TSpinHook | ASpinHook | CautiousHook |
| :--- | :--- | :--- | :--- | :--- | :--- |
| P1 resolves_last_1g | (无定义⚠) | (无定义⚠) | ✅ true | ✅ false | (无定义⚠) |
| P2 payload_has_last_rot | (无定义⚠) | (无定义⚠) | ✅ `p.has_last_rot!=0` | ⛔ false const | (无定义⚠) |
| P3 payload_last_r | (无定义⚠) | (无定义⚠) | ✅ `p.last_r` | ⛔ 0 const | (无定义⚠) |
| P4 payload_last_x | (无定义⚠) | (无定义⚠) | ✅ `(int)p.last_x` | ⛔ 0 const | (无定义⚠) |
| P5 payload_last_y | (无定义⚠) | (无定义⚠) | ✅ `(int)p.last_y` | ⛔ 0 const | (无定义⚠) |

### 1.5 算法回调
| 接口 | NoHook | NoSpinHook | TSpinHook | ASpinHook | CautiousHook |
| :--- | :--- | :--- | :--- | :--- | :--- |
| A1 on_init_rotations | ⛔ {} | ⛔ {} | ✅ build corners3×R | ⛔ {} | ⛔ {} |
| A2 on_rotate_reach | ⛔ {} | ⛔ {} | ✅ `last_rotate_arr[r]\|=reached` | ⛔ {} | ⛔ {} |
| A3 on_emit | ⛔ {} | ⛔ {} | ✅ landings & last & corners3 → spin | ✅ 4 邻阻挡 → aspin bit | ⛔ {} |
| A4 on_search_state_init | ⛔ {} | ⛔ {} | ✅ x_diff/y_diff/block_buffer | ⛔ {} | ⛔ {} |
| A5 check_ready | (无定义⚠) | (无定义⚠) | ✅ count corners | ⛔ false const | (无定义⚠) |
| A6 check_mini_ready | (无定义⚠) | (无定义⚠) | ✅ rotate ready & 不 check map | ⛔ false const | (无定义⚠) |
| A7 apply_emit_1g | (无定义⚠) | (无定义⚠) | ✅ 写 spin/last/is_check | ✅ 写 type | (无定义⚠) |
| A8 apply_emit_20g | (无定义⚠) | (无定义⚠) | ✅ 写 last/spin/is_ready/is_mini_ready | ✅ 4 邻阻挡 → type | (无定义⚠) |

---

## 2. 接口必要性逐一评估

### 2.1 类型族 (T1-T6) — 全部必要, 无冗余

| 接口 | 评估 | 风险 |
| :--- | :--- | :--- |
| T1 LandPoint | ✅ 必要. 三种实体 (`TetrisNode*` / tspin::NodeEx / aspin::NodeEx) 由下游 AI eval 直接消费, 不能合并. | — |
| T2 Config | ✅ 必要. 4 种 (空/tspin/aspin/cautious) 已是最小集合. | — |
| T3 Payload | ⚠ **存在冗余**. NoTSpinPayload 在 NoHook/NoSpinHook/CautiousHook 共用, 但本质是空 struct, [[no_unique_address]] 后 0 字节. 无运行时冗余, 仅源码上可统一为 `EmptyPayload`. | 极低 |
| T4 RotState | ✅ 必要. TSpinHook 独占非空形态 (last_rotate_arr + corners3_arr), 其余 4 个 hook 退化为 EmptyRotState. | — |
| T5 SearchState | ✅ 必要. TSpinHook 独占 (x_diff/y_diff/block_buffer[52], 共 ~220 字节), 其余 4 个 0 字节. | — |
| T6 active_for_piece | ✅ 必要. 编译期裁剪整条 spin 算法链, 是性能关键开关. | — |

**结论**: 类型族无多余需求. T3 NoTSpinPayload 命名冗余, 可改为 `EmptyPayload` 全局共享.

### 2.2 LandPoint trait (L1, L2) — 1 处真冗余, 1 处接口缺失

#### L1 `is_landpoint_none(lp)`
- **NoHook 缺定义** ⚠. 没有任何 strategy 在 NoHook 路径下调 `is_landpoint_none(LandPoint=TetrisNode*)`, 所以编译能过. **但接口契约不完整, CRTP 应统一补一份**.
- NoSpinHook / CautiousHook: ⛔ 永远 `return true` — 行为合理 (它们的 LandPoint.type 永远是 None).
- TSpinHook / ASpinHook: ✅ 真正读 `lp.type`.

**评估**: 接口本身合理. NoHook 缺定义是历史遗漏, R.1 CRTP 顺手补齐.

#### L2 `get_last_node(lp)`
- 同 L1: NoHook 缺定义; NoSpinHook/CautiousHook ⛔ `return nullptr`; ASpinHook ⛔ `return nullptr`.
- 唯一真正用的是 **TSpinHook**, 读 `lp.last`.

**评估**: 接口必要 (TSpinHook 不可缺), 但 4/5 hook 是 nullptr 常量. R.1 CRTP 默认 nullptr 即可.

### 2.3 Config trait (C1-C7) — **结构性过度**

#### 最关键的发现: C1-C7 的 7 个 Config getter, 在每个 hook 里都重写了一遍.
- **NoHook 全部缺定义** ⚠. 因为 NoHook 用于 tag (TagStrategy 内部不读 SpinHook::config_*, 它从自己持有的 `Config*` 直接读字段). `search_tag.h` L18 有注释明确 "不读 SpinHook::config_is_20g". → **NoHook 缺这 7 个接口完全不影响 tag, 因为 tag 根本不用 Hook::config_***.
- **NoSpinHook 全是常量** ⛔. 7 个全部 hardcode `true/false`. 它服务于 path/simulate/simple 在不带 spin 的场景, 行为期望就是"全开关".
- **CautiousHook 6 常量 + 1 字段读** (允许 d 由 fast_move_down 决定).
- **TSpinHook 全部读 `cfg->xxx`**, ASpinHook 6 个读字段 + 1 个常量 false (last_rotate).

**问题 1: Config trait 是否必要?**

Config trait 的存在前提是: `path::SearchWith` 的 4 strategy 调用面在编译期才知道用哪个 hook,
所以无法直接 `cfg->allow_180` (因为 cfg 类型 = `SpinHook::Config`). 必须经过 Hook 这层间接.

→ **Config trait 必要**. 不能删.

**问题 2: 7 个 getter 是否都必要?**

| getter | 调用点 | 评估 |
| :--- | :--- | :--- |
| C1 last_rotate | path/simulate 1g pop, make_path 命中谓词 | ✅ TSpinHook 真用 |
| C2 allow_180 | path/simulate 1g/20g neighbors, run_piece_20g neighbors | ✅ TSpin/ASpin 真用 |
| C3 allow_LR | path 1g/20g make_path neighbors | ✅ TSpin/ASpin 真用 |
| C4 allow_d | path 1g make_path neighbors | ✅ Cautious 用 fast_move_down 改写, TSpin/ASpin 真用 |
| C5 allow_D | path 1g make_path neighbors | ✅ TSpin/ASpin 真用 |
| C6 allow_rotate_move | path 1g make_path neighbors | ✅ TSpin/ASpin 真用 |
| C7 is_20g | strategy 入口 (1g vs 20g 分支) | ✅ TSpin/ASpin 真用 |

→ **7 个 getter 全部必要, 无多余需求**. 但实现重复(R.2 policy 化).

**问题 3: NoHook 缺 C1-C7 是否合理?**

是. TagStrategy + NoHook 路径里 `search_tag.h` 不调 `SpinHook::config_*`, 直接走 `ctx.config_->xxx`.
**但这是个一致性缺口**: 一旦未来有人想把 TagStrategy 与 NoSpinHook/CautiousHook 拼合,
NoHook 没有 Config trait 就会编译错. R.1 CRTP 应该把 7 个 getter 默认填成 `false` (最保守).

### 2.4 Payload trait (P1-P5) — **冗余信号最强**

P1-P5 是 path/simulate 1g pop 用来"从 payload 折回 last_node 的 master 指针"的接口.

仅 `TSpinHook` 真正消费 (`resolves_last_1g=true`, `payload_last_x/y/r/has_last_rot` 都读 payload 字段).
- ASpinHook: ⛔ 全部常量 0 / false (`resolves_last_1g=false` → P2-P5 永远不会被调用).
- NoHook / NoSpinHook / CautiousHook: 全缺定义.

**问题 4: 是否有更简洁的接口形态?**

观察 strategy 1g pop 实际用法 (search_path.h:236-258):
```cpp
if constexpr (SpinHook::resolves_last_1g) {
    if (SpinHook::payload_has_last_rot(lp.extra)) {
        std::uint8_t lr = SpinHook::payload_last_r(lp.extra);
        int lx = SpinHook::payload_last_x(lp.extra);
        int ly = SpinHook::payload_last_y(lp.extra);
        bb::BBState pst{T, lr, lx, ly};
        last_node = ctx.state_to_node(pst);
    }
    SpinHook::apply_emit_1g(...);
} else {
    SpinHook::apply_emit_1g(...);
}
```

实际上 payload → last_node 折回这一段, 完全可以**封装到 hook 自己的 `apply_emit_1g` 里**.
用 P1 + P2-P5 把 payload 拆开后再传给 hook, 等于 hook 把字段暴露给 strategy 又拿回去.

**重构方案 (R.7 新增)**: 把 `apply_emit_1g` 接口签名改成
```cpp
template<class MapT, std::size_t R_count, class StateToNode>
static void apply_emit_1g(LandPoint &lp, Payload const &payload,
                          std::size_t depth, Config const *cfg,
                          StateToNode &&state_to_node,  // closure 包 ctx.state_to_node
                          std::array<MapT, R_count> const &usable, ...);
```
TSpinHook 内部根据 payload 字段调用 `state_to_node(pst)` 折回 last; 其余 hook 直接忽略
StateToNode 闭包. → **P1-P5 5 个接口可以全部删除**, 接口面从 28 → 23.

收益: 接口面收窄, 把"只 TSpin 关心的字段"从公共契约挪回 TSpin 自家. 不损失功能.

### 2.5 算法回调 (A1-A8) — 接口臃肿是主要问题

#### A1-A4: on_init_rotations / on_rotate_reach / on_emit / on_search_state_init
- TSpinHook 与 ASpinHook (后者只用 A3) 真的需要; 其余 hook 全空.
- A1-A4 由 MoveGen 框架在 `if constexpr (kCheckTSpin)` 守护下调用, NoHook 路径下完全裁掉.

**评估**: 接口必要, 实现冗余. R.1 CRTP 默认空体即可.

#### A5 check_ready / A6 check_mini_ready
- A5 仅 `tag::search_t_native` 调用 (search_tag.h:877), 真消费的只有 TSpinHook.
- A6 在 TSpinHook::apply_emit_20g 内部自调, 不被外部 strategy 直接调.

**评估**: A6 可降为 TSpinHook 私有静态方法, 不必出现在公共 Hook 接口面. 减少 1 个接口.

#### A7 apply_emit_1g — **接口签名臃肿**

签名: `(lp, payload, depth, cfg, last_node, usable, r, xb, yb)` = **9 个参数**.

实际使用 (TSpinHook):
- 用: `last_node, payload.has_last_rot, depth, cfg->last_rotate, payload.spin`
- 不用: `usable, r, xb, yb`

实际使用 (ASpinHook):
- 用: `payload.aspin`
- 不用: `last_node, depth, cfg, usable, r, xb, yb` (7/9 个参数)

→ **9 个参数中只有 5 个会被任意 hook 真正读**.

**重构方案 (R.8 新增)**: apply_emit_1g 收窄签名:
```cpp
static void apply_emit_1g(LandPoint &lp, Payload const &payload,
                          std::size_t depth, Config const *cfg,
                          TetrisNode const *last_node) noexcept;
```
删除 `usable, r, xb, yb` 4 参 (无消费方). 收窄 5 参签名.

#### A8 apply_emit_20g — **接口签名更臃肿**

签名: `(lp, last_node, action, depth, cfg, map, state, sunk_node, usable, r, xb, yb)` = **12 个参数**.

实际使用 (TSpinHook):
- 用: `last_node, action, depth, cfg, map, state, sunk_node`
- 不用: `usable, r, xb, yb`

实际使用 (ASpinHook):
- 用: `usable, r, xb, yb` (它就靠这 4 个判 4 邻阻挡)
- 不用: `last_node, action, depth, cfg, map, state, sunk_node` (7 个)

→ **TSpin 与 ASpin 的 12 参列表对应两组截然不同的子集**, 两家 hook 无法共享更窄的签名.

**重构方案 (R.9 新增)**: 拆成两个接口:
```cpp
// 仅 last-rotate / spin-ready 系 hook (TSpinHook) 实现
static void apply_emit_20g_spin(LandPoint &lp, TetrisNode const *last_node,
                                char action, std::size_t depth, Config const *cfg,
                                TetrisMap const &map, SearchState const &state,
                                TetrisNode const *sunk_node) noexcept;

// 仅 4-邻位板阻挡系 hook (ASpinHook) 实现
template<class MapT, std::size_t R_count>
static void apply_emit_20g_geom(LandPoint &lp, std::array<MapT,R_count> const &usable,
                                std::uint8_t r, int xb, int yb) noexcept;
```
strategy run_piece_20g 的 visitor 同时调两个. Hook 各自只实现自家关心的那一支, 另一支 CRTP 默认空体.

**收益**: 接口语义清晰. 12 参收窄成 8 + 5. 风险中等 (oracle_diff 字节级要严格比对).

---

## 3. 多余需求 / 不合理形态汇总

| ID | 现象 | 修正 | 风险 | 行减少 |
| :--- | :--- | :--- | :--- | :--- |
| **D.1** | NoHook 缺 L1/L2/C1-C7/P1-P5/A5/A6/A7/A8 共 16 处 ⚠. 因为 tag 路径不调, 实际能编译, 但**契约不全** | R.1 CRTP 统一补默认实现 | 低 | +60 (新增统一基) |
| **D.2** | NoSpinHook / CautiousHook 的 C1-C7 全是常量 ⛔ | R.2 StaticConfigPolicy 表驱动 | 低 | -25 |
| **D.3** | P1-P5 5 个接口仅服务 TSpinHook 折回 last_node, 实际可下沉到 hook 内部 | R.7 删 P1-P5, 用 StateToNode 闭包传给 apply_emit_1g | 低 | -30, 接口面收 5 |
| **D.4** | NoTSpinPayload 在 3 hook 重复声明 | 改名为 `EmptyPayload` 全局共享 | 极低 | -3 |
| **D.5** | A6 check_mini_ready 仅 TSpinHook 内部用, 不该出现在公共 Hook 接口面 | 降为 TSpinHook private | 低 | -2 行 + 接口面收 1 |
| **D.6** | A7 apply_emit_1g 9 参中 4 参 (usable/r/xb/yb) 任意 hook 都不用 | R.8 收窄签名 5 参 | 低 | -8 |
| **D.7** | A8 apply_emit_20g 12 参分裂为 TSpin/ASpin 两个不重叠子集 | R.9 拆 apply_emit_20g_spin / apply_emit_20g_geom | 中 | -15, 接口语义清晰 |
| **D.8** | A1-A4 在 4/5 hook 是 noop | R.1 CRTP 默认空体 (与 D.1 同 commit) | 低 | -150 |
| **D.9** | NoHook 与 NoSpinHook 仅 LandPoint 类型 (`TetrisNode*` vs `tspin::NodeEx`) 区别. 其余形态完全一致 | 评估能否合并为单一 `NoHook<class LandPointT>` 模板 | 低 | -80 |
| **D.10** | TagStrategy + NoHook 路径里, `tag` 内部直接读 `ctx.config_->xxx` 不走 Hook::config_*. 与其他 strategy 不一致 | 决定: 要么 tag 也改走 Hook::config_*, 要么承认 tag 自有 Config 不经 Hook (现状) | 中 | 0 (语义决策) |

---

## 4. 推荐修正顺序 (从矩阵审计派生的 6 项, 替代旧 R.1-R.5 路线)

| # | 修正 | 关联 D 项 | 风险 | 备注 |
| :--- | :--- | :--- | :--- | :--- |
| **M.1** | BaseSpinHook CRTP + EmptyPayload 改名 | D.1, D.4, D.8 | 低 | 一次抽掉 ~210 行 noop, 顺手补 NoHook 缺口 |
| **M.2** | StaticConfigPolicy 表驱动 | D.2 | 低 | 与 R.2 同 |
| **M.3** | check_mini_ready 降为 TSpinHook private | D.5 | 极低 | 单 commit, 接口面 -1 |
| **M.4** | A7 apply_emit_1g 收窄签名 (删 usable/r/xb/yb) | D.6 | 低 | 同步改 strategy pop 调用点 |
| **M.5** | P1-P5 删除, last_node 折回内化为 hook closure | D.3 | 低 | 接口面 -5 |
| **M.6** | A8 apply_emit_20g 拆 spin/geom 两接口 | D.7 | 中 | run_piece_20g visitor 改为顺序调两支 |

---

## 5. 决策待用户裁决

请用户对以下 10 项 D 类发现逐一裁决，再按 M.1-M.6 顺序落地:

- [ ] **D.1** NoHook 缺 16 处接口契约 — 同意补齐 / 接受现状(只在 tag 用)
- [ ] **D.2** NoSpinHook/Cautious Config 全常量 → policy 化 — 同意 / 否决
- [ ] **D.3** Payload trait P1-P5 删除, 折回内化 — 同意 / 否决
- [ ] **D.4** NoTSpinPayload → EmptyPayload — 同意 / 否决
- [ ] **D.5** check_mini_ready 降为私有 — 同意 / 否决
- [ ] **D.6** apply_emit_1g 收窄 9→5 参 — 同意 / 否决
- [ ] **D.7** apply_emit_20g 拆 spin/geom — 同意 / 否决
- [ ] **D.8** A1-A4 noop 抽到 BaseSpinHook — 同意 / 否决
- [ ] **D.9** NoHook 与 NoSpinHook 合并 (模板化 LandPoint) — 同意 / 否决 / 暂缓
- [ ] **D.10** Tag 是否改走 SpinHook::config_* — 同意改 / 维持现状

每一项审查通过后, 我才动代码.

---

## 6. 上下文锚点

- 关联文件: `.research/flip-bits-cleanup/hook_dedup_plan.md` (BFS 三件套去重方案, 与本矩阵互补).
- HEAD: `92d0804`. 距离 R.6 LandPoint 位板化与 N.3 (`tag` 去 master 指针) 是另外两条独立改造线, 不在本矩阵范围.
- 本矩阵是 **接口层** 审计; `hook_dedup_plan.md` 是 **实现层** 审计. 两份合并最终给出完整重构地图.
