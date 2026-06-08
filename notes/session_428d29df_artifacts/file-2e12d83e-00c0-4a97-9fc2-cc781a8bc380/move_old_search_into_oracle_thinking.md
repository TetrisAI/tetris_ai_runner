# 新 search 框架迁移 — 最终落地计划 (b5a6ca3 之后)

> 用户拍板: "搬回 oracle 文件的从 git 历史中寻找干净的版本，我们现在需要搞定新版本实现"
> 即: 不再纠结搬运的中间态, 重点是把新版 _node 实现做出来; oracle 那边用 git 历史里的干净版本
>
> HEAD: b5a6ca3
> 上轮分歧已对齐: 不需要先做"头解耦"前置 commit, 直接按"先写新, 后搬旧"顺序推

---

## 1. 必须遵守的不变量

- a578cc8: 产线 target 不能 include `oracle/`
- c6c806a: oracle 仅在 diff harness 中链接
- 单一 commit 原则: 每笔 commit 自成一个 atomic 改动
- 不试图编译验证 (无环境)
- 提交前 clang-format
- 不主动 push, 等用户 confirm

## 2. 当前 src/search_* 真实状态

| 文件 | 行数 | 当前框架 | 说明 |
|---|---|---|---|
| search_simple_node | 35+165 | 自写 | 上轮 b5a6ca3 |
| search_path_node | 245+107 | BfsEngine | 8c0ab54 |
| search_simulate_node | 106+155 | BfsEngine | f3a5b5e |
| search_tag_node | 175+245 | BfsEngine | c6c806a |
| **search_aspin** | 76+95 | MoveGenSearch + DefaultASpinHook (5285fde) | 现代化 façade |
| **search_cautious** | 28+473 | 手写 BFS + WallKick | oracle 风格 |
| **search_tspin** | 92+1126 | 手写 BFS + WallKick + last 重放 + 20g | 体量最大 |

## 3. 提交规划 (按顺序 5 笔)

### Commit M1: search_cautious_node (新)

新增文件:
- `src/search_cautious_node.h`
- `src/search_cautious_node.cpp`

设计:
- 复用 BfsEngine
- 提供 `WallKickFullNeighbors` (x/z/c 都展开 wall_kick_*, l/r/L/R, d/D/fast_move_down 二选一)
- 1g 路径: 两阶段循环 (drop-equal-target → straight-equal-target), 与 master 行为对齐
- search 阶段: WallKick 邻居 + IndexedDedup + LandPointCollector
- make_path: WallKickFullNeighbors + ParentTrackingDedup + TargetHitVisitor

CMake 改动: 在 tetris_ai/tetris_ai_runner 的 source 列表里加 `src/search_cautious_node.cpp`

不切换 caller. 单纯加文件.

### Commit M2: search_aspin_node (新)

新增文件:
- `src/search_aspin_node.h`
- `src/search_aspin_node.cpp`

设计:
- 复用 BfsEngine
- 提供 `WallKickFullNeighbors` (与 cautious 共享或单独一份 — 按 Config 分支)
- ASpin 角点判定: lp.type 在 Visitor 里算 (复用 5285fde 之前 `5285fde~1` 版手写公式: 落点 4 个相邻 status 位置都不可放置)
- search() 输出 `LandPoint = TetrisNodeWithASpinType`
- TetrisNodeWithASpinType 类型沿用 `search_aspin::Search::TetrisNodeWithASpinType` 的 ABI (新 namespace 自定义同形 struct)

CMake 改动: 加 `src/search_aspin_node.cpp`

### Commit M3: search_tspin_node (新)

新增文件:
- `src/search_tspin_node.h`
- `src/search_tspin_node.cpp`

设计:
- 自写复合 search (T-spin 不是单纯 BFS, 需要带 last 前驱). 不强行套 BfsEngine
- 1g 分支: WallKick 邻居 + ParentTrackingDedup
- 20g 分支: 沿用 search_path_node 的 20g land_point + bridge 思路, 但 dedup key = (node, last). 这一步可以接受短期偏差 (回归延后)
- check_ready / check_mini_ready: 直接照搬 master `search_tspin::Search::check_ready` 公式
- make_path: 1g 用 ParentTrackingDedup 重建; 20g 用 path_redesign.md 思路

