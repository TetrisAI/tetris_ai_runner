# 多维全面模板化 - 最终执行计划（决策已锁定）

> 关键字: tetris-template multidim-final boardspec rulespec wallkick-template

## 决策总结（用户已确认）

| 维度 | 决策 |
|---|---|
| **W/H** | **属于 RuleSpec 的一部分**，**W ≤ 64**，不支持超过 64 |
| **Note 矩阵** | N=5 仅占位（参数化但默认 N=4），rule 后续补 |
| **AI 改造** | 后续做；**第一版上桥接层让 AI 不动**，且**桥接层不要求支持 W>32**（等价：第一版只服务 W ≤ 32 的 AI） |
| **WallKick** | **也用模板化** |
| **运行时切规则** | 不需要 |
| **运行时调 w/h** | 不需要 |

## 核心架构（最终态）

```cpp
// ---------- 编译期描述 ----------

// 单条踢墙序列：编译期数组
template<int8_t... XYs>  // 平展 [x0,y0,x1,y1,...]
struct WallKickList {
    static constexpr size_t length = sizeof...(XYs) / 2;
    static constexpr int8_t data[sizeof...(XYs)] = {XYs...};
};
using NoKick = WallKickList<>;  // 长度 0

// 单个方块在某旋转下的描述
template<char T, uint8_t R,
         uint32_t L0, uint32_t L1, uint32_t L2, uint32_t L3, uint32_t L4 = 0,
         uint8_t TargetCW  = 0xFF,   // 0xFF 表示无旋转
         uint8_t TargetCCW = 0xFF,
         uint8_t TargetOpp = 0xFF,
         class WkCW  = NoKick,
         class WkCCW = NoKick,
         class WkOpp = NoKick>
struct OpDesc {
    static constexpr char type = T;
    static constexpr uint8_t rotation = R;
    static constexpr uint32_t lines[5] = {L0, L1, L2, L3, L4};  // N≤5
    static constexpr uint8_t target_cw  = TargetCW;
    static constexpr uint8_t target_ccw = TargetCCW;
    static constexpr uint8_t target_opp = TargetOpp;
    using wk_cw  = WkCW;
    using wk_ccw = WkCCW;
    using wk_opp = WkOpp;
};

// RuleSpec：把 W/H/N + 所有 op + 初始状态打包
template<size_t W, size_t H, size_t N, class... Ops>
struct RuleSpec {
    static_assert(W <= 64, "W must be <= 64");
    static_assert(N <= 5,  "N must be <= 5");
    static constexpr size_t width  = W;
    static constexpr size_t height = H;
    static constexpr size_t note   = N;
    using row_t = std::conditional_t<W <= 8,  uint8_t,
                  std::conditional_t<W <= 16, uint16_t,
                  std::conditional_t<W <= 32, uint32_t, uint64_t>>>;
    using ops = std::tuple<Ops...>;

    template<char T> static constexpr TetrisBlockStatus initial_status();
    // ... 其它编译期工具
};

// ---------- 运行时数据结构 ----------

template<class Rule>
struct TetrisMap {
    using row_t = typename Rule::row_t;
    row_t row[Rule::height];
    int32_t height;
    int32_t roof;
    int32_t count;
    // ... 没有 width 字段（编译期常量）
};

template<class Rule>
struct TetrisMapSnap {
    using row_t = typename Rule::row_t;
    row_t row[Rule::note][Rule::height];   // 用 N 替代硬编码 4
    // ... AI 桥接侧只用 row[0..3]
};

template<class Rule>
struct TetrisNode {
    using row_t = typename Rule::row_t;
    static constexpr size_t N = Rule::note;
    static constexpr size_t H = Rule::height;
    static constexpr size_t WK = max_wall_kick;

    TetrisBlockStatus status;
    row_t data[N];
    int32_t top[N];
    int32_t bottom[N];
    int32_t row, height, col, width;
    int32_t low;
    size_t index;
    size_t index_filtered;
    std::vector<TetrisNode const *> const *land_point;

    TetrisNode const *move_left;
    TetrisNode const *move_right;
    TetrisNode const *move_up;
    union {
        struct { TetrisNode const *self; TetrisNode const *move_down; };
        TetrisNode const *move_down_multi[H];
    };
    union { TetrisNode const *rotate_clockwise;        TetrisNode const *wall_kick_clockwise[WK]; };
    union { TetrisNode const *rotate_counterclockwise; TetrisNode const *wall_kick_counterclockwise[WK]; };
    union { TetrisNode const *rotate_opposite;         TetrisNode const *wall_kick_opposite[WK]; };
    // 注意：op 字段彻底删除
};

template<class Rule>
class TetrisContext { ... };

// ---------- AI 桥接层（关键解耦点）----------

// 第一版只支持 W ≤ 32 的桥接：把 row_t (uint8/16/32) 升提为 uint32_t
// 给 AI 看的视图：TetrisMapView { uint32_t row[40]; ... }
// 内部从 TetrisMap<Rule> 实时（或一次性 stage）转换
template<class Rule, std::enable_if_t<Rule::width <= 32>* = nullptr>
struct AiBridgeView {
    uint32_t row[Rule::height];
    // ... 其它 AI 期望字段（roof / count / height / width(=Rule::width)）
};
```

