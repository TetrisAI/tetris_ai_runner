# flip-bits 分支 rule 接口理解（临时调研）

- 时间：2026-06-02 15:05
- 目标：只理解 `flip-bits` 基线分支（oracle，提交 `e3b909b`）里一套 rule（如 `rule_botris`、`rule_srs`）的接口结构与约定
- 约束：本次结论仅作为临时调研记录，**不要写入长期记忆**；等待用户确认后再沉淀
- 代码来源：用户本机目录 `/Users/zhaoming.274/Work/tetris_ai_runner` 中通过 `git show e3b909b:...` 读取的文件

## 1. 先说结论

`flip-bits` 这版里，所谓“一套 rule”，本质上是一个**编译期策略类型**：

```cpp
namespace rule_xxx {
    struct TetrisRule {
        static bool init(int/size_t w, int/size_t h);   // 可选
        static std::map<std::pair<char, uint8_t>, m_tetris::TetrisOpertion> get_opertion();
        static std::map<char, m_tetris::TetrisBlockStatus(*)(m_tetris::TetrisContext const *)> get_generate();
    };
}
```

其中真正必需的稳定接口只有两项：
- `get_opertion()`
- `get_generate()`

`init()` 是**可选钩子**，用于校验该 rule 允许的场地尺寸；如果没实现，框架默认视为总是允许。

## 2. rule 是怎么被框架消费的

### 2.1 rule 不是运行时对象，而是模板参数

典型用法：

```cpp
m_tetris::TetrisEngine<rule_botris::TetrisRule, ai_zzz::Botris, search_aspin::Search>
```

也就是说：
- rule 不需要实例化
- 框架只按静态接口调用它
- AI / Search / Rule 三者在 `TetrisEngine<...>` 中组合

### 2.2 引擎初始化时只读取 rule 的两张表

`TetrisEngine::prepare()` 会把：
- `TetrisRule::get_opertion()` 填到 `shared_context_->opertion_`
- `TetrisRule::get_generate()` 填到 `shared_context_->generate_`

随后 `TetrisContext::prepare()` 用 `generate_` 枚举 piece 类型，再基于 `opertion_` 建整张节点图。

所以 rule 对框架提供的核心信息其实就是：
1. 每种 piece 从哪里生成（spawn status）
2. 每个 `(piece type, rotation)` 对应的形状、旋转函数、踢墙表

## 3. `init()` 的真实约定

`tetris_core.h` 里有一个 `TetrisRuleInit<Rule>` 的 SFINAE 包装：
- 如果 `Rule::init(w, h)` 存在，就调用它
- 如果不存在，就直接返回 `true`

因此：
- `rule_srs / rule_asrs / rule_botris / rule_c2 / rule_ppt / rule_toj` 这类 rule 会显式限制尺寸
- `rule_st / rule_tag / rule_qq` 这类 rule 没有 `init()`，表示只要引擎底层尺寸合法（宽高在核心范围内）就接受

例如：
- `rule_botris::TetrisRule::init(int w, int h)` 要求 `w == 10 && h == 40`
- `BotrisAI3()` 对外输入虽然是 `22` 高，但会把可见场地映射进内部 `10 x 40` 上下文

## 4. `get_generate()` 的约定

返回类型：

```cpp
std::map<char, TetrisBlockStatus(*)(TetrisContext const *)>
```

含义：
- key 是 piece 类型字符，如 `O I S Z L J T`
- value 是一个“生成函数”，输入当前 `TetrisContext`，输出该 piece 的**初始状态** `TetrisBlockStatus`

### 4.1 生成函数只决定初始状态，不负责碰撞或落地

它只给出：
- `t`：方块类型
- `x, y`：初始坐标
- `r`：初始朝向

后续节点扩展、左右移动、旋转、下落、碰撞检查，都是 `TetrisContext` / `TetrisNode` / 搜索器去做的。

### 4.2 不同 rule 的 spawn 约定可以不同

