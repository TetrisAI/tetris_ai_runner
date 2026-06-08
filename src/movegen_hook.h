#ifndef TETRIS_AI_RUNNER_MOVEGEN_HOOK_H_
#define TETRIS_AI_RUNNER_MOVEGEN_HOOK_H_

#include "integer_utils.h"
#include "search_aspin.h"
#include "search_tspin.h"
#include "tetris_core.h"
#include "tetris_shape.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <utility>

//==========================================================================
// Movegen Hook 抽象 (原 search_hook.h, 改名为 movegen_hook.h —— 与 movegen_search.h
// 同前缀, 不再以 "search_" 前缀混淆 search 实现).
//
// 目标: 让框架层 (MoveGen / MoveGenSearch) 完全不持有 spin 专属算法字符串. 五类
// 原本散落在 MoveGen / MoveGenSearch 私有静态成员里的 T-spin 专属算法搬入
// TSpinHook (定义在本文件):
//
//   1. corners3 (≥3 corner 占用位图) — compute_corners3 / build_corners3 /
//      corner_bb_ct
//   2. last_rotate_arr 维护 + find_last_rotate_pred 反查链
//   3. blocked_arr (mini-ready 4 邻 OR 取反位图) — compute_mini_blocked_arr /
//      build_mini_blocked, 替代旧 master 坐标 SearchState (block_data + x_diff /
//      y_diff) + node->rotate_*->check(map) 的 oracle 路径
//   4. emit_with_spin / emit_set — 落点 emit 时把 spin 字段灌入 Payload
//   5. target_blocked_mask / direction_open_mask — Mini 判定 4 方向阻挡
//
// 整套"算法实体" 通过 hook 接口对外提供, MoveGen / MoveGenSearch 只
// 调用接口, 不再认识它们内部的字段名:
//
//   * Hook::RotState<MapT, R>  : BFS 期间 per-rotation 的 spin 元数据, 由
//                                 MoveGen::generate() 持有并传给 apply_kicks /
//                                 emit. NoHook = std::monostate, 0 字节.
//   * Hook::Payload             : LandingPos<Hook>::extra 嵌入的 emit 时元数据
//                                 (spin / last_*). NoHook 为空 struct.
//   * Hook::LandPoint           : MoveGenSearch::search() 输出元素 (本 commit
//                                 起真正映射到 ::search_tspin::Search::TetrisNode-
//                                 WithTSpinType, 不再用 LandPointFwd 占位).
//   * Hook::active_for_piece<T> : 编译期 bool, 决定该 piece 是否进入 spin 路径.
//==========================================================================

namespace m_tetris2
{
    //=========================================================================
    // Payload 收敛 (commit C2 N.1):
    //   * 旧形态 3 种 (NoTSpinPayload / TSpinPayload / ASpinPayload) 合并到 2 种.
    //   * EmptyPayload   : NoHook / NoSpinHook / CautiousHook 共用. 0 字节,
    //     与 [[no_unique_address]] 配合让 LandingPosT<Hook>::extra 不付字节.
    //   * SpinTypePayload: TSpinHook / ASpinHook 共用. 字段排布与旧 TSpinPayload
    //     byte-byte 等价 (rename spin -> type). type 编码:
    //         0 = None
    //         1 = TSpinMini
    //         2 = TSpinFull
    //         3 = ASpin
    //     其余 4 字段 (last_x/last_y/last_r/has_last_rot) 仅 TSpinHook 实写,
    //     ASpinHook 路径下保持 0. 与旧 TSpinPayload 同布局, oracle_diff
    //     baseline (跑 TSpinHook) byte-equal. 旧 ASpinPayload 仅 1 字节
    //     (aspin 字段), 合并后 ASpin 路径 LandingPos.extra 增至 5 字节, 但
    //     ASpin 测试 (aspin_dump 等) 走的是 LandPoint 公开接口 (.type 字段),
    //     与 Payload 内存布局无关, 不破坏测试.
    //=========================================================================
    struct EmptyPayload
    {
    };

    struct SpinTypePayload
    {
        std::uint8_t type;
        std::int8_t last_x;
        std::int8_t last_y;
        std::uint8_t last_r;
        std::uint8_t has_last_rot;
    };

    static_assert(sizeof(SpinTypePayload) == 5,
                  "SpinTypePayload byte layout drift breaks oracle_diff baseline");
    static_assert(offsetof(SpinTypePayload, type) == 0,
                  "SpinTypePayload.type must occupy old TSpinPayload.spin slot");
    static_assert(offsetof(SpinTypePayload, last_x) == 1,
                  "SpinTypePayload.last_x must occupy old TSpinPayload.last_x slot");
    static_assert(offsetof(SpinTypePayload, last_y) == 2,
                  "SpinTypePayload.last_y must occupy old TSpinPayload.last_y slot");
    static_assert(offsetof(SpinTypePayload, last_r) == 3,
                  "SpinTypePayload.last_r must occupy old TSpinPayload.last_r slot");
    static_assert(offsetof(SpinTypePayload, has_last_rot) == 4,
                  "SpinTypePayload.has_last_rot must occupy old TSpinPayload.has_last_rot slot");

    //=========================================================================
    // ActivePiecePolicy — TetrisEngine2 框架用于向 Strategy 传递"哪些 piece
    // 需要激活 spin 路径"的编译期 tag.
    //
    //   ActiveAllPolicy:   全件激活 (ASpin 默认行为)
    //   ActiveNonePolicy:  全件关闭 (AI 不需要 spin / 类型不匹配退化)
    //   ActiveTOnlyPolicy: 仅 T 件激活 (TSpin 默认行为)
    //
    // Strategy 消费时通过 SFINAE 探针判断 Hook::active_for_piece<T, Policy>
    // 是否存在, 存在则使用双参版本, 否则 fallback 到单参 Hook 历史行为.
    // 自定义 Hook 无需声明 Policy 参数即可继续工作.
    //=========================================================================
    struct ActiveAllPolicy
    {
        template<char T>
        static constexpr bool value = true;
    };

    struct ActiveNonePolicy
    {
        template<char T>
        static constexpr bool value = false;
    };

    struct ActiveTOnlyPolicy
    {
        template<char T>
        static constexpr bool value = (T == 'T');
    };

    //=========================================================================
    // BBLandPoint — NoSpinHook / CautiousHook 的无指针 LandPoint.
    //
    //  持有 bb::BBState{t,r,xb,yb}，位板框架接口层完全不持有 TetrisNode 指针。
    //
    //  "无效/nullptr" 哨兵约定：state.t == 0（游戏中合法方块 type 不为 0）。
    //=========================================================================
    struct BBLandPoint
    {
        bb::BBState state{}; // 落点位板坐标（t/r/xb/yb）

        BBLandPoint() = default;
        explicit BBLandPoint(bb::BBState s) : state(s) {}

