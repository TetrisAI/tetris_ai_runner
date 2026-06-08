# Hook 接口矩阵 V2 — C1+C2+C3 后状态

> 基线 commit: `2c1ba33` (C3 完成)
> 旧版: `hook_interface_matrix.md` (C1 之前)

## 1. 五个 Hook 总览

| Hook | 派生 | LandPoint | Payload | RotState | active_for_piece | 消费者 (strategy) |
|------|------|-----------|---------|----------|------------------|-------------------|
| `NoHook` | `BaseSpinHook<NoHook>` | `TetrisNode const *` | `EmptyPayload` | `EmptyRotState` | 全 false | oracle_diff / extreme_rule_diff / perft_movegen 测试桩 |
| `NoSpinHook` | `BaseSpinHook<NoSpinHook>` | `PlainLandPoint` ✨C3 | `EmptyPayload` | `EmptyRotState` | 全 false | path / simple / simulate (qq AI 用) |
| `CautiousHook` | `BaseSpinHook<CautiousHook>` | `PlainLandPoint` ✨C3 | `EmptyPayload` | `EmptyRotState` | 全 false | search_cautious (c2_ai 用) |
| `TSpinHook` | **不派生** | `search_tspin::TetrisNodeWithTSpinType` | `SpinTypePayload` | `{last_rotate_arr, corners3_arr, blocked_arr}` | `T == 'T'` | path / tag (TOJ AI 用) |
| `ASpinHook` | **不派生** | `search_aspin::TetrisNodeWithASpinType` | `SpinTypePayload` | `EmptyRotState` | 全 true | path (ASpin 测试) |

**Payload 收敛**: 4 → 2 ✅
**LandPoint 收敛**: NoSpin/Cautious 不再借用 tspin::NodeEx ✅

---

## 2. 接口矩阵 (C1 删除 4 个接口后)

> ✓ = 实现, ◯ = 派生 BaseSpinHook 默认值, × = 已删除, — = 不适用

### 2.1 编译期 trait

| 接口 | NoHook | NoSpinHook | CautiousHook | TSpinHook | ASpinHook |
|------|--------|------------|--------------|-----------|-----------|
| `LandPoint` | ✓ TetrisNode* | ✓ PlainLandPoint | ✓ PlainLandPoint | ✓ NodeEx<TSpin> | ✓ NodeEx<ASpin> |
| `Payload` | ✓ Empty | ✓ Empty | ✓ Empty | ✓ SpinType | ✓ SpinType |
| `Config` | ✓ {} | ✓ {} | ✓ {fast_move_down} | ✓ tspin::Config | ✓ aspin::Config |
| `RotState<MapT, R>` | ✓ Empty | ✓ Empty | ✓ Empty | ✓ {3 位板} | ✓ Empty |
| `active_for_piece<T>` | ✓ false | ✓ false | ✓ false | ✓ (T=='T') | ✓ true |
| `resolves_last_1g` | ◯ false | ◯ false | ◯ false | ✓ true | ✓ false |

### 2.2 LandPoint trait

| 接口 | NoHook | NoSpinHook | CautiousHook | TSpinHook | ASpinHook |
|------|--------|------------|--------------|-----------|-----------|
| `lp_requires_last_rotate(cfg, lp)` | ◯ false | ◯ false | ◯ false | ✓ cfg→last_rotate && lp.type≠None | ✓ false |
| `get_last_node(lp)` | ◯ nullptr | ◯ nullptr | ◯ nullptr | ✓ lp.last | ✓ nullptr |

`is_landpoint_none` ✨C2 反转重命名, ASpin 短路修复.

### 2.3 Config trait (7 项)

