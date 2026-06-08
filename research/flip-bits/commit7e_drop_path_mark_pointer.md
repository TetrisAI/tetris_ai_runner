# Commit 7e: 退役 path_mark_set 指针重载

## Goal
- 删除 `path_mark_set(TetrisNode*, BBState, char)` /
  `path_mark_set(TetrisNode*, TetrisNode*, char)` 两个指针入口的 set 包装。
- 1g `make_path` 入口最后 1 个 `path_mark_set(node, nullptr, '\0')` /
  `index_filtered_eq(node, ...)` / `cells_key_for(node)` 也改成 BBState 入口。

## 改动 (movegen_search.h)
1. `make_path` 1g 入口：
   - `path_mark_set(node, nullptr, '\0')` -> `path_mark_set_state_root(entry_state, '\0')`。
   - `index_filtered_eq(node, index)` -> `index_filtered_eq_state(entry_state, index)`。
   - `cells_key_for(node)` -> `cells_key_for_state(entry_state)`。
2. 新增 `path_mark_set_state_root(BBState, char)`：等价 master 起点哨兵
   `PrevKey r=0xFF`，与 `PathMark.has()==false` 终止条件配套。
3. 删除两个 `path_mark_set(TetrisNode*, ...)` 重载，已无调用方。

## 验证
- `cmake --build build --target oracle_diff -j4`：0 warning / 0 error。
- `./build/oracle_diff`：1g + 20g 全场景 `# all diffs ok`。
