# C3 Follow-up Notes

提交: `Switch NoSpinHook / CautiousHook to PlainLandPoint` (HEAD 见末尾 git log).

## 落地概要

- 在 `src/movegen_hook.h` 引入 `PlainLandPoint` (与 `EmptyPayload` /
  `SpinTypePayload` 同区段, BaseSpinHook 之前).
- `NoSpinHook::LandPoint` / `CautiousHook::LandPoint` 从
  `::search_tspin::Search::TetrisNodeWithTSpinType` 切到 `PlainLandPoint`.
- BaseSpinHook 派生关系不变 (`NoSpinHook : BaseSpinHook<NoSpinHook>` /
  `CautiousHook : BaseSpinHook<CautiousHook>`); Payload 已在 C2 收敛为
  `EmptyPayload`, 本次只动 LandPoint.

## 调用面是否需要修改

**未改动 ai.cpp / c2_ai 调用面**. 原因:
- `QQTetrisSearch::search` 内 `for (auto const &lp : *src) flatten_landpoint_.push_back(lp.node);` 仅访问 `.node` — PlainLandPoint 与 NodeEx 字段名一致.
- `c2_ai.run(...).target` 拿到的 `RunResult::target` (= `Core::LandPoint`,
  即 PlainLandPoint) 之后只做 `target == nullptr` / `target->attach(...)` /
  `c2_ai.make_path(node, target, map)` — PlainLandPoint 提供的
  `operator==(nullptr_t)` / `operator->()` / `operator TetrisNode const *()`
  覆盖这三处隐式契约.
- ai_zzz::qq::Attack::eval / ai_zzz::C2::eval 接 `TetrisNode const *`,
  落在 `core/tetris_core.h` `TetrisCallAI::CallEval` 的"显式构造 NodeEx
  适配 spin AI"分支之外 — 直接用 `EvalOtherNode == TetrisNode const *`
  通过, 不读 PlainLandPoint 字段.

## PlainLandPoint 接口面相对任务最简定义的扩展

任务给出的最简定义:
```cpp
struct PlainLandPoint {
    TetrisNode const *node = nullptr;
    PlainLandPoint() = default;
    PlainLandPoint(TetrisNode const *n) : node(n) {}
    bool operator==(PlainLandPoint const &o) const { return node == o.node; }
};
```

实际落地补齐 4 个 operator (与 `tspin::TetrisNodeWithTSpinType` 共形):
* `operator TetrisNode const *() const`
* `TetrisNode const *operator->() const`
* `bool operator==(std::nullptr_t) const`
* `bool operator!=(std::nullptr_t) const`

理由: `core/tetris_core.h` 内多处对 `Core::LandPoint` 的隐式契约消费
不限于 `.node` / `==(other)`:
- `tree_node->identity` 类型 = `Core::LandPoint`, 在
  `it->identity->status` / `target->attach(...)` / `target == nullptr`
  / `RunResult::target = (TreeNode->identity ? ... : nullptr)` 等位置
  要求 LandPoint 至少能转 `TetrisNode const *`、能 deref `->`、能与
  nullptr 比较.
- 这些都属于"LandPoint 通用约定"而非 spin 字段, 不违反任务原则
  ("不要自己脑补给 PlainLandPoint 加 spin 字段").

如果只放最简定义, `core/tetris_core.h` 无法编译 (RunResult 三元
表达式 / target == nullptr 守卫直接失败). 调用面不改的代价就是
PlainLandPoint 必须满足这四条隐式契约.

## operator== 行为差异审视

旧 `tspin::TetrisNodeWithTSpinType::operator==` 对比 `node && last && type && flags`. PlainLandPoint 只比 `node`.

NoSpinHook / CautiousHook 路径下:
- Payload = EmptyPayload, 不写 `last_*`;
- active_for_piece 全 false → on_emit / apply_emit_* 全 noop, 永远不
  写 `lp.type` / `lp.last` / `lp.flags`;
- 旧 NodeEx 默认构造 `std::memset(this, 0, sizeof(*this))` 把 last /
  type / flags 全清零, 显式构造 `(TetrisNode*)` 也清 0.

→ 旧路径下 `node==other.node` 成立时, last/type/flags 也都恒 0 必相等;
operator== 退化为 `node==`, 与 PlainLandPoint 行为完全等价. 不存在
"依赖完整字段比较"的下游 (NoSpinHook / CautiousHook 根本不写非零的
非 .node 字段), 也没有 hash / unordered_set 落到 NodeEx 全字段哈希.

## 验收结果

| 验收项 | 结果 |
| :--- | :--- |
| `cmake --build build -j` | ✅ |
| `oracle_diff` | ✅ byte-equal (与 baseline 完全一致) |
| `extreme_rule_diff` | ✅ byte-equal |
| `aspin_dump` | ✅ byte-equal (LandPoint 形态切换不触及 ASpin 路径) |
| `path_node_diff` | ✅ exit 0, byte-equal (使用 build/ 残留二进制) |
| `simulate_node_diff` | ✅ exit 0, byte-equal |
| `tag_node_diff` | 17 failing, **与 C2 baseline 数量相同** (已知 tag-bb-native 残留) |

## 外发现 / 遗留事项

### 任务文档与实情的小差异

任务清单里写"如果发现 NoSpinHook / CautiousHook 调用面访问 lp.spin /
lp.type 等 — 落 c3_blocking.md, 不要自己加字段". 实地排查
(`grep land_point\.spin / land_point\.type / land_point\.is_check / ...
src/`) 结论: 只有 search_tag.h 注释里出现 `land_point.type`, 但
search_tag 仅消费 TSpinHook, 与 NoSpinHook / CautiousHook 无关; 运行时
没有任何调用面读 NoSpinHook 路径的 spin 字段. N.2 方案自洽, 无需开
c3_blocking.md.

### tag_node_diff 残留 (与 C1/C2 一致)

17 failing 全部为 J piece 在 20g 边界 `tss_*` / `i_well_*` / `t_kick_*` 用例
出现 `[only-node] idx=... rot=0 ready=0` (TSpin 路径多 emit 一个 ready=0
的落点). 与本 commit 无关 (TSpin 路径不动). 留待后续 tag 整理.

### tests/ 目录

仓库当前 tests/ 仅剩 `oracle_diff.cpp` / `extreme_rule_diff.cpp` /
`perft_movegen.cpp`. `path_node_diff` / `simulate_node_diff` /
`tag_node_diff` 二进制是历史 build 产物, 源文件不在仓库, 跑出来 exit 0
仅说明使用该二进制内嵌的 baseline 没有差异. 这部分基线行为应该用与
oracle_diff 同步的源代码再补回, 但不在本 commit 范围内, 仅记录.