        // 从任意 spin-aware LandPoint（TetrisNodeWithTSpinType /
        // TetrisNodeWithASpinType 等）退化构造：只保留位置信息，丢弃 spin。
        template<class LP,
                 class = std::enable_if_t <
                             !std::is_same_v<std::decay_t<LP>, BBLandPoint> &&
                         requires(LP const &x)
        {
            x.state;
        }
        >>
            BBLandPoint(LP const &other) : state(other.state) {}

        bool operator==(BBLandPoint const &o) const
        {
            return state.t == o.state.t && state.r == o.state.r &&
                   state.xb == o.state.xb && state.yb == o.state.yb;
        }
        bool operator==(std::nullptr_t) const
        {
            return state.t == 0;
        }
        bool operator!=(std::nullptr_t) const
        {
            return state.t != 0;
        }
    };

    //=========================================================================
    // BaseSpinHook<Derived> — CRTP 基类 (commit C2 N.4).
    //
    // 把 "NoHook / NoSpinHook / CautiousHook 实质为 noop" 的接口集中到一处, 派
    // 生侧只覆盖真正非 noop 的方法. TSpinHook / ASpinHook **不**派生 — 它们
    // 写自家全套接口 (last-rotate / corners3 / payload-folded last 反查 / 4 邻
    // 阻挡), 与默认 noop 语义无重叠.
    //
    // 默认值表 (与 hook_interface_matrix.md 对齐):
    //
    //   * resolves_last_1g     -> false
    //   * lp_requires_last_rotate(cfg, lp) -> false  (改名后语义反转, 见下文)
    //   * get_last_state(lp)   -> nullptr
    //   * config_*(cfg) ×7      -> false
    //   * on_init_rotations / on_rotate_reach / on_emit / apply_emit_1g /
    //     apply_emit_20g / compute_mini_blocked_arr -> noop (空体)
    //
    // 派生侧仍需自定义类型族: LandPoint / Config / Payload / RotState +
    // active_for_piece<T>. 这五项与算法直接耦合, 不放默认值.
    //
    // CRTP 形式: `struct NoHook : BaseSpinHook<NoHook>{...};`. Derived 在基类
    // 内部不被消费, 仅让派生侧的同名重写优雅 shadow 掉默认实现.
    //=========================================================================
    template<class Derived>
    struct BaseSpinHook
    {
        //=== 编译期开关 ===
        static constexpr bool resolves_last_1g = false;

        //=== LandPoint trait ===
        template<class LP>
        static bb::BBState const *get_last_state(LP const &) noexcept
        {
            return nullptr;
        }

        //=== last-rotate 短路谓词 (commit C2 N.4 改名 + 反转语义).
        //  旧 is_landpoint_none(lp): "lp.type 为 None, 不要求 path 末段是旋转,
        //    可以享受 起点==终点 的自落点短路".
        //  新 lp_requires_last_rotate(cfg, lp): true = "lp 要求 path 字符串
        //    以旋转 (z/c/x) 结尾, 不能享受自落点短路".
        //  默认 false: NoHook / NoSpinHook / CautiousHook / ASpinHook 路径下
        //  起点==终点 的"原地落"直接 return 空 path. 仅 TSpinHook 在
        //  cfg->last_rotate && lp.type != None 时返回 true.
        template<class Cfg, class LP>
        static bool lp_requires_last_rotate(Cfg const *, LP const &) noexcept
        {
            return false;
        }

        //=== Config trait (7 个) — 默认全 false ===
        //  非 spin 派生 (NoHook / NoSpinHook / CautiousHook) 的 Config 是空
        //  struct 或仅含一个无关字段 (Cautious 的 fast_move_down), 这些路径
        //  下的 strategy 真正消费的 config_* 只有 1~2 项 (path 端 config_allow_d
        //  / 公共 config_is_20g), 其余项默认 false 即可保持历史行为. 派生侧
        //  按需覆盖个别项 (CautiousHook 覆盖 config_allow_d).
        template<class Cfg>
        static bool config_last_rotate(Cfg const *) noexcept
        {
            return false;
        }
        template<class Cfg>
        static bool config_allow_180(Cfg const *) noexcept
        {
            return false;
        }
        template<class Cfg>
        static bool config_allow_LR(Cfg const *) noexcept
        {
            return false;
        }
        template<class Cfg>
        static bool config_allow_d(Cfg const *) noexcept
        {
            return false;
        }
        template<class Cfg>
        static bool config_allow_D(Cfg const *) noexcept
        {
            return false;
        }
        template<class Cfg>
        static bool config_allow_rotate_move(Cfg const *) noexcept
        {
            return false;
        }
        template<class Cfg>
        static bool config_is_20g(Cfg const *) noexcept
        {
            return false;
        }

        //=== BFS 钩子默认 (空体). 由派生侧的 active_for_piece=false 在 strategy
        //  端 if constexpr 守护下不会被实际调用 — 这里只是契约自洽, 防止派生
        //  类把 BaseSpinHook 当 generic hook 使用时找不到符号. ===
        template<class Spec, char T, class MapT, std::size_t R_count, class RS>
        static void on_init_rotations(MapT const &, RS &) noexcept
        {
        }

        template<class MapT, std::size_t R_count, class RS>
        static void on_rotate_reach(std::uint8_t, MapT const &, RS &) noexcept
        {
        }

        template<class Spec, char T, class LP, class MapT, std::size_t R_count,
                 class OriginArr, class Fn, class RS>
        static void on_emit(MapT const &, int, RS const &,
                            std::array<MapT, R_count> const &,
                            std::array<MapT, R_count> const &,
                            OriginArr const &, OriginArr const &, Fn &&) noexcept
        {
        }

        template<class Spec, char T, class MapT, std::size_t R_count, class RS>
        static void compute_mini_blocked_arr(std::array<MapT, R_count> const &, RS &) noexcept
        {
        }

        template<class MapT, std::size_t R_count, class LP, class Payload, class Cfg>
        static void apply_emit_1g(LP &, Payload const &, std::size_t, Cfg const *,
                                  bb::BBState const *,
                                  std::array<MapT, R_count> const &,
                                  std::uint8_t, int, int) noexcept
        {
        }

        template<class MapT, std::size_t R_count, class LP, class Cfg, class RS>
        static void apply_emit_20g(LP &, bb::BBState const *, char, std::size_t,
                                   Cfg const *, RS const &,
                                   std::array<MapT, R_count> const &,
                                   std::uint8_t, int, int) noexcept
        {
        }
    };

    //不做任何额外计算的 hook. 任何 piece 的 active_for_piece 均为 false,
    //  框架在 if constexpr 守护下裁掉所有 spin/last_rotate/corners 维护逻辑,
    //  与原 EnableMini=false / EnableT=false 路径等价.
    //
    //  派生自 BaseSpinHook<NoHook> 拿到全套默认 noop 接口, 自身只声明类型族.
    struct NoHook : BaseSpinHook<NoHook>
    {
        struct EmptyRotState
        {
        };