CMake 改动: 加 `src/search_tspin_node.cpp`

### Commit M4: 切换 caller 到 _node

改:
- `src/ai_zzz.h`: `search_tspin::Search::*` → `search_tspin_node::Search::*`; `search_aspin::Search::*` → `search_aspin_node::Search::*`
- `src/ai_misaka.h`: 同上
- `src/ai.cpp`:
  - `search_aspin::Search` → `search_aspin_node::Search`
  - `search_cautious::Search` → `search_cautious_node::Search`
  - srs_ai 用的 `MoveGenSearch<rule_toj::rule_spec>` → `search_tspin_node::Search` (产线 SRS T-spin 路径切走)
  - `#include` 改成 `_node` 头
- `src/pso.cpp` / `src/ppt_pso.cpp`: `MoveGenSearch<rule_srs::rule_spec>` → `search_tspin_node::Search`
- `src/botris.cpp` / `src/cmd_tris.cpp`: `search_aspin::Search` → `search_aspin_node::Search`
- `src/vs.cpp`: `search_cautious::Search` → `search_cautious_node::Search`

### Commit M5: 把 src/search_{aspin,cautious,tspin}.{h,cpp} 和框架头搬进 oracle/

搬运清单:
- `src/search_aspin.{h,cpp}` → `oracle/search_aspin.{h,cpp}` (用 `5285fde~1` 版手写干净版本, 不是 HEAD façade 版本)
- `src/search_cautious.{h,cpp}` → `oracle/search_cautious.{h,cpp}` (HEAD 干净, 直接搬)
- `src/search_tspin.{h,cpp}` → `oracle/search_tspin.{h,cpp}` (HEAD 干净, 直接搬)
- `src/search_hook.h` → `oracle/search_hook.h` (它本来就是为 search_aspin/tspin 旧路径服务的 hook)
- `src/movegen_search.h` → `oracle/movegen_search.h`
- `src/tetris_movegen.h` → `oracle/tetris_movegen.h` (MoveGen 实现, 仅 movegen_search 用)
- `src/piece_filter_index.h` → `oracle/piece_filter_index.h` (kFilteredTable, 仅 movegen 用)
- `src/tetris_rule_spec.h` → `oracle/tetris_rule_spec.h` (RuleSpec ops 元数据, 仅 movegen 用)
- `src/tetris_shape.h`: 评估 — 也许产线还要 (rule_*.cpp 看)

CMake 改动:
- tetris_oracle 加 4 对源文件
- tetris_oracle 的 PUBLIC include 加 oracle/ (本来就有)
- 产线 target 的 source 列表里删 search_{aspin,cautious,tspin}.cpp
- oracle_diff / extreme_rule_diff / aspin_dump / perft_movegen 链接 tetris_oracle, include 'oracle' (已是)

这一步的危险点: extreme_rule_diff 用 kFilteredTable, perft_movegen 用 MoveGen, oracle_diff 用 MoveGenSearch+DefaultTSpinHook. 它们都 include 在 oracle/ 里的头. **CMake target_include_directories(... PRIVATE oracle)** 才能编过.

## 4. 单 commit 原则的妥协点

- M1/M2/M3 单纯新增, 编不进 production target (CMake 不加)? 或者加进去并保留旧的? **决定: 加进 CMake. 旧的同时保留. M4 才切 caller.**
- M5 才会真的物理搬旧文件. M1-M4 之间, 旧的 search_{aspin,cautious,tspin}.{h,cpp} 仍在 src/, CMake 仍编它们. 这是必要妥协: 否则 MoveGenSearch / search_hook 会立刻编不过.

## 5. 跳过的不安全步骤

- **回归测试**: 用户已说"回归以后再说". 不写 *_node_diff. 不跑 oracle_diff (需要 build).
- **clang-format**: 每笔 commit 前格式化.
- **不试图编译**.
- **不 push**.

## 6. 接下来执行 M1
