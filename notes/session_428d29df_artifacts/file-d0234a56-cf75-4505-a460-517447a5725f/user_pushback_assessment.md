# 用户反驳的核查结论 (2026-05-28)

## 用户 5 点反驳，逐条核查

### 1. ✅ spawn 位置：用户对，框架未达成

- `tetris_rule_spec.h:76-77` `OpDesc::spawn_x/spawn_y` 字段已存在
- `tetris_shape.h:131-135` 已 export `piece_spawn_x<Spec,T,R>` / `piece_spawn_y<Spec,T,R>`
- 但**所有 rule_*.h 里 OpDesc 填的都是 `0, 0`**（包含 rule_c2.h, rule_srs.h, rule_toj.h …）
- **所有 rule_*.cpp 里 `game_generate_template` 硬编码 `(3, 21)`**:
  - `rule_srs.cpp:16`、`rule_toj.cpp:14`、`rule_ppt.cpp:14`、`rule_c2.cpp:14`、`rule_srsx.cpp:14`、`rule_asrs.cpp:14`
- `movegen_search.h:120, 2025, 2298` 也硬编码 `3, 21`
- 结论：字段建起来了，但没真正接通。要改：
  1. 各 rule 在 OpDesc 上填上 spawn (一般为 (3, 21)，C2 / 大盘可不同)
  2. `game_generate_template` 用 `shape::piece_spawn_x<rule_spec, T, 0>`
  3. `MoveGenSearch::run_piece` 同样切到模板取

### 2. ✅ 旋转数量 per-piece：用户对，框架已经支持只是没贯通

- `tetris_shape.h` `shape::rotation_count<Spec, T>` 已经 per-piece 算
- `tetris_movegen.h` 的 `MoveGen<Spec, T>::R_count` 也是 per-piece, 已用 `if constexpr` 展开
- 但有几个硬上限：
  - `tetris_movegen.h` 多处 `static_assert(R_count <= 4, "...currently unrolled to 4")` —
    需要把展开改成 fold-expression / index_sequence
  - `tetris_shape.h:207` `std::array<Cell, 32>` —— 32 cell 上限；图片里 T-10 / O-10 至多 16 cell 内
  - `movegen_search.h:63` `kMaxR = 4` —— LUT 第二维硬上限。要改成
    `static constexpr int kMaxR = max over T of rotation_count<Spec, T>`
  - `movegen_search.h:2348` LUT 填表手动展开到 R<4

### 3. ✅ build_snap：用户对，我自己 commit 8d 已经在 MoveGenSearch 里删干净了

- 我之前提"search_tag / search_aspin / search_simple 还要保留 TetrisMapSnap" 与 MoveGenSearch 无关
- 那几条老 search 引擎不是我们扩展目标的一部分

### 4. ✅ 不需要 oracle 对比：用户对，我自己设限了

- oracle = `search_tspin::Search`，本就 SRS-7 hardcoded、4 旋转、10x40 盘
- 新框架做的就是"取代它"，再用它做断言反过来锁死了功能域
- oracle_diff 应当退役 / 弱化（或仅作为"SRS 系做回归检查"的可选路径），不应该是接受/拒绝改造的判据

### 5. ✅ C2 kick 表：用户对，已有 RuleSpec 三参足够表达

- `rule_c2.h:17` `using C2Kick = WallKickList<-1,0, +1,0, 0,-1, -1,-1, +1,-1, -2,0, +2,0>` 一份
- 所有 OpDesc 里 cw/ccw/opp 三参都填同一个 `C2Kick`
- 不需要额外改框架，照搬现有 schema 即可

---

## 我之前 pushback 的错误清单

| 我说过的 | 实际情况 |
|---|---|
| "10-piece 需要重写 piece switch / kPieceCount=7" | RuleSpec 用字符索引 ops，不需要硬编码 piece 数；只需把 `piece_index` 改成"在 ops_tuple 里找 type==T 的位置"的编译期表 |
| "5-rotation 是 blocker" | shape::rotation_count 已 per-piece，仅 movegen R_count<=4 手动展开和 LUT 维度需要松开 |
| "12x30 是 blocker" | Map<W,H> / MoveGen<Spec> 已经全模板化，RuleSpec::W,H 改值即可 |
| "[52] buffer 是 SRS hardcode" | 那是 search_tspin / search_tag 老引擎的内部，与 MoveGenSearch 无关 |
| "需要 oracle_diff 守门" | 自我设限，不是用户要求 |

---

## 仍存在的真实差距 (这才是要协商的范围)

A. **TetrisContext**：MoveGenSearch 仍依赖 `context_->get(status)` 把 BBState 反查回 TetrisNode*。
   要支持新的 piece / rotation 数，TetrisContext::generate 也得能为它们造节点。
   仓库里已有 rule_c2 / rule_srsx 等多套规则，TetrisContext 接受任意 RuleSpec 是设计目标，
   现状大概率已能 (但需要核对 init 时是否假设 SRS-7 / 4-rot)。

B. **AI 评估器**: `ai_zzz` / `ai_misaka` / `ai_tag` 的 evaluation 是给 SRS-7 调的；
   不会拒绝异种 piece 输入，但评估质量没有保证。
   用户说"不需要 oracle 对比"=承认评估质量不是本轮目标，能跑就行。

C. **generator (摇骰子)**: rule_c2.cpp 的 `get_generate` 是 7 piece bag。
   要拓 10 piece，需要在生成器一侧也加上对应 entry。

D. **status 字段类型**: TetrisBlockStatus.x/y 是 int8_t (movegen LandingPos 同款)；
   12 列 / 30 行依然在范围内。spawn (3, 21) 同理。

---

## 下一步该做什么 — 等用户拍板

候选最小 commit 序列 (任选)：

**A. Spawn 解硬编码** (3 个 commit, 单一职责)
- (1) tetris_rule_spec / shape：补 OpDesc.spawn 默认值 + 各 rule 填 (3, 21) / 自定
- (2) game_generate_template: 切到 piece_spawn_x/y 模板
- (3) MoveGenSearch: 用 `shape::piece_spawn_x<RuleSpec, T, 0>` 替换硬编码 3, 21

**B. 旋转数 / piece 数解硬编码** (3 个 commit)
- (1) tetris_movegen.h: 把 R_count<=4 的手动展开改成 `[&]<std::size_t...Rs>(std::index_sequence<Rs...>)`
- (2) movegen_search.h: kMaxR / kPieceCount 改由 RuleSpec 推导；piece_index 改 RuleSpec 编译期查表
- (3) shape Cell 数组上限可适度拉大 (eg. 64) — 仅在新 piece 引入时

**C. C2 / 大盘 / 新 piece 真正接通** (大改, 多 commit)
- 必须先把 A、B 做完才有意义

待用户在 A/B/C 里选一个起点，我再开始干活。
