# Pre-5e: oracle_diff 加 MoveGenSearch::make_path 严格自检

## Scope (single file: tests/oracle_diff.cpp)
新增 `mg_path_self_check_one` + `MgSearch` 共享实例, 与既有
`path_self_check_one` 同形, 在 `diff_one` 主循环里挂 hook 跑.
源码侧 `src/movegen_search.h` 不动.

## Why now
5e 要动的是 `cur->index_filtered` 命中比较 / `disable_d` 三谓词
(`node->land_point != nullptr && node->low >= map.roof &&
land_point->open(map)`). 在动这两块前必须先把 MoveGenSearch::make_path
的按键串等价校验钉死, 否则任何回归都会被旧 oracle_diff (只比 search
阶段 r,x,y,spin 元组 + last 字段) 漏掉.

## Validation
- `clang-format -i tests/oracle_diff.cpp` 通过
- `cmake --build build -j --target oracle_diff` 通过
- `./oracle_diff all`: 126/126 全绿, 仅 `sealed_top` spawn 友好告警,
  无 FAIL / mismatch.
- `./perft_movegen all`: exit=0.

## Constraints honored
- 单文件单提交.
- 局部变量未加 const.
- clang-format -i 已执行.
- Commit message 英文, 单段, 纯 ASCII.

## Next (5e)
- 把 `cur->index_filtered` 命中比较换成位板等价 (需把 emit 阶段已经做的
  对称归一化在 BFS 内部也做一份, 或者改用全 (T,r,x,y) 落点集做命中表).
- 把 `disable_d` 三谓词位板化.
- 完成后 `impl_` 仅留 20g 兜底, 5f 处理.
