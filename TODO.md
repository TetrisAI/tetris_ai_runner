# TODO

## Open

- [ ] **ROOT lazy 修复已部署（code 4a77bdea / source aa50 / fat c8a1c492）；readonly 终核 PASS；docs commit 已完成（不 push），不改变 code provenance。** full ctest 43/42/1 rc8 仅 known-safe（三新增回归全清）；strict fresh aa50 九 case compare v3 rc0（165056 / 0 unmatched / per-side unique 164,240 / per-side dup 816 双侧和 1,632）；21 正负自测 case 符合；formal old 5f1f 负控 rc1 4 FAIL、new aa50 formal 11/11 rc0；ConfigTree 24/24（valid 619544 + root shell 1，shell default 不当 AI 评分）；DP 最新 source 重编 12-fresh 46/0 rc0（binary 1e5434fc…04868）；ARM fat 两轮 2-cycle load rc0。边界：1 known fail（safe_regression BPM 断言未削）、x86 动态被 arm-only 注入阻断（不 bypass、未验）、GUI/TSan/perf 未测；不 universal 15-search 等价。上层 App 34f546d… 未动；Java/JAR/ax protected hash 原样。Git commits：7af0537a→…→e0731c6ccbb2→33dc85ad(docs)→4a77bdea(deploy)。Source fingerprints（非 commit）：5f1fbe85…→6edc5bfb…→aa50db56…。
- [x] **virtual-env EnableEnv 回归（已修复，e0731c6ccbb2）**：`GetArgsInvoker(args::Env)` 曾忽略 `EnableEnv` 恒返回 `env<true>`，O(1) tag 在 virtual 节点不门控；旧 7af virtual 返回空 env。C2 `length_ratio` virtual 下按真实 queue 算 64（旧空 env 0.5）。4 生产 Env AI 无 iterate、false 路径生产不可达，misaka 仅 Core boundary 证明；实际证实回归在 C2 虚拟路径。此回归已在 e0731c6ccbb2 修复，新投放 fat `8144cd85` 不含此 bug。证据 `research/eval_get_virtual_env_correction/`、`research/c2_env_fix_resume_20261029/`。
- [x] **C2 strict gate（已通过，有限覆盖）**：strict 9-case 165056 节点 matched/0 unmatched/全字段 NONE；Config 修复 Final GO（onConfigChanged discard 子树+cachegen+frontier，1454 shared key 0 diff）；root eval slot 残留已修（update_root WantEval 刷新 root holder）。完整 ctest 42/41/1（仅 known-safe safe_regression）。**不 claim universal 15-search 等价**。
- [x] **构建与投放（已完成）**：代码 commit `e0731c6ccbb2`，新 fat `8144cd85…fd431`（1,860,720 B，arm64+x86_64 四路径同 hash）已投放四授权路径，arm64 dlopen rc0。x86_64 动态仍被 arm-only 注入阻断未验。DP e073 重编 12-fresh 46/0。旧候选 `44854a21`、旧生产 `36c6febe` 历史未用。证据 `research/build_env_resume_20261029/final/`、`research/eval_get_resume_final/README.md`。
- [ ] 上层 App 误覆盖库的还原仍待用户决定（当前上层库为 34f546d；哈希匹配原库备份 `libs/libcultrisai.dylib.backup_before_dev`=9c9d16ed…992f，明确不恢复、对上层 App 零写入）。docs-only comment commit 与生产 code commit（3770535e）显式分开，待后续提交执行。
- [ ] 性能与强度未测：eval 缓存最优桶数（命中率/内存，现默认 2^16）、wall-ms 强度（io_sim 配对种子/交换座位，预算 `think_ms`）、缓存 on/off 全搜索 path 等价（当前只在单 limit-0 验 path+status）、完整对局/GUI/perf 未跑。
- [ ] C golden 观测 240 cases / 2460 field（旧 tag 边界 UB 在 LLVM 下排除）——待复审后重确认。
- [ ] 对手节奏作为 `ai_zzz::IO` 的评估输入：`OpponentView` 加对手实际 pps（io_uds 用 `opponent.pieces_placed` 和 `frame` 估算，io_sim 用对手的 `Pace`），pending/cancel/opponent_surge 按倍速插值
  - 前提：io_cmaes 的 `games.csv` 按 ratio 分组显示单一权重顾此失彼（快对手组变好时等速组变差）。依据：`.research/io-cmaes-training-quality/plan.md` §4。
- [ ] io_sim 的击杀判定对手免费落子数与客户端 `auto_lethal_free_pieces` 对齐（对手平均节奏 + 12 帧回放刷新 + ping），目前用对手已排好的锁定帧，比实战多知道信息
- [ ] 调查 `tetris_cultris_safe_regression` 的 BPM/combo latch 失败：2026-10-28 Debug 全量验证 23/24 通过，失败断言为 “BPM invalidates old results but preserves root combo latch”；目标使用 `cultris/c2_ai_impl.cpp` 和 `src/ai_zzz.cpp`，不链接本次修改的 CMA-ES trainer。
- [ ] `src/ai.cpp` 的 `TetrisAI` / `BotrisAI3` 每次调用都 `set_ai_config`（combo 表不变也会），而 `TetrisEngine::set_ai_config` 无条件丢弃搜索树和 eval cache，后台思考随之作废；同局面预算下每步耗时 +40–45%（2026-11-06 实测）。改为配置变化时才调用。

## Done

- [x] 2026-11-06 `ai_zzz::IO` 的对手视图移出 `Config`：`RootState::opponent` 经 `make_root_status` 进 `Status`，对手 B2B / 击杀掩码变化只需 `update()`，保留搜索树和 eval cache；`io_sim` 每座位只设一次 config，`io_uds` 的同根判断含对手视图。8 局 io_sim 对局结果逐字段不变，耗时 57 s → 44 s。
