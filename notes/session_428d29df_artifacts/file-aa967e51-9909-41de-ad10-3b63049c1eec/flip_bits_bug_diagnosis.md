# flip-bits 分支调研笔记（v2 · 性能视角）

## 关键时间线

- master（`f26137d`）：可工作基线，row 1=占用语义。
- `a1e4ac6 TetrisMap flip bits`：核心重构提交，row 翻成 1=空。
- `9dff9b7 Botris 11`：当前 HEAD，主要修了 BotrisAI3 与 Botris/Botris_PC eval/get 的局部细节。
- 工作目录已回滚干净。

## 重构目标

1. `TetrisMap::row` 翻为 1=空 / 0=占用；`full(x,y)` 内部改判，调用方语义不变。
2. 引入 `empty_line()` 缓存与 `prepare()/prepare_internal()`：用户填完旧式 row 后调用 prepare 翻转裁宽并重算 top/roof/count。
3. `check / build_snap / attach` 改成 AND 路径，分支更少；行清除写回 `empty_line`。
4. `TetrisContext::full()` 改成"满行的 row 值"=0；`width()/height()/full()` 全部 inline。
5. `ai.cpp` 各入口"循环重算 top/roof/count"统一改成 `map.prepare()`。
6. botris 自身 `under_attack` 已改成 `1<<x` + `prepare_internal`。

## 仍然存在的 BUG（按影响优先级）

### P0 botris target

- `ai_zzz::Botris::init` 中 `TetrisMap` 构造后未初始化为空盘，直接 `attach` 触发 `row[i-1]==full()==0` 的"假消行"，danger_data 整体污染。
- `Botris::map_in_danger_` AND 判定语义反义。
- `Botris_PC::init` 的 col/row mask 退化为 0（暗坑）。

### P1 其它 AI 链路（`ai_zzz` 多个 AI / `ai_ax` / `ai_tag`）

- 同样的 init 假消行 + danger_data AND 反义；
- `col_mask_/row_mask_` 取自 `context->full()`=0，全部退化；
- eval 中 RowTrans/LineCoverBits/LineHole 仍按旧 row 1=占用 写法。

### P2 工具/对战路径

- `the_ai_games / cmd_tris / pso / ppt_pso` 的 `under_attack` 中 `new_line = full() & ~(1<<x)`，新语义下=0，丢失洞。

### misaka

`m_w_mask(context->full())` 退化为 0；内部大量 `~pool.row[y] & m_w_mask` 依赖旧 row 1=占用 写法，需要逐处复核。

## 性能视角下的修法收紧

| 项 | 朴素方案 | 优化方案 | 原因 |
|---|---|---|---|
| `row_mask()` 取宽度位掩码 | 每次调用现算 `width==32 ? 0xFFFFFFFFu : (1<<width)-1` | 在 `TetrisContext` 缓存 `row_mask_` 字段（沿用 master `full_` 的位置/初始化时机） | 与 `TetrisMap::line/empty_line()` 设计对称；零开销 |
| init 路径的初始化 | `map.prepare()`（含 W×H 重算扫描） | 给 `TetrisMap` 加 `void clear()`，仅写 row=line + 清 top/roof/count | 不跑无意义统计；语义"用户路径用 prepare、AI init 用 clear"分得开 |
| `map_in_danger_` 判定 | 在调用现场两个 NOT/AND/OR | `data & ~map.row[h-4+i]`，`data` 在 init 时一次性裁宽并取反成 1=占用 | 1 ANDN/行，与 master 同复杂度，BMI 平台融合成单条指令 |
| eval 内 `~map.row[y]` 复用 | `LineCoverBits ^= map.row[y]` 与 `LineCoverBits |= ~map.row[y]` 各算一次 | 提取临时 `uint32_t inv_row = ~map.row[y] & row_mask_;` | 一次 AND-NOT，下游 OR/XOR 共用，不重复算 |
| Botris::init col/row mask | `(width==32?...:(1<<width)-1) & ~1` 重复表达 | 改用 `context->row_mask() & ~1` | 编译期常量传播等价，可读性更好 |
| `attach` 中 `row==full()` 比较 | 不变 | 不变 | `full()=0` 已折叠成立即数，比 master 字段读还快 |
| `under_attack` 垃圾行初值 | `1 << random_hole_x` | 同上 | 与 botris.cpp 已有写法对齐，纯逻辑 fix |
| build_snap 仅填 roof 段 | 不动 | 留 follow-up 单独提交 | 增量优化，错位风险高 |

## 提交计划

1. **第 1 提交**：`.clang-format`（Allman、4 空格、`PointerAlignment: Right`、`if (` 主流、保留作者 include 顺序、`ColumnLimit: 0`）。
2. **第 2 提交**（核心 BUG 修 + 上述优化）：
   - `tetris_core.h`：
     - `TetrisContext` 新增 `uint32_t row_mask_` 字段、`uint32_t row_mask() const { return row_mask_; }`；
     - `TetrisMap` 新增 `void clear()` helper（rows = line、top/roof/count = 0）。
   - `tetris_core.cpp`：
     - `TetrisContext::prepare(w,h)` 在算完 width/height 后设置 `row_mask_ = width==32 ? 0xFFFFFFFFu : (1<<width)-1`。
   - `ai_zzz.cpp`、`ai_ax.cpp`、`ai_tag.cpp` 各 AI 的 init/eval/map_in_danger_：
     - `col_mask_ = context->row_mask() & ~1; row_mask_ = context->row_mask();`
     - init attach 前 `map.clear();`
     - danger_data 写入：`data[y] = ~map.row[h-4+y] & context->row_mask();`
     - `map_in_danger_` 改成 `data & ~map.row[...]`
     - eval 中循环引入 `inv_row` 临时变量，统一 LineCoverBits/LineHole 写法
   - `the_ai_games.cpp / cmd_tris.cpp / pso.cpp / ppt_pso.cpp`：
     - `under_attack` 中 `uint32_t new_line = 1 << random_hole_x;`
3. **第 3 提交**（misaka 单独隔离）：
   - `ai_misaka.cpp`：`m_w_mask(context->row_mask())`，内部所有 `~pool.row[y] & m_w_mask` 类位运算逐处按新 row 1=空 含义重写。

## 待用户确认

1. 同意添加 `TetrisMap::clear()` helper？（否则回退用 `prepare()`，多一次空扫接受。）
2. 同意把 master 的 `full_` 字段以 `row_mask_` 名义复活、并新增 `row_mask()`？
3. 第 2 提交合并范围（ai_zzz/ai_ax/ai_tag + the_ai_games/cmd_tris/pso/ppt_pso）OK 吗？
4. build_snap 的"只填 roof 段"是否同意放 follow-up，不进本次？
5. MR 目标分支：master / botris / develop 哪个？
