# Compile + Test Pass Report (commit 3b + commit 4)

## Build Environment
- 工作树 HEAD = `f952b28 Add master make_path self-check to oracle_diff`
- CMake Release LTO, gcc, C++20.
- Build dir: `tetris_ai_runner/build/`.

## 编译结果 (全 PASS)
| Target | 状态 |
|--------|------|
| `oracle_diff` | ✅ Built |
| `perft_movegen` | ✅ Built |
| `tetris_ai.so` (含 `MoveGenSearch<rule_toj/rule_srs>` 实例化) | ✅ Built (仅原有 pragma-once warning) |
| `tetris_ai_runner.so` | ✅ Built |

## 测试结果

### `./oracle_diff all` — **126/126 PASS**
- 7 piece × 18 boards = 126 个 case.
- 每个 case 同时跑 3 把尺子:
  1. `(r, x, y, spin)` 集合相等 (commit 3b 的 search 等价性)
  2. `has_last_rot` 在 spin>0 时必为 1 (commit 2-rot 的 last 反查)
  3. **`path_self_check`: master `make_path` 输出按键串 replay 终点等于 lp** (commit 4 新加)
- 终行: `# all diffs ok`

### `./perft_movegen all` — **PASS**
- 7 piece, 计数全部输出, 与历史输出一致.

## 这意味着什么

### Commit 3b 完全验证
- `MoveGenSearch::search` 走原生位板 `MoveGen<RuleSpec, T>` 路径.
- `LandingPos → context_->get(status) → TetrisNodeWithTSpinType` 反查 1:1 等价 master.
- 7 piece × 18 board × 多 r/spin 组合全过, 包括 TST / STSD / SSpin / TSD / TSS_left/right /
  donation / pc_opener / sealed_top / i_well / t_kick / j_left_well / bottom_pocket /
  sz_wall_spin / opp_chamber 等 SRS 边界情形.
- AI 主链路 `ai.cpp` / `pso.cpp` / `ppt_pso.cpp` 编译通过, 运行时不会段错误于 search 阶段.

### Commit 4 完全验证
- `replay_path` 没漏 vs.cpp 的 `node->row >= map.roof` 短路 — 意味着 master make_path
  输出的按键串不依赖那条短路 (BFS 内部已经按 `check(map)` 严格做合法性).
- 这把尺子可以信. 后续 commit 5 (位板 native make_path) 直接拿来比对终点即可.

### 两个 sealed_top 的 oracle warning (无害)
```
[oracle] piece=S spawn (x=3,y=21,r=0) ... failed check on map ...
[ok] sealed_top piece=S count=0
```
这是 sealed_top board 的预期行为 (顶部封死, spawn 无法落入), oracle 与新框架都返回空集.

## 下一步: Commit 5 (Native make_path)

### 目标
让 `MoveGenSearch::make_path` 不再委托 `impl_.make_path`, 改走 *单 cell BFS over status keys + WallKickList* 复刻.

### 路径
1. 在 `MoveGenSearch::make_path` 内部写一份 BFS, 用 `(t, x, y, r) → action_char + parent_status` 的 hash 表代替 `node_mark_`.
2. 邻居展开:
   - l/r/d: status 上做 dx/dy, 用 `Map<W,H>` 的位板 `usable_map_at` 检测合法.
   - L/R: 重复 l/r 直到不能再左/右.
   - x/z/c: 用 `shape::wk_cw/wk_ccw/wk_opp<Spec, T, R>` 列表展开 kick.
   - D: drop 用位板 `Map::drop_distance` 一次到底.
3. 优先级与 master 严格一致 (`Search::make_path` line 156-441 的 D→x→z→c→l→r→L→R→d→D 顺序).
4. **验证**: 直接在 `tests/oracle_diff.cpp::path_self_check_one` 里把 `oracle_search_obj()` 换成
   `MoveGenSearch` 实例, 比对终点必须等于 lp. 全 126 case 必须过.

### 风险
- Master 的 `disable_d` 双 pass 逻辑 (`Search::make_path` line 139, 444-451) 需要复刻.
- `node->land_point` 这条 cache 在 master `disable_d` 判定中使用; 位板版可以用 `inbounds_map<>` 计算 piece 在空盘上的所有可能起始位置.
- 700 行原代码, 拆分若干小 commit 比一刀切稳, 例如:
  - 5a: 在 MoveGenSearch 内部新写一份纯 pointer-graph 的 make_path (复制 master), 仅替换公开调用;
        oracle_diff 仍走 master 验证, 自身 path_self_check 也守门.
  - 5b: 把 wall_kick_*/move_*/rotate_* 替换成位板 / 静态表查询.

## 是否推送 origin?
**仍未推**. 用户编译验证全过, 但还没明确说推送. 等用户确认.
