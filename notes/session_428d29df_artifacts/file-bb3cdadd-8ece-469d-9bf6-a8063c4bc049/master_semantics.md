# master ↔ 新 MoveGen 语义对齐调研（flip-bits）

## 1. y 语义差异
- master `TetrisNode::status.y`（`src/tetris_core.cpp::create_node` 周边）：
  - 来自 piece 在 4x4 矩阵里的逻辑顶行；spawn 时 `status.y = h - Y_spawn - 1`，`h = 40`。
  - `TetrisMap` 的 y 方向**自底向上**，`row[0]` 是底层；位语义是 `1 = 空 / 0 = 已占（含墙外）`。
  - `node.row` 是 piece bbox 在 Map 中的物理起始行 —— `create_node` 通过跳过 4x4 矩阵底部空行收缩 bbox。
- 新 `Piece::y`（`src/tetris_shape.h::detail::cells_impl`，行 ~198–238）：
  - 先按 4x4 矩阵把 cells 收集起来，再用 `cx_min/cy_min` 归一化到 bbox-原点坐标系。`pivot = (1 - cx_min, H-2 - cy_min)`。
  - 因此 `Piece::y` 表示 bbox 底行的物理坐标，已丢掉原 4x4 底部空行的偏移信息。
- 二者描述同一个几何落点：换算偏移就是该 piece+rotation 在 4x4 矩阵里的「首个非空行索引」（`cy_min` 等价物）。仅依赖 piece + rotation，不依赖落点 (x, y)。

## 2. 对称 piece 的 rotation 集合差异
- master `search_tspin::Search`（`src/search_tspin.cpp`）：
  - BFS 用 `IndexFilter` 去重，键里含 `node.row`；对 Z/S/I：r=0 与 r=2 几何相同但 `status.y` / `node.row` 不同，因此**两条都会进入输出**。
  - 即 master 不做对称剪枝；4 个 rotation 都会被各自 BFS 扩展。
- 新 `MoveGen`（`src/tetris_movegen.h::generate`，行 ~127–290）：
  - 从代码看 `R_count = shape::rotation_count<Spec, T>` 是来自 `RuleSpec::ops` 中 `T` 的 `OpDesc` 数量。如果 `rule_srs` 在 RuleSpec 里就只为 Z/S/I 列了 2 个 OpDesc，新 MoveGen 自然只输出 r=0/r=1。
  - 还需直接确认 `src/tetris_rule_spec.h` + `src/rule_srs.cpp/h` 里 Z/S/I 的 ops 数量。
  - 子代理提到的 cobra `canonical_size` 剪枝是 cobra 仓库（`cobra-movegen/src/gen.hpp`）的逻辑，并非已经存在于新 MoveGen 内。

## 3. oracle_diff 现状
- 跑 `./build/oracle_diff` 14 个 FAIL，全部源自上述 (1)+(2)。
- 我们已修 `copy_to_oracle_map` 的位极性，但坐标 / 旋转语义未对齐，未 commit。

## 偏移公式（dump 反推 + 验证）
H = RuleSpec OpLines size = 4。对每个 (piece, r)：
- master_x = new_x − cx_min[r]
- master_y = new_y + (H − 1 − cy_min[r])

逐 piece 校验：

| piece-r | cx_min | cy_min | x_off (-cx_min) | y_off (H-1-cy_min) | dump master_y | dump new_y |
|---|---|---|---|---|---|---|
| O r=0 | 1 | 2 | -1 | 1 | 1 | 0 |
| I r=0 | 0 | 2 | 0  | 1 | 1 | 0 |
| I r=3 | 1 | 0 | -1 | 3 | 3 | 0 |
| Z r=0 | 0 | 2 | 0  | 1 | 1 | 0 |
| Z r=3 | 0 | 1 | 0  | 2 | 2 | 0 |

cx_min / cy_min 已经在 `tetris_shape.h::detail::cells_impl` 编译期算出来；把 `origin_offset = {cx_min, H-1-cy_min}` 加到 `CellList` 即可。

## ZSI 等几何对称 piece 的额外去重
oracle 与 new 在 ZSI 上的差异不仅是偏移，还有去重：
- Z empty：master 仅输出 r=0 (8 条) + r=3 (9 条)。new 输出 r=0..3 全部 (34 条)。
- I empty：master 仅 r=0 (7 条) + r=3 (10 条)。new 全 4 r (34 条)。
- S 同 Z。

### master 的去重机制（关键证据已抓全）

#### 两套独立标记

`tetris_core.h::TetrisNodeMarkTemplate<Filtered>` 模板化出两个 NodeMark 类型，索引完全不同：

| 名称 | 索引 | 用途 |
|---|---|---|
| `node_mark_` | `key->index` | BFS 转移图、记录 piece 单步前驱（last_rotate 标签） |
| `node_mark_filtered_` | `key->index_filtered` | 落点表去重 |

`tetris_core.cpp:296-321` 中 `IndexFilter` 构造时复制 `node.data[0..3] + node.row`（5 个 uint32），按 memcmp 排序去重。也就是说 **`index_filtered` 的等价类 = piece 实际占据的 4 行 cells bitmask + Map 起始 row**。Z r=0 与 Z r=2 / I r=0 与 I r=2 几何完全相同 → `index_filtered` 相同 → 同一类。

但 `index` 是按 `(t, r, x, y)` 独立分配的，r=0 与 r=2 是**不同 index**。

#### BFS 用未去重的 `index`，**完全保留** last_rotate

