# Hook 形态质询答辩 (第三轮)

> 续 round1/round2. HEAD = `92d0804`.

---

## Q10. Payload 合并命名

### 决议: `SpinTypePayload`

```cpp
struct SpinTypePayload {
    std::uint8_t type;           // 0=None / 1=TSpinMini / 2=TSpinFull / 3=ASpin
    std::int8_t  last_x;         // 仅 TSpin 写 (其余路径 0)
    std::int8_t  last_y;
    std::uint8_t last_r;
    std::uint8_t has_last_rot;
};
```
取代 `TSpinPayload` / `ASpinPayload` / `NoTSpinPayload`. 与 `EmptyPayload` 共存:
- `EmptyPayload`: NoHook (LandPoint=裸指针 → 无元数据需求)
- `SpinTypePayload`: TSpinHook / ASpinHook / **以及 NoSpinHook + CautiousHook (经过 N.2 切到 PlainLandPoint 后, 还要决策这两路的 Payload)**

> 等等, NoSpinHook 与 CautiousHook 既然引入 PlainLandPoint, 就**也用 EmptyPayload** 即可,
> 不必沾 SpinTypePayload. 整理一下:

| Hook | LandPoint | Payload |
| :--- | :--- | :--- |
| NoHook | `TetrisNode*` (裸) | EmptyPayload |
| NoSpinHook (N.2 后) | PlainLandPoint | EmptyPayload |
| CautiousHook (N.2 后) | PlainLandPoint | EmptyPayload |
| TSpinHook | tspin::NodeEx | SpinTypePayload |
| ASpinHook | aspin::NodeEx | SpinTypePayload |

→ Payload 类型从 4 种 (NoTSpinPayload/TSpinPayload/ASpinPayload + 隐含) 收敛到 2 种.

---

## Q11. "起点==终点自落点短路" 应该所有非 TSpin 路径都享受

### 用户洞察 (核心)

当前 `search_path.h:841`:
```cpp
if (SpinHook::is_landpoint_none(land_point) &&
    Helpers::cells_key_for(node) == Helpers::cells_key_for(land_point.node))
    return std::vector<char>();
```
短路守卫 = `is_landpoint_none(lp) == true`.

**ASpinHook 的 is_landpoint_none 实现**:
```cpp
static bool is_landpoint_none(LandPoint const &lp) noexcept {
    return lp.type == ::search_aspin::Search::None;
}
```
当 ASpin 真正命中 (`lp.type=ASpin`) 时, `is_landpoint_none = false` → **短路被禁掉**, 起点==终点也要走完整 BFS 才返回空 path.

**这是错的**. 用户判断正确:
- TSpin 命中 (lp.type=TSpinFull/Mini): 必须走 BFS, 因为最后一步必须是旋转, 路径里要有 `z`/`c`/`x`. 短路丢失 path 字符串中的旋转动作 → **不能短路**.
- ASpin 命中 (lp.type=ASpin): ASpin 的判定是"落点 4 邻阻挡", 与 path 字符串无关. 起点==终点 (位板等价) 时, 空 path 即可 → **应该短路**.
- NoSpin / Cautious: type 永远 None → **应该短路**.

### 真正的 trait 语义

`is_landpoint_none` 这个名字误导. 它实际表达的是 **"该 lp 是否要求 path 字符串以旋转结尾"**. 应该改名为 `lp_requires_last_rotate(lp)`, 语义更清晰:

| Hook | lp_requires_last_rotate(lp) |
| :--- | :--- |
| NoHook | false (永远) |
| NoSpinHook | false (永远) |
| CautiousHook | false (永远) |
| ASpinHook | false (永远 — ASpin 不要求路径里有旋转) |
| TSpinHook | `lp.type != None && cfg->last_rotate` |

→ **只有 TSpinHook 在 spin 命中且 cfg 启用 last_rotate 时才返回 true**. 其余全 false, 全部享受自落点短路.

### 顺便: 检视 `index` 选择 (L850)

```cpp
TetrisNode const *land_last = SpinHook::get_last_node(land_point);
bb::CellsKey index = (SpinHook::is_landpoint_none(land_point) || land_last == nullptr)
                         ? Helpers::cells_key_for(land_point.node)
                         : Helpers::cells_key_for(land_last);
```
- TSpin 路径 type != None && land_last != nullptr → 用 last 节点 cells_key (路径终点是 last, 之后旋转一步落到 lp.node).
- 其它路径 → 用 lp.node cells_key.

