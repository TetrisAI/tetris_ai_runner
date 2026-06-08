# Commit 4 (Path Oracle) Handoff

## 状态
- 分支: `flip-bits-clean` (HEAD = `f952b28` Add master make_path self-check to oracle_diff)
- **未推送**, 等用户确认.
- Commit 序列:
  - `4e248a0` Drive MoveGenSearch::search through native bitboard MoveGen (commit 3b)
  - **`f952b28` Add master make_path self-check to oracle_diff (commit 4)** ← 本次落地

## 为什么是 path oracle 而不是 native make_path
原计划 commit 4 = 重写 make_path. 但分析后认为风险过高:
1. `make_path` 是优先级 BFS, 输出按键序列; bit-parallel 不能表达 per-cell predecessor-action.
   "用 MoveGen 重写" 是 misleading — 实际只是把单 cell 指针 BFS 翻译成单 cell 位板 BFS,
   **没有 bit-parallel 加速**, 仅消除对 `wall_kick_*` 等指针图依赖.
2. 当前没有 path-level oracle, 重写 700 行优先级 BFS 一旦写错某个分支顺序,
   oracle_diff 全过但 AI 路径偏 — 用户跑 boe 才能发现.
3. 当前环境无法编译 + 无 path 测试 + 700 行手动重写 = 必然引入回归.

按 "先验证后改造" 原则, **本 commit 4 做 path-level 自洽检查**, 给 commit 5 (native make_path) 落地铺路.

## 提交内容 (`tests/oracle_diff.cpp` 一文件 +194/-1)
- `replay_path(node, keys, map)`: 完全照搬 `src/vs.cpp:120-186` 的按键 replay 语义,
  覆盖 l/r/d/L/R/D/x/z/c 以及 `allow_rotate_move` 分支的 X/Z/C.
- `path_self_check_one(piece, board, name)`:
  - 取 oracle search 输出.
  - 对每个 `TetrisNodeWithTSpinType` 跑 `oracle_search.make_path(spawn, lp, map)`.
  - 在 master 端 replay, 比对终点:
    * `lp.type != None` (T-spin): 严格 `end == lp.node` (rotation 必须精确还原).
    * `lp.type == None` (普通): `index_filtered` 相等 (I/S/Z 0↔2/1↔3 视为同一放置).
- 钩入 `diff_one`, 与现有 `last_fail / a==b` 一并决定 case 通过.

## 风险与下一步

### 已知风险 (需用户编译验证)
- `replay_path` 假设 `node->wall_kick_*` 是 SRS 的 5-step kick list, 第一个 in-bounds 的
  即被选中. 这与 master `Search::make_path` 内部 `for (wk : node->wall_kick_X)` 循环
  逻辑一致. 但如果 master 在某些 board state 下打破这个一致性 (例如 dynamic wallkick),
  本 replay 可能与 make_path 自身的反向回溯不严格自洽.
- 没编译, 没跑过. 用户编译后第一步: `./oracle_diff all`. 若 path-mismatch 出现:
  * 大概率说明 `replay_path` 漏处理了某个边界 (例如 `node->row >= map.roof` 那条
    vs.cpp 的快速路径在 make_path 内不适用 — 我没复制那段, 因为 master make_path
    的 BFS 内每步都做 `check(map)` 而非 roof-bypass).
  * 修法: 在 `replay_path` 的 l/r 分支同步 vs.cpp 的 `node->row >= map.roof` 短路;
    我没加是因为不确定 make_path 输出的按键串里 l/r 步是否假设了这条短路.

### Commit 5 (后续)
有了这把尺子, native make_path 可以这样落地:
1. 在 `MoveGenSearch::make_path` 写一份位板 / 单 cell 指针图 BFS, 复刻
   master Search::make_path 的 D→x→z→c→l→r→L→R→d→D 优先级链.
2. 跑同一份 `replay_path` 验证终点一致.
3. 二进制层面新旧两套按键串无需逐字相等 (BFS 优先级不同分支可达同终点),
   只要终点一致即合格.

## 不可推送
- 与之前一致, 等用户编译验证后再推. 当前 origin 远端零变化.
