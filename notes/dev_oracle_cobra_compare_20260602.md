# dev 分支相对 oracle 与 cobra 的调研记录（2026-06-02）

## 调研范围

- 仓库：`tetris_ai_runner`
- 调研分支：`dev`（HEAD=`6cf6726`）
- oracle 基线：`origin/flip-bits` / `e3b909b`
- 参照仓库：`cobra-movegen`（`big-board`）

## 结论快照

- `dev` 相对 oracle 一共新增 **33 个提交**，主线非常集中：
  1. 用 `tetris_rule_spec.h` 把 rule 的几何、旋转目标、踢墙表抽成**编译期描述**。
  2. 用 `tetris_map.h` / `tetris_simd.h` / `tetris_shape.h` 建立独立于旧 `TetrisContext` 的**位板基础设施**。
  3. 用 `tetris_movegen.h` 落地 **cobra 风格的 bitboard reachability BFS**。
  4. 用 `tests/perft_movegen.cpp` 与 `tests/oracle_diff.cpp` 对空盘和复杂局面做**新旧结果对拍**。
- 这不是把旧引擎整体删掉重写，而是采用 **“新内核 + 旧引擎桥接”** 的路线：
  - 旧 `TetrisEngine / TetrisContext / Search / AI` 仍然存在并继续工作。
  - `RuleSpec -> flatten_rulespec()` 把编译期规则重新桥接回旧运行时 `TetrisOpertion` / `TetrisNode` 网络。
- 与 cobra 的关系可以概括为：
  - **算法核**大量借鉴 cobra：bitboard 棋盘、整图 BFS、usable/landable、canonical rotation。
  - **语义层**保留 oracle：master `TetrisNode` 坐标、旧 rule/search/AI 协议、T-Spin Mini/Full 判定细节、oracle_diff 对拍。

## dev 相对 oracle 的提交分组

### 第一段：为编译期规则打底

提交：`3ca112c` ~ `7077937`

- `3ca112c`：`TetrisNode::check` 热路径改成 BMI `ANDN` 形式。
- `1d6e55d`：统一引入 `row_t`，让 map/node/snap/op 使用同一行类型。
- `46e472f`：引入 `RuleSpec / OpDesc / WallKickList / OpLines` 骨架。
- `80ba299`、`4b25262`、`315de50`：把 SRS / SRSX / 其余规则逐步迁到 `rule_*.h` 里的编译期 `RuleSpec`。
- `5596ad1`：移除 `TetrisNode` 自带 `op` 字段，改为经 context lookup 获取。
- `7077937`：`TetrisEngine::prepare()` 通过 `typename Rule::rule_spec` + `flatten_rulespec()` 建旧桥。

### 第二段：位板基础设施

提交：`6ffe243` ~ `7669ce9`

- `6ffe243`：引入 `tetris_simd.h`、`tetris_map.h` 雏形与 C++20 工具链要求。
- `1fd5f84`：`tetris_shape.h` 支持从 `RuleSpec` 查询方块几何。
- `923c67b`：`Map<W,H>::shifted<Dx,Dy>()`。
- `4234de2`：`Map<W,H>::for_each_set_bit()`。
- `ae245ad`：`Map<W,H>::line_clears()`。
- `7669ce9`：`Map<W,H>::clear_lines()`。

### 第三段：MoveGen 主体与语义补齐

提交：`eb0cf22` ~ `ba9b42f`

- `eb0cf22`：落地 cobra-style MoveGen BFS with rotation and wallkick。
- `97a5cee`、`a051ba6`：修坐标系和落点基准问题。
- `a4bf720`、`c9e3bb6`：补齐 T-Spin Full / Mini 判定。
- `e18f106`：把 rule primitives 正式抽到 `tetris_rule_spec.h`。
- `b872c10`：修 lane padding 和 in-bounds mask 导致的误计数。
- `6f2fdc1`、`bcc6a84`、`7f2d8b7`：把 `shape::op_cells` 归一化、补 pivot，并用 last-rotation reach + 3-corner mask 对齐旧 T-Spin 语义。
- `757192b`：`chash` 为 C++20 改 allocator_traits rebind。
- `46d783c`、`d45f612`、`580cb10`、`ba9b42f`：把输出坐标、TetrisMap polarity、对称旋转去重、T-Spin 语义都压到与 master/oracle 一致。

### 第四段：测试与对拍

提交：`6b10f60`、`7bdcacd`、`c063a12`、`6cf6726`