| 接口 | NoHook | NoSpinHook | CautiousHook | TSpinHook | ASpinHook |
|------|--------|------------|--------------|-----------|-----------|
| `config_last_rotate` | ◯ false | ◯ false | ◯ false | ✓ cfg→last_rotate | ✓ false |
| `config_allow_180` | ◯ false | ✓ true | ✓ true | ✓ cfg→allow_180 | ✓ cfg→allow_180 |
| `config_allow_LR` | ◯ false | ✓ true | ✓ true | ✓ cfg→allow_LR | ✓ cfg→allow_LR |
| `config_allow_d` | ◯ false | ✓ true | ✓ !cfg→fast_move_down | ✓ cfg→allow_d | ✓ cfg→allow_d |
| `config_allow_D` | ◯ false | ✓ true | ✓ true | ✓ cfg→allow_D | ✓ cfg→allow_D |
| `config_allow_rotate_move` | ◯ false | ✓ true | ◯ false | ✓ cfg→allow_rotate_move | ✓ cfg→allow_rotate_move |
| `config_is_20g` | ◯ false | ◯ false | ◯ false | ✓ cfg→is_20g | ✓ cfg→is_20g |

非 spin hook 历史"全开关"行为通过 NoSpin (5 项) / Cautious (3 项) 的显式覆盖保留.
`config_allow_d` 在 Cautious 下 inline 了 `fast_move_down`.

### 2.4 BFS 钩子

| 接口 | NoHook | NoSpinHook | CautiousHook | TSpinHook | ASpinHook |
|------|--------|------------|--------------|-----------|-----------|
| `on_init_rotations` | ◯ noop | ◯ noop | ◯ noop | ✓ build_corners3 | ✓ noop |
| `on_rotate_reach` | ◯ noop | ◯ noop | ◯ noop | ✓ last_rotate_arr |= | ✓ noop |
| `compute_mini_blocked_arr` | ◯ noop | ◯ noop | ◯ noop | ✓ build_mini_blocked | ✓ noop |
| `on_emit` (1g) | ◯ noop | ◯ noop | ◯ noop | ✓ corners3∩last_rotate→3 类 | ✓ 4 邻阻挡→2 类 |
| `apply_emit_1g` | ◯ noop | ◯ noop | ◯ noop | ✓ 写 LP.is_check/.last/.is_ready | ✓ 写 LP.type=ASpin |
| `apply_emit_20g` | ◯ noop | ◯ noop | ◯ noop | ✓ 读位板预算 | ✓ 现算 4 邻阻挡 |

### 2.5 Payload trait (resolves_last_1g 路径专用)

| 接口 | NoHook | NoSpinHook | CautiousHook | TSpinHook | ASpinHook |
|------|--------|------------|--------------|-----------|-----------|
| `payload_has_last_rot(p)` | — | — | — | ✓ p.has_last_rot≠0 | ✓ false |
| `payload_last_r(p)` | — | — | — | ✓ p.last_r | ✓ 0 |
| `payload_last_x(p)` | — | — | — | ✓ p.last_x | ✓ 0 |
| `payload_last_y(p)` | — | — | — | ✓ p.last_y | ✓ 0 |

仅 TSpin 走 `resolves_last_1g=true` 路径, 其它 hook 在 strategy `if constexpr` 守门下不调.

### 2.6 已删除接口 (✨C1)

| 接口 | 状态 |
|------|------|
| `SearchState` (类型族) | × 删除 (master-graph block_buffer[52] / x_diff / y_diff / block_*) |
| `on_search_state_init` | × 删除 (3 个 strategy 的 init() 调用同步删) |
| `check_ready` | × 删除 (TSpinHook 内部 master-graph 算法) |
| `check_mini_ready` | × 删除 (同上) |
| `is_landpoint_none` | × 删除 (✨C2 改名 `lp_requires_last_rotate` 反转语义) |

---

## 3. 行数对比

| 文件 | C1 之前 | 当前 | Δ |
|------|---------|------|---|
| `src/movegen_hook.h` | ~960 | 1131 | +171 (含 BaseSpinHook + PlainLandPoint + 注释) |
| `src/search_path.h` | x | x-3 | -3 (反转命名 visitor 单字段) |
| `src/search_simulate.h` | x | x-3 | -3 |
| `src/search_tag.h` | x | x-12 | -12 (删 hook_state_ + on_search_state_init) |
| `src/search_simple.h` | x | x-9 | -9 |

