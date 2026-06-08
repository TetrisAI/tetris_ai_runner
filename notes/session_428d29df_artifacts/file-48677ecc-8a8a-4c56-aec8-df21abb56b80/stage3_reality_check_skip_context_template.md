# Stage 3 Reality Check — 反驳 TetrisContext 模板化

> 关键字: stage3-reality-check skip-context-template tetris-node-template-priority

## TL;DR

**反对模板化 TetrisContext**。证据：context 不在热路径，30+ 文件改造对 nps 提升 ≈ 0。
真正高收益的是 `TetrisNode<N>` 和 `TetrisMap<W,H>` 模板化（Stage 4/6），它们才在热路径里。

## 证据：context 调用点全在 init 期

### AI 类（ai_zzz.cpp）
所有 `context->...` 调用全集中在各 AI 子类的 init 函数：
- `col_mask_ = context->row_mask() & ~1;`（缓存到成员）
- `row_mask_ = context->row_mask();`（缓存到成员）
- `full_count_ = context->width() * 24;`（缓存到成员）
- `map_danger_data_.resize(context->type_max());`（一次性）

搜索树内层 eval/get 用的是 AI 自己的成员（col_mask_ / row_mask_ / full_count_），**完全不碰 context**。

### Search 类（search_*.cpp）
- `Search::init` 期：`node_mark_.init(context->node_max())`（一次性）
- `Search::init` 期：`block_data_[...] = (1 << (context->width() - 2))`（一次性）
- 搜索热路径里只剩一处 `node->build_snap(map, context_, snap)`（透传），实际命中频次取决于 build_snap 内部。

### 结论
**Context 是个 init 期工厂**，模板化它只能让 init 期常量化——与 nps 无关。

## 真正的热路径依赖

| 优化目标 | 在哪个类上 | 是否需要 TetrisContext 模板化 |
|---------|-----------|----------------------------|
| `data[N]/top[N]/bottom[N]` 编译期化 | TetrisNode | 否（Node 自身模板化） |
| TetrisMap row 数组循环边界编译期化 | TetrisMap | 否（Map 自身模板化） |
| row_t 按 W 缩窄成 uint8/16 | TetrisMap, TetrisNode | 否（同上） |
| AI 内部 col_mask_/row_mask_ 编译期化 | AI 自己 | 否（如果 AI 模板化） |

**Context 模板化的所有"收益"，都能通过 Node/Map/AI 自己的模板化得到。**

## 不可避免的真正成本

但是 — Stage 4 把 `TetrisNode` 模板化为 `TetrisNode<N>` 时，AI/Search 持有的 `TetrisNode const *` 也必须跟着模板化。**30+ 文件的改造成本逃不掉**，只是改的对象从 "TetrisContext" 变成 "TetrisNode"。

那不如比较一下两条路：

| 维度 | 路 A：模板化 TetrisContext（原 Stage 3） | 路 B：跳过 Context，直接 TetrisNode 模板化 |
|-----|---------------------------------------|----------------------------------------|
| 改文件数 | ~30+ | ~30+ |
| 真实 nps 收益 | ≈ 0（init 期常量） | 显著（N/W/H 进入热路径） |
| 收益带来的复杂度 | 同等 | 同等 |
| 顺路解锁 Stage 4/6 | 否（还得再改一轮） | 是（一次到位） |

**路 B 完胜**。

## 推荐执行计划

### 跳过 Stage 3，直接 Stage 4 + Stage 6 合并

1. **TetrisNode 模板化**：`template<size_t N> struct TetrisNode { row_t data[N]; row_t top[N]; row_t bottom[N]; ... };`
2. **TetrisMap 模板化**：`template<size_t W, size_t H> struct TetrisMap { row_t row[H]; ... };`，并按 W 选 row_t 类型（uint8/16/32）
3. **TetrisContext 仍是普通类**，但 build 时返回的 storage 持有 `TetrisNode<N>` 的 type-erased 视图——
   或者更简单：**Context 也接受 `<size_t W, size_t H, size_t N>` 模板参数**（只透传给 storage），但 AI/Search 看到的是 `TetrisContext<W,H,N>` 而非 `TetrisContext<Rule>`，更通用，能跨 Rule 复用。
4. **AI / Search 透传**：`template<size_t W, size_t H, size_t N> class AI { ... };`，所有持有处一次到位。

### 编译期常量来源

- `Rule::rule_spec::width / height / note` 仍是 single source of truth
- 在 TetrisEngine 内部展开成 `<W,H,N>` 透传给 Context/Node/Map/AI/Search
- AI/Search 不依赖具体 Rule，只依赖几何参数 (W,H,N)

## 待决策

请确认：
1. 同意跳过 Stage 3 TetrisContext 模板化吗？
2. 直接合并执行 Stage 4 + Stage 6（TetrisNode/TetrisMap/AI/Search 接受 `<size_t W, size_t H, size_t N>` 模板）吗？
3. 还是保留你最初的 Stage 3 路径，承担"30 文件 + 0 收益"的代价以保持文档定义的阶段顺序？

我的强烈推荐是 (1) + (2)。
