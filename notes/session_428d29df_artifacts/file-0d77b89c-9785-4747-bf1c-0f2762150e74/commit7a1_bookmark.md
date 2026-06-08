# Commit 7a-1 入口书签

## 一句话
让 `rotate_no_kick_TRD` / `try_kick_*` / `first_passing_kick*` 返回 BBState 而不是 TetrisNode\*，BFS 主循环里所有 `context_->get` 砍光，下游消费者改用 BBState→`drop_bb`/`usable_at_bb` 链。

## 改动文件
- `src/movegen_search.h` 单文件。

## 待加 / 改的接口（按出现顺序）

### 1. 在 `rotate_no_kick_TRD` 旁加 sibling: 返回 BBState
```cpp
template<char T, std::uint8_t R, KickDir Dir>
std::optional<BBState> rotate_no_kick_TRD_bb(BBState const &cs) const {
    // 与 rotate_no_kick_TRD 完全一致地算出 tgt; 返回失败时 nullopt.
    // 命中时构造: BBState{T, tgt_r, cs.xb, cs.yb}
    // (master 旋转不改 status.x/y; 因为我们存的是 bbox 左下角 xb/yb,
    //  旋转后 piece_cells 的 origin 变了, 必须用 target rotation 的 origin
    //  把 mx/my 折回, 再用新 origin 转成新的 (xb', yb').)
}
```

**坐标换算细节**（关键，别错）：
- 当前实现 `rotate_no_kick_TRD`：从源 R 的 `piece_cells.origin` 把 `(cs.xb, cs.yb)` 折回 `(mx, my) = master status`。
- 旋转不改 `(status.x, status.y)`。
- 目标 R 的 `piece_cells.origin'`（取 `shape::piece_cells<RuleSpec, T, tgt>.origin`）把 `(mx, my)` 折回 `(xb', yb') = (mx + orig'.x, my - orig'.y)`。
- 出来的 BBState `{T, tgt, xb', yb'}` 与 `state_from_node(context_->get({T,mx,my,tgt}))` 等价。

### 2. `rotate_no_kick_TD` / `rotate_no_kick_dir`: 同样加 `_bb` sibling

### 3. `try_kick_*` / `try_kick_TRD` / `first_passing_kick*`：加 BBState 出口
- 每个 helper 都要先在源边查 board check (`usable_at_bb`)。
- 命中时直接 return BBState；不命中时 nullopt。

### 4. 切调用方
搜索 `first_passing_kick(` 当前所有调用点（grep 结果在 line 331/340/349/609/737/747/757/2174/2182/2190 附近），把 `TetrisNode const *wk = first_passing_kick(...)` + `BBState w = state_from_node(wk)` 改成 `auto wk = first_passing_kick_bb(...)`，直接用 `*wk` 作为 BBState。

### 5. 删旧 TetrisNode\* 重载
**不在 7a-1 做**。等 7a-2 / 7a-3 把所有调用点都切完再删。**7a-1 只新增、不删除**——这是单一提交风险最低的拆法。

## 验证序
1. 加完 `_bb` 重载后立刻编译，确认新代码本身能过。
2. 切第一个调用点（建议从 `run_piece_20g` 内部 BBState 路径起，那里没有 mark 表 TetrisNode\* 依赖）。
3. `oracle_diff` 全场景。
4. 单提交粒度：7a-1 只切 `run_piece_20g` 这一处；其他调用点（make_path 1g BFS、try_kick_chain）拆到 7a-2 / 7a-3。

## 风险点
- 坐标折回公式必须在两端 `piece_cells.origin` 严格对齐。如果 oracle_diff 出 r/x/y 差异，第一时间打印 `master.status.{x,y,r}` vs 我方 `(T, tgt_r, xb', yb')` 反折回的 `(mx, my)` 对一遍。
- `usable_at_bb` 对 R / xb / yb 边界要求 (`xb >= 0 && yb >= 0`)，旋转之后可能越界，旧代码靠 `context_->get` 返 nullptr 吸收，新代码必须显式 bound check。

## 下次会话第一步指令
> 在 `src/movegen_search.h` 中 `rotate_no_kick_TRD` 之后插入 `rotate_no_kick_TRD_bb`（返回 `std::optional<BBState>`），然后逐层加 TD_bb / dir_bb 包装。完成后只切 `run_piece_20g` 内的 first_passing_kick 调用，跑 oracle_diff 验证，单 commit 推。
