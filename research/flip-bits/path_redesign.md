# 上层接入：方案 Y 设计文档（位板 path 重写）

锁定方案 Y：MoveGen 接入上层，**make_path 不复用 search_tspin，直接基于位板重写**，与"消除 TetrisContext"目标同向。

参考：
- 调研：`research/flip-bits/upstream_callers.md`
- 字段消费：`research/flip-bits/ai_consume_and_geometry.md`
- 老 search_tspin 实现：`src/search_tspin.cpp:39-453`（make_path）/ 710-974（make_path_20g）/ 456-708（search）/ 976+（search_t）

---

## 1. 接入面（5 个文件）

| 文件 | 改动 |
|---|---|
| `src/tetris_movegen.h` 或新文件 `src/movegen_search.h` | 加 `class MoveGenSearch` |
| `src/ai.cpp:104, 106, 110, 112, 116, 165` | 模板第三参 `search_tspin::Search` → `MoveGenSearch` |
| `src/pso.cpp:135, 154, 605` | 同上 |
| `src/ppt_pso.cpp:153, 173, 745` | 同上 |
| `src/ai_zzz.h:96-97, 185-186, 298-299` | typedef 来源切换 |
| `src/ai_misaka.h:10-11` | typedef 来源切换 |

`tests/oracle_diff.cpp` 保留 `search_tspin::Search` 作为 oracle baseline，不动。

---

## 2. MoveGenSearch 公开接口（与 search_tspin::Search 同形）

```cpp
class MoveGenSearch {
public:
    enum TSpinType { None, TSpin, TSpinMini };
    struct Config {
        bool allow_rotate_move = false;
        bool allow_180 = true;
        bool allow_d = true;
        bool allow_D = true;
        bool allow_LR = true;
        bool is_20g = false;
        bool last_rotate = false;
    };
    struct TetrisNodeWithTSpinType { /* 与 search_tspin 同形 */ };

    void init(m_tetris::TetrisContext const *context, Config const *config);
    std::vector<TetrisNodeWithTSpinType> const *search(map, node, depth);
    std::vector<char> make_path(node, land_point, map);
};
```

**类型层兼容**：`Config` / `TSpinType` / `TetrisNodeWithTSpinType` 字段顺序 / flags union 完全照搬。这样 ai_zzz.h / ai_misaka.h 的 typedef 只需换源（`search_tspin::Search` → `MoveGenSearch`），AI eval 逻辑一字不改。

**节点表示**（关键决策）：`TetrisNodeWithTSpinType.node` 字段仍是 `m_tetris::TetrisNode const *`。本轮**不动 TetrisNode**——它现在是 LandPoint 的事实承载，全 AI eval / map.attach 都依赖它。本轮目标是消除 path 对 TetrisContext **指针图**的依赖（wall_kick / rotate_* / move_* / drop / index_filtered），不是消除 TetrisNode 本身。后续 TetrisContext 消除阶段再处理。

---

## 3. search() 实现策略

```cpp
auto* search(map, node, depth) {
    land_point_cache_.clear();
    // 1) 把 m_tetris::TetrisMap (1=空, 0=占) 转成 Map<W,H> (1=占)
    // 2) 调 movegen_.generate(...) 出 (Piece, flags)
    // 3) 对每个 (Piece, flags):
    //    - 用 context->get(TetrisBlockStatus{T, master_x, master_y, r}) 反查 TetrisNode*
    //    - 填 TetrisNodeWithTSpinType { node, last=??, type=None,
    //                                   is_check, is_last_rotate, is_ready, is_mini_ready }
    return &land_point_cache_;
}
```

**`last` 字段**：search_tspin 里 `last` 指向"旋转到达 land_point 的前一个 TetrisNode"（即 land_point.node 的旋转源），make_path 用它做 T-spin 末段重放。新 MoveGen 现在只产 last_rotate_arr[r] 不产具体前驱坐标，需要在 BFS 里多记一项（旋转前驱的 (r_src, x_src, y_src)）。本轮在 search 阶段就把这条信息攒出来。

---

## 4. make_path 位板重写：拟人启发式集合（来自 search_tspin.cpp 精读）

### 4.1 算子顺序（拟人化的核心）
```
disable_d=true 第一轮: 优先 D (一次直落) → x → z → c → l → r → L → R     (禁 d/D)
disable_d=false 第二轮: 同上 + d (单步软降) + D (硬降)
```

注意：
- `x/z/c` = 旋转 180/CCW/CW，遵循 SRS 踢墙表，命中第一个不碰撞 kick 后 break
- `l/r` = 单步左右，**`L/R`** = 一直左/右到底（拟人速移）
- `d` = 单步软降，`D` = 硬降到底
- `disable_d` 第一轮的目的：找到不动 `d/D` 也能到的"纯滑路径"，看起来更顺手；找不到再放开

### 4.2 disable_d 触发条件
`land_point.type == None && node->land_point != nullptr && node->low >= map.roof && land_point->open(map)`

含义：
- 不是 T-spin（type=None）
- 当前 spawn 节点 `node->land_point` 表已存在（context 预生成的"以此为起点的所有可达 land set"）
- spawn 高于 map.roof（在棋盘上方有空间）
- land_point 在 open 区域（不需要钻洞）

→ 满足说明可以"纯横向 + 旋转"到达，不必下降。这条是 TetrisContext 强依赖（land_point 表 + roof + open），位板重写要重新表达。

