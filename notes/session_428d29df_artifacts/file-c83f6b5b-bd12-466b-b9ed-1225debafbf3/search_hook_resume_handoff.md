# Search Hook Commit 续接 (2026-05-28)

## 已落地
- `20aedc2` (commit 1) Introduce SearchHook trait to replace EnableMini/EnableT and (T=='T') literals
  - 新文件: `src/search_hook.h` 含 `NoHook` / `TSpinEnableHook`
  - `MoveGen<Spec, T, EnableMini>` → `MoveGen<Spec, T, Hook=NoHook>`
  - `MoveGenSearch<Rule>` → `MoveGenSearch<Rule, Hook=TSpinEnableHook>` (默认行为完全等价 master)
  - `kCheckTSpin = Hook::template active_for_piece<T>`
  - dispatch 包装内 `(T == 'T')` → `Hook::active_for_piece<T>`
  - oracle_diff all 全过 (`# all diffs ok`)
  - **未推送**

## 残留 (要在 commit 2 处理)
- `tetris_movegen.h:399` 还有一处 `if constexpr (T == 'T')`, 在 emit fallback 路径里给非 hook 路径写弱 spin (corners≥3→Full). commit 2 搬迁 LandingPos / emit 时一并清理.
- `run_piece<T, bool EnableMini>` 与 `run_piece_20g<T, bool EnableT>` 模板形参仍是 bool, commit 2 改成 Hook 类型形参.

## Commit 2 任务清单 (DefaultTSpinHook 实体化)

### 目标
- `(T == 'T')` 字面量在所有 src/*.h /*.cpp 中归零.
- 把 corners3 / last_rotate / spin_block_ / emit_with_spin / target_blocked_mask / direction_open_mask 全部搬入 `DefaultTSpinHook`.
- `LandingPos` 拆为 `LandingPos<Hook>`: 基础 (x/y/r) + `[[no_unique_address]] Hook::Payload extra`.
- `MoveGenSearch::HookType` / `MoveGenSearch::NodeEx = Hook::LandPoint` 公开.
- `search_tspin::Search` 内部委托 `MoveGenSearch<Rule, DefaultTSpinHook>` (类签名/AI typedef 不动).
- `MoveGenSearch::init` 中的 spin_block_/spin_x_diff_/spin_y_diff_ 搬入 DefaultTSpinHook.

### 关键文件
- `src/search_hook.h` - 增 DefaultTSpinHook (基本就是 TSpinEnableHook + Payload/RotState/MarkData/LandPoint + on_init/on_emit/on_rotate_reach 静态成员函数)
- `src/tetris_movegen.h` - LandingPos 模板化, MoveGen 内 corners3/last_rotate/emit_with_spin 改调 hook 静态成员 (或保留实现, 让 hook 注入策略)
- `src/movegen_search.h` - 暴露 HookType / NodeEx, run_piece 形参改 Hook
- `src/search_tspin.cpp` - 委托方案待定: 直接构造一个 `MoveGenSearch<Rule, DefaultTSpinHook>` 持有 + 转发, 或保留旧实现 (oracle baseline)

### 验证
- 编译: `cmake --build build -j` 全 target 过
- `./build/oracle_diff all | tail -2` 必须 `# all diffs ok`

### 风险
- LandingPos 模板化会动 perft_movegen / oracle_diff 的 lambda 签名, 同步改测试
- spin_block_ 计算依赖 `RuleSpec::spawn('T', kW, kH)` + `state_node_lut_`, 搬到 hook 后 init 时机要协调 (hook 持状态 vs hook 无状态走 RotState)

### 建议拆分
若 commit 2 一次太大, 可再细分:
- 2a: LandingPos 模板化 + Hook::Payload/MarkData 类型骨架 (行为等价)
- 2b: corners3/last_rotate/emit_with_spin 真正搬入 hook 静态函数
- 2c: spin_block_ + 20g 路径 search_t emit 搬入 hook
- 2d: search_tspin::Search 委托
但用户已选定"4 commit 总数", 所以 commit 2 实质是这 4 步合并, 注意工作量.

## 续接姿势
新会话:
1. `cd tetris_ai_runner && git log --oneline -5` 应见 `20aedc2` 在 HEAD
2. `cmake --build build -j && ./build/oracle_diff all | tail -2` 确认 baseline
3. 读 `research/flip-bits/search_hook_plan_v3.md`
4. 按上面任务清单逐步推进
