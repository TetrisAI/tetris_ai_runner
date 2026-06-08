# Commit 5a: Inline make_path BFS into MoveGenSearch

## Goal of phase
拆解 commit 5 为两步, 降低风险:
- **5a (本提交)**: 把 `search_tspin::Search::make_path` 内的 BFS 原样搬迁到
  `MoveGenSearch::make_path`. 仍使用 `TetrisNode` 指针图
  (`wall_kick_*` / `move_*` / `rotate_*`), 不改算法行为.
  目的是切断 `MoveGenSearch` 对 `impl_.make_path` 的委托, 后续才能用
  bitboard 替换内部邻居生成.
- **5b (下一步)**: 把内联的 BFS 邻居查询替换成 `Map<W,H>` + `shape::wk_*`
  的纯 bitboard 实现, 消除对老指针图的依赖.

## Changes (single file: src/movegen_search.h)
- 在类内新增成员, 与老 `Search` 内部一致:
  - `TetrisNodeMark node_mark_path_;`
  - `std::vector<TetrisNode const *> node_search_path_;`
- `init()` 中调用 `node_mark_path_.init(context->node_max());`
  (与 `search_tspin::Search::init` 同步).
- `make_path(node, land_point, ...)`:
  - 不再 `return impl_.make_path(...)`.
  - 顶部仍保留 20g (`config_->is_20g`) 与非 SRS piece (`!node->is_srs`)
    的兜底, 走 `impl_.make_path` (commit 5b 之前共用).
  - 其余路径完整搬迁老引擎的 BFS:
    - `build_path` lambda 从 mark 重建 key 序列, 含 T-spin 末次旋转的
      wall-kick 索引 (`x/z/c` -> `X/Z/C` 大小写代表 D/L/R/d 链).
    - 队列扩展逻辑 (`disable_d`, `allow_180`, `allow_LR`, `allow_D`,
      `allow_rotate_move`) 1:1 对齐.

## Constraints honored
- 只动 `src/movegen_search.h`, 不重命名/不动 typedef. 上层 `ai.cpp`
  及对外 typedef 维持不变.
- 局部变量未加 `const` (符合个人编码习惯).
- `clang-format -i src/movegen_search.h` 已格式化.
- 未推送, 等待用户确认.

## Validation
- `cmake --build build -j --target oracle_diff perft_movegen tetris_ai tetris_ai_runner`
  全部成功, 仅有 LTO serial-LTRANS 善意提示, 无 error/warning.
- `./oracle_diff all` -> `# all diffs ok`, 126 用例全过, 仅 sealed_top
  spawn 处的 `failed check on map` 友好告警 (预期, spawn 被堵死).
- `./perft_movegen all` -> 输出完整, 无 fail/mismatch/error.

## Next step (commit 5b)
在 `make_path` 内部把 `cur->wall_kick_*`, `cur->move_left/right/down`,
`nL->rotate_*` 等指针访问换成 `Map<W,H>` 的 `check`/`drop` + 编译期
shape kick 表 (`shape::wk_*`). 老 `impl_` 仍可保留作为 20g 与
非 SRS 兜底, 直到 5c 彻底移除.
