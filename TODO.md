# TODO

## Open

- [ ] 性能与强度未测：eval 缓存最优桶数（命中率/内存，现默认 2^16）、wall-ms 强度（io_sim 配对种子/交换座位，预算 `think_ms`）、缓存 on/off 全搜索 path 等价（当前只在单 limit-0 验 path+status）。
- [ ] full Debug rc0 串行套件 37 项 36 pass，唯一 fail 是基线预存 safe-BPM old-generation 断言（pre-existing，非迁移引入）；待修。
- [ ] parallel engine 观测到非确定性，尚未归因。
- [ ] C 组有效域 field-exact 计数 ≥239，最终精确数待 C owner 收口。
- [ ] 未 commit / 未 deploy（Native 双架构 Debug/Release dlopen 步骤已过、C2 专项双架构 6 过）。
- [ ] 对手节奏作为 `ai_zzz::IO` 的评估输入：`OpponentView` 加对手实际 pps（io_uds 用 `opponent.pieces_placed` 和 `frame` 估算，io_sim 用对手的 `Pace`），pending/cancel/opponent_surge 按倍速插值
  - 前提：io_cmaes 的 `games.csv` 按 ratio 分组显示单一权重顾此失彼（快对手组变好时等速组变差）。依据：`.research/io-cmaes-training-quality/plan.md` §4。
- [ ] io_sim 的击杀判定对手免费落子数与客户端 `auto_lethal_free_pieces` 对齐（对手平均节奏 + 12 帧回放刷新 + ping），目前用对手已排好的锁定帧，比实战多知道信息

## Done
- [x] 2026-10-29 eval(map)+GetArgs 契约迁移完成：框架头 `src/tetris_eval_get.h` 落地，全部 15 个 src AI 迁到新契约；换位剪枝已实际全删除，无剪枝重复展开由独立 repeats-500 测试覆盖；engine 组 3+5+7 全 pass；逐 AI 数值对拍 A 384 字段 / B 52 / D 扩大（misaka 121500、ax 18000、farter 3600）/ C golden 独立注册 PASS（241 行 golden，240 cases / 2460 obs matched，其余字段 bit-exact；旧 tag 边界 UB 不语义承诺）；core counter/wrap/tree scale 80→160 测试 pass；Native 双架构 Debug/Release dlopen 步骤全 pass，C2 专项四 slice smoke 6 pass。full Debug rc0 串行 37 项 36 pass，唯一 fail 为基线预存 safe-BPM old-generation 断言。无 ASan dataRace 洁净、全搜索 path 等价、wall-ms 强度、最优桶数仍待测（见 Open）。
- [x] 2026-10-28 落地 eval(map)+GetArgs 框架头 `src/tetris_eval_get.h`：`type_list`/`args::*` 标签、`GetLanding<SpinT>`、`EvalCache<MapT,Result>`（post-map 直接映射、Config 变更 generation 失效）。旧"父 map 各 lane + lp_key"缓存草案被本真实 post-map 设计取代。
- [x] 2026-10-25 改正 Map polarity：ecce526 把 `TetrisMap` 改为 0=occupied 存储后，`io_sim_test`、`ai_zzz_io_test`、`ai_zzz_io_spin_slots_test`、`engine_regression_test`、`amini_witness_test`、`amini_classification_test` 的 fixture 仍按 1=occupied 用 `set_row()` / `row()`，改为 `row_full ^ occupied` / `occupied_row()`；AGENTS.md 的说明同步改正
