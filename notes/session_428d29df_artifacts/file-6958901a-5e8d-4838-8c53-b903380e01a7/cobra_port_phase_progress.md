# Cobra 移植 - Phase 1/2 已落地, Phase 3 待续

> 关键字: cobra-port-progress phase1-phase2-done phase3-handoff

## 已落地的 commits（未 push）

```
ae245ad Add Map<W,H>::line_clears                                (Phase 3 推进)
4234de2 Add Map<W,H>::for_each_set_bit                          (Phase 3 推进)
923c67b Add compile-time shifted<Dx,Dy> on Map<W,H>             (Phase 3 起步)
1fd5f84 Add compile-time piece shape lookup over RuleSpec        (Phase 2)
6ffe243 Add portable bitboard primitives and Map<W,H> skeleton   (Phase 1, 含 C++20 升级)
7077937 Expose RuleSpec via rule headers ...                     (上一阶段)
```

## Phase 3 还差什么

已落地的 Map 算子: `set/clear/get`, `row/set_row`, 位运算 `|&^~`, `==/!=`, `popcount`, `max_y`, `shifted<Dx,Dy>`, `for_each_set_bit`, `line_clears`, `consteval all_mask/col_mask<X>`.

**还需要补:**
- `Map::clear_lines(Map lines)`: 把 line_clears 标记的行抹掉, 上方的行往下塌. 与 cobra `clear_lines` 同义.
- (可选) 显式的 `cast_height<H2>`: cobra movegen 在不同 piece 高度间切换 Board<H1>/Board<H2>, 等价于复制低 lane.

补完后才能写 `src/tetris_movegen.h` 实现 cobra 风格 MoveList.

## 已就绪的地基

| 文件 | 内容 | 验证状态 |
|-----|------|--------|
| `src/tetris_simd.h` | `simd::BoardVec<T, Lanes>` portable SIMD 抽象 | 未编译验证 |
| `src/tetris_map.h` | `Map<W, H>` 编译期棋盘容器 | 未编译验证 |
| `src/tetris_shape.h` | `shape::find_op / piece_h / piece_line / target_* / wk_*` 编译期查询 | 未编译验证 |

## Phase 3 路线 (movegen 重写)

### 入口
- 文件名规划: `src/tetris_movegen.h` (头文件 only, 与 cobra::MoveList 等价)
- 接口规划:
  ```cpp
  template<class Spec, char T>
  class MoveList {
      static constexpr int W = Spec::width;
      static constexpr int H = Spec::height;
      using map_t = Map<W, H>;
  public:
      template<class Fn> void for_each_landing(map_t const &b, Fn &&fn) const;
  };
  ```
- 不再依赖 TetrisNode/TetrisContext
- 形状从 `shape::piece_line<Spec, T, R, I>` 取
- 旋转目标 + 踢墙从 `shape::target_*` / `shape::wk_*` 取

### 核心算法
参考 `cobra-movegen/src/movegen.hpp` 的 `MoveList::generate`:
1. usable_map = 棋盘空位 mask (按 piece+rotation 取交集)
2. landable_map = usable & ~(usable shifted_down 1 行) (落地点 = 该格空且下方有支撑)
3. 从 spawn 位置开始 BFS, 用 SIMD 位运算扩展 LRD/旋转/踢墙
4. 输出去重后的 (rotation, x, y, spin) 集合

### 需要补全的 SIMD 算子 (在 BoardVec 或 Map 上)
- 跨 lane 的小数行 shift (cobra `shift<dx, dy>` 的核心)
- `line_clears` / `clear_lines` (用于 do_move)
- `for_each_set_bit` (用于落地点枚举)

## Phase 4-7 简要 (从执行计划文档复制)

- Phase 4: AI 类全部模板化为 `<int W, int H>`, eval/get 改吃 `PiecePos + Map<W,H>`
- Phase 5: 删除 TetrisContext / TetrisNode / TetrisMap (旧)
- Phase 6: TetrisEngine 重接线为 `<Rule, template<int,int> class AI, template<class,int,int> class Search>`
- Phase 7: SIMD 后端特化 (SSE2/AVX2/NEON)

## 下一会话起步

1. 读 `research/flip-bits/cobra_port_execution_plan.md` 拿到全决策记录
2. 读 `research/flip-bits/cobra_port_phase_progress.md` (本文件) 接续
3. 读 `cobra-movegen/src/movegen.hpp`、`cobra-movegen/src/gen.hpp`、`cobra-movegen/src/board.hpp` 理解 cobra 算法
4. 实现 `src/tetris_movegen.h`, 用 `Map<W,H>` + `shape::*` 重新组装出 cobra::MoveList 的等价物
5. 用 SRS rule 做 perft 自检对拍 cobra
