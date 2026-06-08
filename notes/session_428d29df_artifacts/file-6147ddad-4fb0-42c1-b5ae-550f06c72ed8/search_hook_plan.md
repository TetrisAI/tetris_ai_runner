# Search Hook 重构 — 思路 / 反驳 / 方案选项

> 状态: 等用户拍板. 没有任何 commit.
> 关键字: search hook trait kEnableT EnableMini movegen_search tetris_movegen tspin aspin

## 0. 当前任务陈述
> "目前 search_tspin 对 T 做额外处理, search_aspin 会对所有块做额外处理.
> kEnableT 不应该在框架中出现.
> 应该是每个 Search 可定制的, 其在搜索路径中注入一些逻辑, 用来计算额外的信息"

## 1. 调研定锚 (file:line)

### 框架层硬编码"T-only"的位置 (本次要剥离的目标)
| 文件 | 行 | 内容 | 是否真的 T-专属 |
|---|---|---|---|
| `tetris_movegen.h` | 191 | `static constexpr bool kCheckTSpin = EnableMini && (T == 'T');` | 是. T 之外即便 EnableMini 也不染色. |
| `movegen_search.h` | 1881-1888 | `run_piece_20g_dispatch<T>`: `constexpr bool kEnable = (T == 'T');` | 是. `EnableT` 完全由 piece 字面量决定. |
| `movegen_search.h` | 1891-1896 | `run_piece_dispatch<T>`: 同上 | 是. |

### 框架层"通用 T-spin 知识"的位置 (这些是 hook 化时要搬迁的)
| 文件 | 行 | 内容 |
|---|---|---|
| `tetris_movegen.h` | 25-42 | `LandingPos { spin / last_x/y/r / has_last_rot }` — **T-spin 字段直接挂在框架结构体上** |
| `tetris_movegen.h` | 178 | `template<class Spec, char T, bool EnableMini = false> class MoveGen` |
| `tetris_movegen.h` | 310-313 | `corners3_arr` 仅 kCheckTSpin 时计算 |
| `tetris_movegen.h` | 333,364 | `last_rotate_arr` 在 `expand_rotations` 中 conditionally 维护 |
| `tetris_movegen.h` | 559-579 | `emit_with_spin` — `is_ready/is_mini/full/none` 三档输出 |
| `tetris_movegen.h` | 660-681 | `compute_corners3` — 框架里直接算 T-corner |
| `movegen_search.h` | 150-153 | `using TSpinType / Config / TetrisNodeWithTSpinType = search_tspin::Search::...` — **MoveGenSearch 公开类型直接绑死 search_tspin**, AI 模块 typedef 跟着绑死 |
| `movegen_search.h` | 170-205 | `init()` 中重建 `spin_x_diff_/spin_y_diff_/spin_block_` (T-spin block_data_) — **T 专属字段挂在 MoveGenSearch 成员里** |
| `movegen_search.h` | 1862-1873 | `check_mini_ready_native` — 框架在 emit 时直接调 |
| `movegen_search.h` | 2049-2076 | `run_piece_20g` 内 `if constexpr (EnableT)` 写 `last/is_check/is_last_rotate/is_ready/is_mini_ready` |
| `movegen_search.h` | 2156-2173 | `run_piece` 内同上 |

### search_aspin 的现状
- `search_aspin/h+cpp`: 完全独立的旧 search, **没接入** MoveGen / MoveGenSearch.
- ASpin 判定 (`search_aspin.cpp:411-414`):
  ```cpp
  if ((!move_down || !move_down->check(snap)) &&
      (!move_up || !move_up->check(snap)) &&
      (!move_left || !move_left->check(snap)) &&
      (!move_right || !move_right->check(snap)))
      type = ASpin;
  ```
  适用于所有 piece, 与 piece 类型无关.
- 调用方 (`ai.cpp:322-323`, `botris.cpp:72`): `TetrisThreadEngine<rule_botris::TetrisRule, ai_zzz::Botris, search_aspin::Search>` — 与 search_tspin 是 **engine 模板参数互斥的两套 search**.

