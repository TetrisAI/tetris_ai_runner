# 阶段 0: Oracle 物理隔离 + R 还原 + extreme 退出对拍

## 目标

- 把 master tetris_core 整套 (含老 BFS search_*) 物理搬迁到独立 `oracle/` 目录, 与产线代码 (`src/`) 物理脱钩.
- 撤销 commit `3b713ce` 在 master 端把 `R<=4` 硬假设扩到 `16` 的改动: oracle 维持 SRS-4-rotation 假设, 不再假装支持非 SRS 几何.
- extreme_rule_diff (10 piece × 1..10 旋转) 不再对拍 oracle, 因 oracle 端注册 R>4 piece 必越界.
- 物理隔离, **不是** link 隔离 — 产线模块仍间接 link `tetris_oracle`. link-level 解耦留给 Phase F.

## 子步骤完成清单

- 0a Revert R 扩张
  - `src/tetris_core.h`: 删掉 `const int max_rotation = 16` 常量 + `TetrisMapSnap::row[max_rotation][...]` 还原回 `[4][...]`.
  - `src/tetris_core.cpp`: `node_block_.resize(type_max_ * max_rotation)` / 取下标 / `build_snap` 循环 全部还原 `4`.
  - `src/piece_filter_index.h` 中 `target_ccw_at<...>` 通用递归保留: 它只在新框架 (kFilteredIndex) 用, 与 oracle 无关, 且本身就是更优的实现.

- 0b 物理搬迁 (10 个文件; 已搬到 `oracle/`)
  - `tetris_core.{h,cpp}`
  - `search_simple.{h,cpp}`
  - `search_path.{h,cpp}`
  - `search_simulate.{h,cpp}`
  - `search_tag.{h,cpp}`
  - 移动方式: `git mv` 保留历史. 头文件名不变, 通过给消费者 target 增加 `-I oracle/` include path 让它们继续可见.

- 0c CMake 拆分
  - 新增 STATIC library `tetris_oracle` 收纳上面 5 对 .cpp/.h.
  - `target_include_directories(tetris_oracle PUBLIC oracle src)` (oracle 的 .cpp 仍引 src/ 共享头如 random.h, integer_utils.h).
  - 主库 `tetris_ai` / `tetris_ai_runner` 从源列表移除上述 5 个 .cpp, 改为 `target_link_libraries(... PRIVATE tetris_oracle)`, 同时 `target_include_directories(... PRIVATE src oracle)` 让仍引 `tetris_core.h` 的 `src/*.h` 能找到头.
  - 测试 / 可执行 target (`oracle_diff` / `aspin_dump` / `extreme_rule_diff` / `botris` / `cmd_tris` / `top_pso` / `perft_movegen`) 全部按相同模式接 `tetris_oracle`.
  - `tetris_oracle` 设 `POSITION_INDEPENDENT_CODE ON` 以便被 SHARED `tetris_ai` 静态吸入.
  - `oracle_diff.cpp` / `extreme_rule_diff.cpp` / `aspin_dump.cpp` 中的 `#include "../src/tetris_core.h"` 等改为 `#include "../oracle/...h"`.

- 0d extreme_rule_diff 摘掉对拍
  - master tetris_core 维持 SRS-4-rotation 硬假设. 即便只跑 R<=4 piece (A..D), `TetrisContext::prepare(rule_extreme)` 也会一次性把 R=5..10 piece 全注册进 `node_block_`, 立刻越界 (运行起步即 segfault).
  - 当前 `extreme_rule_diff` 把所有 10 个 piece 全部走新框架 self-consistency dump: `[ok-new-only] <fixture> piece=<X> R=<n>`, 不再调 `oracle_landings`.
  - `oracle_engine()` / `oracle_landings()` 标 `[[maybe_unused]]` 保留, 为 Phase F (oracle 端 R<=4 限制取消, 或新增不依赖 master 的 oracle 实现) 留 entry.
  - 历史 `research/flip-bits/dumps/extreme_*_oracle.txt` 不再被本测试刷新, 作为基线归档保留.

- 0e 决策记录
  - 本文件.

## 物理目录拓扑

```
tetris_ai_runner/
  src/                         产线 (新框架 + 仍依赖 oracle 的过渡层)
    tetris_movegen.h           新 BFS movegen
    movegen_search.h           AI 桥
    search_hook.h              T/A spin hook 抽象
    piece_filter_index.h       新框架 emit-time filter
    search_aspin.{h,cpp}       仍 #include "tetris_core.h" (façade)
    search_tspin.{h,cpp}       仍 #include "tetris_core.h" (façade)
    rule_*.{h,cpp}             两边共享 (RuleSpec + master generate)
    ai_*.{h,cpp}               仍依赖 oracle context
  oracle/                      master tetris_core + 老 BFS, SRS-4-rotation only
    tetris_core.{h,cpp}
    search_simple.{h,cpp}
    search_path.{h,cpp}
    search_simulate.{h,cpp}
    search_tag.{h,cpp}
```

