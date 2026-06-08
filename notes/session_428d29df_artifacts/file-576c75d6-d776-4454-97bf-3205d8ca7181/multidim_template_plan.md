# 多维全面模板化（W/H/Note/Rule）边界澄清

> 关键字: tetris-template multidimension width-template height-template note-matrix rule

## 你最新的目标（4 条）

1. **宽度模板化**：根据 W 自动选 `uint8/16/32/64`
2. **高度模板化**：只维护必要的高度
3. **Note 矩阵模板化**：现状 4×4，未来支持 5×5
4. **规则模板化**：不细说

## 我的判断（直说不绕）

这 4 条同时落地，**不是"再加几个 trait"**，而是把核心数据结构 (`TetrisMap` / `TetrisMapSnap` / `TetrisNode` / `TetrisContext` / `TetrisEngine`) 全部参数化。我先把代价说清楚，再分阶段。

## 1. 现状盘点（影响范围量化）

| 现状 | 位置 | 涉及类型 |
|---|---|---|
| `uint32_t row[max_height]` | `tetris_core.h:37` | `TetrisMap` |
| `uint32_t row[4][max_height]` | `tetris_core.h:150` | `TetrisMapSnap` |
| `uint32_t data[4]` / `top[4]` / `bottom[4]` | `tetris_core.h:235-239` | `TetrisNode` |
| `move_down_multi[max_height]` | `tetris_core.h:268` | `TetrisNode` |
| `uint32_t line1..line4` | `create_node` 模板参数 | `tetris_rule_tools` |
| `uint32_t row` | `ai_*.cpp` 多处直接读 `map.row[y]` | 各 AI |

