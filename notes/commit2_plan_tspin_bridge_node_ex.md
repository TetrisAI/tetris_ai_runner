# Commit 2 规划：DefaultTSpinHook 实体化 + bridge node ex
_记录时间：2026-06-02_

---

## 当前状态（commit 1 之后）

| 层 | 现状 |
|---|---|
| `TSpinHook`（= `DefaultTSpinHook`）| 已完整实现于 `movegen_hook.h`，`apply_emit_1g/20g` 能正确写入 `TetrisNodeWithTSpinType` |
| `tspin::Search`（= `Searcher<PathStrategy, TSpinHook, RuleSpec>`） | 产出 `LandPoint` = `::search_tspin::Search::TetrisNodeWithTSpinType` |
| `ai_tag / ai_zzz / ai_misaka` 的 `TetrisNodeEx` | 仍 typedef 到 `search_tspin::Search::TetrisNodeWithTSpinType` |
| `TetrisCallAI` bridge | 非同型时从裸 `TetrisNode*` 构造 `EvalOtherNode`，**丢失 last/type/flags** |

## 关键已确认事实

1. `TSpinHook::LandPoint` 定义于 `movegen_hook.h:393`：
   ```cpp
   using LandPoint = ::search_tspin::Search::TetrisNodeWithTSpinType;
   ```
   与 AI 侧的 `TetrisNodeEx` typedef 来源相同 → **两侧类型相同**。

2. `TetrisCallAI::CallEval<A,B>` / `CallEval<T,T>` 偏特化：
   - 同型（`A == B`）→ 直接传引用，identity 就是 `TetrisNodeWithTSpinType`，zero-copy ✅
   - 非同型 → 从裸指针重构造，丢 last/type/flags ❌

3. 目前 `tspin::Search` 产出 `TetrisNodeWithTSpinType`，而 `ai_tag / ai_zzz` 的 `TetrisNodeEx` 也是 `TetrisNodeWithTSpinType`
   → 理论上已走同型 branch，commit 2 需要**验证实际路径**是否确实如此。

---

## 目标

让 AI 侧 `eval` / `get` 直接拿到 `TSpinHook` 产出的完整 `TetrisNodeWithTSpinType`（含 last/type/flags），确认零拷贝路径正确。

---

## 工作项细分

### Step 1：验证同型路径
- 在 `TetrisCallAI` 或 `TetrisEngine` 里加编译期断言，固化 `LandPoint == TetrisNodeEx` 这条同型关系
- 确认 `TetrisTreeNode::identity` 字段类型确实是 `TetrisNodeWithTSpinType` 而非 `TetrisNode const*`

### Step 2：bridge node ex 语义确认（待用户回复）
候选理解（A/B/C）见问询消息。

---

## 待确认问题

用户需要说明 "bridge node ex" 具体指哪一层：

- **A.** 验证同型路径 + 补断言，不改代码逻辑
- **B.** 在 `Searcher` 加显式 `bridge_node_ex()` 方法
- **C.** `TetrisTreeNode::identity` 从 `TetrisNode const*` 升格为 `LandPoint`

---

## 与后续 commit 的关系

- commit 3：AI typedef 解耦，`search_tspin` 完整委托 → 依赖 commit 2 完成后同型路径稳固
- 20g 位板重写：仍是 blocker（不阻塞 commit 2）