**新位板等价表达**：
- `node->land_point != nullptr && node->low >= map.roof` → "spawn 在 map.roof 之上 + 当前形态空盘可达"。位板上 roof 就是 `map.roof`，spawn 高度从 piece 几何 + spawn pos 算。`open(map)` → land_point 占据的 4 行在 map 上方的"上方半空"。
- 暂用一个**保守等价**：spawn.y > map_top_filled_row 且 land_point.y > map_top_filled_row + 1。先稳住语义，后续再调精度。

### 4.3 build_path 的 T-spin last 重放
当 `node_index != land_point.index_filtered` 时（命中的不是 land 本身而是 land.last，即"最后一步是旋转"），从 `last` 出发尝试 `x/z/c` 三个 wall_kick 序列，第一个能落到 `land_point.node` 的就追加对应字符。

**位板等价**：last 状态是 (T, last_r, last_x, last_y)，对它做位板版 `apply_kicks<R, R'>`，第一个 kick 终点等于 (T, land.r, land.x, land.y) 时追加字符。

### 4.4 allow_rotate_move（旁路操作）
平移后立刻接旋转的操作 X/Z/C（大写 = "一步组合操作"）。仅 allow_rotate_move=true 时启用。

### 4.5 20g 分支（make_path_20g）
所有移动后立刻 `drop(map)`（重力总在生效）。简化：
- 没有 d/D（重力代劳）
- 没有 disable_d 双轮
- 旋转后立刻 drop
- 不带 allow_rotate_move

---

## 5. 节点身份（去 TetrisContext 化的关键）

老 search_tspin 用 `node->index_filtered`（来自 TetrisNodeMarkFiltered）作 visited key。位板重写要换：
- **visited key**：`(r, x, y)`（含 cells 等价类规范化 r，与 movegen 已实现的 `kCanonicalR` 一致）
- **命中目标**：从 land_point 反推目标 `(r*, x*, y*)`（其中 r* 取 canonical）
- **L/R 整段速移命中**：连续左/右到墙后 visited 标记终点

去 `index_filtered` 后，**land_point 的归一化**（master 用 cells bitmask + 起始 row）由 movegen 的 emit 阶段已经做掉，path BFS 直接对 (canonical_r, x, y) 做 visited 即可。

---

## 6. 分阶段 commit 计划（每笔单一职责）

按你"修改保持单一提交"的规矩：

| # | commit 内容 | 文件 | 风险 |
|---|---|---|---|
| **1** | `src/movegen_search.h` 新文件，写 `MoveGenSearch` 骨架（init + search + 空 make_path return {}），不接入 ai.cpp。`tests/oracle_diff` 加一份 MoveGenSearch::search 路径的 case，确保 search 翻译 / last 字段填充正确 | movegen_search.h, tests/oracle_diff.cpp, CMakeLists.txt | 低，纯加 |
| **2** | make_path 最小可用版（不含 disable_d / allow_rotate_move / 20g）：基础算子 x/z/c/l/r/L/R/d/D + T-spin last 重放。tests/oracle_diff 加 path 一致性维度（与 search_tspin::make_path 比较） | movegen_search.h, tests/oracle_diff.cpp | 中 |
| **3** | make_path 补 disable_d 双轮 + allow_rotate_move（X/Z/C 旁路操作） | movegen_search.h, tests/oracle_diff.cpp | 中 |
| **4** | make_path_20g 分支 | movegen_search.h, tests/oracle_diff.cpp | 中 |
| **5** | 5 处模板参替换（ai.cpp / pso.cpp / ppt_pso.cpp） | ai.cpp, pso.cpp, ppt_pso.cpp | 低（编译期检验） |
| **6** | typedef 迁移（ai_zzz.h / ai_misaka.h） | ai_zzz.h, ai_misaka.h | 低 |

**节奏**：commit 1-4 是新 MoveGenSearch 自我建设，不影响主回路；commit 5-6 才真正把 AI 切到新引擎。每笔之间用 oracle_diff 守门。

---

## 7. 关键风险与待澄清

1. **`disable_d` 触发条件**位板等价的精度：保守版可能让某些 case 退化到第二轮 BFS（多一次跑），不影响正确性，影响 path 视觉。可后续调。
2. **`land_point.last` 字段填充**：search 阶段要在 BFS 里多记 (r_src, x_src, y_src)。这是 movegen 现有逻辑的小扩展，不是大改。
3. **L/R 速移 visited 标记**：老 search_tspin 只标记速移终点（不标中间），新版同步做。
4. **TetrisNode * 反查**：search 阶段把 movegen 的 (T, r_master, x_master, y_master) 用 `context->generate(status)` 反查回 TetrisNode*。这个 API 已存在（`tetris_core.h` TetrisContext::generate）。性能 O(1) hash。

---

## 8. oracle_diff 拓展

新增维度（commit 2 起）：
- 对每个 land_point，在 board 上分别用 `MoveGenSearch::make_path` 与 `search_tspin::Search::make_path` 求 path。
- **不要求字符串相等**（拟人启发式微调允许差异），但要求**回放正确性**：从 spawn 出发按 path 操作后到达 land_point。验证用现有 TetrisNode 模拟即可（apply 字符 → TetrisNode 更新）。
- 全 18 board × 7 piece × N_landings 全绿才放 commit。

---

## 9. 不在本轮范围

- 删除老 search_tspin（保留作 oracle baseline）
- 消除 TetrisContext 本身
- 消除 TetrisNode 本身
- search_simple / search_simulate / search_path 等其他 search 实现的位板化
