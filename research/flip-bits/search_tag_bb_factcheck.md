# search_tag 位板原生化事实校核 (Stage 0)

## 结论

**B 路径**：oracle `search_tag::Search` 的 BFS 与 `PathStrategy<TSpinHook>` 不等价；
`src/search_tag_node` 仍是指针图 BFS (BfsEngine 复刻 oracle), 没有位板原生化。
需要新建 `TagStrategy<SpinHook, RuleSpec>`, 删 `search_tag_node.{h,cpp}`,
把 `tests/tag_node_diff.cpp` 的"被测端"切到 `Searcher<TagStrategy, TSpinHook, rule_srs::TetrisRule::rule_spec>`.

## 关键证据

### 1. oracle/search_tag::Search 的 BFS 邻居与 search_path / search_tspin 不同

`oracle/search_tag.cpp:315-378` (search_t, T piece) 邻居顺序与算法:

```text
d  ->  l  ->  r  ->  z (rotate_counterclockwise, single rotate, NO wall_kick)
                ->  c (rotate_clockwise, single rotate, NO wall_kick)
//没有 'x' (180°)
```

`oracle/search_tag.cpp:215-310` (search, non-T piece) 邻居顺序:

```text
z (single rotate)  ->  c (single rotate)  ->  l  ->  r  ->  d
//没有 'x', 没有 'L'/'R' (扫到尽头), 没有 'D' (drop)
```

对比 `oracle/search_path.cpp:177-322` (search, 普通 path):

```text
x  ->  z  ->  c  ->  l  ->  r  ->  L  ->  R  ->  d (含 'D' 嵌套)
```

对比 `src/search_tspin.cpp:456-700` (生产 T-spin search): 走 wall_kick (`wall_kick_counterclockwise[]`),
有 'x' (allow_180), is_20g 决定 drop 行为. 与 oracle search_tag 不同.

### 2. PathStrategy<TSpinHook> 与 oracle search_tag 不等价

`src/search_path.h:206-234` (run_piece, 1g): `MoveGen<RuleSpec, T, SpinHook>::generate(...)`
内部走的是位板版 wall-kick + 'x' (180°) 完整邻居枚举, 落点集合 ≠ oracle search_tag (后者无 'x'/无 kick).

`src/search_path.h:407-434` (run_piece_20g): 邻居 ' '/'l'/'r' + first_passing_kick_bb 'x'/'z'/'c',
也是 wall-kick 版本, ≠ oracle search_tag 20g (single rotate, 无 'x').

### 3. src/search_tag_node 仍是指针图 BFS, 复刻 oracle search_tag

`src/search_tag_node.h:18-44` (`TagOneGravityNeighbors`):
```cpp
sink(node->rotate_counterclockwise, 'z');   // single rotate, no kick
sink(node->rotate_clockwise, 'c');           // single rotate, no kick
sink(node->move_left, 'l'); ...
//无 'x', 无 'L'/'R'/'D'
```

`src/search_tag_node.cpp:70-132` (`search_t_`): 用 `t_mark_.cover_if(rotate_*, ...)`
单步旋转, 与 oracle search_tag.cpp:351-365 一字对应 (但后者更早就有 cover_if 升级 ' '→'z'/'c').

`src/search_tag_node.cpp:134-200` (search): 几何条件 `node->land_point != nullptr && node->low >= map.roof`
分支到 `engine_20g_`, 否则 `engine_1g_` — 同 oracle.

### 4. tag_node_diff 当前对拍对象

`tests/tag_node_diff.cpp:35-63`:
- 左 (oracle): `oracle_tag_search()` → `search_tag::Search`
- 右 (被测): `node_tag_search()` → `search_tag_node::Search`

两者**都**是指针图实现, 都无位板. tag 的位板原生化**尚未完成**.

### 5. 生产调用面盘点

- `src/the_ai_games.cpp:13,14,371,372,451`: `TetrisEngine<rule_tag, ai_tag::*, search_tag::Search>`
  直接消费 oracle/search_tag (the_ai_games target 仅在 .vcxproj, **不在 CMake 中**, CMake 编译验证不覆盖此 TU).
- `src/ai_tag.h:47,48,109,110`: `typedef search_tag_node::Search::TSpinType TSpinType;`
  `typedef search_tag_node::Search::TetrisNodeWithTSpinType TetrisNodeEx;`
  ai_tag 内部 (ai_tag.cpp:520-523, 674) 访问字段 `is_check / is_last_rotate / is_ready / type`.
