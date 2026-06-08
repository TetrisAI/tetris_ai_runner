# Stage 3 终极决策 — TetrisContext 永不模板化

> 关键字: stage3-final-cobra-target tetris-context-deprecation movegen-rewrite

## 决策依据（用户拍板，2026-05-26）

1. 一切设计为性能让步
2. **最终搜索目标是 cobra 思路**
3. 矩阵大小（N）可变时，attach/build_snap 不再是 4 轮，间接寻址成本被 N 倍放大

## TetrisContext 在两条路下都没有"模板化"的位置

### 现状（commit 7077937）
```
class TetrisContext {
    // 运行期工厂：建立指针网、查表
    std::deque<TetrisNode> node_storage_;
    chash_map<...> node_index_;
    std::vector<TetrisNodeBlockLocate> node_block_;
    std::map<std::pair<char,unsigned char>, TetrisOpertion> opertion_;
    std::map<char, ...> generate_;
    int32_t width_, height_;
    row_t row_mask_;
    size_t type_max_;
    ...
};
```

**TetrisContext 是"指针网建图工厂"，不是"运行期数据访问器"**。AI/Search 在 init 期一次性把 row_mask_/width_/type_max_ 缓存到自己成员，搜索热路径几乎不再回头查 context。

### 路 A — 迁移到 cobra（推荐）

cobra 概念映射：
| 当前 | cobra |
|-----|------|
| `TetrisMap` | `template<int H> Board`（SIMD 向量）|
| `TetrisNode` 指针网 | **不存在**，`piece_table<P,R>()` 现场计算 |
| `TetrisContext` | **不存在**，规则是 template/concept |
| `node_storage_/node_index_/node_block_/place_cache_` | **不存在** |
| `Search::search` | `MoveList<Rules, P, Board<H>>`（SIMD 位运算 movegen）|
| `Rule::rule_spec` | `concept Ruleset { KICKS, SPINS, SPAWN_Y, ENABLE_180 }` |

cobra 性能（M2 MacBook Pro）：
```
Depth: 7 Nodes: 2.65B Time: 5.9s NPS: 445M
```

迁移路径：
1. `TetrisMap` → `Board<int W, int H>`，SIMD 向量存储
2. `Search` 重写为 cobra 风格 `MoveList<Rules, P, Board<H>>` 纯位运算 movegen，抛弃 build_snap/check/snap 指针网搜索
3. TetrisNode 自然萎缩（没人持有）
4. TetrisContext 自然萎缩（没人查 get_block/generate）
5. AI/eval 接受 `Board<H>` 而非 TetrisNode/TetrisContext

**TetrisContext 在路 A 下被自然消亡，不需要"模板化"它。**

### 路 B — 保持现有架构，Stage 3+ 全部冻结

- 接受 commit 7077937 现状
- 不再投入"模板化 TetrisContext"
- 性能优化转向 BMI/SIMD 局部 hotspot
- cobra 作为独立项目立项

**TetrisContext 在路 B 下保持普通类，永不模板化。**

## 矩阵 N 可变的真实成本承担者

如果仅在当前架构下做"N 可变"扩展（不走 cobra），需要改造的是：
- **TetrisNode**：`data[N] / top[N] / bottom[N]` 模板化（不是 TetrisContext）
- **attach / build_snap / check**：内部 4 轮循环改成 `if constexpr` 展开的 N 轮（在 TetrisNode 上模板化）
- **node_block_ 存储**：从 `std::vector<TetrisNodeBlockLocate>` 改成 `std::array<..., type_max * N>`（type_max 来自 Rule，已是编译期常量；不需要 TetrisContext 模板化）

**这些改造点都不在 TetrisContext 上。**

## 结论

无论走 cobra 路 A 还是冻结路 B，**TetrisContext 都不应该模板化**：
- 路 A：TetrisContext 整体消亡，模板化它的 30 文件改造全部沉没
- 路 B：模板化它仅有 <1% 的 build_snap 内 get_block 微优化，不值

唯一仍可能在当前架构下做的"模板化"是：
- TetrisNode `<size_t N>`（与 N 可变需求绑定）
- TetrisMap `<size_t W, size_t H>`（与 row_t 缩窄绑定）
- AI/Search `<size_t W, size_t H>`（让 row_mask_/col_mask_ 进入编译期常量）

**这些都跳过 TetrisContext，独立进行。**

## 待决策

请选：
1. **路 A：启动 cobra 移植**（重写 Search/Map/Node，工作量大但 nps 数量级提升）
2. **路 B：冻结模板化方向**，转向 hotspot 调优
3. **混合路径**：Stage 4-6 在当前架构上做（TetrisNode/TetrisMap/AI/Search 模板化），同时另起 cobra 项目；当 cobra 成熟后切换

我个人推荐 (1) 或 (3)。无论哪个，TetrisContext 都不动。
