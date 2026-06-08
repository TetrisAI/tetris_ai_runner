# Commit 7c: Inline simple-move + drop_bb_state in 1g/20g BFS

## Goal
- 把 `make_path` 1g BFS / `make_path_20g_native` / `run_piece_20g` 中所有
  "BBState +dx/+dy => state_to_node 探测可达 + usable_at_bb 鉴权" 的双重检查
  砍成纯位板检查 (`usable_at_bb`).
- BFS 内 spawn / 出队后 drop / LR 循环 drop 改用 `drop_bb_state`, 直接拿
  `BBState`, 不再借 `state_to_node` 反查 master pointer.

## Code changes (movegen_search.h)
1. `make_path` 1g BFS (line ~746):
   - L / R 单步分支去掉 `try_simple_move`, 改 `BBState ns = cur; ns.xb += ±1;`
     + `usable_at_bb`.
2. `make_path_20g_native`:
   - 入口 `start_sunk` 改 `drop_bb_state(start_state, ...)`.
   - `build_path` 内 `cur_key` 改 `cells_key_for_state` (省 1 次 hash).
   - `check_hit` 改 `check_hit_state(BBState)`, `try_push_neighbor` 走
     `drop_bb_state`.
   - L / R 循环不再 `state_to_node` 探可达, 全用 `usable_at_bb`, 终点存
     `BBState terminal_state`. drop 用 `drop_bb_state`.
3. `run_piece_20g`:
   - spawn 后 sink 改 `drop_bb_state`.
   - 队列出队后 drop 改 `drop_bb_state`.
   - LR / D 邻居生成砍 `try_simple_move`, 改纯 BBState 推进.
   - emit 阶段 `state_to_node(ss)` 仅在 landing 命中时执行, 避免每一帧出队都
     反查 master pointer.
4. 删除 `try_simple_move` 两个重载 (BBState->TetrisNode * 出口) 及其注释,
   全 BFS 已无调用方.

## Why this is safe
- master 平移与 bbox 平移 1:1 等价 (origin 仅平移基准, 不影响 dx/dy 增量).
- `usable_at_bb` 已包含 in-bounds + board collision 双重检查, 与 master
  `move_*` + `check(map)` 等价.
- emit 阶段仍用 `state_to_node` 拿到 master pointer 走 `build_snap` /
  `check_ready_native` / `TetrisNodeWithTSpinType` ctor. 因此 land_point
  集合及 spin tag 完全保持. oracle_diff 1g + 20g 全场景 `# all diffs ok`.

## 验证
- `cmake --build build --target oracle_diff` 编译通过, 无新警告.
- `./build/oracle_diff` 全集 ok.

## 后续
- 7d: `run_piece_20g` 的 `spawn_node = context_->get(spawn_status)` 仍是
  整个 BFS 唯一指针入口. T 路径需要 `spawn_node->build_snap` (依赖
  `context->get_block`); 非 T 路径只需 `spawn_node` 把 spawn_bb 反推, 可用
  直接 BBState `(3 + orig.x, 21 - orig.y, 0)` 替代. 与 emit 阶段
  `state_to_node` 一并清理.
- 7e+: 把 `make_path` 1g 中末段 `build_path(state_to_node(*wk))` 等等的反查
  汇总到 emit 出口, 留作下一刀.