        using Payload = EmptyPayload;
        //LandPoint: MoveGenSearch::search() 输出的元素类型. NoHook 路径下
        //  没有 spin / last 等元数据，与 NoSpinHook 共用 BBLandPoint，
        //  彻底移除 TetrisNode 指针依赖.
        using LandPoint = BBLandPoint;
        //Config: NoHook 路径不持任何配置, 退化为空 struct.
        struct Config
        {
        };
        template<class MapT, std::size_t R_count>
        using RotState = EmptyRotState;

        template<char T, class Policy = void>
        static constexpr bool active_for_piece = false;
    };

    //NoSpinHook: 与 NoHook 同形 — 不计算任何 spin / last_rotate 元数据
    //  (active_for_piece 全 false), BFS 钩子 (on_init_rotations / on_rotate_reach /
    //  on_emit) + LandPoint trait (lp_requires_last_rotate / get_last_state) +
    //  apply_emit_* 由 BaseSpinHook<NoSpinHook> 默认 noop 提供.
    //
    //  与 NoHook 的关键差异: LandPoint 是 BBLandPoint (持有 BBState，无
    //  TetrisNode 指针). NoHook 仅在 search_tspin 内部使用，其 LandPoint 带
    //  spin/last 元数据；NoSpinHook LandPoint 只保留位置信息。
    //
    //  Config trait: NoSpinHook 走的 path / simple / simulate strategies 在
    //  active_for_piece=false 守护下编译期裁掉 spin 算法分支, 但 path 1g/20g
    //  邻居枚举依然消费 config_allow_180 / config_allow_LR / config_allow_d /
    //  config_allow_D / config_allow_rotate_move 决定 BFS 邻居展开的字符集.
    //  历史 NoSpinHook 这 5 项全返回 true ("无配置, 全开关"), 与 BaseSpinHook
    //  默认全 false 不同 — 因此这 5 项必须**显式覆盖**, 不能掉到默认.
    //  config_last_rotate / config_is_20g 默认 false 与历史一致, 继承 base 即可.
    //
    //  目标消费者: search_path_node::PathHook / search_simulate_node::SimulateHook
    //  在各自 header 内 `using` 别名, 物理上落在共用头文件这一份.
    struct NoSpinHook : BaseSpinHook<NoSpinHook>
    {
        struct EmptyRotState
        {
        };

        using Payload = EmptyPayload;
        using LandPoint = BBLandPoint;
        struct Config
        {
        };

        template<class MapT, std::size_t R_count>
        using RotState = EmptyRotState;

        template<char T, class Policy = void>
        static constexpr bool active_for_piece = false;

        //=== Config trait: 历史 "无配置, 全开关" 行为, 必须显式覆盖 base 默认 ===
        static bool config_allow_180(Config const *) noexcept
        {
            return true;
        }
        static bool config_allow_LR(Config const *) noexcept
        {
            return true;
        }
        static bool config_allow_d(Config const *) noexcept
        {
            return true;
        }
        static bool config_allow_D(Config const *) noexcept
        {
            return true;
        }
        static bool config_allow_rotate_move(Config const *) noexcept
        {
            return true;
        }
    };

    //T-spin Hook: 与原 `kEnable = (T == 'T')` 字面量行为完全等价 — 仅 'T' 启用.
    //  此 hook 是 search_tspin / MoveGenSearch 默认装配, AI 端 typedef 不需要改;
    //  oracle_diff baseline 与 master 严格对齐.
    //
    //  框架内的 corners3 / last_rotate / spin_block / emit_with_spin /
    //  target_blocked_mask 等算法全部搬入本 hook 的静态成员函数.
    //
    //  TSpinHook **不**派生 BaseSpinHook — 它写自家完整接口面 (last-rotate /
    //  corners3 / payload-folded last 节点反查), 与默认 noop 语义无重叠.
    struct TSpinHook
    {
        //RotState: BFS 期间维护的 per-rotation 元数据.
        //  - last_rotate_arr[r]: 经"最后一步是旋转 (含 0-kick)"到达的 cell 集合,
        //    在 apply_kicks 内通过 on_rotate_reach 累积; emit 时与 corners3 相交
        //    判定 ready.
        //  - corners3_arr[r]: 在 init 阶段一次性算好的"≥3 corner 占用 (含越界)"
        //    位图. 见 master search_tspin::Search::check_ready.
        //  - blocked_arr[r]: 4 邻 (cw/ccw/opp) 旋转是否全部不可放置的 per-cell
        //    位图. 由 strategy 在 20g 路径起点调 compute_mini_blocked_arr 填入,
        //    供 apply_emit_20g 计算 is_mini_ready (与 master check_mini_ready 同义).
        //    1g 路径不消费此字段 (on_emit 内部走 target_blocked_mask 直接算位运算).
        template<class MapT, std::size_t R_count>
        struct RotState
        {
            std::array<MapT, R_count> last_rotate_arr{};
            std::array<MapT, R_count> corners3_arr{};
            std::array<MapT, R_count> blocked_arr{};
        };

        using Payload = SpinTypePayload;
        using LandPoint = ::search_tspin::Search::TetrisNodeWithTSpinType;
        using Config = ::search_tspin::Search::Config;

        template<char T, class Policy = void>
        static constexpr bool active_for_piece = []() constexpr
        {
            if constexpr (std::is_void_v<Policy>)
                return T == 'T';
            else
                return (T == 'T') && Policy::template value<T>;
        }();

        //=== LandPoint / Config 元数据 trait (MoveGenSearch 走 hook 间接消费) ===
        //=== last-rotate 短路谓词 (commit C2 N.4 改名 + 反转语义).
        //  TSpinHook 是唯一返回 true 的 hook: 当 cfg 启用 last_rotate 且 lp.type
        //  非 None 时, path 字符串必须以旋转 (z/c/x) 结尾, 不能享受 起点==终点
        //  的自落点短路. 旧 is_landpoint_none(lp)=lp.type==None 的语义被反转
        //  到这里, 同时把 cfg->last_rotate 检查内联进来 (strategy 端不再单独
        //  查 config_last_rotate 来判断短路).
        static bool lp_requires_last_rotate(Config const *cfg, LandPoint const &lp) noexcept
        {
            return cfg && cfg->last_rotate && lp.type != ::search_tspin::Search::None;
        }
        static bb::BBState const *get_last_state(LandPoint const &lp) noexcept
        {
            return lp.last.t != 0 ? &lp.last : nullptr;
        }
        static bool config_last_rotate(Config const *cfg) noexcept
        {
            return cfg && cfg->last_rotate;
        }
        static bool config_allow_180(Config const *cfg) noexcept
        {
            return cfg && cfg->allow_180;
        }
        static bool config_allow_LR(Config const *cfg) noexcept
        {
            return cfg && cfg->allow_LR;
        }
        static bool config_allow_d(Config const *cfg) noexcept
        {
            return cfg && cfg->allow_d;
        }
        static bool config_allow_D(Config const *cfg) noexcept
        {
            return cfg && cfg->allow_D;
        }
        static bool config_allow_rotate_move(Config const *cfg) noexcept
        {
            return cfg && cfg->allow_rotate_move;
        }
        static bool config_is_20g(Config const *cfg) noexcept
        {
            return cfg && cfg->is_20g;
        }

