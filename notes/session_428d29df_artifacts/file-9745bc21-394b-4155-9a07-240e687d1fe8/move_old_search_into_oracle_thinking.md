# "旧 search 全部移入 oracle, 实现新 search" — 反驳与对齐

> 触发: 用户上一句 "旧的 search 都放进 oracle，我们实现新的，直接开始吧，回归以后再说"
> 上下文: 上轮我提议在 3 个 commit 里把 search_aspin / search_cautious / search_tspin 迁到 BfsEngine, 用户的回复扩大了范围

---

## 1. 当前 src/search_* 真实状态

| 文件 | 行数 | 依赖 / 框架 | 备注 |
|---|---|---|---|
| search_simple_node.{h,cpp} | 35+165 | 无 (自写 BFS) | 上轮 b5a6ca3 新写的 naive-player |
| search_path_node.{h,cpp} | 245+107 | BfsEngine | 8c0ab54 |
| search_simulate_node.{h,cpp} | 106+155 | BfsEngine | f3a5b5e |
| search_tag_node.{h,cpp} | 175+245 | BfsEngine | c6c806a |
| search_aspin.{h,cpp} | 76+95 | **MoveGenSearch + DefaultASpinHook** (5285fde 现代化版) | façade, 不是手写 BFS |
| search_cautious.{h,cpp} | 28+473 | 手写 BFS | oracle 风格, 全 wall-kick |
| search_tspin.{h,cpp} | 92+1126 | **手写 BFS + WallKick + last 重放 + 20g** | 没进过 MoveGenSearch, 体量是 oracle/search_path 的 1.5x |

## 2. 把它们搬进 oracle/ 的硬障碍

`src/search_hook.h:5-6` 里有:
```cpp
#include "search_aspin.h"   // for search_aspin::Search::TetrisNodeWithASpinType
#include "search_tspin.h"   // for search_tspin::Search::Config / TetrisNodeWithTSpinType
```

`src/movegen_search.h:33` 里有:
```cpp
#include "search_tspin.h"
```

**结果**: 把 `search_aspin.h` / `search_tspin.h` 搬到 oracle/ 的瞬间, 整个 `tetris_ai` / `tetris_ai_runner` / `top_pso` / `botris` / `cmd_tris` / `aspin_dump` 全部编译失败. 解决路径有三条:

- **A.** 连 `search_hook.h` / `movegen_search.h` 一起搬进 oracle/ → 等于"放弃 MoveGenSearch 框架". 这是产线 5285fde 之后辛苦建起来的位板原生加速, 整片代码 (`m4b/movegen + RotState + corners3 + spin_block`) 都失效, **重大回滚**.
- **B.** 让 `search_hook.h` / `movegen_search.h` 不再依赖 `search_aspin/tspin.h` (拷一份 Config / LandPoint 类型出来) → 框架仍在, 但旧 façade 不再生效, 而且新 _node 文件还要自己再供一套 Config 类型, **类型分裂**.
- **C.** 不真的搬, 只是 *rename* `src/search_{aspin,cautious,tspin}.{h,cpp}` → `oracle/` 同时让 oracle target 把它们编进去, 但 `src/search_hook.h` 仍然 `#include "../oracle/search_aspin.h"` → 变相把 `oracle/` 加回 production include path, **直接违反 a578cc8 引入的 oracle/production 隔离**.

## 3. 关键诉求是否自洽

用户上一轮我们已经达成的不变量:
- a578cc8: 产线 target **不能** include `oracle/`
- c6c806a: oracle 仅作为 diff baseline 存在

如果现在执行"旧 search 都搬进 oracle/", 同时不松动上述不变量, 唯一能落地的是 **方案 B** — 也就是说要先做"把 search_aspin/tspin 的公共类型从 oracle 头文件里剥离"这步前置改造, 再搬文件. 否则方案 C / A 都会破坏已立的不变量.

## 4. 我的反驳与提议

**反驳点**:
1. search_aspin 已经是 MoveGenSearch 现代化 façade, 不是手写 BFS. 它跟 search_cautious 不是同一类"旧". 把它搬进 oracle 等于扔 5285fde / edd96ee 两个 commit 的成果. 是不是 a/spin 路径下次再单独议?
2. search_tspin 是真"旧"且体量最大 (1126 行 + WallKick last 重放 + 20g 分支). 直接迁到 BfsEngine 没有 master baseline, 用户已说"回归以后再说" → 我接受短期偏差, 但需要明白迁完之后 oracle_diff 这条 baseline 链会失去对 production 的约束 (production 从 search_tspin 切走, oracle_diff 仍在 oracle vs MoveGenSearch 之间对拍, **不再覆盖产线**).

**提议执行顺序** (待确认):
- **commit Y0**: 拆 search_aspin.h / search_tspin.h 的"公共类型" (Config / TSpinType / TetrisNodeWith*) 到一个不依赖具体 search 实现的头 (例如 `src/search_types.h`), `search_hook.h` / `movegen_search.h` 改 include `search_types.h`. 这一步**不搬文件**, 只解耦头依赖. 产线行为完全不变.
- **commit Y1**: 把 src/search_{aspin,cautious,tspin}.{h,cpp} 物理搬到 oracle/, 加进 tetris_oracle, 产线 CMake target 不再编它们. 此时产线**临时**没有 aspin/cautious/tspin (会编不过), 所以 Y1 必须跟 Y2/Y3/Y4 顺序连续推 (本地暂存, 不 push).
- **commit Y2**: src/search_cautious_node.{h,cpp} (BfsEngine + 全 wall-kick neighbors).
- **commit Y3**: src/search_aspin_node.{h,cpp} (BfsEngine + ASpin 角点判定). 注意 ASpin 角点判定算法目前活在 DefaultASpinHook, 重写到 _node 内部.
- **commit Y4**: src/search_tspin_node.{h,cpp} (BfsEngine + last 前驱 + check_ready/mini + 20g). 算法量最大, 可能还要拆.
- **commit Y5**: ai.cpp / pso.cpp / ppt_pso.cpp / botris.cpp / cmd_tris.cpp / vs.cpp / ai_zzz.h / ai_misaka.h / extreme_rule_diff 切换 typedef.

## 5. 待用户确认

**请回答 Q1 / Q2 / Q3, 我才能开始 Y0**:

- **Q1**: 接受**方案 B (commit Y0 先解耦头)** 这条路径吗? 还是你愿意接受**方案 C (oracle/ 重新进 production include path)** 临时退步?
- **Q2**: search_aspin 真的要搬吗? 它是 MoveGenSearch 现代化版, 不是手写旧 BFS. 单独保留不动也行?
- **Q3**: 把 5285fde / edd96ee 引入的 MoveGenSearch + Hook 框架本身 (search_hook.h / movegen_search.h / RotState / DefaultTSpinHook / DefaultASpinHook), 是否一并视作"旧框架要清理"? 还是它继续留在 src/, 只是上面跑的 search_*.cpp 改名 _node?

---

## 6. 重要假设 (默认)

- 不动 `search_hook.h / movegen_search.h / tetris_movegen.h / piece_filter_index.h / tetris_rule_spec.h / tetris_shape.h` 五个框架头. 它们不是"search 实现".
- 不动 `extreme_rule.h / rule_extreme.h` (它们也用 MoveGenSearch / kFilteredTable).
- oracle_diff / extreme_rule_diff / aspin_dump 这些 diff/工具 target 沿用 oracle baseline.
