# N.1-N.5 工作量重组 — 3 个 commit 切分

> 上一轮: tag 位板化已落 `cd10d5b`. 现在 N.1-N.5 5 项待做合并到 3 个 commit.

## 关键观察

- **N.3 (block_buffer 命名常量) 被 N.5 吸收**: N.5 直接删 SearchState 整个 struct, block_buffer 也一起没了. 命名常量没有意义, **N.3 取消**.
- **N.5 应当先于 N.4**: 先把 `check_ready / check_mini_ready / on_search_state_init` 物理删除, BaseSpinHook 默认值就不用考虑这 3 个接口了.
- **N.2 必须独立**: 它要动 `src/ai.cpp` 的 `QQTetrisSearch::path_/simulate_/simple_` 与 `c2_ai` 的 LandPoint 类型, 是用户编码纪律里"不动 ai.cpp 实例化"的边界跨越项, 必须单独裁决.

## 3 个 commit 划分

### C1 — TSpinHook 瘦身 (吸收 N.5 + N.3)

**目标**: 删掉 search_t_native 已经不消费的死代码.

**改动**:
- `src/movegen_hook.h`:
  - 删 `TSpinHook::SearchState` 整个 struct (含 `block_buffer[52]`, `x_diff`, `y_diff`, `block*`)
  - 删 `TSpinHook::on_search_state_init`
  - 删 `TSpinHook::check_ready`, `check_mini_ready`
  - `TSpinHook::apply_emit_20g` 内调 `check_ready/check_mini_ready` 的两行重写为位板 corners3 + rotate-block 判定 (与 search_t_native 已经走的方式一致), 或者直接读 `RotState::corners3_arr` (apply_emit_20g 当前签名能否拿到 rot_state 需要确认; 若拿不到, 改签名让 strategy 把 corners3 mask 传进来)
  - `using SearchState` 类型族保留 (保 BaseSpinHook 兼容), 但退化为 `EmptyState` 空 struct
- `src/search_path.h` / `search_simulate.h` / `search_tag.h`:
  - 删除所有对 `SpinHook::on_search_state_init` 的调用 (3 处)
  - 删除 `ctx.hook_state_` 字段 (在 ExtrasMixin 内)
  - apply_emit_20g 调用面如签名变化同步改

**风险**: 低. search_t_native 已不消费这一组接口, 其它 strategy 也只是空调用.

**验收**:
- `oracle_diff` byte-equal
- `extreme_rule_diff` byte-equal
- 行减少 ~120

**估算**: 单 commit, sub-agent 1 个 context 完成.

---

### C2 — Hook 形态收敛 + ASpin 短路修正 (合并 N.1 + N.4)

**目标**: 一次性收敛 Payload 类型, 引入 BaseSpinHook CRTP, 修正 ASpin 自落点短路 bug.

**改动**:
- `src/movegen_hook.h`:
  - **N.1 Payload**: 删 `NoTSpinPayload` / `TSpinPayload` / `ASpinPayload`. 新增:
    ```cpp
    struct EmptyPayload {};
    struct SpinTypePayload {
        std::uint8_t type;        // 0=None / 1=TSpinMini / 2=TSpinFull / 3=ASpin
        std::int8_t  last_x, last_y;
        std::uint8_t last_r;
        std::uint8_t has_last_rot;
    };
    ```
    NoHook/NoSpinHook/CautiousHook 用 `EmptyPayload`; TSpinHook/ASpinHook 用 `SpinTypePayload`.
  - **N.4 BaseSpinHook**: 引入 CRTP 基类:
    ```cpp
    template<class Derived>
    struct BaseSpinHook { /* 默认实现 lp_requires_last_rotate / get_last_node /
                            7 个 config_* / on_init_rotations / on_rotate_reach /
                            on_emit / resolves_last_1g=false / payload_* */ };
    ```
    NoHook/NoSpinHook/CautiousHook 改 `: BaseSpinHook<XxxHook>` 派生, 删自家 noop 副本.
    TSpinHook/ASpinHook 不派生 (它们写自家算法).
  - **N.4 反转命名**: `is_landpoint_none(lp)` → `lp_requires_last_rotate(lp)`. 5 个 hook 同步:
    - NoHook/NoSpinHook/Cautious/ASpin: BaseSpinHook 默认 `return false`
    - TSpinHook: `return lp.type != None && cfg && cfg->last_rotate`
- `src/search_path.h` / `search_simulate.h` / `search_tag.h`:
  - 短路守卫: `if (SpinHook::is_landpoint_none(lp) && cells_key(node)==cells_key(lp.node))` → `if (!SpinHook::lp_requires_last_rotate(cfg, lp) && cells_key(node)==cells_key(lp.node))`
  - visitor 命中谓词: `!landpoint_is_none && last_rotate && k==index_landpoint` → `requires_last_rotate && k==index_landpoint` (合并 2 条件为 1)
  - L850 index 选择: `is_landpoint_none(lp) || land_last==nullptr` 维持等价含义 (改用 `!requires_last_rotate(cfg,lp) || land_last==nullptr`)