例如：
- `rule_st`：`(width / 2, height - 2, r=0)`
- `rule_tag`：`(width / 2 - 2, height, r=0)`
- `rule_qq`：`(width / 2 - 2, height - 1, r=0)`
- `rule_botris`：不是统一公式，而是每个 piece 各自给固定初始 `(x, y, r)`，如 `O` 在 `(4,20,0)`，其余大多在 `(3,20,0)`

这说明 **spawn 位置本身就是 rule 的一部分**。

## 5. `get_opertion()` 的约定

返回类型：

```cpp
std::map<std::pair<char, uint8_t>, TetrisOpertion>
```

key：
- `(piece type, rotation)`

value：
- 该 piece 在该旋转态下的 `TetrisOpertion`

`TetrisOpertion` 结构包含：
- `create`：按该旋转态创建一个 `TetrisNode`
- `rotate_clockwise`
- `rotate_counterclockwise`
- `rotate_opposite`
- 三套对应踢墙表：`wall_kick_clockwise / counterclockwise / opposite`

### 5.1 这里的核心不是“动作实现”，而是“状态定义”

每个 `(type, rotation)` entry 实际在声明：
- 这个旋转态的 4x4 形状位图是什么
- 从这个态顺/逆/180 旋后，逻辑上应该转到哪个目标朝向
- 该转动使用哪套踢墙偏移序列

**要特别强调：`TetrisNode` 里旋转相关字段是 `union`。**

也就是说：
- 平时把它当 `rotate_clockwise / rotate_counterclockwise / rotate_opposite` 看时，表示“直接旋转后到达的节点”
- 把它当 `wall_kick_clockwise[] / wall_kick_counterclockwise[] / wall_kick_opposite[]` 看时，**数组第 0 个元素就是这个直接旋转位置**
- 后面的元素才是按 wall kick 表继续尝试出来的位置

这点很容易忘，因为代码里先有“单指针视角”，后面又在 `prepare()` 里统一改写成“踢墙数组视角”。但运行搜索时，遍历 `wall_kick_xxx` 时拿到的第一个候选，本来就是“无偏移直接旋转”的目标节点，不是“第一个 kick 偏移后的节点”。

### 5.2 通常用 `create_node<...>` + `rotate_template<R>` 写

rule cpp 里普遍用：

```cpp
create_node<'T', X, Y, R, line1, line2, line3, line4>
rotate_template<target_r>
```

理解上可以把它看成：
- `create_node`：声明该旋转态的原型节点（piece 类型 / 默认状态 / 4x4 形状）
- `rotate_template<k>`：声明“转完后目标朝向是 `k`”

真正的移动逻辑不在 rule 里写：
- 左右上下移动统一走 `m_tetris_rule_tools::move_left/right/up/down`
- 旋转本体统一走 `rotate_default`
- rule 只负责把“目标旋转态”和“踢墙尝试表”喂给框架

## 6. `create_node` 这层的隐含约定

`m_tetris_rule_tools::create_node(...)` 负责把 rule 里给的 4x4 位图压成真正的 `TetrisNode`：
- 去掉上下左右空白边，得到最小包围盒
- 计算 `top[] / bottom[]`
- 生成 `status`
- 保存到 `node.data[4]`

所以 rule 文件中那四行 `T(a,b,c,d)` 的 4x4 矩阵，就是**方块几何定义的唯一来源**。

这也解释了为什么不同 rule 的差异主要都写在：
- 4x4 形状排布
- 初始 `(x,y,r)`
- 旋转目标态
- 踢墙表

## 7. 引擎对 rule 的默认假设

基于 `TetrisContext::prepare()` / `create()`，我目前确认有这些隐含约定：

### 7.1 `get_generate()` 列出的每种 type，都必须能在 `get_opertion()` 里找到对应 spawn 态

因为 `prepare()` 会先对每个 `generate_[type](this)` 调 `create(...)` 建首节点。
如果生成出来的 `(type, r)` 在 `opertion_` 里没有 entry，`op.create` 就不成立，整套 rule 会坏掉。

