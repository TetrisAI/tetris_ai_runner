# Pre-5f: 把 BFS 命中比较收敛到 index_filtered_eq helper

## Scope (single file: src/movegen_search.h)
纯 mechanical 重构, 把 BFS 主循环里所有 `xxx->index_filtered == index`
(共 22 处) 替换成 `index_filtered_eq(xxx, index)`. helper 内部仍 `n != nullptr
&& n->index_filtered == target`, 严格等价 master ==.

## 不在本笔范围
- `land_point->index_filtered` / `land_point.last->index_filtered` 这种用作
  "拿目标 index" 的取值 (148/156/157/168/179/221), 5f 一起换.
- helper 内部的位板化 (用 cells-mask 等价类替 master IndexFilter), 5f 正题.

## Why 单独一笔
5f 真正改 helper 内部时, 这步先把调用点收敛能让 diff 更小、回滚更安全.
oracle_diff `mg_path_self_check_one` 全绿守门.

## Validation
- clang-format -i 通过.
- 构建 oracle_diff / perft_movegen / tetris_ai / tetris_ai_runner: 全过.
- ./oracle_diff all: 126/126 全绿.
- ./perft_movegen all: exit=0.

## Constraints honored
- 单文件单提交.
- 局部变量未加 const.
- clang-format -i 已执行.
- Commit message 英文, 单段, 纯 ASCII.
- 未推送.
