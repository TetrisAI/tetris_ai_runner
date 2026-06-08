# commit 6 (设计备忘): cell-footprint filtered index 取代 canonical-R 剪枝

## 背景

commit 5 之前 `tetris_movegen.h` 的 `MoveGen::generate` 在 emit 阶段使用编译期表
`kCanonicalR[r]`: 同一 piece 的几何对称 rotation 折成同一个 "canonical" r,
emit 时仅当 `r == kCanonicalR[r]` 才输出, 用来对齐 master `node_mark_filtered_`
在 IndexFilter 维度做的等价类去重.

`kCanonicalR` 表的等价判据来自 `shape::CellList::same_geometry`, 即 piece 在
**pivot 归一化坐标系下** cells 数组逐元素相等. 这只是几何相等性, 与 master
IndexFilter (按 piece 在棋盘上落地的 4 cells 绝对集合 + base row 折一个 5x32 bit
签名 memcmp) 不严格等价:

* master IndexFilter 同时考虑 (xb, yb) 偏移. 例如 SRS S piece 在 (r=0, xb=4) 与
  (r=2, xb=4) 因 pivot 偏移落到同一棋盘 cell 集合 -> master 视作同等价类;
  我们的 `kCanonicalR` 看 piece-local cells, 把 r=0 / r=2 永远绑定为同一
  canonical r, 与 (xb, yb) 无关. 在 SRS-7 + cells_impl 的 row-major 归一化下,
  这两套语义巧合一致.
* 一旦 piece 5+ rotation, 或者 piece_cells 不规范化的 spec, 这个巧合不再成立:
  `kCanonicalR` 会漏掉跨 (xb, yb) 的等价类, 或把不等价的 r 错误折并.

因此本 commit 把 emit 阶段的剪枝改为通用 cell-footprint 去重: 不再依赖
`same_geometry`, 而是按 "(r, xb, yb) 三元组在棋盘上实际占据的 cell-mask"
分组分配 filtered_idx, 与 master `node_mark_filtered_` 在 SRS-7 下严格等价,
对 5+ rotation 也自然正确.

## 设计

### 编译期索引表

`src/piece_filter_index.h` 提供编译期 helper:

* `piece_world_cells<Spec, T, R>(xb, yb)` -> sorted `array<uint16_t, 4>`,
  每个元素是 `(yb + cy) * W + (xb + cx)`. 任何 cell 落到棋盘外 (cx out of
  `[0, W)` 或 cy out of `[0, H)`) 视为 "invalid"; 整个 (r, xb, yb) 标 invalid.
* `compute_filtered_index<Spec, T>()` -> 编译期返回:
  * `count`: 等价类总数 (≤ R_count * W * H, 实际更少).
  * `idx[r * W * H + xb * H + yb]`: 该 (r, xb, yb) 的 filtered_idx,
    invalid 三元组写 sentinel `0xFFFF`.

实现采用线性 group-by: 先把所有 (r, xb, yb) 的 footprint 收到一个
`array<Footprint, N>` 里; 再按 "首次见到该 footprint 的下标 = 该等价类的 idx"
分配 idx. 这是 O(N²) 的朴素算法但 N ≤ 40 * 10 * 4 = 1600, GCC 12 默认
`fconstexpr-ops-limit=33554432` 完全 hold.

(实际我们对 SRS 系采用 R_count = 4, W = 10, H = 40 -> 1600 项, sentinel
1599 -> 等价类总数估计 ~700; bitmap 容量 < 1KB.)

### 运行时去重

`MoveGen::generate` 增加局部:

```
std::array<std::uint64_t, (kFilteredCount + 63) / 64> filtered_mark{};
```

emit 阶段 (r 外层 + popcount 内层):

```
auto idx = kFilteredIndex[r * W * H + xb * H + yb];
if (idx == kFilteredInvalid) continue;
auto bit_word = idx >> 6;
auto bit_mask = uint64_t{1} << (idx & 63);
if (filtered_mark[bit_word] & bit_mask) continue;
filtered_mark[bit_word] |= bit_mask;
emit(...)
```