### 7.2 `get_opertion()` 需要把该 rule 下“可达的旋转态”定义完整

引擎会从生成态出发，不断尝试：
- `rotate_clockwise`
- `rotate_counterclockwise`
- `rotate_opposite`
- `move_left/right/down/up`

并把能到达的新 `status` 全部灌进节点图。

所以一个 rule 不一定非要给每块都写 4 态，但**凡是旋转逻辑会到达的态，都必须有定义**。

### 7.3 旋转函数为 `nullptr` 就表示该方向旋转不支持

例如某些规则里：
- `O` 的旋转函数可以全空，表示不旋
- 大部分老规则的 `rotate_opposite` 为空，表示不支持 180

这不是运行时判断，而是 rule 直接通过空函数指针表达“此动作不存在”。

## 8. Botris / SRS / 其他 rule 的结构差异

### 8.1 Botris / SRS / ASRS / C2 / TOJ / PPT

这一类结构最完整：
- 一般有显式 `init()`
- 7 种 piece 都有较完整旋转态定义
- 有 CW / CCW 踢墙表
- 通常 `rotate_opposite == nullptr`（至少 `rule_botris` 是这样）

### 8.2 ST / QQ / TAG

这一类更像“玩法兼容规则”而不是严格 Guideline 封装：
- 通常没有 `init()`
- 某些 piece 只给 1 态或 2 态
- 很多旋转不带踢墙
- spawn 位置和朝向也更简化

## 9. 对 `rule_botris` 的当前理解

`rule_botris` 在 `flip-bits` 基线里可以这样概括：

- 接口层面和 `rule_srs` 完全同构：`init + get_opertion + get_generate`
- `init(10, 40)` 强制内部上下文固定为 Botris 所需尺寸
- `get_generate()` 用 piece-specific 的生成位置，而不是统一公式
- `get_opertion()` 为 7 个 piece 基本都列了 4 个旋转态
- 每个态都绑定：
  - 4x4 几何
  - CW/CCW 目标朝向
  - 对应踢墙表
- `rotate_opposite` 统一是 `nullptr`，说明这版 rule 自身不定义 180 旋转

我现在的判断是：**在 flip-bits 这版架构里，rule 的职责边界很窄但很清晰——它只描述“方块规则学”，不描述搜索策略，也不描述评分。**

## 10. 暂时不想说得太满的点

以下点我觉得还需要保持谨慎，不建议现在就当成最终结论：

1. `create_node<X,Y,...>` 里的 `X/Y` 不是运行时语义，而是**给规则撰写者用的预处理对齐工具**
   - 目的是让写 rule 的人可以按自己习惯选择参考点，不必被“必须按左上角/左下角对齐”束缚
   - 它只在 `create_node(...)` 预处理 4x4 形状、生成初始 `TetrisNode/TetrisBlockStatus` 时参与
   - 一旦节点图建完，运行时移动、旋转、碰撞、搜索都不再关心这组 `X/Y` 的原始书写意图

2. 各 rule 之间踢墙表是否严格对应某个外部标准命名
   - 例如 `rule_botris` 看起来接近 SRS 风格，但我还没逐项对照外部规范逐表校验

3. `rule_tag / rule_qq / rule_st` 的“少态定义”是否存在刻意的兼容假设
   - 目前只确认结构上允许这样写
   - 但这些 rule 的历史语义还没向上游玩法再核对

## 11. 当前最稳的一句话

如果只说最稳的结论：

> `flip-bits` 基线里的 rule，本质是一个提供两张静态查表（spawn 表 + `(piece, rotation)` 操作表）的模板策略类型；可选再加一个尺寸校验 `init()`。引擎用这两张表预构建整套节点/旋转/踢墙图，因此 rule 的核心约定就是：把每种方块在该玩法下的出生状态、几何形态、旋转目标态和踢墙序列描述完整。
