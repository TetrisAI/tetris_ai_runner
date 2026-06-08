# Cobra 移植 - Phase 3 movegen 骨架已落地

> 关键字: cobra-port-progress phase3-movegen-skeleton

## 已落地的 commits（未 push）

```
a4bf720 Add T-spin Full detection on MoveGen landings             (Phase 3 spin)
a051ba6 Resolve MoveGen coordinate TODOs                         (Phase 3 修正)
97a5cee Fix MoveGen landable y-direction and document coord TODOs (Phase 3 修正)
eb0cf22 Add cobra-style MoveGen BFS with rotation and wallkick   (Phase 3 主体)
7669ce9 Add Map<W,H>::clear_lines                                (Phase 3 收尾)
ae245ad Add Map<W,H>::line_clears                                (Phase 3 推进)
4234de2 Add Map<W,H>::for_each_set_bit                          (Phase 3 推进)
923c67b Add compile-time shifted<Dx,Dy> on Map<W,H>             (Phase 3 起步)
1fd5f84 Add compile-time piece shape lookup over RuleSpec        (Phase 2)
6ffe243 Add portable bitboard primitives and Map<W,H> skeleton   (Phase 1, 含 C++20 升级)
7077937 Expose RuleSpec via rule headers ...                     (上一阶段)
```

## Phase 3 已完成的部分

| 文件 | 落地内容 |
|-----|--------|
| `src/tetris_map.h` | `set/clear/get`、`row/set_row`、`shifted<Dx,Dy>`、`for_each_set_bit`、`line_clears`、`clear_lines`、`all_mask/col_mask` |
| `src/tetris_shape.h` | `find_op`、`piece_h/w`、`piece_spawn_x/y`、`piece_line`、`target_*`、`wk_*`、`op_cells`、`piece_cells` |
| `src/tetris_movegen.h` | `LandingPos`、`movegen::usable_map`、`landable_map`、`bfs_translate`、`MoveGen<Spec, T>::generate(board, fn)` 含旋转 + 踢墙 |

## Phase 3 未解决 / 已埋 TODO

骨架已经能编译, 但**未做 perft 对拍**, 已修复以下两处:

1. **piece_cells 的 row 方向** ✅ a051ba6 已修.
   `op_cells` 把 OpLines row 翻转为 y-up: `cell.y = (N - 1 - row)`.

2. **spawn 坐标** ✅ a051ba6 已修.
   `MoveGen::generate(board, spawn_x, spawn_y, fn)` 由调用方传基准点; 内部沿 y 向上探至棋盘顶部 (cobra Slow init), 与 cobra `Gen::SPAWN_X / RulesT::SPAWN_Y` 语义对齐.

3. **spin 检测** ✅ a4bf720 已加 Full 判定 (corners ≥ 3).
   `LandingPos::spin = 2` (Full) 在 T 块落点 4 个 corner ((0,0)/(0,2)/(2,0)/(2,2)) 至少 3 个被占时设置.
   Mini 区分需要 last-kick 追踪, 等 perft 对拍后再补.

## 下一步建议执行顺序

```
[Phase 3.1] 写一个 cmd_tris 风格的小 perft, 输入 (Rule, T, board), 调 MoveGen::generate
            和现有 search_simple 对比 (rotation, x, y) 集合; 哪一边多/少哪一组就回到
            piece_cells 翻转 / spawn 计算去定位.
[Phase 3.2] 加 spin 判定 (corners bitboard).
[Phase 3.3] 收口 Phase 3, 进入 Phase 4 (AI 模板化).
```

## 已就绪的地基

| 文件 | 内容 | 验证状态 |
|-----|------|--------|
| `src/tetris_simd.h` | `simd::BoardVec<T, Lanes>` portable SIMD 抽象 | 未编译验证 |
| `src/tetris_map.h` | `Map<W, H>` 编译期棋盘容器 | 未编译验证 |
| `src/tetris_shape.h` | `shape::find_op / piece_h / piece_line / piece_cells / target_* / wk_*` | 未编译验证 |
| `src/tetris_movegen.h` | `MoveGen<Spec, T>::generate` cobra 风格 BFS | 未编译验证 |

## Phase 4-7 简要

- Phase 4: AI 类全部模板化为 `<int W, int H>`, eval/get 改吃 `PiecePos + Map<W,H>`
- Phase 5: 删除 TetrisContext / TetrisNode / TetrisMap (旧)
- Phase 6: TetrisEngine 重接线为 `<Rule, template<int,int> class AI, template<class,int,int> class Search>`
- Phase 7: SIMD 后端特化 (SSE2/AVX2/NEON)

## 下一会话起步

1. 读 `research/flip-bits/cobra_port_execution_plan.md` 拿到全决策记录
2. 读本文件确认 Phase 3 的两处 TODO
3. 用 `cobra-movegen` 仓库做 perft 对拍 (相同 rule + 空棋盘 → 落点集合)
4. 修正 piece_cells 翻转和 spawn 转换, 再补 spin 判定
5. perft 通过后进入 Phase 4