`search_tspin.cpp::search_t` (line 976) 与 `search` (line 456) 的 BFS 主循环：
- 平移 / drop：`node_mark_.set(neighbor, node, ' ')` —— 标签 ' ' 表示"上一步不是旋转"。
- 旋转 (x/z/c)：`node_mark_.cover_if(neighbor, node, ' ', 'x'/'z'/'c')` —— `cover_if` 仅当当前标签为 ' ' 时才覆盖为旋转标签。
  - 即"已经被旋转标记的节点不会被后续平移降级"，"已经被平移到达的节点会被后续旋转可达升级"。
- 因为 `node_mark_` 用 `index`，r=0 与 r=2 各自独立维护标签，**对称等价的两个节点之间不会互相影响 last 标签**。

#### 落点输出阶段才用 `index_filtered` 去重

```cpp
if (!node->move_down || !node->move_down->check(snap))
{
    if (node_mark_filtered_.mark(node))      // 仅在 cells 等价类首次到达时输出
        node_incomplete_.push_back(node);
}
```
- 谁先到达谁就被收进 `land_point_cache_`，几何等价的另一个 r 直接被丢弃；
- 但保留下来的这一条 entry 上的 last 标签来自 `node_mark_.get(node)`（line 1085-1089）：
  - `is_last_rotate = (last_op != ' ') || (last == nullptr && depth == 0 && config_->last_rotate)`
  - 这个标签来自该 entry **自己**的那个 (r, x, y) 节点，而不是被丢弃的对称等价节点。
  - 因此"上一步是旋转"信息不会因为去重丢失。

#### 路径回放（find_path）也用 `index_filtered`

`search_tspin.cpp` 的 `make_path` 系列函数里，回放目标节点的判定全是
`node->index_filtered == index` —— 只要"被路径回放找到的节点"和"目标 land_point"的 cells 等价类相同就算到达。
- 因此即使 land_point 上记录的代表 r 是 r=0，但实际 BFS 路径上是经过 r=2 + 旋转 → r=0 也行；
- 这正是用户说的「最终路径搜索也不会丢失」：去重发生在 land_point 这一层，路径搜索本身仍然可以使用对称等价的另一个 r 作为中间节点。

### 对新 MoveGen 的提示

要复刻 master 行为，新 MoveGen 必须做到三点：

1. **BFS 内部继续按 4 个 r 独立扩展**（已经是这样了），不可在 BFS 层做对称剪枝，否则就丢失"上一步是旋转"在 r=2 这条路径上的传播。
2. **landing 的去重 key 取 piece 占据的 cells 等价类**（落地后实际填到 board 上的 4 行 bitmask + 起始 row），而不是 (r, x, y)。在新框架里：去重 key = piece cells 经过 `(new_x, new_y, r)` 平移后投影到 Map 的 bit 集合。
3. **去重后保留下来的代表 entry 必须把当前 r 自己的 `last_rotate_arr[r]` 标签带出来**，不能因为对称等价的另一个 r 没旋转标签就把代表 entry 也降级。
   - 实践上：emit 时若一个 cells 等价类被多个 r 达到，应当用先到（or BFS 序优先）那个 r 的 `last_rotate_arr[r]` 标签 —— 与 master `index_filtered.mark(node)` 的"先到先得"语义一致。

### TODO（影响 B1 / B2 设计）
1. 把 piece 落点的 cells 等价类如何高效计算到 emit 阶段（`Map<W,H>` 已经支持把 piece data shifted 到落点位置，可以直接 OR 出实际占据 bitmask 作 hash）。
2. 验证 master `is_last_rotate` 在 ZSI 几何对称落点上的实际取值，确认我们要不要照搬 r=0 优先的"先到先得"。
3. T 块 spin=2 的差异另算一条 —— `check_ready` 用 `block_data_[node->status.x]`（不依赖 last_rotate），所以 master 的 `is_ready` 是"几何 corner 数 ≥ 3"；`is_last_rotate` 才是"旋转可达"标签。新 MoveGen 当前把 spin 等价于 `last_rotate AND corner≥3`，与 master 不同。要对齐就让 spin 拆成几何（is_ready）+ 行为（is_last_rotate）两个字段，或者在 emit 时仅按几何判定 ready。

## 待用户确认的策略
- **A**：保留新框架 y 语义（bbox 底物理坐标），oracle_diff 把 master 的 `(r, status.y)` → 「几何 r + bbox 底 y」做归一化；如新 MoveGen 真有对称剪枝，去掉，让 r 全 4 个枚举。
- **B**：让新 MoveGen 的 `(r, x, y)` 元组完全等同 master 的 `status.y` 语义（保留 4x4 偏移），改动点扩散到 `cells_impl`、`Piece` 与所有下游。

## 待动手前的两个验证项
1. `rule_srs` 的 RuleSpec 中 Z/S/I 是否只声明了 2 个 OpDesc。位置：`src/tetris_rule_spec.h` + `src/rule_srs.{h,cpp}`。
2. master 对同一几何落点 r=0 / r=2 输出的 spin 标签是否一致（影响后续 T-Spin 测试对齐方式）。

## 本地未推送提交（保持不动）
- `7f2d8b7` Use last-rotation reach and per-rotation 3-corner mask for T-spin
- `757192b` Switch chash to allocator_traits-based rebind for C++20
- `7bdcacd` Add MoveGen oracle diff driver against original search_tspin
- 工作区还有未提交修改：`tests/oracle_diff.cpp::copy_to_oracle_map` 位极性修复。

## 下一步
- 等用户确认走 A / B；
- 然后核实「rule_srs Z/S/I 旋转数 + master spin 一致性」两点；
- 再改 oracle_diff（A 路径）或新 MoveGen + Piece + cells_impl（B 路径），全绿后 commit + push。
