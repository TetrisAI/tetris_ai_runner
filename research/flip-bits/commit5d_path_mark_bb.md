# Commit 5d: make_path BFS visited / predecessor 表位板化

## Scope (single file: src/movegen_search.h)
把 BFS 用的 `node_mark_path_` (master `TetrisNodeMark`, 按 `node->index` 寻址)
替换成位板版 `PathMark`, key = `(r, x_bbox, y_bbox)`. 不动 BFS 队列 (仍是
`std::vector<TetrisNode const *>`), 不动 build_path 末段, 不动 `disable_d`
三谓词, 不动 `index_filtered` 命中比较, 不动 20g 兜底.

## Rationale (拆 5c -> 5d)
- 5c: drop 位板化 (单点等价).
- 5d (本次): visited / predecessor 位板化, 把 BFS 内部对 master `TetrisNode::index`
  的全部依赖移除. 节点身份"对外"仍是 `TetrisNode *` 因为:
  * `node_search_path_` 喂回 `try_simple_move` / `first_passing_kick` 等 helper
    需要 `TetrisNode *` 输入 (那些 helper 内部会再做 (T, r, x, y) 拆解), 5e
    再统一改造可降低耦合.
  * `index_filtered` 命中 / `land_point.last` / `land_point->open(map)` /
    `node->land_point != nullptr && node->low >= map.roof` 还是 master 派生量,
    5e 一次性换掉再删 `impl_`.

## Behavior equivalence proof
1. master `node_mark_` 是 `TetrisNodeMarkTemplate<false>`, 按 `key->index` 寻址,
   而 `node->index` 在 `TetrisContext::prepare` (src/tetris_core.cpp:317-321) 中
   是按 (T, r, x, y) 一一映射的 BFS 序号. 也就是说 master 版的 visited 集合
   语义就是 "(T, r, x, y) 唯一". 单一 piece BFS 中 T 固定, 因此等价于
   "(r, x, y) 唯一".
2. 我们 PathMark 的 key 是 `(r, x_bbox, y_bbox)`, x_bbox/y_bbox = master status
   经 5b 的 origin 公式 (`x_bbox = status_x + origin.x, y_bbox = status_y -
   origin.y`) 映射. 这个映射对每个 (T, R) 是双射 (origin 是常数), 所以
   (r, x_bbox, y_bbox) 与 (r, status_x, status_y) 一一对应. 因此 PathMark.set
   返回 false 当且仅当 master node_mark_.set 也返回 false.
3. predecessor / op 字段透传 TetrisNode * + char, 不丢信息.
4. clear() 的 version 计数器策略与 master `TetrisNodeMarkTemplate::clear`
   语义一致 (版本号机制实现 O(1) 清空, 溢出回卷时全表归零).

## Implementation notes
- `PathMark` 内嵌 `[kR][kCells]` 的 cell_ver_ / cell_prev_ / cell_op_ 平行
  数组, kR=4 / kCells = kW * kH = 10 * 40 = 400, 单个 PathMark 静态体积约
  4 * 400 * (8 + 8 + 1) = 27.2 KB, 与 master `std::vector<Mark>`(node_max
  量级 ~ 数千) 同量级.
- helper:
  * `status_to_bbox<T,R>` / `_T` / 顶层运行时分发: TetrisNode * -> bbox.
  * `path_mark_set(key, prev, op)`, `path_mark_get(key)` 包装.
- BFS 内部所有 `node_mark_path_.set(...)` -> `path_mark_set(...)`,
  `build_path(x, node_mark_path_)` -> `build_path(x)`.
- `init()` 中删除 `node_mark_path_.init(context->node_max())`.
- 类成员 `TetrisNodeMark node_mark_path_` -> `PathMark path_mark_`.

## Validation
- `clang-format -i src/movegen_search.h`: 通过.
- `cmake --build build -j --target oracle_diff perft_movegen tetris_ai
  tetris_ai_runner`: 全部成功, 仅 LTO LTRANS 串行善意提示, 无 error/warning.
- `./oracle_diff all`: `# all diffs ok`, 126/126 用例通过.
- `./perft_movegen all`: exit=0, 无 fail/mismatch.

## Constraints honored
- 单文件单提交 (src/movegen_search.h, +171/-42).
- 局部变量未加 `const`.
- `clang-format -i` 已执行.
- Commit message 英文, 单段, 纯 ASCII, 描述 "相对目标分支多了什么".
- 未推送, 待用户确认.

## Next (5e)
- 把 `cur->index_filtered` 命中比较换成位板等价 (需要先把 oracle_diff 的
  index_filtered 等价类公式落到位板 emit 阶段, 5e 之前要先在 oracle_diff
  里对 path 做严格按键序列等价校验).
- 把 `land_point->open(map)` / `node->land_point != nullptr` /
  `node->low >= map.roof` 的 `disable_d` 三谓词位板化.
- BFS 队列 / 末段 T-spin 重放完成后, 删除 `impl_` 与 20g 之外的所有 master
  指针图依赖. 20g 留给 5f.
