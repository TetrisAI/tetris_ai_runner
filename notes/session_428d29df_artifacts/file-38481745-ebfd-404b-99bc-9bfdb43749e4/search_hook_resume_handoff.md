# Search Hook Commit 续接 (2026-05-28 第 2 轮)

## 已落地 (HEAD = a454c71)
- `20aedc2` (commit 1) Introduce SearchHook trait to replace EnableMini/EnableT and (T=='T') literals
- `096fba9` (commit 2-pre) Remove residual (T=='T') fallback from MoveGen NoHook path
- `508f07a` (commit 2-core) Drop EnableMini/EnableT bool template params from MoveGenSearch run_piece
- `a454c71` (commit 2-finish) Rename TSpinEnableHook to DefaultTSpinHook with backward-compat alias
  - hook 命名与 plan v3 对齐. `using TSpinEnableHook = DefaultTSpinHook` 保留, 测试 / 旧引用零改动.
  - **未推送**

## 现状: Hook 抽象骨架已贯穿全栈
当前框架 (MoveGen + MoveGenSearch) 内已无 `(T == 'T')` / `EnableMini` / `EnableT` 字面量与
bool 模板参数. spin 元数据是否计算与 piece 绑定关系完全由 `Hook::active_for_piece<T>` 编译期
决定. 替换 Hook 即可改变 piece 集合与开关.

## 残留 (commit 3+ 处理, 暂未做)

### 1. Hook 仍是"开关 trait", 算法实体仍在框架内
当前 `TSpinEnableHook` 只暴露 `active_for_piece<T>` 一个静态 bool. corners3 / last_rotate /
spin_block_ / emit_with_spin / target_blocked_mask / direction_open_mask 这些 T-spin 算法仍位于
`MoveGen` / `MoveGenSearch` 内部. 满足"框架对 piece 字面量零知识", 但未满足"框架对 spin
*算法* 零知识".

### 2. LandingPos 仍是 piece-agnostic 但携带 spin 字段
`LandingPos` 的 `spin / last_x/y/r / has_last_rot` 字段对 NoHook piece 全程未写
(NoHook fallback 只产 (x, y, r, spin=0)), 不影响行为, 但形状层面"框架结构体仍持有 spin 概念".

### 3. MoveGenSearch::NodeEx 写死 TetrisNodeWithTSpinType
`land_point_cache_` 仍是 `vector<TetrisNodeWithTSpinType>`, 不随 Hook 变化. 想做 ASpin / 自
定义 spin 必须换 cache 类型.

## Commit 3 任务清单 (DefaultTSpinHook 实体化)

### 目标
- 把 corners3 / last_rotate / spin_block_ / emit_with_spin 等算法搬入 `DefaultTSpinHook`.
- `LandingPos` 拆为 `LandingPos<Hook>`: 基础 (x/y/r) + `[[no_unique_address]] Hook::Payload extra`.
- `MoveGenSearch::NodeEx = Hook::LandPoint` 公开.
- `search_tspin::Search` 内部委托 `MoveGenSearch<Rule, DefaultTSpinHook>`.

### 关键文件
- `src/search_hook.h` - 增 `DefaultTSpinHook { Payload, RotState, MarkData, LandPoint, on_init/on_emit/on_rotate_reach }`.
- `src/tetris_movegen.h` - LandingPos 模板化, MoveGen 内 corners3/last_rotate/emit_with_spin 改调 hook 静态成员.
- `src/movegen_search.h` - 暴露 `using NodeEx = Hook::LandPoint`, 把 `spin_block_/spin_x_diff_/spin_y_diff_` 搬入 hook init.
- `tests/oracle_diff.cpp` / `tests/perft_movegen.cpp` - LandingPos 模板化会动 lambda 签名, 同步改.

### 验证
- `cmake --build build -j` 全 target 过.
- `./build/oracle_diff all | tail -2` 必须 `# all diffs ok`.

## Commit 4 任务清单 (DefaultASpinHook + search_aspin 委托)
- `src/search_hook.h` - 增 `DefaultASpinHook { active_for_piece = true (所有 piece), LandPoint = TetrisNodeWithASpinType, on_emit 做 4 方向阻挡 → ASpin/None }`.
- `src/search_aspin.cpp` - 内部委托 `MoveGenSearch<Rule, DefaultASpinHook>`.
- ASpin oracle baseline 生成 (在做 commit 3 之前先抽样跑一份原 search_aspin 输出做基准).

## Commit 5 (可选, 正交化收口) bridge_node_ex
- `tetris_core.h` `TetrisCallAI<TetrisAI, LandPoint>` 现路径 `node_ex(node)` 已自动从 LandPoint
  ctor 构造 `AI::TetrisNodeEx`, 实质上已经具备"AI 不关心的字段保留默认值"的语义.
  显式定义 `bridge_node_ex<Dst, Src>` trait 把这一行为正式暴露 (主要价值: 加 type-trait check
  防止未来 AI/Search 字段错位时静默退化), 不动现有 ctor.

## 续接姿势
新会话:
1. `cd tetris_ai_runner && git log --oneline -5` 应见 `508f07a` 在 HEAD.
2. `cmake --build build -j && ./build/oracle_diff all | tail -2` 确认 baseline.
3. 读 `research/flip-bits/search_hook_plan_v3.md` + 本文.
4. 优先做 commit 3 (DefaultTSpinHook 实体化), 完成后 oracle_diff 应仍 0 差异.
