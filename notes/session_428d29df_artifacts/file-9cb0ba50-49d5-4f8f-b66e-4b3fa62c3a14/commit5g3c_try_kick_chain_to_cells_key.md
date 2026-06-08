# Commit 5g-3c: try_kick_chain_to 改用 CellsKey 比较 + BBState 入口

## Scope (single file: src/movegen_search.h)
1. 改 try_kick_chain_to 签名: 接受 BBState const &last_state 与 CellsKey
   target_key (而不是两个 TetrisNode *), 内部走 first_passing_kick(t, dir, BBState, ...)
   命中后用 cells_key_for(hit) == target_key 判定.
2. build_path 末段调用方:
   - 用 state_from_node(land_point.last) 把 last 转 BBState (一次, 在 build_path lambda 外
     的 capture 处准备就够了, 但因为 last 与 target 可能为 nullptr, 仍需先做 null guard).
   - 直接复用现成的 index_landpoint = cells_key_for(land_point.node).
3. 旧 first_passing_kick(TetrisNode*, ...) / try_kick_chain_TR pointer-x/y 入口仍要保留,
   因为 BBState 重载内部调到的 try_kick_chain_TR 是模板共用的, 不动它. 这里只删
   try_kick_chain_to 的指针入口, 把比较换成 CellsKey.

## 等价性
master 末段:
  hit_master = wall_kick_chain(last, dir).check(map)
  return hit_master == target_master
->
  hit_master = first_passing_kick(t, dir, last_master, usable_arr)
  return hit_master == target_master   (旧版)
->
  hit_master = first_passing_kick(t, dir, last_state_BB, usable_arr)
  return hit_master && cells_key_for(hit_master) == target_key  (新版)

注意 CellsKey 是绝对 cells 集合的等价类: 当 hit 与 target 在同一 (T, R) 等价类下
处于不同 (xb, yb) 也会命中相同 CellsKey 吗? 答: T/I/S/Z/L/J/O 这 7 个 piece
在 SRS 下不同 status 的 cells 集合都是各异的 (相同 cells set 一定是同一节点),
所以 cells_key_for 在 (T, R, xb, yb) -> cells set 是单射.

但 build_path 的 last + target 都是 master 系下的合法节点, 关键问题: kick 链命中
的 hit 是否一定与 master target 是同一节点? 旧版用指针相等; 新版改 CellsKey
相等. 由于 cells_key 与节点身份双射, 二者等价.

## 不在本笔范围
- 删旧指针版 helpers (try_simple_move(TetrisNode*, ...) 等). slice 5 后已只剩
  build_path 末段需要它们; 5g-3c 后这部分调用已切到 BBState. 但 try_kick_chain_TR
  的指针入口 (status_x, status_y) 仍由 BBState 重载内部使用 (折出 mx, my 直接
  传), 可保留.
- impl_ 兜底 20g 限制 (5h).

## Validation 计划
- `cmake --build build -j`: 全过.
- `./oracle_diff all`: # all diffs ok, 含 mg_path_self_check_one.
- `./perft_movegen all`: exit=0.

## Constraints honored
- 单文件单提交.
- 局部变量未加 const.
- clang-format -i 须执行.
- Commit message 英文 ASCII.
- 推送前等用户确认.
