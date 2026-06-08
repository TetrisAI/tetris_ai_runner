# Stage 1.B 完成记录 - SRS 规则迁移到 RuleSpec 桥接层

> 关键字: stage1b srs-bridge ruleSpec opdesc oplines wallkicklist flatten

## 当前进度（commit 80ba299）

- **Stage 0** (1d6e55d): row_t typedef 落地 ✓
- **Stage 1.A** (46e472f): RuleSpec/OpDesc/OpLines/WallKickList 骨架 ✓
- **Stage 1.B** (80ba299): SRS 规则改造为 RuleSpec + 桥接层 ✓

## Stage 1.B 关键点

### tetris_core.h 新增

- `OpDesc<...>` 新增 `int8_t X = 0, int8_t Y = 0` 模板参数（成员 `spawn_x` / `spawn_y`），保证桥接 `create_node<T, X, Y, R, ...>` 等价。
- `OpLines<Ls...>::data[N+1]`、`WallKickList<XYs...>::data[N+1]`：用 `+1` 哨兵槽避免空 pack 触发零长数组（C++14 限制）。
- `m_tetris::detail` 桥接层：
  - `op_create_bridge<Op>`：调用 `m_tetris_rule_tools::create_node`（**目前固定 N=4**，编译期 `static_assert`）。
  - `RotateSelector<R, IsNone>`：偏特化在 R=kOpRotateNone(0xFF) 时返回 `nullptr`，否则返回 `&rotate_template<R>`。改用 `using RotateFn = bool (*)(...)` 类型别名，避免 C++14 静态 constexpr 函数指针成员的 ODR 麻烦。
  - `to_wallkick<Wk>`：复制 WallKickList::data 到 TetrisWallKickOpertion。
  - `OpFlattener<Map, Tuple, I, Size>`：tuple 递归展开。
- `flatten_rulespec<Rule>()`：单一入口，把 RuleSpec 平展为 `std::map<{t, r}, TetrisOpertion>`。
- 桥接模板的实现放在 `m_tetris_rule_tools` 命名空间声明之后（`namespace m_tetris { namespace detail { ... }}`），解决前向引用问题。

### rule_srs.cpp 改造

- 用 `using` 声明全部 26 个 OpDesc + 8 个 IKick + 8 个 JLSTZ 共享 wallkick。
- 主体为 `using SrsRule = RuleSpec<10, 40, 4, op_O1, op_I1..op_T4>;`。
- `init()` 用 `SrsRule::width / SrsRule::height`，`get_opertion()` 直接 return `flatten_rulespec<SrsRule>()`。
- `get_generate()` / `game_generate_template` 不变。
- 行为预期完全等价（wall kick 数据、rotate 目标 R、spawn x/y、lines 都逐项对照过原版）。

## 待办（Stage 1.C 之后）

### Stage 1.C：迁移其它 9 个规则

`rule_srsx / rule_qq / rule_st / rule_tag / rule_asrs / rule_botris / rule_c2 / rule_ppt / rule_toj`：

- 部分规则不止 0/0 spawn，存在 `<'I', 2, 1, 0>` 等不同 X/Y。
- 部分规则 piece 集合不同（如 ppt/toj 有更多旋转）；都是 N=4，可直接复用桥接层。
- 部分规则的 wallkick 表全是 NoKick（如 rule_qq、rule_st），实例化 `OpDesc<..., kOpRotateNone, kOpRotateNone, kOpRotateNone>` 即可（默认参数省略）。
- **建议每个 rule 单独一次 commit**。

### Stage 2-6（不变）

- Stage 2: 删 TetrisNode::op，TetrisContext 持有 op_table。
- Stage 3: TetrisContext / TetrisEngine 模板化为 `<Rule>`。
- Stage 4: N 完全可变（去掉桥接层 N==4 限制）。
- Stage 5: AiBridgeView。
- Stage 6: row_t 真按 W 缩窄。

## 风险提示

- 当前**未编译**（环境无编译器）。依赖用户机器编译验证。
- `+1` 哨兵槽对 SRS（每个 OpLines 都是 4 行）实际不会触发零长，仅是兜底。
- 桥接层对 `target_cw == kOpRotateNone (0xFF)` 的兼容是关键：原代码 `nullptr` 表示无旋转，桥接层通过 RotateSelector 偏特化等价输出。

## 下一步建议（用户确认）

是否继续 Stage 1.C？建议从最简单的 rule_srsx 开始（与 srs 几乎同结构），再逐个推 ppt/toj 这种 piece 数量多的。
