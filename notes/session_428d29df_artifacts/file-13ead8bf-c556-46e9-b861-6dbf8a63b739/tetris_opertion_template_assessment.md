# TetrisOpertion 全面模板化可行性评估

> 关键字: tetris-opertion template-refactor compile-time consteval rule-system

## 0. 我的总体态度

**部分赞成、部分反对，需要先达成共识再动手。**

- "去掉函数指针、去掉 std::map<pair, TetrisOpertion>、把规则改成编译期数据" 的方向是正确的，但是**前提条件比较苛刻**：
- 全面模板化（编译期固化规则）会与本框架"运行时可切换规则 + 运行时 w/h"的核心契约直接冲突，如果你要的是"全部改成模板"，那至少先决定：**还要不要 `TetrisEngine<TetrisRule, ...>` 之外的运行时规则切换？还要不要支持运行时 w/h？**

下面把代价、收益、风险、和"不打破契约的渐进方案"都写清楚。

## 1. 摸清现状：`TetrisOpertion` 现在到底承担什么

### 1.1 字段构成（src/tetris_core.h:209-225）
```cpp
struct TetrisOpertion {
    TetrisNode (*create)(size_t w, size_t h, TetrisOpertion const &op);
    bool (*rotate_clockwise)(TetrisNode&, TetrisContext const*);
    bool (*rotate_counterclockwise)(TetrisNode&, TetrisContext const*);
    bool (*rotate_opposite)(TetrisNode&, TetrisContext const*);
    TetrisWallKickOpertion wall_kick_clockwise;     // length + WallKickNode[16]
    TetrisWallKickOpertion wall_kick_counterclockwise;
    TetrisWallKickOpertion wall_kick_opposite;
};
```

**4 个函数指针 + 3 个踢墙表（每个 ~132B）≈ 总 ~430B/枚。**

### 1.2 谁实际调用 op
- **指针网构建期（`TetrisContext::build`）**：调 `op.create / op.rotate_*`（src/tetris_core.cpp:328、477、595）。
- **指针网构建期**：读 `op.wall_kick_*`（src/tetris_core.cpp:479-481）。
- **运行时（搜索/AI/落子）**：**完全不调 op**——所有移动/旋转/踢墙都通过 `TetrisNode::move_left/move_down_multi/wall_kick_clockwise[]` 这样的预计算指针走。

### 1.3 关键事实（决定模板化的实际收益）
1. **`TetrisNode::op` 字段在运行时是死字段**——只在构建期被读，从来不在搜索热路径上触发函数调用。
2. **构建期是一次性 startup cost**（每次 `build` 一次），不是 hot path。
3. **`TetrisNode` 大小被 `op` 字段拉大约 ~430B**，但因为指针网节点有 ~max_height + 16*3 个指针，总体 sizeof(TetrisNode) 已经几百字节，op 占比并非压倒性。

### 1.4 当前规则数量
SRS / Botris / TOJ / QQ / C2 / PPT / ASRS / SRSX / ST / TAG **共 10 套规则**，每套规则一个 `TetrisRule::get_opertion()` 返回 `std::map<{type,r}, TetrisOpertion>`。

## 2. "全面模板化"的几种诠释及代价/收益

理解你说的"全面模板化"可能有三个层级，我分别评估：

### 层级 A（轻量）：仅模板化创建/旋转函数指针
**做法**：把 `op.create / op.rotate_*` 4 个函数指针替换为：
- 在 `TetrisOpertion` 上加 `template<char T, uint8_t R> struct TetrisOpertionTraits` 之类的静态信息；
- 但仍然以 `std::map<{t,r}, TetrisOpertion>` 形式注册到 `TetrisContext`。

**代价**：低；改动只在 `tetris_core.h` 的 `TetrisOpertion` 定义 + 10 个 rule_*.cpp 的 op 表创建处。
**收益**：去掉间接调用，但因为这些只在 build 期调用，**热路径零提升**；唯一的好处是代码更清晰，编译器可以把 rotate 内联进 build 循环。
**风险**：低。
**净评价**：**不值得做**——收益太薄。

### 层级 B（中量）：模板化 op 数据 + 编译期注册表
**做法**：
- `TetrisOpertion` 改成 `template<char T, uint8_t R, /* 4 行 mask */, /* wall kick lists */> struct TetrisOpertionT`；
- 每个规则用 variadic 把所有 op 编入一个类型列表 `RuleSpec<...op_T1, op_T2, ..., op_O1, ...>`；
- `TetrisContext` 改为 `TetrisContext<RuleSpec>`，在构建期通过 `RuleSpec::for_each([](auto op) {...})` 注册节点。

**代价**：
- `tetris_core.h` 中 `TetrisContext / TetrisEngine` 大概率要变成 `TetrisContext<TetrisRule>`、`TetrisEngine<TetrisRule, TetrisAI, ...>`——但**目前 TetrisEngine 已经是 template<TetrisRule, ...>**，所以这一层改动量比想象中小（已经是模板嵌套）。
- 10 个 rule_*.cpp 全部改为 `using TetrisRule = m_tetris::Rule<op_O1_t, op_I1_t, ...>;` 形式。
- `bool TetrisRule::init(int, int)` / `get_opertion()` / `get_generate()` 接口要重设计。

