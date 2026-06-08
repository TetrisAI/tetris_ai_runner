# TetrisOpertion 全面模板化执行计划（共识版）

> 关键字: tetris-opertion template-refactor execution-plan rulespec compile-time

## 背景共识

用户已经确认：
- 运行时**不需要**切换规则（同一二进制只跑一种规则）。
- 运行时**不需要**调 w/h（编译期固定）。

这意味着可以一路推到「层级 B + 部分层级 C」，但 **w/h 是否要编入模板**仍需斟酌——因为现有 `TetrisMap.row[max_height]` 已经是 `max_height=40` 编译期常量，运行时 w/h 只影响初始化和 `TetrisRule::init` 的合法性判断，并未影响数据结构。所以：

- **w/h 不写入模板**（保持现状），节省二进制体积、避免大改 `TetrisMap`。
- **规则全面模板化**，废弃 `std::map<{t,r}, TetrisOpertion>` 注册路径。

## 总体架构（最终态）

```cpp
// 1. 单条 op 的编译期描述
template<char T, uint8_t R, uint32_t L1, uint32_t L2, uint32_t L3, uint32_t L4,
         class CW, class CCW, class OPP,    // 旋转目标 R（用 OpTraits 引用，可为 void）
         class WKCW, class WKCCW, class WKOPP>
struct OpDesc {
    static constexpr char type = T;
    static constexpr uint8_t rotation = R;
    static constexpr uint32_t lines[4] = {L1, L2, L3, L4};
    using rotate_cw  = CW;
    using rotate_ccw = CCW;
    using rotate_opp = OPP;
    using wk_cw      = WKCW;     // struct { static constexpr WallKickNode data[]; static constexpr size_t length; }
    using wk_ccw     = WKCCW;
    using wk_opp     = WKOPP;
};

// 2. 整个规则的编译期清单
template<class... Ops>
struct RuleSpec {
    static bool init(int w, int h);          // 编译期写死的合法尺寸
    template<char T> static constexpr auto initial_status();  // T1/T2/...
    using opertion_list = std::tuple<Ops...>;
};

// 3. TetrisContext 模板化
template<class Rule>
class TetrisContext { ... };

// 4. TetrisEngine 已经是模板，只是把 Rule 换成 RuleSpec 类型
template<class Rule, class AI, class Search, class Status, class TaskRunner>
class TetrisEngine { ... };
```

## 收益项（按确定性排序）

1. **`TetrisNode::op` 字段被彻底删除** → `sizeof(TetrisNode)` 缩约 **430B**，指针网内存降 30%-50%；
2. **踢墙表去重**：现在每个 TetrisNode 拷贝一份完整 op，包含 3 张踢墙表；模板化后踢墙表是 RuleSpec 的静态成员，**全场地共享一份**；
3. 构建期的 `op.create / op.rotate_*` 全部内联，build 时间下降；
4. `std::map<{t,r}, TetrisOpertion>` 整套注册逻辑被静态分派替代，启动确定性更好（虽然时间影响小）。

## 阶段拆分（保单一提交原则）

每个阶段一个 commit，独立可编译可推送：

### 阶段 1：删除 `TetrisNode::op` 字段（不动模板化）

**改动范围**：`tetris_core.h / tetris_core.cpp` + 不影响 10 个 `rule_*.cpp`。

**做法**：
1. `TetrisOpertion` 字段保留，但从 `TetrisNode` 移走。
2. `TetrisContext` 内部新增 `op_table_[type_index][r]` 直接索引（替代 `std::map<pair, TetrisOpertion> opertion_`）；
3. `TetrisContext::build` 中 `node.op.xxx` 全部改为 `op_table_[t_idx][r].xxx`；
4. 因 `m_tetris_rule_tools::create_node / rotate_template` 接收 `TetrisOpertion const&`，需要把它们的签名改为接收 `TetrisContext const*` 内的查询路径，或改为接收 `(t, r)`。

**风险点**：
- `TetrisNode` 之前有大量"复制 node"的代码（`tetris_core.cpp:327, 348, 372`）会带 `node.op` 字段，这些位置都要清理；
- `m_tetris_rule_tools::create_node` 的 `op` 参数当前用于在 build 期把 op 写回 node，删字段后这条链断掉，需要用 type+r 索引到 context 的 op_table。

**Commit message 候选**：
```
Strip TetrisOpertion from TetrisNode and centralize op table on context
```

### 阶段 2：模板化单条 op（OpTraits）

**改动范围**：`tetris_core.h` + 10 个 `rule_*.cpp` 的 op 定义形式。

**做法**：
1. 引入 `template<char T, uint8_t R, uint32_t L1..L4> struct ShapeTraits` 编译期持有方块形状；
2. 引入 `template<class CW, class CCW, ...> struct WallKickTable`；
3. 把 `op.rotate_clockwise` 等函数指针替换为 `static constexpr bool rotate(TetrisNode&, TetrisContext const*)` 静态成员；
4. 10 个 rule_*.cpp 仍然返回 `std::map<{t,r}, TetrisOpertion>`，但 `TetrisOpertion` 内部的字段类型从函数指针变为"静态函数模板的实例化结果"，调用点改为常量函数指针（这阶段保留运行时签名以减小回归面）。

**Commit message 候选**：
```
Promote per-piece operations to static OpTraits descriptors
```

### 阶段 3：编译期 RuleSpec（替换 std::map 注册）

**改动范围**：`tetris_core.h / tetris_core.cpp` + 10 个 `rule_*.cpp` + 6 个 `search_*.cpp` 的 `TetrisEngine` 实例化。

**做法**：
1. 每个 rule_*.cpp 用 `using TetrisRule = m_tetris::RuleSpec<op_O1_t, op_I1_t, ...>;`；
2. `TetrisRule::get_opertion()` 接口删除；
3. `TetrisContext` 改为 `template<class Rule> class TetrisContext`，build 期通过 `for_each_op` 编译期展开注册节点；
4. 所有持有 `TetrisContext` 的位置（`TetrisEngine`、`search_*` 中的 `TetrisContext const*`）跟着模板化。

**风险点**：
- `TetrisContext` 模板化后，搜索器签名 `Search::search(TetrisMap const&, TetrisNode const*, level)` 已经不依赖 `TetrisContext` 的具体类型（多通过 `TetrisNode::move_*` 间接拿到），所以 search_*.cpp **大概率不需要改**；
- `cmd_tris.cpp / botris.cpp / vs.cpp` 等入口要把 `using namespace rule_xxx` 后续的 `TetrisRule` 用法跟着模板化。

**Commit message 候选**：
```
Replace runtime opertion map with compile-time RuleSpec
```

## 阶段 1 的具体文件清单（先动这个）

```
src/tetris_core.h        - 删 TetrisNode::op；TetrisContext 加 op_table_
src/tetris_core.cpp      - build 路径改用 op_table_；create_node/rotate_template 签名调整
（10 个 rule_*.cpp 不动 - 仍走 get_opertion() std::map）
（其它文件不动）
```

预计 diff 量：< 200 行；可以确保编译通过且行为等价。

## 我现在要做的事

1. **本回合不动代码**——把执行计划交给你确认；
2. 你拍板"按阶段 1 开干"或"调整范围"后，我开始改 src/tetris_core.{h,cpp}；
3. 阶段 1 完成后我会把 diff 和单一 commit 准备好，**等你确认后**再生成 patch/bundle。