        //=== 1. corners3 ===
        template<class Spec, char T, class MapT, std::size_t R_count>
        static void on_init_rotations(MapT const &board, RotState<MapT, R_count> &rs)
        {
            [&]<std::size_t... Rs>(std::index_sequence<Rs...>)
            {
                (build_corners3<Spec, T, MapT, R_count, static_cast<std::uint8_t>(Rs)>(board, rs), ...);
            }(std::make_index_sequence<R_count>{});
        }

        //=== 2. last_rotate_arr 累积 ===
        template<class MapT, std::size_t R_count>
        static void on_rotate_reach(std::uint8_t dst_r, MapT const &reached,
                                    RotState<MapT, R_count> &rs) noexcept
        {
            rs.last_rotate_arr[dst_r] |= reached;
        }

        //=== 4. emit ===
        template<class Spec, char T, class LP, class MapT, std::size_t R_count,
                 class OriginArr, class Fn>
        static void on_emit(MapT const &landings, int r, RotState<MapT, R_count> const &rs,
                            std::array<MapT, R_count> const &search,
                            std::array<MapT, R_count> const &usable_arr,
                            OriginArr const &origin_xs, OriginArr const &origin_ys, Fn &&fn)
        {
            MapT ready = landings & rs.last_rotate_arr[r] & rs.corners3_arr[r];
            MapT blocked_all = MapT(MapT::all_mask());
            target_blocked_mask<Spec, T, MapT, R_count>(r, usable_arr, blocked_all);
            MapT mini_set = ready & blocked_all;
            MapT full_set = ready & ~mini_set;
            MapT none_set = landings & ~ready;
            emit_set<Spec, T, LP, MapT, R_count, OriginArr>(full_set, r, 2, search, true,
                                                            origin_xs, origin_ys, fn);
            emit_set<Spec, T, LP, MapT, R_count, OriginArr>(mini_set, r, 1, search, true,
                                                            origin_xs, origin_ys, fn);
            emit_set<Spec, T, LP, MapT, R_count, OriginArr>(none_set, r, 0, search, false,
                                                            origin_xs, origin_ys, fn);
        }

        //=== 3. compute_mini_blocked_arr ===
        //  4 邻 (cw/ccw/opp) 旋转是否全部不可放置的 per-cell 位图. 与 master
        //  Search::check_mini_ready 同义 (master 是逐 (节点, 方向) 跑 ->check(map),
        //  位板版借用 target_blocked_mask 已有的 direction_open_mask 做按方向 OR
        //  再取反). 由 20g 路径在 BFS 入口调一次, 填入 RotState::blocked_arr,
        //  apply_emit_20g 直接读 (r, xb, yb) 位.
        //
        //  注意与 on_emit 内 target_blocked_mask 共用同一份"按方向 OR 后取反"
        //  公式 (见 private 段); 这里展开成逐 r 对外暴露, 让 strategy 不必自己
        //  reflect into private helper.
        template<class Spec, char T, class MapT, std::size_t R_count>
        static void compute_mini_blocked_arr(std::array<MapT, R_count> const &usable_arr,
                                             RotState<MapT, R_count> &rs)
        {
            [&]<std::size_t... Rs>(std::index_sequence<Rs...>)
            {
                ((build_mini_blocked<Spec, T, MapT, R_count, static_cast<std::uint8_t>(Rs)>(
                     usable_arr, rs.blocked_arr[Rs])),
                 ...);
            }(std::make_index_sequence<R_count>{});
        }

        //=== apply_emit_1g / apply_emit_20g ===
        //commit (strategy-deref): apply_emit_1g 不再持 TetrisContext, 改由
        //  strategy 在调用前将 (last_x, last_y, last_r) 折成 BBState,
        //  作为 last_state 透传进来.
        template<class MapT, std::size_t R_count>
        static void apply_emit_1g(LandPoint &node_ex, Payload const &payload,
                                  std::size_t depth, Config const *cfg,
                                  bb::BBState const *last_state,
                                  std::array<MapT, R_count> const &usable_arr,
                                  std::uint8_t r, int xb, int yb) noexcept
        {
            (void)usable_arr;
            (void)r;
            (void)xb;
            (void)yb;
            node_ex.is_check = true;
            bool spawn_seed_rotate =
                (payload.has_last_rot == 0 && depth == 0 && cfg && cfg->last_rotate);
            node_ex.is_last_rotate = (payload.has_last_rot != 0) || spawn_seed_rotate;
            //commit C2 (N.1): payload.spin -> payload.type. 编码不变 (0/1/2),
            //  仅字段更名以与 ASpin (type=3) 共用 SpinTypePayload.
            node_ex.is_ready = (payload.type != 0);
            node_ex.is_mini_ready = (payload.type == 1);
            if (last_state != nullptr)
            {
                node_ex.last = *last_state;
            }
        }

        //=== Payload trait: 让 strategy 能查询 "本次 emit 是否需要解 last 节点".
        //  TSpinHook 的 SpinTypePayload 有 has_last_rot / last_x/last_y/last_r,
        //  strategy 在 resolves_last_1g==true 路径上读这些字段, 通过
        //  state_to_node 折回 master 指针, 不再走 ctx.
        static constexpr bool resolves_last_1g = true;

        static bool payload_has_last_rot(Payload const &p) noexcept
        {
            return p.has_last_rot != 0;
        }
        static std::uint8_t payload_last_r(Payload const &p) noexcept
        {
            return p.last_r;
        }
        static int payload_last_x(Payload const &p) noexcept
        {
            return static_cast<int>(p.last_x);
        }
        static int payload_last_y(Payload const &p) noexcept
        {
            return static_cast<int>(p.last_y);
        }

        template<class MapT, std::size_t R_count>
        static void apply_emit_20g(LandPoint &node_ex, bb::BBState const *last_state,
                                   char action, std::size_t depth, Config const *cfg,
                                   RotState<MapT, R_count> const &rs,
                                   std::array<MapT, R_count> const &usable_arr,
                                   std::uint8_t r, int xb, int yb)
        {
            (void)usable_arr;
            if (last_state != nullptr)
                node_ex.last = *last_state;
            node_ex.is_check = true;
            bool spawn_seed_rotate =
                (last_state == nullptr && depth == 0 && cfg && cfg->last_rotate);
            node_ex.is_last_rotate = (action != ' ') || spawn_seed_rotate;
            //commit (drop-search-state): is_ready / is_mini_ready 不再走 master
            //  坐标系 (x_diff/y_diff/block_data 列模板) + node->rotate_*->check(map)
            //  的 oracle 路径. 全部读位板预算表:
            //    - corners3_arr[r].get(xb, yb): "≥3 corner 占用 (含越界)" 与
            //      master Search::check_ready 同义.
            //    - blocked_arr[r].get(xb, yb): "4 邻旋转全不可放置" 与 master
            //      Search::check_mini_ready 的旋转邻域取反同义. is_mini_ready =
            //      is_ready & blocked.
            bool is_ready = rs.corners3_arr[r].get(xb, yb);
            bool is_blocked = rs.blocked_arr[r].get(xb, yb);
            node_ex.is_ready = is_ready;
            node_ex.is_mini_ready = is_ready && is_blocked;
        }

