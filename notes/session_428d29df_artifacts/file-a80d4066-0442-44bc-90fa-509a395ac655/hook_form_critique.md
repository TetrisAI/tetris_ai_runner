# Hook 形态质询答辩 (落盘)

> 状态: 用户对 5 项形态提出质询. 本文逐题给出现状原因 + 是否合理 + 修正建议.
> HEAD = `92d0804`. 关联: `hook_interface_matrix.md` / `hook_dedup_plan.md`.

---

## Q1. `NoTSpinPayload` 名字不够中立

### 现状
```cpp
struct NoTSpinPayload {};  // movegen_hook.h:54
```
被 NoHook / NoSpinHook / CautiousHook 三家共用 `using Payload = NoTSpinPayload;`.

### 质询合理
名字带 `TSpin` 前缀, 但实际语义是 "**该 hook 不附加 emit 元数据**", 与 TSpin 无任何关联.
ASpinHook 也是"无 last-rotate 谱系", 但它没用 NoTSpinPayload, 因为名字暗示着"不是
TSpin 的 payload" → 接口语义被名字误导.

### 建议
改名为 `EmptyPayload` (或 `NullPayload`). 全局共享同一类型, 与"hook 不携带额外元数据"
语义对齐. **同时审视**: ASpinHook 自家的 `ASpinPayload {std::uint8_t aspin}` 是不是
其实也可以归并 — 它只有 1 个 bit, 完全可以用 `LandPoint.type == ASpin` 表达 (TSpinHook
的 spin 也用 LandPoint.type 表达, ASpinHook 多搞一个 payload 字段是多余).

| 方案 | 落地 | 行减少 | 风险 |
| :--- | :--- | :--- | :--- |
| **Q1.a** 仅改名 NoTSpinPayload → EmptyPayload | 全局 sed | -3 | 极低 |
| **Q1.b** 同时去掉 ASpinPayload, ASpin emit 直接写 lp.type | on_emit 内不写 payload, 写 lp 的字段经 apply_emit_1g | -10 + 接口面收窄 | 低 |

**推荐: Q1.a + Q1.b 合并为一个 commit**.

---

## Q2. `NoSpinHook` 为什么需要 `tspin::NodeEx`?

### 现状
```cpp
struct NoSpinHook
{
    using LandPoint = ::search_tspin::Search::TetrisNodeWithTSpinType;  // 携带 type/last/flags
};
```
被 path/simulate/simple 在不带 spin 场景使用 (qq_ai 三档: Path / Simulate / Simple).

### 质询合理
现有注释 (movegen_hook.h:113-121) 解释为:
> MoveGenSearch::make_path 内部会访问 land_point.node, 所以 LandPoint 必须是
> 带 .node 成员的 NodeEx 结构.

**这个解释只解释了"需要带 .node 字段", 不解释为什么必须用 tspin::NodeEx**.

### 真实原因 (现状的历史成因)

1. 早期 path/simulate 走 `search_path_node.cpp` 时, LandPoint 直接就是 `tspin::NodeEx`,
   因为这两个 strategy 共享一份"统一 NodeEx 字段"以减少 ai 端区分;
2. 当前 `ai_zzz::qq::Attack::eval(TetrisNode const *node, ...)` (ai_zzz.cpp:110) **直接
   接收 TetrisNode\***, 不接收 NodeEx. 也就是说 **NoSpinHook 路径下, 下游 AI 根本
   不读 type/last/flags 这些 TSpin 字段**, 仅用到 `.node`;
3. tspin::NodeEx 包含的 type / last / flags / TSpinType 枚举 → 在 NoSpinHook 路径下
   全部冗余. NoSpinHook 仅需 LandPoint 是个能拿到 `TetrisNode const*` 的薄壳.

### 评估: 不合理

NoSpinHook 用 tspin::NodeEx 是**类型借壳**, 把 TSpin 域字段放到不需要 spin 的路径里:
- 字段 type / last / flags 全部为 0/None;
- 但 ctx.land_point_cache_ 的元素大小被撑大;
- ai_zzz::qq 也得 include tspin POJO 才能编 (实际并不消费).

### 建议

引入 `m_tetris::PlainLandPoint`:
```cpp
struct PlainLandPoint
{
    TetrisNode const *node = nullptr;
    PlainLandPoint() = default;
    PlainLandPoint(TetrisNode const *n) : node(n) {}
    bool operator==(PlainLandPoint const &o) const { return node == o.node; }
};
```
让 `NoSpinHook::LandPoint = PlainLandPoint`. 与 NoHook (裸 `TetrisNode*`) 仍然区别 — NoHook
的 LandPoint 直接是指针, NoSpinHook 的 LandPoint 是个含 `.node` 的薄壳, 满足
`SpinHook::get_last_node` / `is_landpoint_none` 等 trait 通过 `.node`/`type` 寻址的诉求.

