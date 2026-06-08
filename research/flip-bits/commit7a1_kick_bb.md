# Commit 7a-1: first_passing_kick_bb

## Goal
把 `first_passing_kick` (BFS 主热路径里旋转踢墙判定) 砍掉 `context_->get`：
- 旧路径: `first_passing_kick` 内部 `try_kick_chain_TR` 命中后 `context_->get({T,nx,ny,tgt})` 返回 `TetrisNode*`，调用方再 `state_from_node(node)` 反推 BBState (= 第二次坐标折算)。
- 新路径: 直接用 `(T, target_r, status_x, status_y)` 折回 `(xb, yb)` 返回 `BBState`，省掉 hash 表查询和反推。

## 新增接口 (movegen_search.h, 仅新增)
1. `build_state_from_master<T, Target>(status_x, status_y) -> BBState`：把 master `(mx,my)` 直接折成 bbox `(xb,yb)`。公式 = `status_to_bbox_TR` 反向 (`xb = mx + orig.x; yb = my - orig.y`)。
2. `try_kick_chain_TR_bb<T,Target,WkList>(int,int, usable_arr) -> std::optional<BBState>`：与 `try_kick_chain_TR` 1:1 对照，命中即 build_state，未命中 nullopt。
3. `try_kick_TRD_bb`、`try_kick_TD_bb`、`first_passing_kick_dir_bb`、`first_passing_kick_bb`：dispatch 链对应的 `_bb` 版本。

## 切换调用点
仅切 `run_piece_20g` 内 T-piece 三处 (Opp/Ccw/Cw)：
```cpp
auto wk = first_passing_kick_bb(T, KickDir::*, ss, usable_arr);
if (wk) {
    if (try_cover_with_parent(*wk, ss, '*')) queue.push_back(*wk);
}
```
旧的 `make_path` (1g) 路径 / `try_kick_chain_to` 等仍然用指针版本，留待后续 commit 拆。

## 验证
- `oracle_diff` 全场景: `# all diffs ok`。
- 编译通过，无新警告。

## 后续 (7a-2 / 7a-3)
- 7a-2: 切 `make_path` 1g BFS 内的 `first_passing_kick` 三处 (line 328/337/346)。
- 7a-3: 切 `make_path` 中的二级 `first_passing_kick` (line 605~)。
- 7a-4: 当所有调用切完后，删旧的 `TetrisNode*` 出口 (`first_passing_kick`、`try_kick_TRD`、`try_kick_TD`、`try_kick_chain_TR`、`first_passing_kick_dir`)。