    private:
        template<class MapT, int Cx, int Cy>
        static MapT corner_bb_ct(MapT const &board) noexcept
        {
            MapT shifted = board.template shifted<-Cx, -Cy>();
            return ~shifted;
        }

        template<class Spec, char T, class MapT, std::size_t R_count, std::uint8_t R>
        static void build_corners3(MapT const &board, RotState<MapT, R_count> &rs)
        {
            constexpr int pvx = shape::piece_cells<Spec, T, R>.pivot.x;
            constexpr int pvy = shape::piece_cells<Spec, T, R>.pivot.y;
            MapT bl = corner_bb_ct<MapT, pvx - 1, pvy - 1>(board);
            MapT br = corner_bb_ct<MapT, pvx + 1, pvy - 1>(board);
            MapT tl = corner_bb_ct<MapT, pvx - 1, pvy + 1>(board);
            MapT tr = corner_bb_ct<MapT, pvx + 1, pvy + 1>(board);
            rs.corners3_arr[R] = (bl & br & (tl | tr)) | (tl & tr & (bl | br));
        }

        template<class Spec, char T, class LP, class MapT, std::size_t R_count,
                 class OriginArr, class Fn>
        static void emit_set(MapT const &set, int r, std::uint8_t spin,
                             std::array<MapT, R_count> const &search,
                             bool with_last, OriginArr const &origin_xs,
                             OriginArr const &origin_ys, Fn &&fn)
        {
            std::int8_t ox = origin_xs[r];
            std::int8_t oy = origin_ys[r];
            set.for_each_set_bit([&](int x, int y)
                                 {
                LP lp{};
                lp.x = static_cast<std::int8_t>(x - ox);
                lp.y = static_cast<std::int8_t>(y + oy);
                lp.r = static_cast<std::uint8_t>(r);
                //commit C2 (N.1): SpinTypePayload.type 取代旧 TSpinPayload.spin,
                //  byte-byte 等价 (offset 0, uint8_t). 0/1/2 编码不变.
                lp.extra.type = spin;
                if (with_last)
                {
                    find_last_rotate_pred<Spec, T, MapT, R_count, LP, OriginArr>(
                        static_cast<std::uint8_t>(r), x, y, search, lp,
                        origin_xs, origin_ys);
                }
                fn(lp); });
        }

        template<class Spec, char T, class MapT, std::size_t R_count, class LP, class OriginArr>
        static void find_last_rotate_pred(std::uint8_t dst_r, int dst_x, int dst_y,
                                          std::array<MapT, R_count> const &search,
                                          LP &lp, OriginArr const &origin_xs,
                                          OriginArr const &origin_ys)
        {
            lp.extra.has_last_rot = 0;
            [&]<std::size_t... Rs>(std::index_sequence<Rs...>)
            {
                (find_last_rotate_pred_impl<Spec, T, MapT, R_count, LP, OriginArr,
                                            static_cast<std::uint8_t>(Rs)>(
                     dst_r, dst_x, dst_y, search, lp, origin_xs, origin_ys),
                 ...);
            }(std::make_index_sequence<R_count>{});
        }

        template<class Spec, char T, class MapT, std::size_t R_count, class LP, class OriginArr,
                 std::uint8_t SrcR>
        static void find_last_rotate_pred_impl(std::uint8_t dst_r, int dst_x, int dst_y,
                                               std::array<MapT, R_count> const &search,
                                               LP &lp, OriginArr const &origin_xs,
                                               OriginArr const &origin_ys)
        {
            if (lp.extra.has_last_rot)
                return;
            constexpr std::uint8_t cw = shape::target_cw<Spec, T, SrcR>;
            constexpr std::uint8_t ccw = shape::target_ccw<Spec, T, SrcR>;
            constexpr std::uint8_t opp = shape::target_opp<Spec, T, SrcR>;
            if constexpr (cw != kOpRotateNone)
            {
                if (cw == dst_r)
                    try_kick_pred<Spec, T, MapT, R_count, LP, OriginArr, SrcR, cw,
                                  shape::wk_cw<Spec, T, SrcR>>(dst_x, dst_y, search, lp,
                                                               origin_xs, origin_ys);
            }
            if (lp.extra.has_last_rot)
                return;
            if constexpr (ccw != kOpRotateNone)
            {
                if (ccw == dst_r)
                    try_kick_pred<Spec, T, MapT, R_count, LP, OriginArr, SrcR, ccw,
                                  shape::wk_ccw<Spec, T, SrcR>>(dst_x, dst_y, search, lp,
                                                                origin_xs, origin_ys);
            }
            if (lp.extra.has_last_rot)
                return;
            if constexpr (opp != kOpRotateNone)
            {
                if (opp == dst_r)
                    try_kick_pred<Spec, T, MapT, R_count, LP, OriginArr, SrcR, opp,
                                  shape::wk_opp<Spec, T, SrcR>>(dst_x, dst_y, search, lp,
                                                                origin_xs, origin_ys);
            }
        }

        template<class Spec, char T, class MapT, std::size_t R_count, class LP, class OriginArr,
                 std::uint8_t SrcR, std::uint8_t DstR, class KickList>
        static void try_kick_pred(int dst_x, int dst_y,
                                  std::array<MapT, R_count> const &search, LP &lp,
                                  OriginArr const &origin_xs, OriginArr const &origin_ys)
        {
            constexpr int dx_origin = shape::piece_cells<Spec, T, DstR>.origin.x -
                                      shape::piece_cells<Spec, T, SrcR>.origin.x;
            constexpr int dy_origin = shape::piece_cells<Spec, T, SrcR>.origin.y -
                                      shape::piece_cells<Spec, T, DstR>.origin.y;
            std::size_t total_kicks = KickList::length + 1;
            for (std::size_t k = 0; k < total_kicks; ++k)
            {
                int dx, dy;
                if (k == 0)
                {
                    dx = dx_origin;
                    dy = dy_origin;
                }
                else
                {
                    dx = KickList::data[(k - 1) * 2] + dx_origin;
                    dy = KickList::data[(k - 1) * 2 + 1] + dy_origin;
                }
                int src_x = dst_x - dx;
                int src_y = dst_y - dy;
                if (!MapT::is_ok_x(src_x) || !MapT::is_ok_y(src_y))
                    continue;
                if (!search[SrcR].get(src_x, src_y))
                    continue;
                lp.extra.last_x = static_cast<std::int8_t>(src_x - origin_xs[SrcR]);
                lp.extra.last_y = static_cast<std::int8_t>(src_y + origin_ys[SrcR]);
                lp.extra.last_r = static_cast<std::uint8_t>(SrcR);
                lp.extra.has_last_rot = 1;
                return;
            }
        }

