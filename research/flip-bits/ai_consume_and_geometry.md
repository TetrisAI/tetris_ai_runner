# ai_zzz 对 T-spin 的消费与几何等价性调研

本文档回答了关于 `ai_zzz` 评价函数逻辑与新 MoveGen 几何去重可行性的两个关键问题。

## Q1: ai_zzz 对 land_point T-spin 字段的真实消费

在 `ai_zzz::TOJ::get` 函数中，AI 实际上通过读取 `TetrisNodeWithTSpinType` 的多个字段来判定 T-spin 类别并据此分配攻击力和评分。

1. **实际读取的字段**：`is_check` (由 T 块判定触发), `is_last_rotate`, `is_ready`, `is_mini_ready`。
2. **判定逻辑 (src/ai_zzz.cpp:792-805)**：
   - **基础准入条件**：必须满足 `eval_result.clear > 0 && node.is_check && node.is_last_rotate`。若最后一步不是旋转 (`is_last_rotate == false`)，则直接视为普通落点。
   - **T-spin Mini**：在准入条件下，若 `eval_result.clear == 1 && node.is_mini_ready`，判定为 `TSpinMini`。
   - **T-spin (Full)**：在准入条件下，若不符合 Mini 且 `node.is_ready`，判定为 `TSpin`。
3. **关键结论**：`ai_zzz` 严格要求 `is_last_rotate == true`。这意味着任何通过平移到达的落点（即使满足 3-corner 且处于阻塞状态）都不会被计为 T-spin。

```cpp
// src/ai_zzz.cpp:792
if (eval_result.clear > 0 && node.is_check && node.is_last_rotate)
{
    if (eval_result.clear == 1 && node.is_mini_ready)
        node.type = TSpinType::TSpinMini;
    else if (node.is_ready)
        node.type = TSpinType::TSpin;
}
```

## Q2: 用编译期手段识别"几何等价 rotation"的可行性

1. **Consteval 比较**：可行。`shape::CellList` 是包含 `count` 和 `std::array<Cell, 32>` 的结构体，可以编写 `consteval` 函数逐个比较前 `count` 个 Cell 的 `(x, y)` 坐标。
2. **规范化排序**：`cells_impl()` (src/tetris_shape.h:206) 目前按行号递增（Y 递减）和位图扫描顺序生成 Cell，逻辑已具备确定性。在归一化至 bbox 原点后，相同形状生成的 cells 序列必然一致。若需更强的健壮性，可以在 `cells_impl` 返回前对 `buf` 进行 `std::sort`（C++20 `constexpr` 支持）。
3. **Usability/Landability 等价论证**：
   - `usable_map` (src/tetris_movegen.h:52) 的本质是棋盘位图按 piece 中所有 cell 坐标 `(cx, cy)` 进行 `shifted<-cx, -cy>` 后的交集。
   - 若 R1 与 R2 几何等价（即 `piece_cells` 集合相同），则其对应的位移操作集合完全一致，生成的 `usable_map` 必然相等。
   - `landable_map` 仅取决于 `usable_map` 及其垂直位移，故也必然相等。

## Q3 (bonus): emit 阶段做 cells-bitmask 去重的高效手法

在 `emit` 阶段，为了识别不同 (r, x, y) 但最终占据棋盘格子相同的重复落点（如 I 块水平旋转对称），可以使用 piece 的**落地绝对行号 + 占据行的 bitmasks** 作为 key。

**高效做法**：利用 `shape::find_op<Spec, T, R>::lines` 预存的 row masks。

```cpp
// 伪代码: 计算 O(1) 去重 key
auto& op = shape::find_op<Spec, T, r>;
int base_y = map_y + origin_y[r]; // 计算 piece 在 Map 中的起始行
uint64_t key = (uint64_t)base_y << 48; // 预留高位给行号
for(int i=0; i < op::lines::size; ++i) {
    // 假设 W <= 12 (如 10+2), 4 行共 48 bit 刚好塞进一个 uint64
    uint32_t row_mask = op::lines::data[i] << (map_x + origin_x[r]);
    key |= (uint64_t)row_mask << (i * 12);
}
// 最终 key 包含 (y, row0_mask, row1_mask, row2_mask, row3_mask)
```
对于标准 10 宽棋盘，4 行 mask (各 10bit) + 6bit 行号共 46bit，一个 `uint64_t` 即可完美承载。