ASpin 路径 type=ASpin 但 `land_last=nullptr` → `index = cells_key(lp.node)`. 行为正确, 不需要改.

→ **L850 行为不依赖语义错位, 不用动**.

### 检视 visitor 命中谓词 (L924, L1052)

```cpp
visitor.landpoint_is_none = SpinHook::is_landpoint_none(land_point);
// 命中: !landpoint_is_none && k == index_landpoint && last_rotate
```
ASpin 路径 last_rotate=false → 第二条恒不命中, landpoint_is_none 在 ASpin 路径**实际不影响命中结果**. 改名后:
```cpp
visitor.requires_last_rotate = SpinHook::lp_requires_last_rotate(land_point);
// 命中: requires_last_rotate && k == index_landpoint
```
合并一个条件, 更清晰. (config_last_rotate 已被 lp_requires_last_rotate 内联, strategy 端少查一次).

### 影响面

只有 ASpin 路径"起点==终点"时受影响:
- 当前: 走完整 BFS, 命中后返回空 path (visitor.hit, build_path(found) 在起点 self-prev 协议下返回空);
- 修正后: 直接 return 空 path.

**性能改善**, **行为一致** (输出空 path). Oracle byte-equal — 因为 oracle 这条路径的"起点==终点"边界 case 与位板完全等价.

### 评估: 应该改

| 方案 | 落地 | 风险 |
| :--- | :--- | :--- |
| **Q11.a** rename: `is_landpoint_none` → `lp_requires_last_rotate`, 反转语义 | 改 5 个 hook + 4 strategy 调用面 | 中 |
| **Q11.b** 仅修 ASpinHook::is_landpoint_none 让它永远 true (维持名字) | ASpinHook 单点改 | 低 |
| **Q11.c** 维持名字, 加新 trait `requires_last_rotate(lp)`, 短路守卫与 visitor 切换到新 trait | Hook 加接口, 短路与 visitor 切换 | 低 |

**推荐: Q11.a** (一次性正名). 命名误导是历史包袱, 拖到 R.6 LandPoint 位板化时清理代价更大. 现在改, BaseSpinHook 默认值与 visitor 命中谓词一并简化.

> 兼容性: `is_landpoint_none` 与 `get_last_node` 在改名后, 也建议合并形态 — 这两个 trait 实际是配套的 (last_rotate 路径要拿 last node), 可以合并为:
> ```cpp
> // 返回 nullptr 表示不要求 last_rotate; 否则返回 last node.
> static TetrisNode const *required_last_node(Config const *cfg, LandPoint const &lp);
> ```
> strategy 端 `bool requires = (last != nullptr)`. 接口面再 -1.

---

## 12. 整合修正路线 (Q1 - Q11 总表)

| # | 修正 | 关联 Q | 风险 |
| :--- | :--- | :--- | :--- |
| **N.1** | EmptyPayload + SpinTypePayload (Q10) | Q1, Q7, Q10 | 低 |
| **N.2** | PlainLandPoint, NoSpin + Cautious 切换 | Q2, Q3 | 中 |
| **N.3** | block_buffer 52/+10 → 命名常量 | Q4, Q8 短期 | 低 |
| **N.4** | BaseSpinHook CRTP 默认 + `is_landpoint_none` 更名 `lp_requires_last_rotate` (反转语义), 让 ASpin 享受自落点短路 | Q5, Q6, Q9, Q11 | 中 |
| **N.5 远期** | tag 位板化时整体删 SearchState / check_ready | Q8 中期 | 中 |

---

## 13. 待用户裁决

- [ ] **Q10** Payload 命名 `SpinTypePayload` (代替 ExTypePayload), NoSpin/Cautious 经 N.2 后用 EmptyPayload — 同意 / 否决
- [ ] **Q11.a** 把 `is_landpoint_none` 反转改名 `lp_requires_last_rotate`, ASpin 路径享受自落点短路 — 同意 / 否决
- [ ] **Q11 合并 trait** 进一步把 `is_landpoint_none + get_last_node` 合并为 `required_last_node(cfg, lp) -> TetrisNode const*` — 同意 / 否决 / 暂缓
- [ ] **N.4 与 N.1 合并落 1 个 commit, 还是各自独立** — 推荐意见: N.1/N.4 各自独立 (前者形态, 后者语义/性能, 分开易回滚)

每项审查通过后再开始动代码。
