# Commit 3b 接入 - 已落地交接

## 状态
- 分支: `flip-bits-clean` (HEAD = `4e248a0` Drive MoveGenSearch::search through native bitboard MoveGen)
- **未推送**, 等用户确认.
- Commit 序列:
  - `42923c1` Route AI engines through MoveGenSearch adapter
  - `aba81fb` Extend LandingPos with rotation predecessor placeholders
  - `92ec953` Track last-rotate predecessor in MoveGen LandingPos
  - `dcb9b85` Parameterize MoveGenSearch on RuleSpec
  - **`4e248a0` Drive MoveGenSearch::search through native bitboard MoveGen** ← 本次落地

## 6 条假设核对结果
1. ✅ `Map<W,H>::row_t / row_full / set_row` 在 `src/tetris_map.h:73-161` 全部存在, 字段名/语义与草稿完全一致.
2. ✅ 但**不需要**: oracle_diff 既有调用 `MoveGen::generate(board, 3, 21)` 直接对比 `LandingPos.{r,x,y}` vs `master node->status.{r,x,y}`, 已经是 master 坐标系, 无需 origin 偏移换算.
3. ✅ 与第 2 条同根: spawn 直接传 `(3, 21)`, 不动.
4. ✅ canonical r 已对齐 (oracle_diff 全 7 piece × 多场地 126 个 case 全过).
5. ❌ 草稿写错: master 非 T 分支 *不* 设 `is_check=true` (search_tspin.cpp:526/625 仅 push_back). 已修正为只在 T 块内置.
6. ❌ 真 BUG, 已修正: `is_20g` 模式整体退化到老引擎 `impl_.search`, 不走 MoveGen.

## 实现要点 (commit 4e248a0)
- 公开类型/签名零变化, AI 端继续走 typedef.
- `search()`:
  - 入口 check + `is_20g` 退化保护.
  - 按 `node->status.t` switch 7 piece (T 走 EnableMini=true).
  - 未知/非 SRS piece → `impl_.search`.
- `run_piece<T, EnableMini>`:
  - `build_board(map)`: master row 1=空 → map_t row 1=占, `(~row[y]) & row_full`.
  - `MoveGen::generate(board, 3, 21, collect)`.
  - `collect`: `context_->get({T, lp.x, lp.y, lp.r}) → TetrisNode*`, T 块按 `lp.spin/has_last_rot` 写满 `is_check / is_last_rotate / is_ready / is_mini_ready / last`, 与 search_tspin.cpp:1086-1091 1:1.
- `make_path()` 仍委托 `impl_.make_path` (commit 4 才接管).

## 已知行为差异 (无害)
对纯平移到达 corners3 满足的位置 (没有 last_rotate 前驱), 例如 T 块直接平移落到 3-corner 但没经过任何 rotation:
- master: `is_check=1, is_last_rotate=0, is_ready=1, is_mini_ready=可能 1`
- 新 (T 走 MoveGen): `is_check=1, is_last_rotate=0, is_ready=0, is_mini_ready=0`

AI 评估门 `is_check && is_last_rotate && (is_ready || is_mini_ready)` 因 `is_last_rotate=0` 都被 false, 评估结果一致. 但若有任何代码路径单独读 `is_ready` (本仓库内 grep 过 `is_ready` 仅见于 ai_zzz.cpp / ai_misaka.cpp 上述 AND 链), 行为不变.

## 风险与下一步
- **未编译**, 当前环境无编译条件. 提交是基于:
  - oracle_diff 验证过 LandingPos.{r,x,y,spin} ≡ master node 字段
  - 静态对照 master Search::search_t emit 块逐字段 1:1 复制
  - 类型层 `MoveGenSearch` typedef 与 `search_tspin::Search` 行为接口完全一致
- 推送前请用户/或 CI 编译一次跑 oracle_diff + 任意一个 AI 主流程冒烟.
- 下一步:
  - **commit 4 (make_path 重写)**: 走位板 BFS, 严格复刻 D→x→z→c→l→r→L→R→d→D 启发链 + LR swift + disable_d 双 pass + T-spin replay.
  - 草稿副本不再需要 (movegen_search.h.commit3b_draft) — 本轮已落地, 可删可留.

## 不可推送
- 待用户对源码 review 通过 + 编译/测试确认后才 push.
- 当前 origin 仍停在用户上传的 dcb9b85 之前的状态.
