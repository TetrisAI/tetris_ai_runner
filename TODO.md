# TODO

## Open

- [ ] 删除 19e143c 的同深度换位剪枝
  - 原因：同一盘面换个摆放顺序，抗风险能力和攻击都不一样。换位键不含累计得分（`like`、`value`、`sent`、`surge_sent`、`cancelled`、`b2b_gained`、`max_combo`），所以先弹出的副本胜出，之后加宽出的更优副本也被丢掉。`get` 依赖整条序列，不能去重。
  - 代码：删除 `src/tetris_core.h` 里的 `kTranspositions`、`Context::expanded_keys`、`TetrisTreeNode::transposition_key`，以及 `run` 里 `push_one` 的跳过分支；删除 `IO::Status::transposition_key`（`src/ai_zzz_io.h/.cpp`）。`hash_combine` 保留给 eval 缓存。
  - 测试：删除 `test_transposition_key`（`test/ai_zzz_io_test.cpp`），以及 `beam_expands_each_position_once_per_depth` 和它的 `kTranspositionKey` 探针（`test/engine_regression_test.cpp`）。
  - AGENTS.md：AI contract 里删掉 `Status::transposition_key()` 的两处；`tetris_engine_regression` 的覆盖列表删掉 per-depth transposition skipping。
- [ ] eval 结果缓存（直接映射）
  - `eval` 只看盘面，结果可以复用；`get` 仍然每个节点调用一次，`scored()` 的含义不变。
  - 键：父节点 `map` 的每个 lane 加上 `lp_key(lp)`（含 spin class），用 `hash_combine` 合成 64 位。落子后的盘面和 `clear` 都由这两者决定，不用入键。
  - 表：放在 `Context` 里，每个引擎一份。桶数是 2 的幂，每桶存一个 `{key, Result}`，下标取键的低位。前台和后台由 `PauseBackground` 互斥，不加锁。
  - 查找：入口是 `alloc_eval_typed`（引擎里唯一调用 eval 的地方）。桶里的键相同就直接用缓存的 `Result`；不同就调 `eval`，覆盖这个桶。不要冲突链，不要开放寻址，不淘汰也不清理。空桶要和所有真实键区分开。
  - 失效：`prepare()`（AI `init`）时重置。`update()` 不清，和树里已经存下的 `child->result` 保持一致。
  - AGENTS.md：在 AI contract 里写明 `eval` 只能依赖它的参数和 `Config`，因为 `TetrisEngine` 靠这一点做缓存。现有 ai_zzz 的 eval 都满足：没有 `mutable`，也不读 `lp.last`。
- [ ] 确定 eval 缓存的桶数：在 io_sim 典型对局里测命中率和内存后再定（`IO::Result` 含 `spawn_blocked` 和 `spin_slots` 两个按方块类型的数组）
- [ ] 验证 eval 缓存替换换位剪枝的效果
  - `tetris_engine_regression` 新增一项：从缓存取到的 `Result` 和直接 `eval` 的结果相同，覆盖同一格子、不同 spin class 的落点（amini）。
  - Debug 构建下 ctest 全部通过。
  - 强度：用 io_sim 配对种子、交换座位，和 HEAD 对比，预算用 `think_ms`（墙钟时间）。`nodes` 预算数的是 `get` 的调用次数，缓存省下的 eval 时间不会体现出来。19e143c 在同为 32768 个局面时测得换位剪枝胜率 0.698 [0.635, 0.761]，所以按 `nodes` 比只会看到取消去重的损失。同时报告每次搜索的 `scored()` 和缓存命中率。
- [ ] 对手节奏作为 `ai_zzz::IO` 的评估输入：`OpponentView` 加对手实际 pps（io_uds 用 `opponent.pieces_placed` 和 `frame` 估算，io_sim 用对手的 `Pace`），pending/cancel/opponent_surge 按倍速插值
  - 前提：io_cmaes 的 `games.csv` 按 ratio 分组显示单一权重顾此失彼（快对手组变好时等速组变差）。依据：`.research/io-cmaes-training-quality/plan.md` §4。
- [ ] io_sim 的击杀判定对手免费落子数与客户端 `auto_lethal_free_pieces` 对齐（对手平均节奏 + 12 帧回放刷新 + ping），目前用对手已排好的锁定帧，比实战多知道信息

## Done
- [x] 2026-10-25 改正 Map polarity：ecce526 把 `TetrisMap` 改为 0=occupied 存储后，`io_sim_test`、`ai_zzz_io_test`、`ai_zzz_io_spin_slots_test`、`engine_regression_test`、`amini_witness_test`、`amini_classification_test` 的 fixture 仍按 1=occupied 用 `set_row()` / `row()`，改为 `row_full ^ occupied` / `occupied_row()`；AGENTS.md 的说明同步改正
