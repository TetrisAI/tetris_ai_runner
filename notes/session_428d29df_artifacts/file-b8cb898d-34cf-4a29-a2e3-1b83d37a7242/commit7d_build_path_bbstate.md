# Commit 7d: build_path lambda 改 BBState + 20g spawn 折叠

## Goal
- 让 1g `make_path` 的 `build_path` lambda 直接吃 `BBState`；删掉 BFS 主循环
  里所有 `build_path(state_to_node(*wk))` / `build_path(*rn)` 之类的反查。
- 把 1g BFS 中残留的 `drop_bb(...)` -> `path_mark_set(node*)` 链 (`disable_d`、
  `allow_D`) 改成 `drop_bb_state` + `path_mark_set_state` + `index_filtered_eq_state`，
  整条 BFS 不再持有 `TetrisNode *`。
- `run_piece_20g` 入口 `spawn_node = context_->get(spawn_status)` 折成 BBState
  公式 `build_state_from_master<T,0>(3,21)`；T 路径 `build_snap` 需要的 master
  pointer 改在末段一次性反查。

## 改动 (movegen_search.h)
1. `make_path` 1g：
   - `build_path` 签名从 `TetrisNode const *cur` 改为 `BBState const &cur_state`，
     `cur_key` 改 `cells_key_for_state`，`(r,xb,yb)` 直接从 `cur_state` 取。
   - 调用方全部改：13 处 `build_path(state_to_node(...))` -> `build_path(...)`，
     入口 `build_path(node)` -> `build_path(entry_state)`。
   - `disable_d` / `allow_D` 三处 `drop_bb` 链改 `drop_bb_state` +
     `path_mark_set_state` + `index_filtered_eq_state`。
2. `run_piece_20g`：
   - 删 `TetrisBlockStatus spawn_status; spawn_node = context_->get(...)` 八行，
     替成 `BBState spawn_bb = build_state_from_master<T,0>(3,21);`。
   - T 路径 `build_snap` 在 `if constexpr (EnableT)` 内 `state_to_node(spawn_bb)`
     反查一次（只命中一次，整体调用次数不变）。

## 安全性
- `build_state_from_master` 已用于 7a-1 的 kick 链，公式 (`xb = sx + orig.x;
  yb = sy - orig.y`) 与 master `status_to_bbox_TR` 反向 1:1 等价。
- `build_path` 改 BBState 后 `path_mark_get_bb(r, xb, yb)` 直接吃 bbox 系，
  `cells_key_for_state` / `cells_key_for(node)` 是 `(T, R, xb, yb)` -> 等价 cells，
  替换前后一一对应。
- `disable_d` 路径用的 `path_mark_set_state` / `index_filtered_eq_state`
  与原 `path_mark_set(node*)` / `index_filtered_eq(node*)` 已在 7a-2 / 7c 验过等价。

## 验证
- `cmake --build build --target oracle_diff`：无 warning / error。
- `./build/oracle_diff`：1g + 20g 全场景 `# all diffs ok`。

## 后续可清理
- 1g `make_path` 仍保留 `path_mark_set(TetrisNode*, BBState, char)` /
  `path_mark_set(TetrisNode*, TetrisNode*, char)` 等指针入口的 set 包装，可与
  `state_to_node` / `state_from_node` 一并退役（仅 emit 阶段还要 master pointer）。
- `run_piece` (1g 非 20g) 中的 `context_->get(status)` 是另一组 emit 反查点。
