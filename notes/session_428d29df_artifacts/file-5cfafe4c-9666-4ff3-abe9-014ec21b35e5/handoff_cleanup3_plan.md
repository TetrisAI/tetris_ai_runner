# Handoff: cleanup-3 plan (replace node_check_bb with usable_at_bb)

## Status snapshot
- Branch: `flip-bits` (local, unpushed)
- Last commit: `9b2a4bb` (Inline make_path BFS _s wrappers into BBState overloads)
- Baseline: `oracle_diff all` + `perft_movegen all` 全绿
- Context 已满，本会话不再改代码，下次直接按本计划执行。

## Goal of cleanup-3
彻底删除 `node_check_bb(TetrisNode const*, ...)` 这一过渡 helper。
BFS 内每一次 `node_check_bb(nX, usable_arr)` 都对应：
1. 上游已经从 `usable_arr` 鉴权过的 `BBState cur_state`；
2. 通过 `try_simple_move(cur_state, dx, dy)` 或 `rotate_no_kick(piece_t, dir, parent_state)` 获得 `TetrisNode const* nX`；
3. 我们已有 `usable_at_bb(BBState, usable_arr)`（5g-3b slice 5 引入）。

**等价改写规则：**
```
TetrisNode const *nX = try_simple_move(cur_state, dx, dy);
if (nX && path_mark_set(nX, cur_state, 'C') && node_check_bb(nX, usable_arr))
```
变为：
```
BBState nX_state;
TetrisNode const *nX = try_simple_move_bb(cur_state, dx, dy, &nX_state);
if (nX && path_mark_set(nX, cur_state, 'C') && usable_at_bb(nX_state, usable_arr))
```

且 `node_search_path_.push_back(state_from_node(nX))` 全部替换为 `node_search_path_.push_back(nX_state)`，省一次反查。

> 注意：`try_simple_move` 当前已在内部计算 BBState（slice 5），仅返回 master 节点。
> cleanup-3 需要给 `try_simple_move`/`rotate_no_kick` 增加可选 `BBState* out` 出参，
> 或增加同名重载返回 `pair<TetrisNode const*, BBState>`，避免 `state_from_node` 二次反查。

## 命中点清单 (search_replace 锚点)
位于 `src/movegen_search.h` 中 `make_path` 主 BFS 内（行号近似，重构后 BFS 区间集中）：

| # | 调用栈 | 出现处描述 |
|---|--------|-----------|
| 1 | `try_simple_move(cur_state, -1, 0)` -> nL | 左移分支根 |
| 2 | `rotate_no_kick(..., Opp, nL_state)` | nL 派生 180 |
| 3 | `rotate_no_kick(..., Ccw, nL_state)` | nL 派生 Z |
| 4 | `rotate_no_kick(..., Cw,  nL_state)` | nL 派生 C |
| 5 | `try_simple_move(cur_state,  1, 0)` -> nR | 右移分支根 |
| 6 | `rotate_no_kick(..., Opp, nR_state)` | nR 派生 180 |
| 7 | `rotate_no_kick(..., Ccw, nR_state)` | nR 派生 Z |
| 8 | `rotate_no_kick(..., Cw,  nR_state)` | nR 派生 C |
| 9 | `rotate_no_kick(..., Opp, cur_state)` | 同点 180（若存在） |
|10 | `rotate_no_kick(..., Ccw, cur_state)` | 同点 Z |
|11 | `rotate_no_kick(..., Cw,  cur_state)` | 同点 C |

> 实际计数以 `grep -n 'node_check_bb' src/movegen_search.h` 为准；预计 8~12 处。

## 步骤
1. **接口加宽（无功能变化）**：给 `try_simple_move(BBState, dx, dy)` 与 `rotate_no_kick(...)` 增加 `BBState* out_state = nullptr` 出参；返回前若非空则写入。先编译，提交一笔："Surface child BBState from BFS move helpers"。
2. **替换调用点**：12 个分支挨个改写为 `usable_at_bb(child_state, usable_arr)` + `node_search_path_.push_back(child_state)`。每改 2~3 个分支就 `oracle_diff sample-N`，确保不破坏。
3. **删除 `node_check_bb`**：grep 确认 zero references 后，删函数定义。提交："Drop node_check_bb after BFS migrates to usable_at_bb"。
4. **验证**：`oracle_diff all` + `perft_movegen all` 全绿。

## Cleanup-3 之后立刻进入 5h
- 收紧 `impl_` 引擎兜底范围，仅 20g 启用；非 20g 直接 trap。
- 然后才能开始 20g 位板路径重写。

## 提交规范提醒
- 一次变更 = 一次提交；commit message 描述 MR 相对目标分支的差异，不写"我又调了一下"。
- MR 标题英文、描述中文、提交英文且 ASCII。
- 推送由用户口头确认后再发起。