**`uint32_t` 字面量在 src/*.{h,cpp} 出现 103 处**，`max_height` 出现 13 处，`data[4]/top[4]/bottom[4]/line1..4` 出现 22 处。**全部要替换**。

## 2. 4 条目标分别评估

### 2.1 宽度模板化 → 根据 W 选 `uint8/16/32/64`

**做法**：
```cpp
template<size_t W>
using row_t = std::conditional_t<W <= 8,  uint8_t,
              std::conditional_t<W <= 16, uint16_t,
              std::conditional_t<W <= 32, uint32_t, uint64_t>>>;
```

**收益**：
- W ≤ 8 时 4 倍 cache 密度（一行 1 字节，TetrisMap 整盘 ≤ 64 字节进 L1）
- W ≤ 16 时 2 倍 cache 密度
- W = 10（标准 SRS）→ 仍是 uint16_t，行宽减半 → 指针网/搜索全套受益

**问题点**：
- `BitCount(row)` / shift / mask 等所有位操作都要按 row_t 类型走，AI 评估器内部直接读 `uint32_t` 的位置（src/ai_ax.cpp 等）必须**全部跟着泛化**，否则只在标准 W=10 上类型不匹配 → AI 评估器集体改造
- `wall_kick_clockwise[16]`、踢墙计算、`ai_misaka.cpp` 中 `unsigned rowdata` 这类裸类型也要改
- W>64 怎么办？回到 `uint64_t[ceil(W/64)]` 数组形式，**类型签名彻底分两套**

**实际成本**：6 个 search_*.cpp + 5+ 个 ai_*.cpp + 10 个 rule_*.cpp，**全部模板化**。任何"只看 `uint32_t` 写硬编码"的地方都要改成 `RowT`。

### 2.2 高度模板化 → 只维护必要的高度

**做法**：`template<size_t H> ... row[H]; move_down_multi[H];`

**问题点**：
- 现在 `max_height = 40` 是兜底（SRS 里实际 H=20，多出来的 20 行是 buffer + spawn area），如果 H 真的精确化，**spawn 区/碰撞区/隐藏行**逻辑要全部重新核算
- AI 评估器很多写 `for (int y = 0; y < max_height; ++y)`，要跟着改
- `move_down_multi[H]` 现在是 union 的最大成员，决定整个 union 大小 → H 缩小后内存收益**实质**

**实际成本**：search_* / ai_* / rule_* 中所有 `max_height` 使用点全改，约 13 处直接命中 + 间接更多。

### 2.3 Note 矩阵模板化 → 4×4 → 5×5

**做法**：`template<size_t N> ... data[N]; top[N]; bottom[N];` 以及 `create_node<T,X,Y,R, line1..lineN>`。

**问题点**：
- `create_node` 当前是 `template<line1, line2, line3, line4>`，**变长模板参数**改造比较自然：`template<uint32_t... Lines>`，或者 `template<size_t N, std::array<uint32_t, N>>`
- 5×5 需要新规则吗？如果未来要支持 Pentomino（5 格方块），那 Note 必然 ≥5；如果只是更大方块（如 PuyoPuyo 风格），可能 ≥6
- `WallKickOpertion::data[max_wall_kick]` 不受影响
- **`block.data[16]` / `node.data[N]` / `node.top[N]`** 这些维度变了，所有索引循环 `for (int i=0; i<4; ++i)` 都要改成 `for (int i=0; i<N; ++i)`

**实际成本**：相对独立，但触及 src/tetris_core.cpp 中所有 4×4 硬编码循环，**约 30+ 处**。

### 2.4 规则模板化

按上一轮已经达成共识的方案做（OpDesc + RuleSpec）。

## 3. 4 条同时做的"乘法效应"

`TetrisEngine` 模板参数：现在是 `<Rule, AI, Search, Status, TaskRunner>`（5 个），改造后需要变成：

```cpp
template<size_t W, size_t H, size_t N, class Rule, class AI, class Search, ...>
class TetrisEngine;
```

**或者**用一个 `BoardSpec` 把 W/H/N 打包：

```cpp
template<size_t W, size_t H, size_t N>
struct BoardSpec {
    static constexpr size_t width = W;
    static constexpr size_t height = H;
    static constexpr size_t note_size = N;
    using row_t = ...;
};

template<class Board, class Rule, class AI, class Search, ...>
class TetrisEngine;
```

**强烈建议用 BoardSpec 打包**，不然顶层模板参数列表会膨胀到难以阅读。

## 4. 必须先达成的共识（动手前要回答）

### Q1：W ≤ 64 是否够用？还是要支持 W>64（用数组）？
- 现状 SRS=10、Botris=10、TOJ=10、QQ 不确定；
- 如果 W>64 要支持，**类型签名必须设计成数组形态**（`std::array<uint64_t, ceil(W/64)>`），而不是单一标量；
- 我建议：**第一版只支持 W ≤ 64 的标量形态**，未来需要 W>64 再做数组扩展。

### Q2：H 的"必要高度"如何判定？
- 是否每种规则配套 H 常量？(SRS → H=24, ASRS → H=??, ...)
- spawn 区/隐藏行/buffer 是包含在 H 内还是另算？
- **建议**：H 直接给 RuleSpec 一个静态成员，BoardSpec 默认从 RuleSpec 取。

### Q3：Note 5×5 现在做还是占位？
- "未来支持"和"现在就上"代价完全不同。占位（仅模板参数化，主线仍 N=4）成本低；现在就上 5×5 还要补全 5×5 的 op 数据。
- **建议**：本期只做模板参数化，N=4 走通，5×5 留作后续扩展。

### Q4：AI 评估器同步泛化的接受范围？
- `ai_ax.cpp` 等大量直接读 `uint32_t row[]`，类型化后这些位置统统要改；
- 如果担心 AI 大改，可以让 `BoardSpec` 提供一个 `read_row_as_uint64(y) -> uint64_t`，AI 内部继续按 uint64 读，**牺牲一点 W ≤ 16 的极致性能**换 AI 改动量；
- **建议**：第一版接受这个"统一 uint64 视图"，AI 完全不动；后续再针对每个 AI 做位类型特化。

## 5. 我建议的执行路径（按代价由低到高）

> 一阶段一 commit，遵守"单一提交"原则。

### 阶段 0（先决条件）：抽出 BoardSpec 骨架
- 引入 `template<size_t W, size_t H, size_t N> struct BoardSpec`，但所有现有代码仍走 `BoardSpec<32, 40, 4>` 实例（与现状等价）
- 抽 `using row_t = uint32_t` typedef，让现存所有 `uint32_t` 改成 `RowT`，**但实际类型不变**
- 测试目标：编译通过，运行无差异
- **Commit**: `Introduce BoardSpec typedef without changing instantiation`

### 阶段 1：规则模板化（之前共识的内容）
- 删 `TetrisNode::op` 字段
- OpDesc + RuleSpec
- 编译期注册节点
- **Commit**: `Migrate per-rule operations to compile-time RuleSpec`

### 阶段 2：Note 矩阵参数化（仍 N=4）
- `data[N] / top[N] / bottom[N] / lines[N]` 全部参数化，但默认 N=4
- 5×5 留接口不实现
- **Commit**: `Parameterize note matrix size (N=4 default)`

### 阶段 3：高度模板化
- `H` 引入到 BoardSpec，AI 内部 `max_height` 引用 BoardSpec::height
- **Commit**: `Parameterize board height via BoardSpec`

### 阶段 4：宽度类型选择（性能收益最大但风险最高）
- 根据 W 选择 `uint8/16/32/64`
- AI 评估器同步泛化（或采用 Q4 的"统一 uint64 视图"过渡方案）
- **Commit**: `Select row storage type based on width`

## 6. 我的强烈建议

1. **不要一次性提一个巨型 commit**，按上面 5 个阶段单提一周一个
2. **阶段 0 一定要先做**——这是后续所有改造的"底盘"，否则每个阶段都要重新摸一遍裸 `uint32_t`
3. **阶段 4 留到最后**——因为它最容易把 AI 评估器拖下水
4. **Q1-Q4 必须先回答**

## 我现在不动代码，等你回答 Q1-Q4

请回答：
- Q1：W ≤ 64 标量形态够用？还是需要 W>64 数组形态？
- Q2：H 由 RuleSpec 提供？
- Q3：N=5 现在就上还是仅占位？
- Q4：AI 内部接受"统一 uint64 视图"过渡方案吗？

或者你直接说"按你的阶段 0-4 推进，先做阶段 0"，我开干。