- `tests/perft_movegen.cpp`：先验证空盘枚举。
- `tests/oracle_diff.cpp`：把原始 `search_tspin` 当 oracle，对拍 `(r,x,y,spin)`。
- 后续不断补 TSD / STSD / SSpin / donation / 高堆 / 贴墙 kick edge cases 等棋盘。

## 组件说明

### 1. `src/tetris_rule_spec.h`

**解决什么问题**
- 把原来散在 `rule_*.cpp` + `tetris_core.h` 里的几何和踢墙定义变成可被新位板模块直接消费的编译期描述。

**怎么用**
- 每个 rule 头文件内定义 `using XxxRule = RuleSpec<W,H,N, Ops...>`。
- 单个旋转态用 `OpDesc<'T', R, Lines, spawn_x, spawn_y, target_cw, target_ccw, target_opp, wk_cw, wk_ccw, wk_opp>` 描述。
- 旧引擎通过 `flatten_rulespec<typename Rule::rule_spec>()` 还原成运行时 `std::map<pair<char,uint8_t>, TetrisOpertion>`。

**对应 cobra 哪一部分**
- 接近 `ruleset.hpp` 的 `RulesetBase`，但更重。

**与 cobra 的区别**
- cobra 只保留 movegen 需要的最小规则位；
- dev 的 `RuleSpec` 要完整承载 oracle 的几何学与旋转学，并反哺旧引擎桥接层。

### 2. `src/tetris_map.h` + `src/tetris_simd.h`

**解决什么问题**
- 提供与旧 `TetrisMap` 解耦的位板棋盘，实现整图位移、按行读写、满行检测、消行。

**怎么用**
- `using Board = Map<10, 40>;`
- `board.set(x,y)` / `get(x,y)` / `row(y)` / `set_row(y,v)`
- `board.shifted<Dx,Dy>()` 做整图平移
- `board.line_clears()` / `board.clear_lines(lines)` 做消行

**对应 cobra 哪一部分**
- 直接对应 `board.hpp` 的 `Board<H>`。

**与 cobra 的区别**
- cobra 固定宽 10、高模板集合 `6/12/18/24/48`；
- dev 让 `W/H` 都模板化，并保留 `row()/set_row()` 兼容旧 `TetrisMap` 行视图。

### 3. `src/tetris_shape.h`

**解决什么问题**
- 在编译期把 `RuleSpec` 中的 `OpDesc` 还原成可供 MoveGen 使用的几何数据：cells、pivot、origin、旋转目标、kick 表。

**怎么用**
- `shape::find_op<Spec, T, R>` 找到旋转态。
- `shape::piece_cells<Spec, T, R>` 取归一化后的 cell 列表、pivot、origin。
- `shape::target_cw/target_ccw/target_opp`、`shape::wk_*` 提供旋转元信息。

**对应 cobra 哪一部分**
- 对应 `header.hpp` 的 piece geometry + `gen.hpp` 中会消费这些几何的部分。

**与 cobra 的区别**
- cobra 用 anchor + offset 现场算旋转几何；
- dev 从 oracle 的 4x4 `OpLines` 反推规范化 geometry，并额外保存 `origin` 来映射回旧 `TetrisNode::status` 坐标。

### 4. `src/tetris_movegen.h`

**解决什么问题**
- 在不依赖 `TetrisContext/TetrisNode` 的情况下，做 cobra-style bitboard BFS，输出所有可达落点。

**怎么用**
- `movegen::MoveGen<Spec, 'T', true>::generate(board, spawn_x, spawn_y, callback)`
- `EnableMini=true` 时只对 T 块启用 Mini/Full T-Spin 语义；其他块或无需 spin 信息时走 `false`。
- 输出类型是 `LandingPos{x, y, r, spin}`。

**内部关键链路**
- `usable_map()`：该旋转下哪些 anchor 坐标不碰撞。
- `landable_map()`：哪些可放坐标已经着地。
- per-rotation `search[r]`：维护每个朝向当前已达坐标集合。
- 平移扩张收敛后，再 `expand_rotations()` 做 kick 旋转扩张。
- `emit_with_spin()` 用 `last_rotate_arr + corners3_arr + target_blocked_mask()` 区分 Full / Mini / None。
- `kCanonicalR` 对 O/I/S/Z 等对称朝向做输出级去重。

**对应 cobra 哪一部分**
- 直接对应 `movegen.hpp` 的 `MoveList`。