## 2. 自洽性反驳 (在动手前要先讲清楚)

### 反驳点 A: "kEnableT 不应该在框架中出现" 这话只在 `movegen_search` 的 piece-dispatch 层成立
- `tetris_movegen.h:191` 的 `kCheckTSpin = EnableMini && (T == 'T')` 严格说**也是框架**, 也得改.
- `LandingPos { spin / last_x/y/r / has_last_rot }` 字段也是框架在直接持有 T-spin 概念. 真正彻底的 hook 化要把这些字段也踢出 `LandingPos`, 改成 trait 自定义的 payload 类型.
- 我的看法: 如果只把 1881-1896 两个 `(T=='T')` switch 拿掉, 不动 `MoveGen` 和 `LandingPos`, 那只是把硬编码从 movegen_search 表层挪到 tetris_movegen 里, **没真解耦**.
- 因此本次必须连 `MoveGen<EnableMini>`、`LandingPos.spin/last_*`、`MoveGenSearch::TetrisNodeWithTSpinType` 一起走.

### 反驳点 B: "search_aspin 会对所有块做额外处理" 这一前提与"接入 hook"强耦合
- search_aspin 现在压根不走 MoveGenSearch. 如果 hook 重构只发生在 MoveGenSearch / MoveGen 内, search_aspin 自己保持独立, 那 hook 接口的设计就必须能容纳 "ASpin 是落点完成后才能算的, 看的是 4 个方向 collision check, 不依赖 last_rotate 链", 与 T-spin 的 hook (依赖 `last_rotate_arr` 与 `corners3`) 是**形态不同**的两类:
  - **T-spin hook**: 在 BFS 内随 search 维护 per-rotation 的 `last_rotate_arr`, 依赖踢墙链.
  - **A-spin hook**: 在每个 emit 出来的 landing 上做一次 4-direction `usable[r].get(...)` 查询, 与 `last_rotate_arr` 无关.
- 这两类如果用同一个 hook 接口, 接口必须够通用 (允许 hook 决定"是否需要 last_rotate 维护"、"是否需要 corners 维护", 0 开销裁掉不需要的).
- 反过来: 如果只想把 search_tspin 的 T-spin 计算 hook 化, 而 search_aspin 仍然走老 search_aspin BFS, 那 hook 接口就只需要服务 T-spin, **不必为 ASpin 留接口**, 范围小很多.

### 反驳点 C: "每个 Search 可定制" 暗示什么样的"Search"?
- 解释 1: `MoveGenSearch` 自己作为 *唯一* Search, 通过 trait 模板参数让消费侧 (AI typedef) 选择 "T-spin trait" 还是 "A-spin trait" 还是 "no-spin trait". → 那等价于让 `search_tspin::Search` / `search_aspin::Search` 退化为 trait.
- 解释 2: 保留 `search_tspin::Search` / `search_aspin::Search` 的现有类签名 (engine 模板参数仍是它), 但内部都委托 `MoveGenSearch<RuleSpec, Trait>`, trait 由各自 search class 提供. → 上层兼容, 改造小.
- 哪种是你想要的? 解释 2 更稳, 但解释 1 才彻底.

## 3. 方案选项

### Plan H1 — 最小手术 (只剥 piece-dispatch 层 (T=='T'), 不动 LandingPos / MoveGen)
- 改动 1: `movegen_search.h:1881-1896` 把 `(T == 'T')` 替换成编译期 trait `Trait::piece_needs_spin<T>::value`, trait 由模板参数注入 (默认 trait 是当前的 SRS-7 T-only 选择, 全等价).
- 改动 2: `MoveGenSearch` 增加 `template<class RuleSpec, class Trait = DefaultTSpinTrait<RuleSpec>>`.
- 改动 3: `tetris_movegen.h:191` 同步用 trait 替代 `(T == 'T')`.
- **不动**: `LandingPos`, `TetrisNodeWithTSpinType`, search_aspin.
- 优点: 改动小 (~80 行), oracle_diff 不变.
- 缺点: 框架结构里仍持有 spin/last_* 字段; "T-spin 概念"仍漏在 LandingPos 上, hook 没真分层.

