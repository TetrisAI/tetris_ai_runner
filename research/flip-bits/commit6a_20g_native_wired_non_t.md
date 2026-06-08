# commit 6a: 把 20g 位板原生 BFS 接到 search() (非 T piece)

## 上下文
- commit 5h 之前: `MoveGenSearch::search` 在 `is_20g` 分支整体退化到 `impl_.search`
  (老 master `search_tspin::Search`).
- commit 5h 之后: 非 20g 分支以外的 7-piece SRS 路径都走位板原生; 但 `is_20g`
  整体仍委托 `impl_`.
- run_piece_20g<T> 在 commit (skel) 已实现, 等价语义是: spawn 出来先 sink, 然后
  BFS 出队后再 sink, 邻居 (LR/D/cw/ccw/180) 入队不 sink, landing 用 cells_key
  去重.
- 主要 blocker: master `node_mark_filtered_` 用 `node->index_filtered` 合并
  R0/R2 (I/S/Z) 翻转等价类. 位板 BFS 选择的代表元只跟 cells_key 有关, 跟 master
  status.r 选哪个不挂钩. 同一 cells_key 在 master 侧出现的 (r, x) 与位板侧不同,
  oracle_diff 对拍会把它们视作不同 Tup.

## 本次改动

### oracle_diff.cpp
- `oracle_landings` 多了 `is_20g` 参数.
- 20g flavor 下 oracle / new 两侧都把 Tup 的 r/x 替换成 `(index_filtered, 0)`,
  y 字段替换成 `node->row`.
  - 等价类内 master 对每个 cells_key 都收敛到同一个 `index_filtered` (那是
    `index_filter` map 在 7-piece 全节点构造时的 hash 桶 ID).
  - row 是 piece 在场景中的最低物理行; 同 cells_key 必然同 row, 从而消除我之前
    用 `status.y` 时 (r=1 vs r=3) 数值差导致的 false-positive.
- diff_one 调用 oracle_landings 时透传 is_20g.

### movegen_search.h
- `search()` 的 `is_20g` 分支按 piece type 分发:
  - O / I / L / J / S / Z 进位板 `run_piece_20g<T>(map)`.
  - T 仍走 `impl_.search` (run_piece_20g 还没补 last/spin/is_last_rotate, T
    专属字段, kick 元数据要从 BFS 边追踪到 emit 节点).

## 验证
- `oracle_diff` 全部通过 (1g + 20g flavor, 7 pieces × 17 boards).
- `perft_movegen all` 编译通过, 不是 pass/fail 测试 (输出 dump).

## 下一步候选
1. 把 T 块的 last/spin/is_last_rotate 沿 BFS 追踪起来 (master `search_t_*`
   分支同源逻辑), 让 `run_piece_20g<'T'>` 也能产出 spin 元数据, 完全脱离
   `impl_`.
2. 重写 `make_path` 在 20g 模式下也走位板, 跟 search 一起把 `impl_` 移除.
3. 完成上述两步后 (commit 6b/6c), 把 `impl_` 字段彻底删除, 清理对
   `search_tspin::Search` 的依赖.

## TODO 清单 (落到代码中的)
- `run_piece_20g<T>` 当前 emit 的 `TetrisNodeWithTSpinType` 默认 ctor (flags=0),
  没有写 is_check / is_last_rotate / is_ready / is_mini_ready / last. 跟非 T 块
  master 行为一致, 没引入 diff. T 块需要专门的 emit 路径.
- emitted_keys 是 vector + 线性扫, O(n^2) 去重. 7-piece 落点上限不超过 ~80,
  暂不优化.