## CMake target 拓扑

```
tetris_oracle (STATIC)
  ├─ oracle/{tetris_core, search_simple, search_path, search_simulate, search_tag}.cpp
  └─ PUBLIC include: oracle/, src/

tetris_ai (SHARED)        --link--> tetris_oracle
tetris_ai_runner (SHARED) --link--> tetris_oracle
top_pso (exe)             --link--> tetris_oracle
botris (exe)              --link--> tetris_oracle
cmd_tris (exe)            --link--> tetris_oracle
perft_movegen (exe)       --link--> tetris_oracle  (经 src/search_aspin.h 间接 #include)
oracle_diff (exe)         --link--> tetris_oracle  (本来就是它的目的)
aspin_dump (exe)          --link--> tetris_oracle  (产线 façade 间接依赖)
extreme_rule_diff (exe)   --link--> tetris_oracle  (oracle entry 仍编译, 运行时不调用)
```

> 阶段 0 让所有 target 都吃 `oracle/` include path + 链接 `tetris_oracle`. 这跟 link 解耦是正交的 — 物理上 master 已经搬走, 看起来也是独立 lib, 后续 Phase F 只需要把 `src/search_aspin.h` / `src/search_tspin.h` / `src/ai_*.h` 里 `#include "tetris_core.h"` 全部抽走, 就能让产线 target 真正不再 link `tetris_oracle`.

## R>4 测试策略变更

- 之前 (commit `3b713ce`): 试图通过 master `max_rotation = 16` 让 R>4 piece 也能在 master oracle 端跑, 与新框架对拍. 这把 SRS-only 假设强行扩到 16, 长期不可持续 (master 还有大量隐式 4 假设, 且 Phase F 准备彻底丢弃 master).
- 阶段 0 后: master 不再为 R>4 让步, R>4 piece 全部脱离 master oracle 对拍. extreme_rule_diff 改为单边 dump:
  - `kFilteredCount` 自洽 (新框架内部的 emit-time filter index 合并相同).
  - 历史 `*_new.txt` dump 仍可用 git 历史回溯比较, 作为回归探针.
- SRS 范围 (`oracle_diff`) 完全不受影响, 仍 byte-equal.

## 验证

```
cmake --build build -j   全部 target 通过编译
./build/oracle_diff      # all diffs ok
./build/extreme_rule_diff # all extreme-rule diffs ok (50 个 [ok-new-only])
./build/aspin_dump /tmp/aspin_after_phase0.txt
                         # board=... count=... 行与 baseline 完全一致
                         # (baseline 字段是 commit 172d0b8 之前的 idx/row/type 格式, 历史遗留;
                         #  本次差异仅来自该 commit 删 row 字段 + ef582c3 新增 aspin_all_spin
                         #  fixture, 与本阶段无关.)
./build/perft_movegen all  完整跑通
```

## 后续阶段提示 (非本 commit 范围)

- Phase A/B/C: 把 `src/search_aspin.h` / `src/search_tspin.h` / `src/ai_*.h` / `src/rule_*.h` 中显式的 `tetris_core.h` 依赖逐层抽走, 直到产线模块不再需要 master.
- Phase F: 当所有产线 .h 都不再 `#include "tetris_core.h"` 时, 主库 `tetris_ai` / `tetris_ai_runner` 不再 link `tetris_oracle` — 此时 oracle 真正只服务 oracle_diff.


---

## Phase 0 收尾状态 (2026-05-29)

- 编译验证：`cmake -DCMAKE_BUILD_TYPE=Release && cmake --build .` 全部 target 通过 (tetris_oracle / tetris_ai / tetris_ai_runner / oracle_diff / extreme_rule_diff / aspin_dump / perft_movegen / botris / cmd_tris / top_pso)。
- Smoke test：
  - `./extreme_rule_diff` → `# all extreme-rule diffs ok`（oracle 已脱钩，所有 R>4 piece 走 new-only 分支）
  - `./oracle_diff` → `# all diffs ok`（R<=4 仍可与 oracle 对拍）
- 格式化：staged 范围内 8 个 oracle/* 文件由 clang-format 重排（用户规则下保留局部变量非 const）。
- 单 commit 已落地（commit hash: 6758178，title: `Archive legacy oracle into oracle/ directory and decouple extreme rules`），**未 push**，等待用户确认后再推送。
