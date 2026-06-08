# AI 位板化改造设计方案（完整版）
_记录时间：2026-06-02，基于充分代码调研后撰写_

---

## 一、当前 AI 层依赖的旧接口全盘点

### 1.1 棋盘数据（TetrisMap）

| 接口 | 语义注意 | 代表用法 |
|---|---|---|
| `map.row[y]` | **1=empty，0=occupied**（旧语义，与位板相反）| 行遍历、bit XOR/AND 操作 |
| `map.top[x]` | 每列已填格顶高（0-based+1，y方向向上） | find low/attack_x，tilt 计算 |
| `map.roof` | 最大列顶高（= max over all top[]） | 行遍历上界 |
| `map.height` | 棋盘总行数（编译期固定） | 危险检测、Dead Zone 判断 |
| `map.width` | 棋盘总列数（编译期固定） | 遍历边界 |
| `map.count` | 已填格数（the_ai_games_old 里用于 building 判断） | 计算棋盘密度 |
| `map.full(x, y)` | 返回 `((row[y]>>x)&1)==0`，即 empty=false | ColTrans 左右边界格判断 |
| `context_->full()` | 满行值 `(1<<W)-1`（即 row_mask）| `map.row[y] == context_->full()` → 找第一个满行 |

### 1.2 落点节点（TetrisNode / TetrisNodeEx）

| 接口 | 用途 | 出现位置 |
|---|---|---|
| `node->row + node->height` | 落点 bounding box 顶行 y | ai_tag, ai_zzz::qq::Attack, eval 里算 LandHeight / node_top |
| `node->status.x` | 落点 anchor x（bounding box 左列） | ai_zzz::qq::Attack: Middle = `fabs((x+1)*2 - map.width)` |
| `node->row, node->height` | bbox 底行+高度 | ai_tag_old, the_ai_games, qq::Attack, misaka 等所有 AI |
| `node.type` / `node.is_check` / `is_ready` / `is_last_rotate` | T-Spin 判定 | ai_tag::the_ai_games, ai_zzz::TOJ_PC 等 |

这些字段均来自 `TetrisNodeWithTSpinType`（已有的 `TetrisNodeEx` wrapper）和 `TetrisNode` 本身的 bounding box 字段。

### 1.3 运行时上下文（TetrisContext）

| 接口 | 用途 |
|---|---|
| `context_->type_max()` | 片种数（`map_danger_data_` 的 resize 参数） |
| `context_->generate(i)` | 生成第 i 种 piece 的 TetrisNode（用于预算危险 mask） |
| `context_->width()` / `->height()` | 棋盘尺寸（仅 init() 里用） |
| `context_->row_mask()` | 行满 mask → `row_mask_` / `col_mask_` |
| `context_->full()` | 满行值（= row_mask，在 eval 里用来找第一个满行） |

---

## 二、核心难点分析

### 2.1 `map.row[]` 的 bit 语义翻转

- **旧**：`row[y]` 中 1=empty，0=occupied；`~map.row[y] & row_mask` = 该行的占据 bits
- **新位板**：`m.row(y)` 中 1=occupied；`m.row(y)` = 该行的占据 bits（直接就是旧的 `~map.row[y] & row_mask`）

规律非常清晰：所有 AI 里的 `~map.row[y] & row_mask_` → 新位板 `m.row(y)`，所有 `map.row[y]` → `~m.row(y) & full_row`。

### 2.2 `map.top[x]` — 位板里不存在，需构造

位板 `Map<W,H>` 没有预计算的 `top[]` 数组。需要一个内联 helper：

```cpp
// 从位板逐列找最高占据格（bit-scan 向高位扫描）
// O(H/64) 对 20 行 = 常数
template<int W, int H>
inline int col_top(Map<W,H> const &m, int x) {
    for (int y = H - 1; y >= 0; --y)
        if ((m.row(y) >> x) & 1) return y + 1;
    return 0;
}
```

但 AI 里 `map.top[]` 被大量使用（find low_x，tilt 计算），在热路径里每格 O(H) 扫描会有代价。更好的方式：**eval 开头一次性填 `int top[W]`**：

```cpp
// 进入 eval 时，先构造一次 top[] 数组
int top[MapT::W]{};
for (int x = 0; x < MapT::W; ++x) top[x] = col_top(m, x);
```

这样后续所有 `map.top[x]` 替换为 `top[x]`，逻辑不变，性能可接受（一次 W*H 扫描）。

### 2.3 `map.roof` — 可从 top[] 推导

```cpp
int roof = *std::max_element(top, top + MapT::W);
```

### 2.4 `map.count` — 位板逐行 popcount

```cpp
int count = 0;
for (int y = 0; y < roof; ++y) count += std::popcount(m.row(y));
```

### 2.5 `context_->full()`（row_mask）— 编译期常量

`MapT::full_row` 或 `(row_t(1) << W) - 1`（编译期 constexpr）。

### 2.6 `map_in_danger_` 里的 `context_->type_max()` + `context_->generate(i)`