> **注意**: 一旦改 NoSpinHook::LandPoint, 下游 `qq_ai.search_config()` 调用面(`Search::Path/Simulate/Simple`) 与 `QQTetrisSearch::search` 返回类型也要变. 需要 ai.cpp 配合, 但 eval 函数签名已经是裸 `TetrisNode*`, 影响范围有限.

| 方案 | 落地 | 风险 | 备注 |
| :--- | :--- | :--- | :--- |
| 维持现状 | — | — | 字段冗余, 名字暗示 TSpin |
| **引入 PlainLandPoint** | NoSpinHook 切换, ai.cpp QQTetrisSearch 同步改 | 中 | 字段干净, 命名自洽 |
| 直接退化为 `TetrisNode*` | NoSpinHook 与 NoHook 合并 (D.9) | 中高 | 但 trait `.node` 寻址需要改 |

**推荐: 引入 PlainLandPoint, 与 NoHook 的裸指针并存**. NoHook (Tag) 与 NoSpinHook (Path/Simulate) 各自最薄, 谁也不沾 tspin.

---

## Q3. `CautiousHook` 为什么需要 `tspin::NodeEx`?

### 现状
```cpp
struct CautiousHook
{
    using LandPoint = ::search_tspin::Search::TetrisNodeWithTSpinType;
};
```

### 真实原因
与 Q2 同根源 — CautiousHook 服务 c2_ai (Cultris II), `ai_zzz::C2::eval(TetrisNode const *node, ...)` (ai_zzz.cpp:1923) 同样**直接接收 TetrisNode\***, 不接收 NodeEx. CautiousHook 的所有 SpinHook trait (active_for_piece 全 false / type 永远 None / last 永远 nullptr) 都没用上 NodeEx 的 spin 字段.

### 评估: 不合理

c2_ai 没有 spin 概念, **绝不应该用 tspin::NodeEx**. 现状是因为:
- CautiousHook 是后期补的, 当时 NoSpinHook 已经用 tspin::NodeEx, 直接复用了同一个 LandPoint 形态省事;
- 历史上 oracle 的 `search_cautious::Search::TetrisNodeWithTSpinType` 就叫这个名字 (oracle/search_cautious 那里也是 typedef 自 tspin::NodeEx 的 alias).

### 建议
与 Q2 同步: `CautiousHook::LandPoint = PlainLandPoint`. **Cautious 与 NoSpin 应该共用同一个最薄的 LandPoint, 都不沾 tspin**.

---

## Q4. `TSpinHook::block_buffer[52]` — 52 这个魔数

### 现状
```cpp
struct SearchState
{
    int x_diff = 0;
    int y_diff = 0;
    std::uint32_t block_buffer[52] = {};  // movegen_hook.h:251
    std::uint32_t *block = nullptr;       // 偏移 +10 后的写入起点
};
```
`on_search_state_init` 中 `state.block = state.block_buffer + 10;` (L346).
**oracle 同名魔数**: `oracle/search_tspin.h:44` `uint32_t block_data_buffer_[52];`,
`oracle/search_tspin.cpp:14` `block_data_ = block_data_buffer_ + 10;`. 完全 1:1 复刻.

### 质询合理
- 52 没有源码注释解释;
- 偏移 +10 也没有解释;
- 看起来像 "(width 上限 ~32 + 左右各 10 缓冲) ≈ 52" 的工程直觉.

### 真实约束 (反推 oracle 写入面)
oracle/search_tspin.cpp:31-34 的写入:
```cpp
for (int x = 1; x < width - 1; ++x)
    block_data_[x - x_diff_] = ...;        // x ∈ [1, width-2]
block_data_[0 - x_diff_] = (1 << 1);
block_data_[width - 1 - x_diff_] = ...;
```
读取 (oracle/search_tspin.cpp:1101): `block_data_[node->status.x]`.

写入下标范围 = `[0, width-1] - x_diff` = `[-x_diff, width-1 - x_diff]`.
读取下标范围 = `node->status.x` ∈ `[some_low, some_high]`.

**给出 52 的反推**:
- `node->status.x` 来自 master 的 piece pivot, 其取值范围依赖于 RuleSpec 的最大棋盘宽度;
- 现有 RuleSpec: srs (10w), c2 (10w), tag (10w), botris (10w), qq (16w 旧扩展). 所以
  `width` 上限实际是 ~16 左右; `x_diff` 也 ≤ 5 左右;
- 52 = 32 + 20 这种"宽度 32 上限 + 左 10 + 右 10"的保守缓冲, 但**32 这个宽度上限并不存在**, 现有所有 rule_spec 的 width 都 <= 16.

### 评估: 不合理 (魔数, 且过度)

52 对当前所有 rule_spec 来说浪费很多空间 (实际只用 ~26 字), 但移除它需要:
1. 把 52 与 +10 都改写成 `kMaxW + 2*kMaxXDiff` 的常量;
2. 或者 RuleSpec 暴露 `static constexpr int kMaxBlockBuffer` 让 SearchState 模板化.

### 建议

