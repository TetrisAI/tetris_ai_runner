# 接入 Plan Y 进度交接（commit 92ec953 之后）

## 已落地

| commit | 内容 |
|---|---|
| 42923c1 | 5 个文件模板第三参 `search_tspin::Search` → `m_tetris::MoveGenSearch`（adapter 兜底委托）|
| aba81fb | `LandingPos` 扩 `last_x/y/r/has_last_rot` 字段（占位，has_last_rot=0）|
| 92ec953 | MoveGen emit 阶段反查 last-rotate 前驱，填 `last_*`；oracle_diff 新增 `check_last_one` 校验 |

## 还未做（Plan Y 剩余）

### commit 3 — MoveGenSearch::search 切位板原生

当前 `src/movegen_search.h` 全委托 `search_tspin::Search::search`。要做的：

1. **模板化 MoveGenSearch on RuleSpec**：让它持有 `MoveGen<RuleSpec, T, EnableMini>` 七份实例（per-piece）
   - 改 5 文件：`src/ai.cpp`、`src/pso.cpp`、`src/ppt_pso.cpp` 的模板第三参
     从 `m_tetris::MoveGenSearch` → `m_tetris::MoveGenSearch<rule_toj::TetrisRule::rule_spec>` / `rule_srs::TetrisRule::rule_spec`
   - `ai_zzz.h` / `ai_misaka.h` 的 typedef 不需要再依赖具体 search 类，可以直接 typedef 到 `m_tetris::MoveGenSearchTypes` 公共空间（把 `TSpinType / Config / TetrisNodeWithTSpinType` 抽出 MoveGenSearch 之外）

2. **TetrisMap → Map<W,H>**：每次 search 入口转一次（W=10, H=40 hot path）

3. **piece dispatch**：search(map, node, depth) 里读 `node->status.t`，switch 到 `MoveGen<Spec, 'T', kMini>::generate(...)` 等 7 个分支

4. **LandingPos → TetrisNodeWithTSpinType**：
   - `node = context_->get(TetrisBlockStatus{T, lp.x, lp.y, lp.r})`
   - `is_check = true`
   - `is_last_rotate = lp.has_last_rot != 0 || (depth==0 && config_->last_rotate)`
   - `is_ready = (lp.spin != 0)`
   - `is_mini_ready = (lp.spin == 1)`
   - `last = lp.has_last_rot ? context_->get({T, lp.last_x, lp.last_y, lp.last_r}) : nullptr`

5. **oracle_diff 守门**：对拍跑全 18 board × 7 piece 的 (r, x, y, spin) + check_last_one。

### commit 4 — make_path 位板原生重写

参考 `path_redesign.md` 4.1-4.5：拟人化算子顺序 D→x→z→c→l→r→L→R→d→D，disable_d 双轮，allow_rotate_move 旁路，T-spin last 重放，20g 分支。

**关键约束**（用户原话）：path 必须保持完全一致；LR 速移必须照搬。

### commit 5/6 — 20g + 删 oracle baseline 之外的老路径

## 关键风险点

1. **`context_->get(status)` O(1) 反查**：依赖 hash，性能 OK
2. **TetrisContext 仍未消除**：本轮接入仅消除"指针图依赖"，TetrisContext 本身保留（见 `path_redesign.md` 5）
3. **search_tspin 删除时机**：保留作 oracle baseline 直到所有 commit 落地，最后一笔再 evict

## 当前 oracle_diff 状态

- 编译: `cd build && make oracle_diff -j8`
- 运行: `build/oracle_diff all` → 126/126 ok（含 last 字段断言）

## 未推送的 commit

92ec953（commit 2-rot）和上一轮的 42923c1 / aba81fb 都未推送，等用户确认后一次性 push。
