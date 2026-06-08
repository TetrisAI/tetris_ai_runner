# 20g 重写：在动手前的反驳与共识请求

## TL;DR
**现状：oracle_diff / perft_movegen 均不覆盖 20g。直接做 20g 位板 BFS 重写 = 无回归网重构。**
本工程节奏从一开始就是"每步 oracle_diff 全绿"，到 20g 这一步前提就破了，不能闷头改。

## 调研核实
- `oracle_diff.cpp` 全文未出现 `is_20g = true`，构造的 `Config` 默认 `is_20g=false`。
- `perft_movegen` 同理，没有 fixture 把 `is_20g` 翻成 true。
- `search_tspin::Search` 的 20g 实现（src/search_tspin.cpp 466、617、981、995 与函数 `make_path_20g` 710）至此从未被本仓内自动化测试碰过。

## 选项
- **A**：补 20g oracle/perft fixture（推荐）。
  - 扩 `oracle_diff` 让 fixture 跑两轮：`is_20g=false` + `is_20g=true`。
  - 在 master 路径作为 oracle 的同一断言下，跑本仓的位板新代码（candidate）。
  - 然后再开 20g 位板 BFS 重写。
- **B**：用户提供独立的 20g 测试 / 回归 fixture，我接入。
- **C**：明确接受无网迁移风险，硬干 step 1。

## 步骤详细（如选 A）
1. 在 `tests/oracle_diff.cpp` 增加 `bool is_20g_flavor` 的循环外包装，每个 fixture 跑两遍。
2. master 路径与位板路径用同一 `Config`，断言 `land_point_cache_` 集合（按等价类排序后）逐项相等。
3. 现状下：master 跑 20g 走 `search_tspin::Search` 老逻辑；位板这边 `MoveGenSearch::search` 仍走 `impl_.search`（即同一个老逻辑）—— 所以 A 的第一步会是恒等通过的。这就证实了 oracle 框架本身在 20g 下也能跑。
4. 接着进入 step 1 改造 `run_piece_20g<T>`，oracle 立即开始报实际 diff。

## 已落地的 step 1 调研材料
保留在 `tetris_ai_runner/research/flip-bits/20g_search_master_analysis.md`，
包含 master `search`/`search_t`/`make_path_20g` 的 20g 分支语义剖析与位板侧已有原料清单。

## 等待用户裁决
按你"如果改动要求不自洽就先反驳"的指令，我在你回复 A/B/C 之前不再动 src/。