### Plan H2 — 半解耦 (LandingPos 模板化, T-spin 字段下放到 trait payload)
- 改动 1: `LandingPos` 拆成基础部分 (x/y/r) 和扩展部分. 扩展部分由 trait 决定 (e.g. `LandingPos<Trait>` 用 `[[no_unique_address]] Trait::Payload extra`).
- 改动 2: `MoveGen` 在 emit 阶段调 `Trait::on_emit(landings, last_rotate_arr, corners*, lp)` 让 trait 写 payload.
- 改动 3: `MoveGen` 的 `corners3_arr / last_rotate_arr / emit_with_spin / target_blocked_mask / direction_open_mask` 全部搬进 trait 里 (trait 提供 `prepare_per_rotation_state(...)` 和 `on_emit(...)`).
- 改动 4: `MoveGenSearch` 用 trait 决定 `TetrisNodeWithSpinType` 别名.
- **不动**: search_aspin (它继续用旧路径).
- 优点: hook 接口真正存在, T-spin trait 可被替换/新增 piece 自带 spin 也支持.
- 缺点: 改动量大 (~400-600 行), 涉及 oracle_diff 关键路径, 风险高. 也不解决 search_aspin 的非 hook 化问题.

### Plan H3 — 全解耦 (search_tspin / search_aspin 都用 hook 重写, 走 MoveGenSearch)
- 在 H2 基础上: 把 `search_aspin::Search` 改成 trait, 也走 MoveGenSearch BFS, hook 在 emit 时按 4 方向 collision 染色.
- 改动量超大 (~800-1200 行), 涉及到底要不要保留 `search_aspin::Search` 的旧 BFS (botris.cpp/cmd_tris.cpp 是直接 typedef 它的).
- 优点: 框架最干净, 真正"Search 自定义钩子".
- 缺点: 风险极高, oracle_diff 只覆盖 SRS-7 T-spin, ASpin 没有对照基准, 容易引入静默回归.

## 4. 我倾向

**先做 Plan H1**, 后续再视需要升级到 H2.
理由:
1. 任务陈述里明确点名 "kEnableT 不应该在框架中出现" — H1 已经满足该字面要求. (kEnableT/(T=='T')` 从 movegen_search 完全消失, 由 trait 决定.)
2. `tetris_movegen.h:191` 的 `kCheckTSpin = EnableMini && (T == 'T')` 也可以同步换成 `kCheckTSpin = EnableMini && Trait::needs_spin<T>::value`, 这一处仍在框架, 但 piece 的"T 字面量"已被消掉.
3. H2 涉及 LandingPos 重构, 是 Plan B (master 解绑) 的范畴, 跟"去 kEnableT"目标耦合度低, 容易越界.
4. search_aspin 先不动 — 它本来就跟 movegen_search 平行, hook 化它是另一个独立项目 (而且没有 oracle baseline).

## 5. 风险清单 (针对 H1)
- T-spin trait 默认要保持 `T-only` 行为, 不能让 'I/O/J/L/S/Z' 任何一个误开 spin (oracle_diff 会立刻挂).
- `MoveGenSearch::init()` 里 spin_block_/spin_x_diff_ 计算依赖 spawn('T'), trait 化后这一段要么仍只在 T-trait 下被调用, 要么搬进 trait::init.
- 现有 AI typedef (`ai_zzz.h`, `ai_misaka.h`) 都直接写 `search_tspin::Search::TetrisNodeWithTSpinType`. 如果 MoveGenSearch 加了 trait 模板参数, 调用侧 `using Engine = TetrisEngine<Rule, AI, MoveGenSearch>` 还是 `MoveGenSearch<Rule, TSpinTrait>`? 要看 ai.cpp 里 engine 实例化处.

## 6. 待用户决定
1. 选 **H1 / H2 / H3**?
2. 改动是否要在同一 commit 里完成 (push 前若不自洽, 要重整)?
3. search_aspin 是不是这次根本不动?