**收益**:
- Payload 4 → 2
- BaseSpinHook 抽掉 ~250 行 noop
- ASpin 路径"起点==终点"享受自落点短路 (修 bug)

**风险**: 中. 改动延伸到 4 个 strategy 的 make_path 入口 + visitor. oracle_diff 必须 byte-equal.

**验收**:
- `oracle_diff` byte-equal
- `extreme_rule_diff` byte-equal
- ASpin 边界 case 的 path 字符串与 oracle 一致 (oracle 在该边界也返回空 path)

**估算**: 单 commit, sub-agent 1 个 context 完成. 中等复杂度.

---

### C3 — LandPoint 形态收敛 (N.2)

**目标**: NoSpinHook 与 CautiousHook 不再借用 `tspin::NodeEx`, 改用最薄壳 `PlainLandPoint`.

**改动**:
- `src/movegen_hook.h`:
  ```cpp
  struct PlainLandPoint {
      TetrisNode const *node = nullptr;
      PlainLandPoint() = default;
      PlainLandPoint(TetrisNode const *n) : node(n) {}
      bool operator==(PlainLandPoint const &o) const { return node == o.node; }
  };
  ```
  - `NoSpinHook::LandPoint = PlainLandPoint`
  - `CautiousHook::LandPoint = PlainLandPoint`
- `src/ai.cpp`:
  - `QQTetrisSearch` 内 path_/simulate_/simple_ 三个 `Searcher` 实例化沿用 NoSpinHook → 它们的 `LandPoint` 自动变 `PlainLandPoint`.
  - `QQTetrisSearch::search` 返回类型 (vector<LandPoint>) 跟着变. ai_zzz::qq::Attack::eval 接收的是 `TetrisNode const*`, 需要在调用时从 `lp.node` 取出 — 当前已经是从 `lp.node` 取 (因 NoSpinHook 的 NodeEx 也是 `.node` 字段), 所以**调用面无需改动**.
  - c2_ai 同理.

**风险**: 中. 跨越用户编码纪律里"不动 ai.cpp 实例化"的边界. 但实例化形态不变, 只是模板内部 LandPoint 类型改名 — 应该编译过, ai_zzz::qq / C2 的 eval 与 get 函数签名不变.

**验收**:
- `oracle_diff` / `extreme_rule_diff` byte-equal (PlainLandPoint operator== 行为与 NodeEx 的 .node== 一致)
- ai.cpp 编译过
- qq_ai 三档 (Path/Simulate/Simple) 与 c2_ai 行为不变

**估算**: 单 commit, sub-agent 1 个 context 完成. 中等复杂度.

---

## 推荐落地顺序

```
C1 (TSpinHook 瘦身)  →  C2 (Hook 形态 + ASpin 短路)  →  C3 (PlainLandPoint)
```

理由:
1. **C1 先** — 最低风险, 把 search_t_native 已经"水到渠成"的死代码先清掉, 后续 BaseSpinHook 默认值表面收窄
2. **C2 次** — 中等风险, 但纯粹动 hook 形态 + strategy 调用面, 不动 ai.cpp; 顺手修 ASpin 自落点短路 bug
3. **C3 末** — 最高风险, 跨入 ai.cpp 实例化边界, 单独 commit 便于独立审阅与回滚

每个 commit 独立验收 oracle_diff + extreme_rule_diff. 都不 push, 由用户审阅决定.

## 总收益估计

- 行减少: ~120 (C1) + ~250 (C2) + ~50 (C3) ≈ 420 行
- 接口面: SearchState/check_ready/check_mini_ready/on_search_state_init 删除 (-4); is_landpoint_none → lp_requires_last_rotate (-1, 重命名后 strategy 端少调一次 config_last_rotate); Payload 类型 4 → 2; LandPoint 类型 4 → 3 (TetrisNode\* / PlainLandPoint / tspin::NodeEx / aspin::NodeEx 中 NoHook 与 NoSpin/Cautious 共用 PlainLandPoint 后还剩 4)

## 边界情况

- **N.4 中 `lp_requires_last_rotate` 是否需要 `cfg` 参数**: TSpinHook 实现需要查 `cfg->last_rotate`, 所以接口签名应是 `(Config const *cfg, LandPoint const &lp)`. BaseSpinHook 默认值忽略两个参数返回 false. strategy 调用面之前是 `is_landpoint_none(lp)` 单参, 改后多带 cfg.
- **C2 是否影响 N.5 后的 apply_emit_20g 签名**: C1 删掉 `check_ready/check_mini_ready` 后, apply_emit_20g 内部需要重新计算 ready (改用 corners3_arr + landings 位运算). C1 commit 内必须把这一段位板化, 否则编不过.

## 待您裁决

- [ ] 同意 3 commit 切分 (C1 → C2 → C3) — 同意 / 修改 / 否决
- [ ] 是否立刻开始 C1 — 同意 / 暂停 / 修改方案