由于 invalid (cell 落在棋盘外) 永远不会出现在 BFS 可达集合 (`landings = search[r] &
landable_arr[r]` 已经被 `usable_map` / `inbounds_map` 限制在棋盘内), 实际只是
sentinel 兜底, 不会触发跳过.

### 顺序保证

* emit 阶段把 r 外循环改为 `kCcwVisit[k]` 顺序遍历 (master `tetris_core.cpp`
  注册阶段从 r=0 出发沿 `rotate_counterclockwise` 链入队, IndexFilter
  让 CCW 链上更早的 r 拿走 canonical idx; SRS 4 旋转链 `[0, 3, 2, 1]`).
* 编译期表 `compute_table` 内 idx 分配也用同一 CCW 顺序: 同 footprint 出现
  时, 第一个 (r, xb, yb) 拿到新 idx; 后到的复用. 这样 r=1 / r=3 几何等价
  的 I 竖块在 master 视角永远以 r=3 出现, 我们一致.
* 内层 (xb, yb) 走线性序, 与 master BFS 入队序 (左→右, 下→上) 等价.
* oracle_diff 在 SRS-7 1g / 20g 下要求落点 (r, x, y, spin) 与 master 完全一致.

### 编译期 ops 控制

朴素 N² group-by 把 sorted footprint 数组 (4 × uint16) 逐元素比较, GCC 12
计入 `array::operator[]` 的 ops 后会撞 `-fconstexpr-ops-limit=33554432`
导致 cc1plus OOM. 优化:

* footprint 打包成 `std::uint64_t` (4 cells × 16 bits, 升序; 不足 4 cell
  高位补 `0xFFFF`). 整数相等比较 ops O(1).
* 已分配 idx 维护一个 side-array `assigned_key / assigned_idx`, 长度
  ≤ 等价类数 (远 < N), 线性扫成本 O(N · K) 而非 O(N²).
* CMakeLists.txt 仍把 `-fconstexpr-ops-limit` 调到 `512M` 留余量, 后续
  table 增大不至于再撞.

### 越界处理 (与 master 注册路径对齐)

master `tetris_core.cpp:296-322` 把 `IndexFilter(node)` 装进 `std::map`, key
是 `node.data[0..3]` 的 5x32 bit 签名. 注册阶段 BFS 只会塞 `node->check(map)`
通过的 (status.t, status.r, status.x, status.y), 而 `check` 在 spawn / rotate /
move 各处都用 in-bounds + board collision 一起守门. 因此 cell 落到棋盘外的
(r, xb, yb) 在 master 那边不会进入 IndexFilter, 我们也用 sentinel `0xFFFF`
排除它们, 不参与 filtered_idx 分配, 等价类划分对齐.

### bitset vs generation counter

选 `std::array<std::uint64_t, ceil(N / 64)>` + 每次 `MoveGen::generate` 入口
`{}` 默认初始化清零. N 大约几百 (实际 SRS T 块 ~140 + I 块 ~80...), 每次清
零 < 100 字节, 一次 `memset` 远快于 master `TetrisNodeMarkFiltered` 走 version
counter. 实现也最简, 不引入额外字段.

### 删除项

* `src/tetris_movegen.h`: `kVisitOrder` / `visit_order_at` /
  `canonical_for` / `build_canonical_r` / `kCanonicalR` 全套及对应注释.
* `src/search_hook.h`: NoHook / DefaultTSpinHook / DefaultASpinHook 的
  `emit_canonical_r_only<T>` trait 与相关注释.
* `src/tetris_movegen.h` 中错误的 367-372 注释 (描述不再适用的 canonical-R
  与 master IndexFilter 关系).

## 验证

* `cmake --build build -j`: 必须 clean.
* `./build/oracle_diff`: 必须 `# all diffs ok` (SRS-7 1g / 20g 全部场地).
* `./build/aspin_dump`: 与 `aspin_after.txt` 比对; type 字段 baseline 全是 0
  无法看 ASpin 标签, 主要看 (idx, row) 集合.
