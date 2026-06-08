# Cobra 移植 - Phase 3 movegen 骨架已落地

> 关键字: cobra-port-progress phase3-movegen-skeleton

## 已落地的 commits（未 push）

```
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

骨架已经能编译, 但**未做 perft 对拍**, 含以下两处坐标系不确定:

1. **piece_cells 的 row 方向**
   - `OpLines` 中 row 0 是 4xN 矩阵的视觉顶部, 但 `Map<W,H>` 是 y-up.
   - `create_node` 在 `tetris_core.cpp:629` 用 `data[0..3] = {line4, line3, line2, line1}` 做了一次倒序.
   - movegen 当前直接用 `piece_cells` 的 (col, row) 喂给 `shifted<-cx, -cy>()`, 没翻转.
   - **修复方向**: 在 `op_cells` 里把 row 改为 `(Op::lines::size - 1 - i)`, 或者在 movegen 用 cells 时做翻转.
   - 已在 `tetris_movegen.h:44` 标注 `TODO(phase3-coord)`.

2. **spawn 坐标**
   - 当前 `seed_y = H - spawn_y - 1`, 与 `create_node` 的 `status.y = h - spawn_y - 1` 对齐.
   - 但 movegen 的 seed 是"基准点位置", create_node 的 status.y 是"node 整体的 y", 二者**不**等价 (相差 piece 高度).
   - 已在 `tetris_movegen.h:147` 标注 `TODO(phase3-spawn)`.

3. **spin 检测**
   - 当前 `LandingPos::spin = 0` 占位; cobra 用 corners 位图区分 None/Mini/Full.
   - 等 perft 平移版对拍通过后再补.

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