        //=== 5. target_blocked_mask / direction_open_mask: Mini 4 方向阻挡判定 ===
        template<class Spec, char T, class MapT, std::size_t R_count>
        static void target_blocked_mask(int r,
                                        std::array<MapT, R_count> const &usable_arr,
                                        MapT &blocked)
        {
            [&]<std::size_t... Rs>(std::index_sequence<Rs...>)
            {
                (target_blocked_mask_impl<Spec, T, MapT, R_count,
                                          static_cast<std::uint8_t>(Rs)>(r, usable_arr, blocked),
                 ...);
            }(std::make_index_sequence<R_count>{});
        }

        template<class Spec, char T, class MapT, std::size_t R_count, std::uint8_t R>
        static void target_blocked_mask_impl(int r,
                                             std::array<MapT, R_count> const &usable_arr,
                                             MapT &blocked)
        {
            if (r != static_cast<int>(R))
                return;
            constexpr std::uint8_t cw = shape::target_cw<Spec, T, R>;
            constexpr std::uint8_t ccw = shape::target_ccw<Spec, T, R>;
            constexpr std::uint8_t opp = shape::target_opp<Spec, T, R>;
            if constexpr (cw != kOpRotateNone)
                blocked &= ~direction_open_mask<Spec, T, MapT, R_count, R, cw,
                                                shape::wk_cw<Spec, T, R>>(usable_arr);
            if constexpr (ccw != kOpRotateNone)
                blocked &= ~direction_open_mask<Spec, T, MapT, R_count, R, ccw,
                                                shape::wk_ccw<Spec, T, R>>(usable_arr);
            if constexpr (opp != kOpRotateNone)
                blocked &= ~direction_open_mask<Spec, T, MapT, R_count, R, opp,
                                                shape::wk_opp<Spec, T, R>>(usable_arr);
        }

        //compute_mini_blocked_arr 用. 与 target_blocked_mask_impl 同形, 只是
        //  对 fixed R 输出整张 blocked 位图 (而不是合并 r 的 if-then 分发).
        template<class Spec, char T, class MapT, std::size_t R_count, std::uint8_t R>
        static void build_mini_blocked(std::array<MapT, R_count> const &usable_arr, MapT &out)
        {
            out = MapT(MapT::all_mask());
            constexpr std::uint8_t cw = shape::target_cw<Spec, T, R>;
            constexpr std::uint8_t ccw = shape::target_ccw<Spec, T, R>;
            constexpr std::uint8_t opp = shape::target_opp<Spec, T, R>;
            if constexpr (cw != kOpRotateNone)
                out &= ~direction_open_mask<Spec, T, MapT, R_count, R, cw,
                                            shape::wk_cw<Spec, T, R>>(usable_arr);
            if constexpr (ccw != kOpRotateNone)
                out &= ~direction_open_mask<Spec, T, MapT, R_count, R, ccw,
                                            shape::wk_ccw<Spec, T, R>>(usable_arr);
            if constexpr (opp != kOpRotateNone)
                out &= ~direction_open_mask<Spec, T, MapT, R_count, R, opp,
                                            shape::wk_opp<Spec, T, R>>(usable_arr);
        }

        template<class Spec, char T, class MapT, std::size_t R_count,
                 std::uint8_t SrcR, std::uint8_t DstR, class KickList>
        static MapT direction_open_mask(std::array<MapT, R_count> const &usable_arr)
        {
            constexpr int dx_origin = shape::piece_cells<Spec, T, DstR>.origin.x -
                                      shape::piece_cells<Spec, T, SrcR>.origin.x;
            constexpr int dy_origin = shape::piece_cells<Spec, T, SrcR>.origin.y -
                                      shape::piece_cells<Spec, T, DstR>.origin.y;
            MapT inbounds_dst = inbounds_for_rotation<Spec, T, MapT, DstR>();
            MapT prior_inbounds{};
            MapT open{};
            std::size_t total_kicks = KickList::length + 1;
            for (std::size_t k = 0; k < total_kicks; ++k)
            {
                int dx, dy;
                if (k == 0)
                {
                    dx = dx_origin;
                    dy = dy_origin;
                }
                else
                {
                    dx = KickList::data[(k - 1) * 2] + dx_origin;
                    dy = KickList::data[(k - 1) * 2 + 1] + dy_origin;
                }
                MapT kick_inbounds = shift_runtime(inbounds_dst, -dx, -dy);
                MapT chosen = kick_inbounds & ~prior_inbounds;
                MapT kick_usable = shift_runtime(usable_arr[DstR], -dx, -dy);
                open |= chosen & kick_usable;
                prior_inbounds |= kick_inbounds;
            }
            return open;
        }

        template<class Spec, char T, class MapT, std::uint8_t R>
        static MapT inbounds_for_rotation()
        {
            constexpr auto cnt = shape::piece_cells<Spec, T, R>.count;
            return inbounds_for_rotation_impl<Spec, T, MapT, R>(
                std::make_index_sequence<cnt>{});
        }

        template<class Spec, char T, class MapT, std::uint8_t R, std::size_t... Is>
        static MapT inbounds_for_rotation_impl(std::index_sequence<Is...>)
        {
            using Op = shape::find_op<Spec, T, R>;
            constexpr auto cells = shape::op_cells<Op>;
            MapT result = MapT(MapT::all_mask());
            MapT all_ones = MapT(MapT::all_mask());
            ((result &= all_ones.template shifted<-cells.cells[Is].x, -cells.cells[Is].y>()), ...);
            return result;
        }

        template<class MapT>
        static MapT shift_runtime(MapT const &m, int dx, int dy) noexcept
        {
            MapT result{};
            using row_t = typename MapT::row_t;
            constexpr row_t full = MapT::row_full;
            constexpr int H = MapT::height;
            for (int y = 0; y < H; ++y)
            {
                int src_y = y - dy;
                if (src_y < 0 || src_y >= H)
                    continue;
                row_t r = m.row(src_y);
                if (dx > 0)
                    r = static_cast<row_t>((r << dx) & full);
                else if (dx < 0)
                    r = static_cast<row_t>(r >> (-dx));
                result.set_row(y, r);
            }
            return result;
        }
    };

    //向后兼容旧名:
    using DefaultTSpinHook = TSpinHook;
    using TSpinEnableHook = TSpinHook;

