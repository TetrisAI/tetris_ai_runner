# flip-bits 分支调研笔记

## 关键时间线

- master（`f26137d`）：当前在跑、可工作但不保证无 BUG 的基线。
- `a1e4ac6 TetrisMap flip bits`：本次重构提交。
- `9dff9b7 Botris 11`：当前 HEAD，针对 BotrisAI3 与 Botris/Botris_PC eval/get 的局部调整。
- 工作目录已回滚到 `9dff9b7`，`.clang-format` 草稿已删除。

## flip-bits 的改造目标（基于 a1e4ac6 + 此前 Botris 9/10 演进）

1. 翻转 `TetrisMap::row` 的位语义：
   - 旧：1=占用，0=空。
   - 新：1=空，0=占用。
   - `TetrisMap::full(x,y)` 实现改为 `((row[y]>>x)&1)==0`，对调用方语义不变（仍然回答"该格是否有方块"）。
2. 引入 `TetrisMap::empty_line()`（缓存的宽度位掩码，作为"全空行的 row 值"）和 `TetrisMap::prepare()`：用户用旧的 1=占用 形式填充 row 后调用 `prepare()` 翻转裁宽并重算 `top/roof/count`。`prepare_internal()` 不翻转。
3. `TetrisContext::full()` 由"宽度位掩码"改为"满行的 row 值"，新语义下统一返回 0。`width_/height_/full()` 全部改为 inline。
4. 关键热点改用 AND 累加可放位（`check`、`build_snap`、`attach`），减少分支与按位运算次数；`build_snap` 默认初值改为 `empty_line`，`attach` 写入位用 `&= ~data`，整行清除时写回 `empty_line`。
5. 多处 `ai.cpp` 中"用户填充 row 之后手动循环重算 top/roof/count"被统一替换为 `map.prepare()`。
6. `botris.cpp` 的 view 默认开、调试样例切换、`under_attack` 中垃圾行直接写 `1<<x`（新语义：一个洞、其它占用）等，已配合新语义改造。

设计意图小结：把"占用 mask 直接=0"作为热点；让 `attach`/`check`/`build_snap` 的循环更对称，AND 路径性能更优；上层用户用 `prepare()` 解决"填完旧式 row 后初始化"的样板代码。

## 仍然存在的 BUG

下面列的 BUG 都是因为重构时**没把所有依赖旧语义的位置同步翻**导致的。按影响范围分级。

### P0 botris 可执行（CMake 中的 `botris` 目标，测试 ai_zzz::Botris 自对弈）

botris target 链路：`botris.cpp` + `ai_zzz.cpp` + `tetris_core.cpp` + `search_aspin.cpp` + `rule_botris.cpp` + `random.cpp` + `integer_utils.cpp`。

#### BUG-1：`ai_zzz::Botris::init` 中 `TetrisMap` 构造后未 prepare

```cpp
TetrisMap map(context->width(), context->height());
TetrisNode const *node = context->generate(i);
node->attach(context, map);
std::memcpy(map_danger_data_[i].data, &map.row[18], sizeof map_danger_data_[i].data);
```

`TetrisMap(w,h)` 内部 `memset(this,0,...)`，row 全 0。新语义下 0=占用，所以这是"全部已占用"。
紧接 `node->attach`：

1. `map.row[r+i] &= ~data[i]` 把方块位设 0，但 row 已 0，无变化。
2. 后续 `if(map.row[row+i-1] == context->full())`，`context->full()==0`，全行均命中；`memmove`+`map.row[h-1]=empty_line` 触发"假消行"。
3. 若 height 为 4，循环走 4 次，连消 4 次假消行。最终 `map.row[18..21]` 已经被 attach 的 top 计算和"假消行"双重污染，`map_danger_data_` 完全错的。

后果：`map_in_danger_` 在评估"放下后是否会顶到 22 行以上"时永远给错的判断，导致 Botris 危险判定失效，进而 `Botris::get` 中 `safe<=0` 的分支被错误触发或错过。

#### BUG-2：`ai_zzz::Botris::map_in_danger_` AND 语义反转

```cpp
return map_danger_data_[t].data[0] & map.row[h-4] | ... ;
```

新语义下 `map.row` 1=空，`map_danger_data_` 也是按 row 直接 memcpy（同样 1=空）。AND 结果是"两者均空"的位 ⇒ 与"碰撞"含义相反。

#### BUG-3：`Botris::init` 自身 col_mask/row_mask 是对的，但 `Botris_PC::init` 不对

```cpp
// Botris_PC::init
col_mask_ = context->full() & ~1;
row_mask_ = context->full();
```

`context->full()==0`，所以 `col_mask_==0`，`row_mask_==0`。`Botris_PC::eval` 中：

```cpp
RowTrans += map.roof == map.height ? ZZZ_BitCount(map.empty_line() & ~map.row[map.roof-1]) : map.width - ZZZ_BitCount(map.row[map.roof-1]);
```

