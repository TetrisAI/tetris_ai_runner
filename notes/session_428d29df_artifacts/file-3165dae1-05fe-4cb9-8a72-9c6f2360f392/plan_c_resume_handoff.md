# Plan C 续接 (2026-05-28 update)

## 已落地 commit
- ee41299 Lift Map W upper bound from 32 to 64                     (C-A: cobra Map<W,H> 容器)
- e73c860 Drop hardcoded SRS-7 dispatch in MoveGenSearch          (C-C: piece-switch / r-switch 全部 fold)
- 88ba227 Lift legacy bridge layer width upper bound 32 -> 64     (C-A2: row_t 32->64, TetrisMap top, AI HoleNum, etc.)

## 仍未做
- C-B1: TetrisNode::data/top/bottom[4] 解除 N=4 假设 (~150 lines, 触及 attach / check / open / move_*)
- C-B2: op_create_bridge static_assert(Lines::size==4) 解除 (~50 lines, 触及 RuleSpec 注册侧)

C-B1 / C-B2 不影响现有 SRS-7 行为, 因为现有 piece 都是 N=4. 留作后续会话.

## 边界 (与原 plan_c_pushback.md 保持一致)
即使 C-B1 / C-B2 也做完, 仓库里仍然没有具体的"大盘规则定义" / "新 piece OpDesc" / "5-rot kick 表".
这些是规则作者层的输入, 不是框架层能凭空生成的.

## oracle_diff 守门
3 个 commit 之间任意 checkout 都需要重跑 oracle_diff:
  cd build && cmake --build . -j && ./oracle_diff | tail -3
期望最后一行是 "# all diffs ok".
