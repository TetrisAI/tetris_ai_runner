# Commit 4 设计备忘 (DefaultTSpinHook 算法实体化)

## 目标
把以下 T-spin 专属算法/数据从 MoveGen / MoveGenSearch 搬入
`m_tetris::DefaultTSpinHook`, 让框架文件 (`tetris_movegen.h`,
`movegen_search.h`) 不再持有这些字符串的"算法实现":

1. `corners3` (compute_corners3 / build_corners3 / corner_bb_ct)
2. `last_rotate_arr` 的累积 + `find_last_rotate_pred` 反查
3. `spin_block_` / `spin_x_diff_` / `spin_y_diff_` (检测 ready 用列模板)
4. `emit_with_spin` / `emit_set` 中 spin 相关逻辑
5. `target_blocked_mask` / `direction_open_mask` (Mini 判定 4 方向阻挡)

## Hook 接口面 (本 commit 落地)

### 1. 嵌套类型 — 由框架持有, 类型本身定义在 hook 内
```cpp
struct DefaultTSpinHook {
    using Payload  = TSpinPayload;          // 已有
    using LandPoint = TetrisNodeWithTSpinType; // 真正完成
    template<char T> static constexpr bool active_for_piece = (T == 'T');

    // BFS 期间维护的 per-rotation 状态. MoveGen 把它当成黑盒数据,
    // 仅在调 hook 静态接口时传入引用. 对 NoHook 是 std::monostate, 0 字节.
    template<class MapT, std::size_t R_count>
    struct RotState {
        std::array<MapT, R_count> last_rotate_arr{};
        std::array<MapT, R_count> corners3_arr{};
    };

    // MoveGenSearch::init() 一次性建表用的 search-side state, 持
    // spin_x_diff/y_diff/block 列模板. NoHook 同样退化为 monostate.
    struct SearchState {
        int x_diff = 0;
        int y_diff = 0;
        std::uint32_t block_buffer[52]{};
        std::uint32_t* block = nullptr;
        void init(TetrisContext const*); // 移自 search_tspin.cpp / MoveGenSearch::init
    };
};
```

### 2. 静态算法接口
- `template<class Spec, char T, class MapT, std::size_t R_count>
   static void on_init_rotations(MapT const& board, RotState<MapT, R_count>& rs)`
  搬入 compute_corners3 / build_corners3 / corner_bb_ct.

- `template<class MapT, std::size_t R_count>
   static void on_rotate_reach(std::uint8_t dst_r, MapT const& reached,
                               RotState<MapT, R_count>& rs)`
  把 `last_rotate_arr[dst_r] |= reached` 包装. MoveGen apply_kicks 内只看到这一句.

- `template<class Spec, char T, class MapT, std::size_t R_count, class LP,
            class OriginArr, class Fn>
   static void on_emit_set(MapT const& landings, int r,
                           RotState<MapT, R_count> const& rs,
                           std::array<MapT, R_count> const& search,
                           std::array<MapT, R_count> const& usable_arr,
                           OriginArr const& origin_xs, OriginArr const& origin_ys,
                           Fn&& fn)`
  搬入 emit_with_spin / target_blocked_mask / direction_open_mask /
  find_last_rotate_pred / emit_set. 入口仅一个, MoveGen 不再认 spin 字段.

- `static bool check_ready(TetrisMap const& map, TetrisNode const* node,
                           SearchState const& st)` 搬自 MoveGenSearch 私有
  `check_ready_native`.
- `static bool check_mini_ready(TetrisMap const& map,
                                TetrisNodeWithTSpinType const& node)` 同上.

### 3. NoHook 对偶
全部接口给空实现 / monostate, 类型成员保持. 用 `if constexpr` 守护已成体系
(`Hook::active_for_piece<T>`), 这里只需补足"NoHook 路径下 SearchState 也是空".

## 框架侧改动

### `tetris_movegen.h`
- 删除以下 private 静态成员 (字面量算法搬走):
  - `corner_filled` (只服务 corners3, 移走) / `corner_bb_ct` / `corner_bb`
  - `compute_corners3` / `build_corners3`
  - `find_last_rotate_pred` / `find_last_rotate_pred_impl` / `try_kick_pred`
  - `target_blocked_mask` / `target_blocked_mask_impl` / `direction_open_mask`
  - `emit_with_spin` / `emit_set`
- `generate()` 内部:
  - `last_rotate_arr / corners3_arr` 局部数组改成
    `typename Hook::template RotState<map_t, R_count> rot_state{};`
    (NoHook = monostate, 0 字节, `if constexpr` 守护.)
  - `compute_corners3 -> Hook::template on_init_rotations<Spec, T, map_t, R_count>(board, rot_state)`.
  - 输出循环里, `kCheckTSpin` 分支直接调 `Hook::template on_emit_set<...>(landings, r, rot_state, search, usable_arr, kOriginXs, kOriginYs, fn)`.
  - NoHook 分支保持现有 lambda for_each_set_bit (与 commit 3 状态一致).
- `apply_kicks` 内 `last_rotate_arr[DstR] |= rotate_reached;`
  替换为 `Hook::on_rotate_reach(DstR, rotate_reached, rot_state)` (rot_state 通过
  generate -> expand_rotations 链路一路传下去).

### `movegen_search.h`
- 私有字段 `spin_x_diff_/spin_y_diff_/spin_block_buffer_/spin_block_` 收敛到
  `[[no_unique_address]] typename Hook::SearchState hook_state_;`
- `init()` 中 spin_block 重建逻辑替换为 `hook_state_.init(context_)`.
- `check_ready_native / check_mini_ready_native` 改为 `Hook::check_ready / Hook::check_mini_ready`,
  把 `hook_state_` 传过去.
- 仅在 `if constexpr (Hook::template active_for_piece<'T'>)` 守护下做这些 init 与
  emit, NoHook 路径完全跳过.

## 风险点
- `RotState` / `SearchState` 在 NoHook 下用 `std::monostate` 对外, MoveGen 内部
  `if constexpr` 守护实例化路径. 关键点: NoHook 走 NoHook 路径, 不会触达
  on_emit_set, 所以 NoHook 的 SearchState/RotState 无需提供算法实现.
- `LandPoint = TetrisNodeWithTSpinType` 第一次真正落到 hook 上, 需要 `search_hook.h`
  include `search_tspin.h` (或 forward declare). 已在 commit 3 留了 include 锚点.
- AI 端 (ai_zzz / ai_misaka / ai_tag / pso / ppt_pso / botris) typedef 不动, hook
  内嵌 LandPoint 与 search_tspin::Search::TetrisNodeWithTSpinType 同型, AI ctor 直通.
- spin_block_/x_diff_/y_diff_ 在 init() 阶段 RuleSpec::spawn 取 'T' spawn — NoHook
  路径下不需要这些字段, SearchState 干脆是 monostate, init 不调用.

## 验证
- cmake --build build -j: 0 error 0 warning.
- ./build/oracle_diff: `# all diffs ok`.
- 不要 push, 单 commit 落地.