**这是整个改造最重要的一环**，见下面第三节。

---

## 三、核心设计：编译期 `DangerMask<RuleSpec, MapT>` 取代 map_danger_data_

### 3.1 旧 init() 做了什么

```cpp
// 每个 AI 的 init() 共同逻辑：
map_danger_data_.resize(context->type_max());
for (size_t i = 0; i < context->type_max(); ++i) {
    TetrisMap map(context->width(), context->height());
    TetrisNode const *node = context->generate(i);
    node->attach(context, map);   // 或 node->move_down->attach(...)
    for (int y = 0; y < 4; ++y)
        map_danger_data_[i].data[y] = ~map.row[map.height - 4 + y] & full;
    for (int y = 0; y < 3; ++y)  // 累积 OR：data[y+1] |= data[y]
        map_danger_data_[i].data[y + 1] |= map_danger_data_[i].data[y];
}
```

这产生的是：第 i 种 piece 在棋盘顶部放下后，顶部 4 行的**累积占据 mask**（方向：data[0] = 第 H-4 行，data[3] = 第 H-1 行，且每行 OR 了更低行的 mask）。

### 3.2 编译期可以完整计算出这个 mask

piece 的形状在 `RuleSpec`（或 `OpDesc`）里是编译期已知的。piece 落在棋盘顶部时的 4 行占据位置是纯几何计算，与运行时无关。

**设计：**

```cpp
// ai_bitboard_utils.h
template<class RuleSpec, class MapT>
struct DangerMask {
    using row_t = typename MapT::row_t;
    static constexpr int N = RuleSpec::kPieceCount;
    struct Entry { row_t data[4]; };  // cumulative from top row downward

    static constexpr std::array<Entry, N> table = /* constexpr init */;
};
```

`map_in_danger_()` 改写为：

```cpp
template<class RuleSpec, class MapT>
static std::size_t map_in_danger(MapT const &m) {
    using D = DangerMask<RuleSpec, MapT>;
    constexpr int H = MapT::H;
    std::size_t danger = 0;
    for (int i = 0; i < D::N; ++i) {
        auto &e = D::table[i];
        // 位板语义 1=occupied；检查顶4行是否被 piece 的危险 mask 覆盖
        if ((m.row(H-4) & e.data[0]) | (m.row(H-3) & e.data[1]) |
            (m.row(H-2) & e.data[2]) | (m.row(H-1) & e.data[3]))
            ++danger;
    }
    return danger;
}
```

- `context_` 成员字段**完全消失**
- `init()` 只保留 `config_ = config`
- 不再需要 `map_danger_data_` vector
- 不再需要 `col_mask_` / `row_mask_` 成员（变成 `MapT::full_row` 和 `MapT::full_row & ~row_t(1)`）

### 3.3 `DangerMask::table` 的 constexpr 初始化思路

piece shape 数据来源：`tetris_shape.h` 中的 `piece_line<Spec, T, R, y>`（编译期行 mask）。
- 对每种 piece type，遍历所有 rotation，取 spawn 状态（rotation=0），
- 计算 piece 位于棋盘顶部的行 mask（相对 y=H-4 到 y=H-1），
- 做 4 行的 bitwise OR 累积，写入 `Entry::data[0..3]`。

`constexpr` lambda + `std::array` 折叠初始化，C++17 完全支持。

---

## 四、AI 类模板参数化方案（Q2：不膨胀，跟随 Type 数量）

### 4.1 模板参数是 RuleSpec

```cpp
// 旧
class the_ai_games {
    m_tetris::TetrisContext const *context_;
    std::vector<MapInDangerData> map_danger_data_;
    int col_mask_, row_mask_;
    ...
};

// 新
template<class RuleSpec>
class the_ai_games {
    using MapT = typename RuleSpec::MapT;  // Map<W, H>
    // 无 context_ 成员
    // 无 map_danger_data_ vector
    // 无 col_mask_ / row_mask_
    Config const *config_;
    ...
    Result eval(TetrisNodeEx const &node, MapT const &map, MapT const &src_map, size_t clear) const;
};
```

模板只按 RuleSpec 展开（每个平台一个实例），不会随棋盘尺寸乘积膨胀。

### 4.2 init() 重构

```cpp
// 旧：void init(TetrisContext const *context, Config const *config)
// 新：void init(Config const *config)    ← context 参数消失
void init(Config const *config) {
    config_ = config;
    // 不再有任何 context 相关操作
}
```

`DangerMask<RuleSpec, MapT>::table` 是编译期静态数据，无需运行时初始化。

### 4.3 eval / get 签名变更

```cpp
// 旧
Result eval(TetrisNode const *node, TetrisMap const &map, TetrisMap const &src_map, size_t clear) const;

// 新
Result eval(TetrisNodeEx const &node, MapT const &map, MapT const &src_map, size_t clear) const;
// （对已有 TetrisNodeEx 参数的 AI，签名不变，只换 TetrisMap → MapT）
```

