# Commit 5e: make_path 入口 disable_d 三谓词位板化

## Scope (single file: src/movegen_search.h)
把 `disable_d` 4 子句中后 3 个 master 派生量替换成位板等价 helpers, BFS 主循环
/ 命中比较 / build_path 全部不动.

## Master expression
```
disable_d = land_point.type == None
         && node->land_point != nullptr
         && node->low >= map.roof
         && land_point->open(map);
```

## 位板等价
- `node->land_point != nullptr`: SRS 7 piece 在 prepare 阶段必建好 place_cache_
  (tetris_core.cpp:411-412), SRS dispatch 后恒为 true. 移除该子句.
- `node->low >= map.roof`:
  * `node->low` = spawn 所有 rotation 下的 min(cell y) (tetris_core.cpp:429-437).
  * 位板版 `spawn_min_cell_y_T<T>`: 对 spawn (R0) 取 yb + min cell y.
    (master `node->low` 是 init 时跨 rotation 取 min, 我们这里只取 R0; 启发式
    保守等价: 若 R0 不满足则 disable_d=false, 退化到双轮 BFS, 不影响正确性.)
  * `board_roof`: popcount==0 ? 0 : max_y+1, 与 master TetrisMap.roof 一致.
- `land_point->open(map)`:
  * master: `for col in piece.width: bottom[col] >= top[col]` 任一成立.
  * 位板版 `open_bb_TR<T,R>`: 对 piece 的 cells 按 local cx 折最低 cy, 求绝对
    `yb + min_cy` 与 `board_col_top(xb + cx)` 比较, 任一列绝对最低 >= top
    即 open.
  * `board_col_top`: 该列从高到低扫第一个 set bit, 返回 y+1; 全空返 0.

## Why 这是启发式 + 安全网
master `disable_d` 控制 BFS 第一轮是否禁掉 d/D. 错判: 多走一轮 BFS,
按键串可能略不同, 但 make_path 仍会到达目标. oracle_diff 的
`mg_path_self_check_one` (上一笔加的) 会 replay 按键串, 终点必须等于
lp.node 才算过. 全绿 = 行为足够等价.

## Validation
- `clang-format -i src/movegen_search.h` 通过.
- `cmake --build build -j --target oracle_diff perft_movegen tetris_ai
  tetris_ai_runner`: 全部成功, 仅 LTO LTRANS 善意提示, 无 error/warning.
- `./oracle_diff all`: `# all diffs ok`, 126/126 全绿 (含新加的 mg-path-self-check).
- `./perft_movegen all`: exit=0.

## Constraints honored
- 单文件单提交.
- 局部变量未加 const.
- clang-format -i 已执行.
- Commit message 英文, 单段, 纯 ASCII, 描述 "相对目标分支多了什么".
- 未推送, 待用户确认.

## Next (5f)
- BFS 命中比较 `cur->index_filtered` 位板化 (需要把 emit 阶段的 IndexFilter
  等价类规则在 BFS 内实现).
- BFS 队列 / 末段 T-spin 重放 / impl_ 兜底依次清理. 20g 留最后.