净行变化: ~+135 行 (主要是 BaseSpinHook 框架 + 详细注释), 但**接口面收窄 5 个**, 同形 noop 语义集中.

---

## 4. 仍可改进的点 (FYI, 非任务)

### 4.1 `lp_requires_last_rotate` 内联 cfg

**现状**: TSpinHook 实现中 inline 了 `cfg && cfg->last_rotate`, 让 TagStrategy 不消费 cfg 时得用 `get_last_node(lp) != nullptr` 作 proxy (search_tag.h L1103).

**潜在改造**: 拆成两个独立 trait:
- `lp_has_spin_landpoint(lp)` — 仅看 lp.type
- `cfg_demands_last_rotate(cfg)` — 仅看 cfg

调用方按需组合. Tag 只查前者, Path/Simulate 查 `&&`. 收益: 语义正交, 但 strategy 端多写 1 次 `&&`. 边际收益, 暂不改.

### 4.2 NoHook 的 `config_allow_*` 全 false

**现状**: NoHook 不像 NoSpinHook 那样显式覆盖 5 项 `config_allow_*`. 因为 NoHook 唯一消费者是测试桩 (oracle_diff / perft_movegen), 它们走非 path 路径或自己控制邻居字符集.

**风险**: 如果未来有人把 NoHook 装到 PathStrategy, 邻居枚举会瘫痪 (allow_180/LR/d/D 全 false). 

**建议**: 加 `static_assert` 在 PathStrategy 装配处检查 `config_allow_180/LR/d/D` 至少一个非 false, 或者文档化 NoHook 不可装 PathStrategy. 不在本轮范围.

### 4.3 ASpinHook 的 `payload_last_*` 假实现

**现状**: ASpinHook 的 `payload_has_last_rot` 等返回 0/false, 因为 `resolves_last_1g=false` 守门下 strategy 不调. 这些"假实现"占 5 行 + 注释 5 行.

**潜在改造**: 把 `payload_*` 套件挪进 BaseSpinHook 默认值 (默认 false/0). ASpinHook 派生 BaseSpinHook 后这些就免写了 — 但 ASpinHook 当前**不派生** (理由: 它写自家 on_emit / apply_emit_1g / apply_emit_20g 全套).

**两条路**:
- A. 让 ASpinHook 派生 BaseSpinHook, 同时覆盖 6 个算法钩子 — payload_* 套件免写
- B. 把 payload_* 套件抽成独立 mixin (`PayloadNoLastTrait`), ASpinHook 多重继承
- C. 维持现状, 5 行注释自然代价

C 最简洁, 当前选择.

### 4.4 BaseSpinHook 默认 BFS 钩子 vs active_for_piece 守门

**现状**: BaseSpinHook 的 `on_init_rotations / on_rotate_reach / compute_mini_blocked_arr / on_emit / apply_emit_*` 都是 noop. NoHook/NoSpin/Cautious 派生后理论上也走默认 noop.

**实际**: strategy 通过 `if constexpr (SpinHook::active_for_piece<T>)` 守门, 在 active=false 时根本不调这些钩子. 默认 noop 只是"契约自洽" (派生侧任意场景调都不挂掉).

**结论**: 设计冗余但安全. 维持.

---

## 5. 状态总结

✅ **C1 完成**: SearchState / 4 接口删除, 所有 strategy 走纯位板
✅ **C2 完成**: Payload 4→2, BaseSpinHook CRTP, ASpin 短路修复
✅ **C3 完成**: PlainLandPoint, NoSpin/Cautious 不借 tspin::NodeEx

oracle_diff / extreme_rule_diff / aspin_dump / path_node_diff / simulate_node_diff: byte-equal
tag_node_diff: 17 failing (与 cd10d5b 一致, 非本次回归 — 已在 c1/c2/c3 followup 中标注)

接口面健康, 各 Hook 形态收敛到位. 后续需要扩展 (例如新 spin 类型) 直接派生 BaseSpinHook + 覆盖个别接口即可.
