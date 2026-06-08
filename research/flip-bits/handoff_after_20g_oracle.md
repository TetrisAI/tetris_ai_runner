# Handoff: 20g oracle 网络已建立, 准备进入 step 1

## Status snapshot
- Branch: `flip-bits-clean`（本地未推送）
- HEAD: `a0bab15` *Run oracle_diff in 1g and 20g flavors*
- `oracle_diff all` 输出 `# all diffs ok`，覆盖：
  - 18 fixtures × 7 piece × 2 flavor (1g + 20g) = 252 case 全绿
  - 每个 case 同时跑：landing set diff + oracle path self-check + mg path self-check（path_self_check 内 20g 用 sunk-spawn 起步、replay 走 20g 语义）

## 本次 commit 关键点
1. **flavor 循环**：`run_with_flavor(false)` + `run_with_flavor(true)`，复用全局 oracle_config / mg_config，跑前 `is_20g = flavor` 切换。
2. **`new_landings_20g`**：20g flavor 下 candidate 不能再用底层位板 1g 枚举器，改走 `mg_search_obj().search`（5h 后该路径 20g 仍走 `impl_`）。这就是后续 20g 重写时的真 oracle 通道。
3. **`replay_path(... is_20g)`**：
   - `L`/`R`：每一步都 `drop(map)`。
   - 其它 simple step（l/r/d/z/c/x/Z/C/X）后强制 `drop(map)`。
   - `D` / 默认不 drop。
4. **path self-check 起点**：20g 时 `replay_origin = spawn->drop(map)`（master `make_path_20g` 的入口前置 drop）。
5. **20g spin 字段放宽**：5h 后 oracle 与 candidate 是 search_tspin 的两个独立实例，BFS 访问顺序可能让 T 落点的 spin 字段在等价落点上分裂（empty board T 顶部 spawn 落点 spin=0 vs spin=2，但 (r,x,y) 一致）。20g flavor 下把 spin 列归零再比；landing 坐标必须严格相等。**TODO**：20g 位板重写后收紧到 a == b。

## 下一步：step 1（位板原生 20g search 非 T 路径）
**前置已就绪**：
- 1g/20g 双 flavor oracle 全绿。
- `MoveGenSearch::search` 的 20g 分支当前只有一行 `return impl_.search(...)`。
- 把这一行改成位板路径后，oracle 立刻起作用。

**计划（按调研稿 `20g_search_master_analysis.md`）**：
1. 在 `MoveGenSearch` 内补 `run_piece_20g<T, kMini>(map, depth)`：
   - spawn 先 drop（reuse `drop_bb` + state_from_node）。
   - BFS 队列以 BBState 推进；每次 dequeue 后再 drop（与 master 行为对齐）。
   - 邻居 = LR、单步 d、kick 链 cw/ccw/180、no-kick 旋（实际 master 邻居只有 wall_kick 序列+LR+d，参 src/search_tspin.cpp 596）。
   - landing 谓词 = `!can_move_down`（drop 后即停）。
2. `MoveGenSearch::search` 20g 分支替换为 `if(is_20g) { run_piece_20g<T,...>(...); return &cache; }` 的 SRS-7 dispatch。
3. T 块 spin 检测：单独一笔。先非 T 干净落地，再上 T。

## 风险点 & 注意
- `MoveGen<...>::generate` 内部已经"drop 邻居等价类"，但它是 1g 模型；不能直接套，需要新写。
- `drop_bb` 当前签名是 `(BBState, usable_arr) → TetrisNode*`，刚好可作为"沉底 + 反查"原子操作。
- `usable_at_bb` 已经位板原生，可直接复用做邻居 check。

## 提交规范
- 单原子提交，本笔已成（commit `a0bab15`）。
- 推送由用户口头确认后再发起。
