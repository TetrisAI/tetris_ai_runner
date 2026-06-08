# Commit 5g-3b (slice 5): BFS allow_LR 长循环 BBState 化

## Scope (single file: src/movegen_search.h)
1. 新增 helper `usable_at_bb(r, xb, yb, usable_arr)`: 边界 + `usable_arr[r].get`.
2. BFS 的两段 `if (allow_LR)` 块改成 BBState 主语义:
   - 入口直接试 `xb -= 1` (或 +1), 命中后保留 BBState 而不是 TetrisNode 指针.
   - 内层 while 全部走 BBState; 终点一次 state_to_node 反查 master.
   - 终点入队用现成的 BBState, 省掉 state_from_node 来回.
3. 旧 `try_simple_move(TetrisNode*, dx, 0)` / `node_check_bb` 暂保留 (其它分支
   还在用), 后续 dead code 清扫一并删.

## 等价性
原始语义:
```
nL = try_simple_move_s(cur_state, -1, 0);          // = context_->get({t, mx-1, my, r})
if (nL && node_check_bb(nL, usable_arr)) {
  while (true) {
    next = try_simple_move(nL, -1, 0);
    if (!next || !node_check_bb(next, usable_arr)) break;
    nL = next;
  }
  // commit nL
}
```
两次检查:
- `try_simple_move` 实际 `context_->get`, 仅过滤 master 节点存在性 (大致是 master x 范围).
- `node_check_bb` 转 bbox 并 `usable_arr[r].get(xb, yb)` (含 bbox 边界).

新语义保留两次检查, 把 bbox 迭代提到主路径:
```
BBState nL_state = cur_state;
nL_state.xb -= 1;
TetrisNode const *nL = state_to_node(nL_state);    // 1:1 等价 try_simple_move
if (nL && usable_at_bb(nL_state.r, nL_state.xb, nL_state.yb, usable_arr)) {
  while (true) {
    BBState try_state = nL_state;
    try_state.xb -= 1;
    TetrisNode const *next = state_to_node(try_state);
    if (!next) break;
    if (!usable_at_bb(try_state.r, try_state.xb, try_state.yb, usable_arr)) break;
    nL = next;
    nL_state = try_state;
  }
  // commit (nL, nL_state) -- 入队直接 push_back(nL_state).
}
```
`state_to_node` 与 `try_simple_move(TetrisNode*, -1, 0)` 等价: 二者都终结于
`context_->get({t, mx-1, my, r})` (state_to_node_TR 的 origin 折回结果).
`usable_at_bb` 与 `node_check_bb` 等价: 都做 bbox 边界 + `usable_arr[r].get(xb, yb)`.

净优化: 终点 `node_search_path_.push_back(nL_state)` 省掉 `state_from_node`
反推一次.

## 不在本笔范围
- try_kick_chain_to 末段 cells_key 重做 (5g-3c).
- 旧指针版 helpers (try_simple_move/node_check_bb/...) 整体清扫.

## Validation 计划
- `cmake --build build -j`: 全过.
- `./oracle_diff all`: # all diffs ok, 含 mg_path_self_check_one.
- `./perft_movegen all`: exit=0.

## Constraints honored
- 单文件单提交.
- 局部变量未加 const.
- clang-format -i 须执行.
- Commit message 英文 ASCII, 描述 "MR 相对目标分支多了什么".
- 推送前等用户确认.
