# TODO

## Open

- [ ] 本次后续补强内容待提交与部署：full Debug rc0 已跑 41 tests / 40 pass / 1 fail（唯一 pre-existing cultris_safe_regression 旧 BPM-generation；41 = 37 原 + 初阶 negative + 两 failure-API acceptance 各 46 assert + Config v5 23 assert；日志 build_validation_followup/Testing/Temporary/LastTest.log）。新生产 commit + 四库重建部署待执行，当前仍 4b2d506a / fat 34f546d，未写新 hash。
- [ ] 性能与强度未测：eval 缓存最优桶数（命中率/内存，现默认 2^16）、wall-ms 强度（io_sim 配对种子/交换座位，预算 `think_ms`）、缓存 on/off 全搜索 path 等价（当前只在单 limit-0 验 path+status）、完整对局/GUI/perf 未跑。
- [ ] C golden 观测 240 cases / 2460 field（旧 tag 边界 UB 在 LLVM 下排除）。
- [ ] 上层 App 越权覆盖：已找到哈希匹配原库备份，**明确不恢复、对上层 App 零写入**；如要恢复须用户明确授权。
- [ ] 对手节奏作为 `ai_zzz::IO` 的评估输入：`OpponentView` 加对手实际 pps（io_uds 用 `opponent.pieces_placed` 和 `frame` 估算，io_sim 用对手的 `Pace`），pending/cancel/opponent_surge 按倍速插值
  - 前提：io_cmaes 的 `games.csv` 按 ratio 分组显示单一权重顾此失彼（快对手组变好时等速组变差）。依据：`.research/io-cmaes-training-quality/plan.md` §4。
- [ ] io_sim 的击杀判定对手免费落子数与客户端 `auto_lethal_free_pieces` 对齐（对手平均节奏 + 12 帧回放刷新 + ping），目前用对手已排好的锁定帧，比实战多知道信息

## Done
- [x] 2026-10-29 core follow-up 专项（已实测通过，bounded）：prepare 非零 2^n / 元素 bytes bound 拒绝、try_lookup false/unprepared out 不变、Engine/Thread prepare 传播、无参 run/run_hold 优雅 false vs 带 map 调用 NDEBUG fatal、NoConfig thread 空 holder/readonly current best/visit_tree；D 失败 API 46 assert（Debug/-O2 NDEBUG = 40 base + 6 NoCfgThread），SIGABRT×2 与 Result copy runtime error 不吞；multi-widen 64 步两 mode scored 30914 hits 65 vs 0、final target/path/status exact；Config v5 独立 23 assert pass（旧树 cfg1->cfg2 vs fresh：619544 Eval/raw direct、3630 virtual parents piece-best child.status.get 独立 rank avg 全 4 字段、两 phase root 输入保、true-clear pos 311、get-only config gen 3->5）；parallel bounded（旧/new 同 5-TSD wall 300ms 独占过、并发搜索量降，未 TSan 不排除全局 race、不据探针推阈值、原断言未削）。
- [x] 2026-10-29 eval(map)+GetArgs 契约迁移完成：框架头 `src/tetris_eval_get.h` 落地，全部 15 个 src AI 迁到新契约；换位剪枝已实际全删除，无剪枝重复展开由独立 repeats-500 测试覆盖；engine 组 3+5+7 全 pass；逐 AI 数值对拍 A 384 字段 / B 52 / D 扩大（misaka 121500、ax 18000、farter 3600）/ C golden 独立注册 PASS（241 行 golden，240 cases / 2460 obs matched，其余字段 bit-exact；旧 tag 边界 UB 不语义承诺）；core counter/wrap/tree scale 80->160 测试 pass；Native 双架构 Debug/Release dlopen 步骤全 pass，C2 专项四 slice smoke 6 pass。full Debug rc0 串行 37 项 36 pass，唯一 fail 为基线预存 safe-BPM old-generation 断言。无 ASan dataRace 洁净、全搜索 path 等价、wall-ms 强度、最优桶数仍待测（见 Open）。
- [x] 2026-10-28 落地 eval(map)+GetArgs 框架头 `src/tetris_eval_get.h`：`type_list`/`args::*` 标签、`GetLanding<SpinT>`、`EvalCache<MapT,Result>`（post-map 直接映射、Config 变更 generation 失效）。旧"父 map 各 lane + lp_key"缓存草案被本真实 post-map 设计取代。
- [x] 2026-10-25 改正 Map polarity：ecce526 把 `TetrisMap` 改为 0=occupied 存储后，`io_sim_test`、`ai_zzz_io_test`、`ai_zzz_io_spin_slots_test`、`engine_regression_test`、`amini_witness_test`、`amini_classification_test` 的 fixture 仍按 1=occupied 用 `set_row()` / `row()`，改为 `row_full ^ occupied` / `occupied_row()`；AGENTS.md 的说明同步改正