| 方案 | 修正 | 风险 | 备注 |
| :--- | :--- | :--- | :--- |
| **Q4.a** | 维持 52 + 10 但加注释解释来源 (历史 oracle 1:1) | 极低 | 仅文档化 |
| **Q4.b** | 把 SearchState 改成 `std::uint32_t block_buffer[Helpers::kBlockBufferLen]`, 由 RuleSpec 推导 | 低 | 需要给 Helpers 算 `kMaxW + 2*kSafePad` |
| **Q4.c** | 重写成 `std::array<std::uint32_t, kMaxW + 20>`, 把 +10 偏移改成 `kSafeXDiffMax = 10` 命名常量 | 低 | 仅形态修正 |

**推荐: Q4.c**. 不动语义, 仅把 52 / +10 替换成有名字的常量, 说明这是"left/right padding to allow negative x_diff write"的保守缓冲. 与 oracle 仍 byte-equal.

---

## Q5. `NoSpinHook` 为什么需要 `is_landpoint_none`?

### 现状
```cpp
static bool is_landpoint_none(LandPoint const &) noexcept { return true; }  // 永远 true
```

### 调用面 (path/simulate make_path)
search_path.h:841, 909, 922, 977, 983, 1051; search_simulate.h: 类似.
```cpp
if (SpinHook::is_landpoint_none(land_point) && Helpers::cells_key_for(node) == ...)
    return std::vector<char>();  // 自落点短路
...
bool landpoint_is_none = SpinHook::is_landpoint_none(land_point);
visitor.landpoint_is_none = landpoint_is_none;  // 命中谓词区分 None vs spin 路径
```

### 质询合理
NoSpinHook 总是 `return true`, 等价于:
- 自落点短路恒生效;
- visitor 命中谓词永远进 "None 路径".

→ **这两个 if 在 NoSpinHook 路径下都是死代码** (要么恒进, 要么恒不进). 但 strategy 端
为了与 TSpinHook 共享同一份 make_path 算法骨架, 不能在源码上短路掉.

### 评估: 接口必要 (不能删), 但 `return true` 常量是合理的

is_landpoint_none 在 TSpinHook/ASpinHook 是真实判定, 在 NoSpinHook/Cautious 退化为常量
是 trait 通用做法, 与 `Hook::active_for_piece<T> = false` 让 if constexpr 守护把整段
spin 算法裁掉是同形.

→ **这一项不是冗余, 是 trait 默认值**. 但 R.1 BaseSpinHook CRTP 应该把它默认实现
`return true`, NoSpinHook/Cautious 不必各自再写一次.

### 建议
不删, 但走 BaseSpinHook 默认 `return true`, 删 NoSpinHook/Cautious 自己的副本.
TSpinHook/ASpinHook override 自家版本.

---

## Q6. `CautiousHook` 为什么需要 `is_landpoint_none`?

### 现状
同 Q5, `return true` 常量.

### 评估
同 Q5. CautiousHook 实际不走任何"区分 None 与 spin 路径"的 make_path 分支
(Cautious 没有 spin), 所以 `return true` 让 strategy 进入"None 总分支", 行为正确.

### 建议
同 Q5: 收敛到 BaseSpinHook 默认实现, Cautious 不再自己写一遍.

---

## 7. 综合建议 (4 个新增 commit)

| # | 修正 | 关联 Q | 风险 | 行减少 |
| :--- | :--- | :--- | :--- | :--- |
| **N.1** | NoTSpinPayload → EmptyPayload, ASpinPayload 检视 | Q1 | 极低 | -10 |
| **N.2** | 引入 `m_tetris::PlainLandPoint`, NoSpinHook + CautiousHook 切换 | Q2 / Q3 | 中 | 字段干净, 不沾 tspin |
| **N.3** | block_buffer 魔数 52 / +10 → 命名常量 (Q4.c) | Q4 | 低 | 仅形态 |
| **N.4** | BaseSpinHook CRTP 默认实现 is_landpoint_none / get_last_node / 7 个 config | Q5 / Q6 | 低 | -150 (与 D.1/D.8 同 commit) |

> N.4 与之前 hook_dedup_plan.md 的 R.1 / hook_interface_matrix.md 的 M.1 重叠, 可以合并.

---

## 8. 待用户裁决

- [ ] **Q1** NoTSpinPayload → EmptyPayload 改名 (Q1.a) — 同意 / 否决
- [ ] **Q1.b** ASpinPayload 也合并 (lp.type 表达 aspin bit) — 同意 / 否决 / 暂缓
- [ ] **Q2** NoSpinHook 切换到 PlainLandPoint, 不再用 tspin::NodeEx — 同意 / 否决
- [ ] **Q3** CautiousHook 切换到 PlainLandPoint, 不再用 tspin::NodeEx — 同意 / 否决
- [ ] **Q4** block_buffer[52] + 10 → 命名常量 (Q4.c, 维持字节等价) — 同意 / 否决
- [ ] **Q5/Q6** is_landpoint_none 收敛到 BaseSpinHook 默认 (return true) — 同意 / 否决

每项审查通过后再开始动代码; 不会一次性合并执行.