## 阶段拆分（每阶段单一 commit）

### 阶段 0：抽 RowT typedef（无行为变化）
- 在 `tetris_core.h` 顶部 `using row_t = uint32_t;`（**全局** typedef，不带模板）；
- 全代码 103 处 `uint32_t` 中**仅替换那些代表"盘面行"或"方块行"的位置**（不动 BitCount 等位算法本身的 uint32_t 字面量）；
- 不引入 RuleSpec / BoardSpec，仅做 typedef；
- 编译通过 + 行为等价。
- **Commit**: `Introduce row_t typedef across map/node/snap/op storage`

### 阶段 1：引入 RuleSpec 骨架 + WallKick 模板化
- 引入 `template<size_t W, size_t H, size_t N, class... Ops> struct RuleSpec`；
- 引入 `template<int8_t... XYs> struct WallKickList`；
- 把 10 个 `rule_*.cpp` 中的 `TetrisOpertion` 字面量改造为 `OpDesc<...>` 模板特化；踢墙表用 `WallKickList<...>`；
- 此阶段**仍保留 `TetrisRule::get_opertion()` 接口**，内部把 OpDesc 平展为 `std::map<{t,r}, TetrisOpertion>`，让 TetrisContext::build 不动；
- 优先把单一 rule（先选 SRS）改完跑通，再把其它 9 个规则跟进。
- **Commit**: `Express piece operations as compile-time OpDesc with templated wall kicks`

### 阶段 2：删 `TetrisNode::op` 字段，TetrisContext 持有静态 op 表
- 改 `TetrisContext` 持有 `op_table_[type_count][N]`（N=Rule::note 或编译期 4），用于 build 期查表；
- `m_tetris_rule_tools::create_node / rotate_template` 签名调整为接收 `TetrisContext<Rule> const*` + `(t, r)`；
- 指针网构建期所有读 `node.op.xxx` 改为 `context->op_table_[t_idx][r].xxx`；
- 收益落地：`sizeof(TetrisNode)` 缩 ~430B。
- **Commit**: `Remove TetrisNode op field and centralize op table on context`

### 阶段 3：TetrisContext / TetrisEngine 模板化为 `<Rule>`
- `template<class Rule> class TetrisContext`；
- `TetrisEngine<Rule, AI, Search, ...>` 的第一个参数语义不变，但 `Rule = RuleSpec<...>`；
- 删除 `TetrisRule::init / get_opertion / get_generate` 路径，全部走编译期；
- 检查 search_*.cpp 是否真的不需要改（绝大多数应该不需要）。
- **Commit**: `Parameterize TetrisContext over RuleSpec`

### 阶段 4：N=5 占位
- `OpDesc` 新增 L4 参数（默认 0），允许 N=5；
- 所有 N=4 的硬编码循环改为 `for (size_t i=0; i<Rule::note; ++i)`；
- 默认 N=4 主线行为不变。
- **Commit**: `Parameterize note matrix size up to 5`

### 阶段 5：AI 桥接层（让 AI 不改也能跑）
- `template<class Rule> struct AiBridgeView` 把 `TetrisMap<Rule>::row_t row[H]` 提升为 `uint32_t row[H]`；
- 现有 ai_*.cpp 接收 `AiBridgeView` 而非 `TetrisMap<Rule>`，`AiBridgeView` 提供 `width()/height()/roof()/count()` 等查询；
- 第一版只支持 W ≤ 32，编译期 `static_assert(Rule::width <= 32)` 保护；
- AI 不动一行代码就能跑。
- **Commit**: `Add AI bridge view for W<=32 boards`

### 阶段 6：W ≤ 16 / W ≤ 8 类型缩窄真实落地
- RuleSpec::row_t 真的根据 W 选择 uint8/16/32/64；
- TetrisMap / TetrisNode / TetrisMapSnap 真正用窄类型存储；
- AI 桥接层负责拼宽 → uint32_t 给 AI 看；
- 这一步对 build/search 路径**有性能影响**，需要小心 BitCount 等位算法的类型。
- **Commit**: `Select row storage width by RuleSpec::W`

## 阶段 0-1 可以**今天就做**

阶段 0 是纯 typedef 重命名，阶段 1 是 OpDesc/WallKickList 引入但保持运行时接口，**两个加起来 < 500 行 diff，零行为变化**。我建议合并为一个 commit 推一波，验证 SRS 跑通后再继续。

## 我现在不动代码，等你最后一句

请回答其中一个：

1. **「按阶段 0 开干，单独提交」** — 最小步进，每步等你确认。
2. **「合并阶段 0+1，推一个『骨架就位』commit」** — 一次性把 RuleSpec/OpDesc/WallKickList 骨架落地，但 SRS 之外的 9 个规则也要跟进（工作量是 1 的 ~4 倍）。
3. **「调整阶段顺序」** — 你有更优排法。

我倾向 2（一次把骨架立起来更顺手），但风险在于一个 commit 改 10 个 rule_*.cpp，回归面大。如果你也认 1，我立即开干阶段 0。
