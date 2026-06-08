# Commit 6d: Inline T-spin readiness check into MoveGenSearch

## 目标
切断 `MoveGenSearch::run_piece_20g<T,true>` 对 `impl_.check_ready / impl_.check_mini_ready` 的最后一处依赖，把 T-spin 三角支撑判定的状态机彻底搬入 `MoveGenSearch`，为后续删除 `impl_` 字段铺路。

## 改动
1. `src/movegen_search.h`
   - 顶部 include 新增 `integer_utils.h`（提供 `ZZZ_BitCount`）。
   - `init()` 中新增 T spawn pillar mask 重建（`spin_x_diff_ / spin_y_diff_ / spin_block_buffer_ / spin_block_`），公式与 `search_tspin.cpp:13~36` 严格一致。
   - 新增成员函数：
     - `check_ready_native(map, node)` 复刻 `search_tspin.cpp:1097~1119` 公式。
     - `check_mini_ready_native(snap, node)` 复刻 `search_tspin.cpp:1121~1124` 公式。
   - `run_piece_20g<T,true>` 末段 emit 把 `impl_.check_ready / impl_.check_mini_ready` 改为本地 `_native` 版本。
2. `src/search_tspin.h` 中 `check_ready / check_mini_ready` 仍为 `public`，待 commit 6e 一并恢复 `private` + 删除 `impl_`。

## 验证
- `cmake --build .` 全量目标 100% 构建通过。
- `./oracle_diff` 全量 1g + 20g 全场景通过（输出末行 `# all diffs ok`）。

## 后续
- Commit 6e：删除 `impl_` 字段及 `search_tspin.h` 中不再需要 `public` 暴露的 `check_ready / check_mini_ready`，恢复为 `private`；`search()` 走全 native 路径，老引擎仅在 `make_path`（如还需要）时构造。
- Commit 6f：处理 20g T 块 spin tag 与 master BFS 顺序差异，移除 `oracle_diff` 中的零化容忍。