单从这一行看，`Botris_PC::eval` 实际上**没用到** `col_mask_/row_mask_`（已用 `empty_line()` 直接表达），所以掩码是 0 不影响 eval 计算结果——只是无用变量，不属于功能性 BUG。可以选择性清理，或者一起改正以保持仓库自洽（推荐改）。

`Botris::init` 已显式 `(context->width()==32?...:(1<<width)-1) & ~1`，规避了 `full()` 调用，但是表达比较啰嗦。后面用 `row_mask()` 抽象后可以一并简化。

### P1 其它 AI（不在 botris target，但仍编入 dll/ runner，对应 SCM 真用环境）

下列 init 都使用 `context->full()` 当宽度位掩码（值 0），并把 row[h-4..h-1] memcpy 进 danger_data，存在与 Botris 相同的 attach 假消行 + AND 反转 + col_mask 退化为 0 的 BUG：

- `ai_zzz::Attack::init / eval / map_in_danger_`
- `ai_zzz::Dig::init / eval / map_in_danger_`
- `ai_zzz::TOJ::init / eval / map_in_danger_`
- `ai_zzz::TOJ_PC::init / eval`
- `ai_zzz::TOJ_v08::init / eval / map_in_danger_`
- `ai_zzz::C2::init / eval / map_in_danger_`
- `ai_ax::AI::init / eval / map_in_danger_`（ai_ax.cpp）
- `ai_tag::the_ai_games_old::init / eval / map_in_danger_`（ai_tag.cpp）
- `ai_tag::the_ai_games::init / eval / map_in_danger_`（ai_tag.cpp）
- `ai_misaka::Pool::m_w_mask(context->full())`（值 0；后续所有 `&pool.m_w_mask` 都退化）

另外 eval 内部的 `ColTrans/RowTrans` 计算依赖 `row` 的位语义：
- `ZZZ_BitCount(row ^ (row << 1))` 数的是"过界次数"，与位语义无关，不需要改。
- `ColTrans += !map.full(0,y) + !map.full(width_m1,y)`：`full(x,y)` 已隔离语义，不需要改。
- `RowTrans += ZZZ_BitCount(row_mask_ & ~map.row[0])`：旧语义里数的是 row[0] 中的"空位"个数。新语义下 row[0] 已经 1=空，正确写法应为 `ZZZ_BitCount(map.row[0] & row_mask_)`，等价于 `ZZZ_BitCount(map.row[0])`（row 已裁宽）。
- `RowTrans += ZZZ_BitCount(map.roof==map.height ? row_mask_ & ~map.row[map.roof-1] : map.row[map.roof-1])`：旧语义里第一项数的是顶行"空位"，第二项数的是 roof-1 行"占用位"。新语义下应分别是 `ZZZ_BitCount(map.row[roof-1])` 和 `ZZZ_BitCount(~map.row[roof-1] & row_mask_)`。
- `LineCoverBits |= map.row[y]; LineHole = LineCoverBits ^ map.row[y]`：旧语义中 `LineCoverBits` 表示"上方曾经出现过的占用列"，与当前行 XOR 得到"该列被覆盖但本行为空"的洞。新语义下要保持原意，需要 `LineCoverBits |= ~map.row[y]; LineHole = (LineCoverBits ^ ~map.row[y]) & row_mask`，即 Botris::eval 已经在 9dff9b7 中使用的写法。其它 AI 的 eval 都还没更新。

### P2 对战/工具链（当前不在 botris target，但其它 cmake 目标依赖）

- `the_ai_games.cpp under_attack`：第 487 行 `map.row[y-line] == ai.context()->full()`，新语义下 `full()=0`，新语义里 0=全占用，仍然对。但第 496 行 `uint32_t new_line = ai.context()->full();` 后 `new_line &= ~(1<<x);`：意图是"垃圾行带一个洞"，新语义下应为 `new_line = 1<<x`（一个洞、其余占用），现在变成 0 → 全占用，丢失洞。
- `cmd_tris.cpp / pso.cpp / ppt_pso.cpp` 的 `under_attack`：垃圾行 `ai.context()->full() & ~(1<<x)`，同上需改成 `1<<x`。这些工具的 attack 行后还跑了 `map.full(mx,my)` 的循环重算，由于 `full(x,y)` 已隔离语义，循环重算本身没问题；问题只在 `new_line` 的初值。
- `botris.cpp` 自己的 `under_attack` 已经在 a1e4ac6 中改成 `1<<x` + `prepare_internal`，正确。

### 体系性建议

