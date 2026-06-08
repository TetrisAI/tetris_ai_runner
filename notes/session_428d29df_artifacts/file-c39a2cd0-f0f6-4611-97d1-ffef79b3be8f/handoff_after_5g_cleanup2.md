# Handoff after 5g cleanup-2

## 当前分支与本地 commits (未推送)
分支: flip-bits-clean (基于 flip-bits)
HEAD: 9b2a4bb

5g 系列 (按时间顺序, 12 个):
1. 06fb133 Compare make_path BFS hits via bitboard cells-key
2. 349e298 Drop TetrisNode pointers from make_path PathMark predecessors
3. a4620ec Switch make_path BFS queue to BBState coordinates
4. d207399 Drive make_path BFS body via BBState neighbor wrappers
5. 28fe379 Make try_simple_move BBState-native for make_path BFS
6. 3f62531 Make first_passing_kick BBState-native for make_path BFS
7. 767695f Make rotate_no_kick BBState-native for make_path BFS
8. 559da5f Make drop_bb BBState-native for make_path BFS
9. bf455b4 Walk make_path BFS LR-shift loops in BBState coordinates
10. f3bef63 Compare make_path tail wall-kick replay via CellsKey
11. eeb1ef0 Drop dead TetrisNode pointer overloads from make_path helpers
12. 9b2a4bb Inline make_path BFS _s wrappers into BBState overloads

## 验证基线
- cmake --build build -j: clean (LTO warning 不算).
- ./oracle_diff all: # all diffs ok (含 mg_path_self_check_one).
- ./perft_movegen all: exit=0.
每个 commit 都过这套基线.

## 接下来的 TODO
1. cleanup-3 (单文件单提交): BFS 主循环里 9 处 `node_check_bb(p, usable_arr)`
   全替换为 `usable_at_bb(state.r, state.xb, state.yb, usable_arr)`. 然后删
   `node_check_bb`.
   - nL/nR 块 (l/r 入口): 已有 cur_state, 平移后 BBState 还没计算, 需要先把
     `try_simple_move(cur_state, dx, 0)` 这一调用拆成
     `BBState n_state = cur_state; n_state.xb += dx; nP = state_to_node(n_state);`,
     然后用 n_state 直接 usable_at_bb. 这样可以连同 cleanup-3 + 进一步优化
     (一次 state_to_node 替代 BBState 重载内的 1 次).
   - rn 块 (X/Z/C 入口): rn 是 rotate_no_kick(BBState) 的输出, 旋转后的 r/xb/yb
     等于 source state.r 的 target_r 与同一 (xb, yb). 实际只要根据 source state +
     rotate dir 推出新的 state.r 即可. 但 rotate_no_kick_TRD 里的 target_r
     dispatch 为 constexpr, 外部不易直接复用. 较保险做法: 仍用现有 rn (master),
     仅在 rn 非空时 state_from_node(rn) 反推 BBState, 然后 usable_at_bb. 性能上
     仍多一次 status_to_bbox, 但比 node_check_bb 内的 dispatch 快.
   - nD 块 (d 入口): 同 nL/nR, 平移 (0,-1).

2. 5h: 严格限定 impl_ 兜底引擎到 20g.
   - search() 已经在 20g 与非 SRS 走 impl_; 后者来自 master 不区分 SRS 的
     现实 (cmd_tris/botris 的 X 形 piece). 如果只关心 7-piece SRS, 可在
     init 时 assert / 在 search/make_path 默认 piece_t in 7 set.
   - 实测调用方都是 SRS 7-piece, 但删除非 SRS 兜底要确保所有 caller 都不会
     传 X. 建议先在调用前置加 DEBUG_ASSERT, 跑一段时间无触发再删.

3. 20g 路径重写: 在 5h 落地后, 把 search() / make_path 的 20g 分支也搬到
   位板; 设计草案见 research/flip-bits/path_redesign.md (BFS 每邻居都 drop).

## 文件指针
- src/movegen_search.h 是唯一被改动的文件 (12 commits).
- research/flip-bits/ 下保存了所有 commit 的设计依据笔记, 文件名按
   commit5x_xxx.md 编号, 与 commit message 一一对应.

## 用户编码偏好提醒
- 局部变量不加 const.
- Commit message 英文 ASCII, 描述 "MR 相对目标分支多了什么" (不要写本笔做了
  什么).
- MR 描述中文, MR 标题英文.
- 不要试图编译运行 (但本环境实际可编译, oracle_diff/perft_movegen 都跑通).
- 单文件单提交, 推送前等用户确认.