**收益**：
- 编译期常量传播 → 旋转/创建逻辑可以被完全内联到 build 循环；
- `TetrisNode::op` 字段**可以删除**（因为信息全在 `RuleSpec`，构建期通过 type+r 索引到 `RuleSpec::lookup<T,R>()`）；
- `sizeof(TetrisNode)` 缩小 ~430B → cache-line 友好（指针网遍历在树搜索的 expand 阶段是 hot path）；
- 踢墙表可以放进 `RuleSpec` 的静态成员，避免每个 TetrisNode 拷贝一份。

**风险**：
- **破坏现有 `TetrisRule` 多态契约**：现在多个 ai_*.cpp / search_*.cpp 通过 `TetrisEngine<rule_srs::TetrisRule, ...>` 选择规则，模板参数化整体可控；但 `TetrisContext` 内部很多地方用 `std::map<{t,r}, TetrisOpertion>` 遍历，要改为编译期分派；
- **失去运行时切规则的能力**：以前如果想"同一进程内同时跑两套规则"，可以构造两个 `TetrisContext`；模板化后两个 `TetrisContext<RuleA>` 和 `TetrisContext<RuleB>` 是不同类型，不能放进同一容器；
- **增加编译时间**：10 套规则 × 多个 AI × 多个搜索器 → 模板实例爆炸，编译时间会显著变长。

**净评价**：**有价值，但要先确认你能接受"规则编译期固化"和"运行时不再切换规则"**。

### 层级 C（激进）：把 w/h 也编入模板
**做法**：`TetrisContext<RuleSpec, W, H>` 全编译期；类似 cobra 走 `template<size_t W, size_t H>`。

**代价**：极高——本框架的 `TetrisMap`、`TetrisEngine`、`AI接口`、`search_*` 全部要带 W/H 模板参数。

**收益**：W/H 编译期常量后，行索引/列偏移都能被常量折叠，可能拿到 1.1-1.3x 的整体加速。

**风险**：
- **破坏运行时 w/h 自适应**：本框架在 `TetrisRule::init(w, h)` 检查规则是否支持当前 w/h，比如 SRS 要求 `w==10 && h==40`，但 ASRS 可能允许其它尺寸；
- 多 W/H 实例化爆炸：要支持 W ∈ {6, 7, 8, 10, 12, 14, ...}，每种宽度都得实例化一次 build / search / AI，**目标二进制可能膨胀 5-10 倍**；
- 与"`uint32_t row` 最大宽度 32"的运行时灵活性彻底冲突。

**净评价**：**不建议**。除非你确定本框架就服务于 W=10/H=40 这一种盘面。

## 3. 我推荐的路径：层级 B 的"渐进版"

### 3.1 阶段 1：消除运行时 op 字段（零外部接口变化）
- `TetrisNode::op` 字段移除；
- `TetrisContext` 内部新增 `op_table_[type_index][r]`，build 期所有读 `node.op.xxx` 的地方改为 `context->op_table_[t_idx][r].xxx`；
- 收益：`sizeof(TetrisNode)` 缩 ~430B；指针网内存降 ~30%-50%；树搜索 expand 的 cache 命中率上升。

### 3.2 阶段 2：把 `op.create / op.rotate_*` 改为 traits（仍然是 std::map 注册）
- `m_tetris_rule_tools::create_node` 和 `rotate_template` 已经是模板，**但目前以函数指针形式存进 op 字段**；
- 改为 `template<char T, uint8_t R> struct OpTraits { static TetrisNode create(...); static bool rotate_*(...); };`，op 注册时直接 `info[{T,R}] = OpTraits<T,R>::desc()`，build 期直接调静态函数。
- 收益：构建期内联展开，减少间接跳转；规则正确性更容易在编译期校验。

### 3.3 阶段 3（可选）：编译期 RuleSpec
- 当且仅当上面两阶段验证通过、且确实有性能瓶颈在指针网构建期时再做。

## 4. 反驳那些"看上去诱人但实际无收益"的诱惑

- **诱惑 A**："改成模板后 rotate 可以编译期内联，搜索就快了。"
  → **错**。搜索运行时根本不调 op.rotate，调的是预计算的 `node->rotate_clockwise` 指针。模板化只加速 build 期。
- **诱惑 B**："改成模板后可以删掉 std::map，启动更快。"
  → **对，但启动只有几毫秒**，对一个长跑型 AI 服务可以忽略。
- **诱惑 C**："cobra 完全模板化所以快。"
  → cobra 快是因为 mask BFS + SIMD，不是因为模板。模板只是 cobra 顺手的语法选择。
- **诱惑 D**："反正都要改，一步到位。"
  → 一步到位会让你失去"运行时切规则"和"运行时调 w/h"两个工程灵活性，且 10 个 rule 文件一次性改完风险极高。

## 5. 建议的下一步对话

请先回答这两个问题，我们再决定动手范围：

1. **运行时是否需要切规则？**（即同一二进制能否跑 SRS 和 Botris？）
   - 如果 No → 层级 B 可以做。
   - 如果 Yes → 只能做层级 A 或我推荐的"阶段 1（删 node.op 字段）"。

2. **运行时是否需要调 w/h？**（即一个二进制能否同时支持 W=10 和 W=14？）
   - 如果 No → 层级 C 可以考虑（但仍不推荐，二进制膨胀大）。
   - 如果 Yes → 层级 C 直接放弃。

我个人的强烈倾向是：**先做"阶段 1：删 node.op 字段"**——风险最低、收益（指针网瘦身）最确定、不破坏任何对外契约。其它都先放后面。