    //=========================================================================
    // ASpinHook
    //
    // 把 search_aspin::Search 内部 "落地后 4 个相邻 status 位置都不可放置" 的
    // 角点判定搬到 hook. 与 TSpinHook 不同:
    //   * ASpin 不需要 corners3 / Mini / last-rotate 谱系 — RotState / SearchState
    //     退化为空 struct, on_init_rotations / on_rotate_reach / on_search_state_init
    //     全部空实现.
    //   * Payload 复用 SpinTypePayload (commit C2 N.1): type 字段编码
    //     0=None / 3=ASpin. 其余 4 个 last_* 字段空载 (ASpin 不需要 last 反查).
    //   * active_for_piece<T> = true: ASpin 对所有 piece 都计算.
    //   * Config 与 LandPoint 直接复用 search_aspin::Search::* 公开类型, 保持
    //     外部 API 不动.
    //
    // ASpinHook **不**派生 BaseSpinHook — 它自己写 on_emit / apply_emit_1g /
    // apply_emit_20g (4 邻位板阻挡). lp_requires_last_rotate 必须返回 false
    // (commit C2 N.4 修正: 旧 is_landpoint_none 在 ASpin 命中时返回 false, 让
    // ASpin 路径"起点==终点"被错误地拒绝自落点短路 — 反转后修复此 bug).
    //=========================================================================
    struct ASpinHook
    {
        struct EmptyRotState
        {
        };

        using Payload = SpinTypePayload;
        using LandPoint = ::search_aspin::Search::TetrisNodeWithASpinType;
        using Config = ::search_aspin::Search::Config;

        template<class MapT, std::size_t R_count>
        using RotState = EmptyRotState;

        //ASpin 对所有 piece 都参与 4 方向阻挡判定.
        template<char T, class Policy = void>
        static constexpr bool active_for_piece = []() constexpr
        {
            if constexpr (std::is_void_v<Policy>)
                return true;
            else
                return Policy::template value<T>;
        }();

        //=== LandPoint / Config trait ===
        //=== last-rotate 短路谓词 (commit C2 N.4 修正).
        //  ASpin 不要求 path 字符串以旋转结尾 (它的判定是落点 4 邻几何阻挡, 与
        //  path 无关), 因此恒返回 false — 让 "起点 == 终点" 自落点短路在 ASpin
        //  路径上正常生效. 旧 is_landpoint_none(lp) 在 lp.type=ASpin 时返回
        //  false (= "不是 None, 不能短路"), 强制 ASpin 落点走完整 BFS — 是 bug.
        //  反转后, ASpin 命中且起点==终点的场景直接 return 空 path, 与 oracle
        //  在该 corner case 下行为一致 (oracle 走完整 BFS 但 build_path(found)
        //  在起点 self-prev 协议下也回空 path).
        static bool lp_requires_last_rotate(Config const *, LandPoint const &) noexcept
        {
            return false;
        }
        static bb::BBState const *get_last_state(LandPoint const &) noexcept
        {
            return nullptr;
        }
        static bool config_last_rotate(Config const *) noexcept
        {
            return false;
        }
        static bool config_allow_180(Config const *cfg) noexcept
        {
            return cfg && cfg->allow_180;
        }
        static bool config_allow_LR(Config const *cfg) noexcept
        {
            return cfg && cfg->allow_LR;
        }
        static bool config_allow_d(Config const *cfg) noexcept
        {
            return cfg && cfg->allow_d;
        }
        static bool config_allow_D(Config const *cfg) noexcept
        {
            return cfg && cfg->allow_D;
        }
        static bool config_allow_rotate_move(Config const *cfg) noexcept
        {
            return cfg && cfg->allow_rotate_move;
        }
        static bool config_is_20g(Config const *cfg) noexcept
        {
            return cfg && cfg->is_20g;
        }

        //=== 算法接口 ===
        template<class Spec, char T, class MapT, std::size_t R_count>
        static void on_init_rotations(MapT const &, RotState<MapT, R_count> &) noexcept
        {
        }

        template<class MapT, std::size_t R_count>
        static void on_rotate_reach(std::uint8_t, MapT const &,
                                    RotState<MapT, R_count> &) noexcept
        {
        }

        //compute_mini_blocked_arr: ASpin 不消费 RotState::blocked_arr (它的
        //  apply_emit_20g 直接读 usable_arr 4 邻判 ASpin), 因此 noop. 但
        //  签名与 TSpinHook 一致, 让 strategy 在 active_for_piece=true 路径上
        //  统一调用而不必 if constexpr 拆分两个 hook.
        template<class Spec, char T, class MapT, std::size_t R_count>
        static void compute_mini_blocked_arr(std::array<MapT, R_count> const &,
                                             RotState<MapT, R_count> &) noexcept
        {
        }

        //=== on_emit (1g) ===
        template<class Spec, char T, class LP, class MapT, std::size_t R_count,
                 class OriginArr, class Fn>
        static void on_emit(MapT const &landings, int r, RotState<MapT, R_count> const &,
                            std::array<MapT, R_count> const & /*search*/,
                            std::array<MapT, R_count> const &usable_arr,
                            OriginArr const &origin_xs, OriginArr const &origin_ys, Fn &&fn)
        {
            MapT const &u = usable_arr[r];
            MapT not_up = ~u.template shifted<0, 1>();
            MapT not_down = ~u.template shifted<0, -1>();
            MapT not_left = ~u.template shifted<-1, 0>();
            MapT not_right = ~u.template shifted<1, 0>();
            MapT aspin_set = landings & not_up & not_down & not_left & not_right;
            MapT none_set = landings & ~aspin_set;
            std::int8_t ox = origin_xs[r];
            std::int8_t oy = origin_ys[r];
            aspin_set.for_each_set_bit([&](int x, int y)
                                       {
                LP lp{};
                lp.x = static_cast<std::int8_t>(x - ox);
                lp.y = static_cast<std::int8_t>(y + oy);
                lp.r = static_cast<std::uint8_t>(r);
                //commit C2 (N.1): SpinTypePayload.type 编码 ASpin=3. 旧
                //  ASpinPayload.aspin (1=ASpin) 字段语义被 type=3 取代.
                lp.extra.type = 3;
                fn(lp); });
            none_set.for_each_set_bit([&](int x, int y)
                                      {
                LP lp{};
                lp.x = static_cast<std::int8_t>(x - ox);
                lp.y = static_cast<std::int8_t>(y + oy);
                lp.r = static_cast<std::uint8_t>(r);
                lp.extra.type = 0; // None
                fn(lp); });
        }

        //=== check_ready / check_mini_ready (ASpin 不消费; 提供空实现保持模板对称) ===
        //commit (drop-search-state): check_ready / check_mini_ready 已从公共 hook
        //  接口面下线 (TSpinHook 内 apply_emit_20g 改读 RotState 的位板预算表).
        //  ASpinHook 自然不需要再提供这两个空实现.

        //=== apply_emit_1g ===
        //commit (strategy-deref): 与 TSpinHook 同形, 去掉 TetrisContext 参数.
        //  ASpinHook 不消费 last_state (resolves_last_1g=false), strategy 走该
        //  快路径时直接把 nullptr 传进来.
        template<class MapT, std::size_t R_count>
        static void apply_emit_1g(LandPoint &node_ex, Payload const &payload,
                                  std::size_t depth, Config const *cfg,
                                  bb::BBState const * /*last_state*/,
                                  std::array<MapT, R_count> const &usable_arr,
                                  std::uint8_t r, int xb, int yb) noexcept
        {
            (void)depth;
            (void)cfg;
            (void)usable_arr;
            (void)r;
            (void)xb;
            (void)yb;
            //commit C2 (N.1): payload.type==3 即 ASpin (与 SpinTypePayload 编码
            //  对齐: 0=None / 3=ASpin). 等价于旧 payload.aspin!=0.
            node_ex.type = (payload.type == 3) ? ::search_aspin::Search::ASpin
                                               : ::search_aspin::Search::None;
        }