- `TetrisContext::full()` 现在的语义 = "满行的 row 值"=0，与"宽度位掩码"是两件事。建议在 `TetrisContext` 增加 `row_mask()` 内联（用 `width_` 实时计算或缓存），把所有"作为宽度位掩码"使用的 `full()` 调用替换为 `row_mask()`。这样：
  - `attach/clear_low/clear_high` 中"row==full()"判定不变（满行=0）；
  - `*_init` 中 `col_mask_/row_mask_` 改用 `row_mask()`；
  - `ai_misaka::Pool::m_w_mask` 也改用 `row_mask()`；
  - `Attack::init` 中的 `check_line_*`（构造"差一格的接近满行"的 row 值）改用 `row_mask()`。

## 修复计划（拟分两次提交，仍在 flip-bits 分支）

> 用户先前的工作约定：每次变更单一提交，提交前格式化，先达成共识再开始。下面计划等用户确认后再执行。

### 第 1 步：补 .clang-format

依据当前仓库主流风格（Allman、4 空格、`PointerAlignment: Right`、`if (` 主流）生成 `.clang-format`。该文件单独入一次提交，不混入代码改动；后续所有手改文件使用它格式化（仅格式化我修改的文件，不全仓重排）。

### 第 2 步：补齐位翻转遗漏，单提交

文件改动一览：

1. `src/tetris_core.h`：在 `TetrisContext` 增加 `uint32_t row_mask() const`（基于 `width_` 实时计算）。
2. `src/ai_zzz.cpp`
   - 把所有 init 中 `context->full() & ~1` / `context->full()` 替换为 `context->row_mask() & ~1` / `context->row_mask()`；
   - 所有 init 在 `node->attach(context, map)` 之前补一次 `map.prepare()`，避免假消行；
   - 所有 `memcpy(&map.row[h-4]...)` 后再做 `data[y] = ~map.row[h-4+y] & row_mask` 转换，使 danger_data 仍然以"1=占用"语义存储（保持后续 `data & ~map.row[...]` 检测能命中）；扩张方式 `|=` 保持不变；
   - `map_in_danger_` 中的 `data & map.row[...]` 改为 `data & ~map.row[...]`；
   - `Attack::eval` / `Dig::eval` / `TOJ::eval` / `TOJ_v08::eval` / `C2::eval` 中 RowTrans/LineCoverBits/LineHole 的位语义按 `Botris::eval` 在 9dff9b7 的写法对齐；同步把 `LineHole` 的 well 检测保持一致（well 检测当前用 `LineCoverBits` 的左/中/右 3 位模式判定，需要也跟着翻成"1=占用"形式）；
   - `Botris::init` 简化：`col_mask_ = context->row_mask() & ~1; row_mask_ = context->row_mask();` 与上下文一致；
   - `Botris_PC::init` 的 col/row mask 也用 `row_mask()`，保持自洽（即使 eval 不直接用，仍清理暗坑）。
3. `src/ai_ax.cpp`：和 `ai_zzz::Attack` 同步处理（init prepare、danger_data 翻转、eval 的 RowTrans/LineCoverBits/LineHole/Well 位语义重写）。
4. `src/ai_tag.cpp`：`the_ai_games_old / the_ai_games` 同步处理。
5. `src/ai_misaka.cpp`：`Pool::m_w_mask(context->row_mask())` 替换；同时 misaka 内部所有 `~pool.row[y] & pool.m_w_mask` 类计算需要复核——misaka 旧写法假定 `row` 1=占用、`~row` 表示空位。新语义下 `~pool.row[y]` 已经表示"占用"。需要重新匹配每一处的语义意图（这一块代码量较大，且历史几乎不动，需要小心审视）。
6. `src/the_ai_games.cpp`：`new_line = 1 << random_hole_x;` 取代 `full() & ~(1<<x)`。
7. `src/cmd_tris.cpp`：同上。
8. `src/pso.cpp`：同上。
9. `src/ppt_pso.cpp`：同上。

> 关于 misaka：因 misaka 的 `eval/findT*` 处大量原始位运算依赖 `row` 1=占用 的旧语义，这次扩散修复必然会牵动大量代码。为了控制风险，建议把 misaka 的修复独立成第 3 步提交（仍属本次 BUG 修复任务），避免和 ai_zzz/ai_ax/ai_tag 的逻辑共享同一个提交。

### 验证方式

- 当前环境无法编译执行，所有修改完成后只能做静态自查（diff 通读 + 语义图谱比对）。
- 想加保险，可以人工眼读 `Botris::eval`（9dff9b7 已迁移版）作为标准，把其它 eval 的 row 语义统一成相同写法。

## 待用户确认事项

1. 上述分级是否符合你对"BUG"的优先级直觉？是否同意按"先 .clang-format → 再 ai_zzz/ai_ax/ai_tag → 最后 ai_misaka"分两到三次提交？
2. P2（the_ai_games / cmd_tris / pso / ppt_pso 的 under_attack）是否要合并到本次修复（建议合并，避免下次又要单独动这些文件）？
3. 是否允许同时把"目前实际没用到的 col_mask_/row_mask_（如 Botris_PC）"也改齐，避免日后误用？我倾向于改。
