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

**Spawn 自适应支持 (rule_st / rule_qq / rule_tag / rule_botris) — 补充约束**
- rule_st.cpp: `(width/2, height-2)` — W/H 函数
- rule_qq.cpp: `(width/2 - 2, height-1)` — W/H 函数
- rule_tag.cpp: `(width/2 - 2, height)` — W/H 函数
- rule_botris.cpp: `O -> (4, 20)`, 其它 piece `(3, 20)` — **per-piece** 不同
- 三条都是 width/height 函数, 不是常量, 静态字面 OpDesc.spawn_x 表达不了
- botris 又把 spawn 区分到 piece 类型 — 完整签名是 `spawn(char PT, int W, int H)`
- 设计选项:
  * OPT-1: OpDesc.spawn 仍存 fixed 字面量, 但 RuleSpec 增加 `SpawnPolicy` 模板参,
    内置 `FixedSpawn<X,Y>` / `AdaptiveCenter<DX, DY>` 等; rule 顶层选 policy.
  * OPT-2: OpDesc.spawn 改成支持小型公式 — 每 piece 重复, 不推荐
  * OPT-3 (★ 推荐): RuleSpec 加 `static constexpr (int, int) spawn(char PT, int W, int H)`,
    各 rule 自填:
      - SRS 系: `return {3, 21}`  (PT/W/H 全忽略)
      - rule_st: `return {W/2, H-2}`  (PT 忽略)
      - rule_qq: `return {W/2-2, H-1}`
      - rule_tag: `return {W/2-2, H}`
      - rule_botris: `return {PT=='O' ? 4 : 3, 20}`
- OPT-3 一次满足 W/H 函数 + per-piece 两个维度

**A4 修正：OpDesc.spawn_x / spawn_y 不删，它是 init 期的"用户坐标系锚点"**
- 用户澄清：OpDesc.spawn_x / spawn_y 仅在 init 期被读, 用途是
  "用户视角坐标系 → 框架内部规范化坐标系"的对齐.
- 含义：rule 作者写 OpLines 时, 用的可能是 "以 piece 的某个特定 cell 为原点" 的坐标系
  (不是矩阵 4x4 左下/左上). OpDesc.spawn_x/y 描述该锚点 cell 在矩阵里的位置.
  这是给 *用户写 rule* 的便利, 不同规则下作者描述位置的方式不同.
- 现状证据：
  * `tetris_core.cpp::create_node(w, h, T, X, Y, R, ...)` 把 X/Y 传进去, 设置
    status = (T, X, h-Y-1, R), 建出每个 piece 的 base 节点.
  * `tetris_core.h:2573` `op_create_bridge` 直接拿 `Op::spawn_x / spawn_y` 调 create_node,
    所以 OpDesc.spawn_x/y 的真实角色是 **base 节点位置参数**, 不是游戏 spawn.
  * rule_st.h 全填 `(2, 1)`、rule_botris.h 给 O 填 `(1, 0)` / 其它 `(0, 0)` — 都是
    "作者口中的 spawn 锚点对应矩阵的哪个 cell".
- 与 `RuleSpec::spawn(PT, W, H)` 的关系：**正交两层**
  * `RuleSpec::spawn(PT, W, H)` = 用户视角下"piece 出生位置"
  * `OpDesc.spawn_x / spawn_y`  = 用户视角下的"位置锚点对应矩阵哪个 cell"
  * 框架内部规范化坐标 = `RuleSpec::spawn(PT, W, H)` 经 OpDesc 锚点折算后的 (sx, sy)
- 结论：A4 不再是"删字段"; 改为"明确文档+保留作者便利". 实际改动在 A1~A3 落地时
  额外注意一点: MoveGenSearch / game_generate_template 取 spawn 时, 默认从
  `RuleSpec::spawn(PT, W, H)` 取用户视角值; create_node / TetrisContext 内部链路
  仍走 OpDesc.spawn_x/y 不动.

---

**B. 旋转数 / piece 数解硬编码** (3 个 commit)
- (1) tetris_movegen.h: 把 R_count<=4 的手动展开改成 `[&]<std::size_t...Rs>(std::index_sequence<Rs...>)`
- (2) movegen_search.h: kMaxR / kPieceCount 改由 RuleSpec 推导；piece_index 改 RuleSpec 编译期查表
- (3) shape Cell 数组上限可适度拉大 (eg. 64) — 仅在新 piece 引入时

**C. C2 / 大盘 / 新 piece 真正接通** (大改, 多 commit)
- 必须先把 A、B 做完才有意义

待用户在 A/B/C 里选一个起点，我再开始干活。