- `src/ai_tag.cpp` 不直接 include search_tag*, 但 ai_tag.h 的 typedef 会传到上层 the_ai_games target.

CMake 实际构建的 tetris_ai / tetris_ai_runner SHARED 库都包含 `src/ai_tag.cpp` + `src/search_tag_node.cpp`,
但**没有 `TetrisEngine<..., search_tag_node::Search>` 实例化**, 因此 search_tag_node 的对外 API
仅被 tag_node_diff 与 the_ai_games (vcxproj-only) 实质消费.

### 6. 字段类型迁移可行性

`search_tspin::Search::TetrisNodeWithTSpinType` 与 `search_tag_node::Search::TetrisNodeWithTSpinType`
字段 (node, last, type, is_check, is_last_rotate, is_ready) 一一对应; `is_mini_ready` 是 search_tspin 多
出来的, 不影响 ai_tag 既有访问. `TSpinType::TSpin` 在两个枚举中都是 1 (None=0, TSpin=1).

→ ai_tag.h 把 typedef 改到 search_tspin 类型, 行为 100% 兼容.

## 实施计划 (B 路径)

1. 新建 `src/search_tag.h`: `TagStrategy<SpinHook, RuleSpec>` (按 commit 2 strategy 三件套范式).
   - Context: 复用 `PathMarkMixin / BfsQueueMixin / StateNodeLutMixin / detail::tag::ExtrasMixin`.
   - search(): T piece 走 search_t_native (cover_if + single rotate); non-T 几何判定 20g/1g 路径.
     - 1g 邻居 z/c/l/r/d, **single rotate** via `Helpers::rotate_no_kick_bb`, **no 'x'**, **no L/R/D**.
     - 20g 邻居 z/c/l/r/d, single rotate, 加 `!open` 等价 (位板上 bridge probe 与 search_tag_node 同形).
   - make_path(): 邻居 z/c/l/r/d, single rotate, 末段对 land_point.last → land_point.node 单步 rotate
     反查 (与 oracle search_tag.cpp:39-159 一一对应).
2. 删 `src/search_tag_node.{h,cpp}`.
3. 改 `src/ai_tag.h`: `TetrisNodeEx = search_tspin::Search::TetrisNodeWithTSpinType`,
   `TSpinType = search_tspin::Search::TSpinType`. include 改 `search_tspin.h`.
4. 改 `tests/tag_node_diff.cpp`: 把 node_tag_search() 换成
   `Searcher<TagStrategy, TSpinHook, rule_srs::TetrisRule::rule_spec>` 实例,
   land_point 类型与 make_path 逻辑同步.
5. 改 `CMakeLists.txt`: 删四处 `src/search_tag_node.cpp`; tag_node_diff 链接 search_tspin / search_path /
   search_aspin (search_tag.h header-only). 同步 vcxproj.filters / tetris_ai_dll / tetris_ai_runner /
   the_ai_games / tetris_ai_runner.vcxproj.
6. 跑 oracle_diff / path_node_diff / simulate_node_diff / tag_node_diff 全绿.

### 与 PathStrategy 的本质差异 (TagStrategy mixin 列表)

| 维度 | PathStrategy<TSpinHook> | TagStrategy<TSpinHook> |
|---|---|---|
| 1g 邻居 | x/z/c/l/r/L/R/d/D + rotate_move 二级 | z/c/l/r/d (no x, no L/R/D, no rotate_move) |
| 1g 旋转 | wall-kick (first_passing_kick_bb) | single rotate (rotate_no_kick_bb) |
| 20g 邻居 | ' '/'l'/'r' + 'x'/'z'/'c' kick | z/c/l/r/d, single rotate |
| 20g 选路 | 由 SpinHook::config_is_20g 决定 | 由几何 (node->low >= map.roof && land_point ptr) 自动选 |
| make_path 末段 | wall-kick 链 (try_kick_chain_to) | single rotate 反查 (last->rotate_cw/ccw == node) |
| Mini ready | TSpinHook::check_mini_ready | TagStrategy 不读 mini (oracle search_tag 无 mini) |

(Mini 字段在 LandPoint 上仍存在但 TagStrategy emit 时不写 — 与 search_tag_node 当前行为一致.)
