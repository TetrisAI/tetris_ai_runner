# C1 收尾杂项

## 实际 commit
- hash: `b29f30d`
- title: `refactor(hook): drop TSpinHook SearchState scaffolding`
- 父节点: `cd10d5b` (tag 位板化)

## 验收
- `cmake --build build -j` 干净 (Release LTO).
- `./build/oracle_diff`: # all diffs ok (TagStrategy + PathStrategy + SimulateStrategy, 7 piece × 1g/20g).
- `./build/extreme_rule_diff`: # all extreme-rule diffs ok.

## 方案外发现 / 副作用

1. **`src/movegen_hook.h` 文件权限变化**: commit 顺带把 mode 从 `100755` 降到
   `100644`. 这个文件本来不该是可执行权限, 这次修复掉了; 但记录下来以防 review
   反向 nitpick.

2. **`examples/dummy_strategy.h`** 也在 SearchState 删除范围内. 它不是 strategy
   主链 (只是文档/编译保护), 但 `dummy_strategy_compile_check` target 直接 fail
   过一次. 已同步删 `SearchState using` / `hook_state_` / `on_search_state_init`
   调用. 行为对齐其它 strategy.

3. **`compute_mini_blocked_arr`**: TSpinHook 内新增的方法. 公式与 `target_blocked_mask`
   (private 段, on_emit 内部走 1g 用) 共用同一份 `direction_open_mask` 取 OR
   后取反. 这里展开到 per-r per-cell 一次性预算, 让 20g 路径的 emit 端
   `apply_emit_20g` 只做 `rs.blocked_arr[r].get(xb, yb)` 一次取位.

   非 T 形 (EnableT=false) hook 的 `compute_mini_blocked_arr` 全部退化成 noop,
   `RotState::blocked_arr` 退化成 0 长度 / 空 struct, 0 字节占位; 由
   `if constexpr (EnableT)` 守门.

4. **ASpinHook::compute_mini_blocked_arr** 也加了空实现 — 不是因为 ASpin
   消费 blocked_arr (它不消费), 而是 strategy 在 `active_for_piece=true` 路径
   上想统一调用而不必 if constexpr 拆 ASpin / TSpin. 注释里写明了.

5. **`search_simulate.h` init() 不再调用 on_search_state_init** — 之前的
   init 体一直在调, 但 SimulateStrategy 配的 hook 是 NoSpinHook,
   on_search_state_init 是空体, 所以行为零变化. 一并清掉.

6. **N.5 收尾后, BaseSpinHook CRTP / 默认实现裁剪 (n1_n4_pending.md N.4)**
   现在已经名存实亡: 公共接口面只剩 `apply_emit_1g` / `apply_emit_20g` /
   `on_init_rotations` / `compute_mini_blocked_arr` / `on_emit` 5 个真接口
   + `Payload` / `LandPoint` / `Config` / `RotState` 4 个类型族 + 一组
   payload trait (`resolves_last_1g`, `payload_has_last_rot` 等).
   是否引入 BaseSpinHook 看 C2 (Payload 收敛 + ASpin 短路) 之后再判断, 这次
   commit 不动.

## 不在本 commit 内 / 留给 C2

- `BaseSpinHook` CRTP 默认实现裁剪.
- Payload 收敛 (SpinTypePayload + EmptyPayload).
- `lp_requires_last_rotate` 让 ASpin 路径吃 1g 短路.

## 不在本 commit 内 / 留给 C3

- LandPoint 收敛 (PlainLandPoint for non-spin 路径).