---

## 五、`node->row + node->height`（落点高度）问题（Q4）

### 5.1 当前情况

`node->row + node->height` 是 bounding box 顶 y，出现在 `eval()` 里计算 `LandHeight` / `node_top`。

在位板体系里，落点由 `LandPoint`（search 产物）携带。`TetrisNodeEx` 里的 `node` 字段仍是 `TetrisNode const*`，直接有 `->row + ->height`，**现阶段可以直接保留读取**。

### 5.2 若将来彻底移除 TetrisNode 指针

设计一个**编译期 bbox 表**：

```cpp
template<class RuleSpec>
struct PieceBBox {
    struct Entry { int8_t height, width, y_min, x_min; };
    // [piece_index][rotation]
    static constexpr auto table = /* from tetris_shape piece_line<> */;
};
```

`LandPoint` 携带 `(piece_index, rotation, x, y)`，则：
- `node->row + node->height` → `lp.y + PieceBBox<RuleSpec>::table[lp.piece][lp.r].height`
- `node->status.x` → `lp.x`

**现阶段（Phase 1）**：`TetrisNodeEx` 仍含 `TetrisNode const*`，保留直接读取，不引入 PieceBBox。  
**未来（Phase 2）**：当 TetrisNode 指针被删除时，PieceBBox 表自动补上。

---

## 六、`col_mask_` / `row_mask_` 成员替换

| 成员 | 用途 | 新写法 |
|---|---|---|
| `row_mask_` | `(1<<W)-1`，用于 AND 截断 | `MapT::full_row`（编译期 constexpr） |
| `col_mask_` | `row_mask_ & ~1`，用于 ColTrans XOR | `MapT::full_row & ~row_t(1)` |

---

## 七、`map.full(x, y)` 的语义

旧：`full()` 返回 **true = 有方块**（但实现是 `((row[y]>>x)&1)==0`，即 `row` bit=0=有方块）。  
新：位板 `m.get(x, y)` 或 `(m.row(y) >> x) & 1` = 1 有方块。

所有 `map.full(0, y)` → `(m.row(y) & 1) != 0`；`!map.full(0, y)` → `(m.row(y) & 1) == 0`。

---

## 八、分步改造计划（从简单到复杂）

| 阶段 | 工作 | AI 类 |
|---|---|---|
| **基础设施** | 新建 `ai_bitboard_utils.h`：`DangerMask<>`, `col_top()`, `make_top_array()`, `col_mask_v<>`, `row_mask_v<>` | — |
| **Step 1** | `ai_zzz::Dig` 模板化（最简单，eval 参数少） | Dig |
| **Step 2** | `ai_zzz::qq::Attack` 模板化 | qq::Attack |
| **Step 3** | `ai_tag::the_ai_games_old` 模板化 | the_ai_games_old |
| **Step 4** | `ai_tag::the_ai_games` 模板化（有 `map_for_tspin_`，稍复杂） | the_ai_games |
| **Step 5** | `ai_tag::the_ai_games_enemy` 模板化（最简单，无 TetrisMap eval 参数） | the_ai_games_enemy |
| **Step 6** | `ai_zzz::TOJ_v08`, `ai_zzz::C2`, `ai_zzz::Botris`, `ai_zzz::Botris_PC` 模板化 | — |
| **Step 7** | `ai_zzz::TOJ`, `ai_zzz::TOJ_PC` 模板化（最复杂，Status 里有 map 指针） | TOJ, TOJ_PC |
| **Step 8** | `ai_misaka::misaka` 模板化（Result 里存 TetrisNode*/TetrisMap* 原始指针，需单独讨论） | misaka |

---

## 九、待确认问题（向用户提问）

**Q-A**：`ai_zzz::TOJ_PC::Result` 和 `ai_zzz::TOJ::Status` 里存了 `TetrisMap const *save_map`，位板化后该字段改为 `MapT const *save_map`，意味着 Result/Status 也变成依赖 MapT 的类型。你倾向于：
1. `Result` / `Status` 也变成模板类内类型（跟随 AI 类一起模板化）？
2. 保留原始指针，外层转换？

**Q-B**：`ai_misaka::misaka::Result` 存了 `TetrisNode const *node` 和 `TetrisMap const *map`——位板化后 `TetrisNode` 指针继续保留（Phase 1 策略），但 `TetrisMap const *map` 需要换成 `MapT const *map`，同样影响 `Result` 类型。处理方式同 Q-A？

**Q-C**：`the_ai_games_old::get()` 里用了 `context_->type_max()` 和 `context_->height()` 计算危险棋盘判断（`BoardDeadZone`），以及 `context_->width()` 计算 building 阈值。这些用法改造后均变为 `MapT::W`/`MapT::H`/`DangerMask::N`，**但 `get()` 的签名目前没有 MapT 参数**，需要通过模板类成员方法访问。是否接受把这部分逻辑也移进模板类方法里（不改变外部调用签名）？