        //=== Payload trait: ASpin 不需要 last 反查 ===
        static constexpr bool resolves_last_1g = false;
        static bool payload_has_last_rot(Payload const &) noexcept
        {
            return false;
        }
        static std::uint8_t payload_last_r(Payload const &) noexcept
        {
            return 0;
        }
        static int payload_last_x(Payload const &) noexcept
        {
            return 0;
        }
        static int payload_last_y(Payload const &) noexcept
        {
            return 0;
        }

        //=== apply_emit_20g ===
        template<class MapT, std::size_t R_count>
        static void apply_emit_20g(LandPoint &node_ex, bb::BBState const * /*last_state*/,
                                   char /*action*/, std::size_t /*depth*/,
                                   Config const * /*cfg*/,
                                   RotState<MapT, R_count> const & /*rs*/,
                                   std::array<MapT, R_count> const &usable_arr,
                                   std::uint8_t r, int xb, int yb)
        {
            auto blocked = [&](int x, int y) -> bool
            {
                if (!MapT::is_ok_x(x) || !MapT::is_ok_y(y))
                    return true;
                return !usable_arr[r].get(x, y);
            };
            bool aspin = blocked(xb, yb + 1) && blocked(xb, yb - 1) &&
                         blocked(xb - 1, yb) && blocked(xb + 1, yb);
            node_ex.type = aspin ? ::search_aspin::Search::ASpin
                                 : ::search_aspin::Search::None;
        }
    };

    //向后兼容旧名:
    using DefaultASpinHook = ASpinHook;

    //=========================================================================
    // CautiousHook: c2_ai 走 MoveGenSearch 时挂的 hook 变体. 不计算任何 spin/last
    //  元数据 (active_for_piece 全 false), LandPoint 用 BBLandPoint (持有
    //  BBState，无 TetrisNode 指针). Config 仅暴露 fast_move_down 一个字段,
    //  与上层 c2_ai 历史调用面一致.
    //
    //  CautiousHook 派生自 BaseSpinHook<CautiousHook> 拿到默认 noop 的 BFS 钩子
    //  / LandPoint trait / apply_emit_*. 自家覆盖 4 个 config_*:
    //   * config_allow_180 / config_allow_LR / config_allow_D : 历史恒 true,
    //     与 NoSpinHook 同形 — path 1g 邻居枚举读这些项决定字符集.
    //   * config_allow_d : 依赖 fast_move_down 字段 (true 走 'D', 不展开 'd').
    //  其余 (config_last_rotate / config_allow_rotate_move / config_is_20g)
    //  历史均为 false, 继承 BaseSpinHook 默认即可.
    //=========================================================================
    struct CautiousHook : BaseSpinHook<CautiousHook>
    {
        struct EmptyRotState
        {
        };

        using Payload = EmptyPayload;
        using LandPoint = BBLandPoint;
        struct Config
        {
            bool fast_move_down = false;
        };

        template<class MapT, std::size_t R_count>
        using RotState = EmptyRotState;

        template<char T, class Policy = void>
        static constexpr bool active_for_piece = false;

        //=== Config trait ===
        //  fast_move_down=true  : make_path 1g 末段只走 'D', 不展开 'd' 邻居.
        //  fast_move_down=false : 同时枚举 'd' 与 'D', 与 SRS 系默认一致.
        //  其余开关固定: 180-kick / LR 多步 / D 落底 一律开启.
        static bool config_allow_180(Config const *) noexcept
        {
            return true;
        }
        static bool config_allow_LR(Config const *) noexcept
        {
            return true;
        }
        static bool config_allow_d(Config const *cfg) noexcept
        {
            return cfg && !cfg->fast_move_down;
        }
        static bool config_allow_D(Config const *) noexcept
        {
            return true;
        }
    };

    //=========================================================================
    // detail::DeduceSpinPolicy<AI> — 从 AI class 的 EvalSpinType 别名自动
    // 推导 ActivePiecePolicy.
    //
    // 推导规则:
    //   AI 定义了 using EvalSpinType = <SpinT>
    //     ├─ EvalSpinType 是 search_tspin::Search::TSpinType
    //     │    → ActiveTOnlyPolicy
    //     ├─ EvalSpinType 是 search_aspin::Search::ASpinType
    //     │    → ActiveAllPolicy
    //     └─ 其他 (未来新类型)
    //          → ActiveNonePolicy (保守回退)
    //   AI 未定义 EvalSpinType
    //     → ActiveNonePolicy (无 spin AI, 不需要 spin)
    //
    // 设计约定:
    //   * AI 不必知道 Policy 体系, 推导纯靠 EvalSpinType alias 反查.
    //   * ai_zzz::tspin::* / ai_misaka::* / ai_tag::the_ai_games* 用
    //     TSpinType → ActiveTOnlyPolicy.
    //   * ai_zzz::aspin::* 用 ASpinType → ActiveAllPolicy.
    //   * ai_zzz::qq/c2, ai_farter, ai_easy, ai_ax 无 EvalSpinType
    //     → ActiveNonePolicy.
    //=========================================================================
    namespace detail
    {
        //--- 步骤 1: 探测 AI::EvalSpinType 是否存在 ---
        template<class AI, class = void>
        struct has_eval_spin_type : std::false_type
        {
        };
        template<class AI>
        struct has_eval_spin_type<AI, std::void_t<typename AI::EvalSpinType>> : std::true_type
        {
        };

        //--- 步骤 2: 根据 EvalSpinType 的具体类型选 Policy ---
        //  主模板: 无 EvalSpinType 或未知类型 → ActiveNonePolicy (保守).
        template<class AI, bool HasSpin = has_eval_spin_type<AI>::value>
        struct DeduceSpinPolicyImpl
        {
            using type = ActiveNonePolicy;
        };
        //  有 EvalSpinType 的偏特化: 再根据实际类型分发.
        template<class AI>
        struct DeduceSpinPolicyImpl<AI, true>
        {
        private:
            using SpinT = typename AI::EvalSpinType;

        public:
            using type = std::conditional_t<
                std::is_same_v<SpinT, ::search_tspin::Search::TSpinType>,
                ActiveTOnlyPolicy,
                std::conditional_t<
                    std::is_same_v<SpinT, ::search_aspin::Search::ASpinType>,
                    ActiveAllPolicy,
                    ActiveNonePolicy>>;
        };
    } // namespace detail

    //--- 公开入口 ---
    template<class AI>
    using DeduceSpinPolicy = typename detail::DeduceSpinPolicyImpl<AI>::type;

} // namespace m_tetris2

#endif // TETRIS_AI_RUNNER_MOVEGEN_HOOK_H_