**与 cobra 的区别**
- 核心 reachability 算法相近；
- dev 必须恢复 oracle 语义：
  - 输出 master 坐标而不是 bbox anchor 原坐标；
  - last-rotate 标记参与 T-Spin 判定；
  - Mini 不是 cobra 风格简化判定，而是按旧 `search_tspin` 兼容。

### 5. `tests/perft_movegen.cpp`

**解决什么问题**
- 先在空盘上验证新 MoveGen 的枚举结果和数量是否稳定。

**怎么用**
- `perft_movegen <piece>` 或 `perft_movegen all [--mini]`
- 内置一份 minimal SRS RuleSpec，避免直接 include 老 rule 依赖整套 core/chash。

**对应 cobra 哪一部分**
- 对应 `apps/bench.cpp` 的 perft/枚举驱动。

**与 cobra 的区别**
- cobra 更偏 benchmark；
- dev 的 perft 更像“接 oracle_diff 之前的空盘自检器”。

### 6. `tests/oracle_diff.cpp`

**解决什么问题**
- 把新 MoveGen 与旧 oracle `search_tspin` 在相同棋盘上逐落点对拍，确保不丢语义。

**怎么用**
- `oracle_diff <piece>` 或 `oracle_diff all`
- oracle 侧：`TetrisEngine<rule_srs::TetrisRule, ai_zzz::TOJ, search_tspin::Search>` + 独立 `search_tspin::Search`
- new 侧：`MoveGen<NewRule, T, kMini>::generate(...)`
- 对拍单位：`(r, x, y, spin)` 元组集合。

**对应 cobra 哪一部分**
- cobra 没有直接对应物。

**与 cobra 的区别**
- cobra 没有历史 oracle 包袱，所以不需要这个层。
- dev 的主要价值正是“引入 cobra 核心思路，但不丢 oracle 功能”。

### 7. `src/tetris_core.h/.cpp` bridge 变更

**解决什么问题**
- 新编译期规则出现后，旧 `TetrisEngine / TetrisContext` 仍需继续工作。

**怎么用**
- 各 `rule_*.h` 暴露 `using rule_spec = detail::XxxRule;`
- `TetrisEngine::prepare()` 中：
  - `shared_context_->opertion_ = flatten_rulespec<typename TetrisRule::rule_spec>();`
  - `shared_context_->generate_ = TetrisRule::get_generate();`
- `detail::to_opertion<Op>()` / `op_create_bridge<Op>()` / `RotateSelector<>` 把编译期 `OpDesc` 变回旧 `TetrisOpertion`。

**对应 cobra 哪一部分**
- cobra 没有；因为 cobra 没旧引擎要兼容。

**与 cobra 的区别**
- 这是 dev 特有的“迁移适配层”，目的是让位板化过程可渐进落地，而不是一次性推倒重来。

## 与 cobra 的总对应关系

- `RuleSpec` ↔ `ruleset.hpp`：都表达规则，但 dev 明显更完整。
- `Map` ↔ `Board<H>`：都做 packed-bit board 与整体位移。
- `shape` ↔ `header.hpp` + `gen.hpp` 几何侧：都在服务 usable/landable 和旋转。
- `MoveGen` ↔ `MoveList`：都是 bitboard reachability engine。
- `perft_movegen` ↔ `apps/bench.cpp`：都是枚举/计数驱动。
- `oracle_diff` ↔ 无：这是 dev 为了兼容旧语义额外多出来的一层。

## 当前验证结果

- 已在沙盒通过 CMake 构建：`oracle_diff`、`perft_movegen`
- `./build/perft_movegen T --mini` 输出 34 个落点
- `./build/oracle_diff all` 覆盖 empty / tst / stsd / tsd / donation / sealed_top / 贴墙 kick 等局面，结果为 `# all diffs ok`
- 运行日志里有多条 `[oracle] piece=... spawn ... failed check` 提示，但对应 `sealed_top` 场景最终仍是 `count=0` 且总体对拍通过；说明测试主动覆盖了“出生即堵死”的分支

## 一句话总结

- `dev` 不是单纯“把 cobra 搬进来”，而是**用 cobra 风格的位板可达性引擎，重写 oracle 的 movegen 核心，同时保留旧 rule/search/AI 协议和历史语义**。
- 它的最大工程价值不在于新增一个更快的 MoveGen，而在于：**把整条位板化路线接进现有大工程时，没有断掉 oracle 这条验证链。**
