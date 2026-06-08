#ifndef TETRIS_AI_RUNNER_SEARCH_SIMPLE_H_
#define TETRIS_AI_RUNNER_SEARCH_SIMPLE_H_

//==========================================================================
// SimpleStrategy<SpinHook, RuleSpec>: 位板 simple-naive search strategy.
//
// 与旧 MoveGenSearch<RuleSpec, SimpleNoSpinHook> 在 search()/make_path() 上
// 行为完全等价 — 与 src/search_simple_node::Search 1:1 等价的位板实现:
//
//   search():  起点 usable 守门 -> spawn-row fast path (node->land_point !=
//              nullptr && node->low >= map.roof) 直接遍历 piece-definition
//              spawn-row 节点 + drop -> 否则 rotation graph BFS 出所有
//              reachable rotation, 在每个 rotation 上 drop + 沿 ±x 横走 +
//              drop, cells_key 等价类去重.
//   make_path(): Stage 1 = rotation graph BFS 找最少旋转前缀; Stage 2 =
//                横向 L/R 走到 land_point.x; Stage 3 = drop, 仅 'D' 落地.
//
// 关键不变量:
//   * 不消费 bb::run_bb_bfs / bb::EnqueueDecision — rotation BFS 与 lateral
//     run 都是简单循环, 自闭合于本 strategy 内部.
//   * 旋转邻居用 rotate_no_kick_bb (无 wall-kick); 与 search_simple_node 同形.
//   * Hook 当前唯一消费者是 NoSpinHook (active_for_piece 全 false), search
//     不写 spin/last 元数据, make_path 也不消费末段 wall-kick 重放.
//==========================================================================

#include "bb_state.h"
#include "movegen_context.h"
#include "movegen_hook.h"
#include "movegen_searcher.h"
#include "movegen_strategy.h"
#include "tetris_core.h"
#include "tetris_rule_spec.h"
#include "tetris_shape.h"

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>
#include <vector>

namespace m_tetris2
{
    namespace movegen
    {
        //=== Simple strategy 私有 mixin: SpinHook 依赖字段 ====================
        // 与 detail::path::ExtrasMixin / detail::simulate::ExtrasMixin 同形,
        // 但 simple 走 spawn-row fast path 时需要 TetrisContext (master
        // node->land_point spawn-row 列表是 piece-definition 表, 由
        // 旧初始化阶段预建).
        //commit 3: 落到 detail::simple:: 子命名空间, 标注 "SimpleStrategy
        //  私有实现细节, 第三方扩展不应直接消费".
        namespace detail
        {
            namespace simple
            {
                template<class SpinHook, class RuleSpec>
                struct ExtrasMixin
                {
                    using Config = typename SpinHook::Config;
                    using LandPoint = typename SpinHook::LandPoint;

                    Config const *config_ = nullptr;
                    std::vector<LandPoint> land_point_cache_{};
                };

                template<char T, class EvalCallback, class LP, std::size_t... Rs>
                inline void dispatch_r(std::uint8_t r, LP const &lp, EvalCallback &cb, std::index_sequence<Rs...>) {
                    ((static_cast<std::uint8_t>(Rs) == r ? (cb.template operator()<T, static_cast<std::uint8_t>(Rs)>(lp), true) : false) || ...);
                }
            } // namespace simple
        } // namespace detail

        template<class SpinHook, class RuleSpec, class Policy = void>
        struct SimpleStrategy
        {
        private:
            using Helpers = bb::Helpers<RuleSpec>;
            using map_t = typename Helpers::map_t;
            using piece_info_t = typename Helpers::piece_info_t;

            static constexpr int kW = Helpers::kW;
            static constexpr int kH = Helpers::kH;
            static constexpr int kMaxR = Helpers::kMaxR;
            static constexpr int kPieceCount = Helpers::kPieceCount;

            static constexpr std::uint8_t kSimpleRotBad = 0xFFu;

            // rotation BFS 的 per-r 节点信息. visited_state 仅当该 r 被访问后
            // 有效; parent_r == kSimpleRotBad 标识根节点 (起点 spawn).
            struct SimpleRotEntry
            {
                bb::BBState state{};
                std::uint8_t visited = 0;
                std::uint8_t parent_r = kSimpleRotBad;
                char op = '\0';
            };

        public:
            using LandPoint = typename SpinHook::LandPoint;
            using Config = typename SpinHook::Config;

            // simple 不需要 PathMarkMixin / BfsQueueMixin: rotation BFS 用栈数组,
            // lateral run 用循环, 全部函数局部.
            using Context = MoveGenContext<RuleSpec,
                                           detail::simple::ExtrasMixin<SpinHook, RuleSpec>>;

            //=== init ==========================================================
            static void init(Context &ctx, Config const *config)
            {
                ctx.config_ = config;
            }

            //=== search ========================================================
            static std::vector<LandPoint> const *
            search(Context &ctx, TetrisMap const &map, bb::BBState const &spawn, std::size_t depth)
            {
                (void)depth;
                ctx.land_point_cache_.clear();
                char piece_t = static_cast<char>(spawn.t);
                if (piece_info_t::index_of(piece_t) < 0)
                {
                    assert(false && "SimpleStrategy::search: piece type outside RuleSpec");
                    return &ctx.land_point_cache_;
                }
                map_t board = Helpers::build_board(map);
                std::array<map_t, kMaxR> usable_arr{};
                Helpers::build_usable_for_piece(piece_t, board, usable_arr);

                std::vector<bb::CellsKey> emitted_keys;
                emitted_keys.reserve(32);

                bb::BBState start_state = spawn;
                if (!Helpers::usable_at_bb(start_state.r, start_state.xb, start_state.yb, usable_arr))
                    return &ctx.land_point_cache_;

                // fast path: spawn 高于 roof 时, 走 spawn-row land_point 列表.
                //   master node->land_point 是旧初始化阶段建好的
                //   piece-definition 表 (spawn rotation chain × 每 r 的 spawn-row
                //   leftmost→rightmost), 与棋盘无关. 位板等价: 用 inbounds_arr
                //   重建一份等价 BBState 列表 (Helpers::enumerate_spawn_row_states),
                //   再对每个 spawn-row state 走 drop_bb_state. 与 oracle/search_
                //   simple.cpp:73-79 完全等价.
                //
                //commit (strategy-deref): 旧版直接读 master `node->low >= map.roof`.
                //  现在改 `Helpers::is_above_roof_bb(start_state, map.roof)` —
                //  位板侧从 entry BBState 取 piece cell-min-y 与 map.roof 比较,
                //  语义与 oracle 对齐 (spawn entry 完全悬空在屋顶以上).
                if (Helpers::is_above_roof_bb(start_state, map.roof))
                {
                    std::array<map_t, kMaxR> inbounds_arr{};
                    Helpers::build_inbounds_for_piece(piece_t, inbounds_arr);
                    std::vector<bb::BBState> spawn_states;
                    Helpers::enumerate_spawn_row_states(piece_t, start_state, inbounds_arr, spawn_states);
                    for (bb::BBState const &spawn_state : spawn_states)
                    {
                        auto sunk = Helpers::drop_bb_state(spawn_state, usable_arr);
                        if (!sunk)
                            continue;
                        bb::CellsKey k = Helpers::cells_key_for_state(*sunk);
                        bool dup = false;
                        for (bb::CellsKey ek : emitted_keys)
                        {
                            if (ek == k)
                            {
                                dup = true;
                                break;
                            }
                        }
                        if (dup)
                            continue;
                        emitted_keys.push_back(k);
                        LandPoint lp;
                        lp.state = *sunk;
                        ctx.land_point_cache_.push_back(lp);
                    }
                    return &ctx.land_point_cache_;
                }

                // slow path: rotation BFS.
                std::array<SimpleRotEntry, kMaxR> rot_entries{};
                std::vector<std::uint8_t> rot_order;
                rot_order.reserve(kMaxR);
                collect_rotations_bb(piece_t, start_state, usable_arr, rot_entries, rot_order);

                for (std::uint8_t r : rot_order)
                {
                    emit_simple_drops_for_rotation(ctx, rot_entries[r].state, usable_arr, emitted_keys);
                }
                return &ctx.land_point_cache_;
            }

            //=== search_eval ===================================================
            // Phase 2 推式接口：SimpleStrategy 的薄包装实现。
            //==================================================================
            template<class EvalCallback>
            static void search_eval(Context &ctx, Map<RuleSpec::width, RuleSpec::height> const &board,
                                    bb::BBState const &spawn, std::size_t depth,
                                    EvalCallback &on_land)
            {
                (void)depth;
                char piece_t = static_cast<char>(spawn.t);
                if (piece_info_t::index_of(piece_t) < 0)
                {
                    assert(false && "SimpleStrategy::search_eval: piece type outside RuleSpec");
                    return;
                }
                std::array<map_t, kMaxR> usable_arr{};
                Helpers::build_usable_for_piece(piece_t, board, usable_arr);
                if (!Helpers::usable_at_bb(spawn.r, spawn.xb, spawn.yb, usable_arr))
                    return;

                std::vector<bb::CellsKey> emitted_keys;
                emitted_keys.reserve(32);
                int roof = Helpers::board_roof(board);

                if (Helpers::is_above_roof_bb(spawn, roof))
                {
                    std::array<map_t, kMaxR> inbounds_arr{};
                    Helpers::build_inbounds_for_piece(piece_t, inbounds_arr);
                    std::vector<bb::BBState> spawn_states;
                    Helpers::enumerate_spawn_row_states(piece_t, spawn, inbounds_arr, spawn_states);
                    for (bb::BBState const &spawn_state : spawn_states)
                    {
                        auto sunk = Helpers::drop_bb_state(spawn_state, usable_arr);
                        if (!sunk)
                            continue;
                        bb::CellsKey k = Helpers::cells_key_for_state(*sunk);
                        bool dup = false;
                        for (bb::CellsKey ek : emitted_keys)
                        {
                            if (ek == k)
                            {
                                dup = true;
                                break;
                            }
                        }
                        if (dup)
                            continue;
                        emitted_keys.push_back(k);
                        LandPoint lp;
                        lp.state = *sunk;
                        bool dispatched = false;
                        [&]<std::size_t... Is>(std::index_sequence<Is...>) {
                            (((!dispatched && piece_info_t::type_at(Is) == piece_t) ? (
                                detail::simple::dispatch_r<piece_info_t::type_at(Is)>(sunk->r, lp, on_land,
                                    std::make_index_sequence<Helpers::template rcount_v<piece_info_t::type_at(Is)>()>{}),
                                dispatched = true
                            ) : false), ...);
                        }(std::make_index_sequence<kPieceCount>{});
                    }
                    return;
                }

                std::array<SimpleRotEntry, kMaxR> rot_entries{};
                std::vector<std::uint8_t> rot_order;
                rot_order.reserve(kMaxR);
                collect_rotations_bb(piece_t, spawn, usable_arr, rot_entries, rot_order);

                for (std::uint8_t r : rot_order)
                {
                    std::vector<bb::BBState> sunk_states;
                    emit_simple_drop_states_for_rotation(rot_entries[r].state, usable_arr, emitted_keys, sunk_states);
                    for (bb::BBState const &sunk_state : sunk_states)
                    {
                        LandPoint lp;
                        lp.state = sunk_state;
                        bool dispatched = false;
                        [&]<std::size_t... Is>(std::index_sequence<Is...>) {
                            (((!dispatched && piece_info_t::type_at(Is) == piece_t) ? (
                                detail::simple::dispatch_r<piece_info_t::type_at(Is)>(sunk_state.r, lp, on_land,
                                    std::make_index_sequence<Helpers::template rcount_v<piece_info_t::type_at(Is)>()>{}),
                                dispatched = true
                            ) : false), ...);
                        }(std::make_index_sequence<kPieceCount>{});
                    }
                }
            }

            //=== make_path =====================================================
            static std::vector<char>
            make_path(Context &ctx, bb::BBState const &spawn,
                      LandPoint const &land_point,
                      Map<RuleSpec::width, RuleSpec::height> const &board)
            {
                std::vector<char> path;
                char piece_t = static_cast<char>(spawn.t);
                bb::BBState const &land_state = land_point.state;
                if (land_state.t == 0 ||
                    piece_t != static_cast<char>(land_state.t) ||
                    current_master_y(spawn, piece_t) < current_master_y(land_state, piece_t))
                {
                    return path;
                }
                if (piece_info_t::index_of(piece_t) < 0)
                {
                    assert(false && "SimpleStrategy::make_path: piece type outside RuleSpec");
                    return path;
                }
                std::array<map_t, kMaxR> usable_arr{};
                Helpers::build_usable_for_piece(piece_t, board, usable_arr);

                bb::BBState start_state = spawn;
                if (!Helpers::usable_at_bb(start_state.r, start_state.xb, start_state.yb, usable_arr))
                    return path;

                // Stage 1: rotation graph BFS.
                std::array<SimpleRotEntry, kMaxR> rot_entries{};
                std::vector<std::uint8_t> rot_order;
                rot_order.reserve(kMaxR);
                collect_rotations_bb(piece_t, start_state, usable_arr, rot_entries, rot_order);

                std::uint8_t target_r = static_cast<std::uint8_t>(land_state.r);
                if (target_r >= kMaxR || !rot_entries[target_r].visited)
                    return path; // 目标 rotation 不可达, 空 path.

                // 回溯旋转链 (target_r → start_state.r), 反序后即首段 path.
                std::vector<char> rotation_ops;
                rotation_ops.reserve(kMaxR);
                for (std::uint8_t cursor = target_r; cursor != start_state.r;)
                {
                    SimpleRotEntry const &e = rot_entries[cursor];
                    if (e.parent_r == kSimpleRotBad)
                    {
                        return std::vector<char>();
                    }
                    rotation_ops.push_back(e.op);
                    cursor = e.parent_r;
                }
                for (auto it = rotation_ops.rbegin(); it != rotation_ops.rend(); ++it)
                    path.push_back(*it);

                // Stage 2: 横向 L/R 走到 land_point.x.
                bb::BBState cursor_state = rot_entries[target_r].state;
                int target_x = current_master_x(land_state, piece_t);
                while (current_master_x(cursor_state, piece_t) < target_x)
                {
                    bb::BBState nxt = cursor_state;
                    nxt.xb = static_cast<std::int8_t>(nxt.xb + 1);
                    if (!Helpers::usable_at_bb(nxt.r, nxt.xb, nxt.yb, usable_arr))
                        break;
                    path.push_back('r');
                    cursor_state = nxt;
                }
                while (current_master_x(cursor_state, piece_t) > target_x)
                {
                    bb::BBState nxt = cursor_state;
                    nxt.xb = static_cast<std::int8_t>(nxt.xb - 1);
                    if (!Helpers::usable_at_bb(nxt.r, nxt.xb, nxt.yb, usable_arr))
                        break;
                    path.push_back('l');
                    cursor_state = nxt;
                }

                // Stage 3: drop, 比 cells_key.
                auto sunk = Helpers::drop_bb_state(cursor_state, usable_arr);
                if (!sunk)
                    return std::vector<char>();
                bb::CellsKey land_key = Helpers::cells_key_for_state(land_state);
                if (Helpers::cells_key_for_state(*sunk) != land_key)
                    return std::vector<char>();
                path.push_back('D');
                return path;
            }

        private:
            //=== rotation graph BFS (与旧 collect_rotations_bb 1:1 等价) ========
            static void collect_rotations_bb(char piece_t,
                                             bb::BBState const &start,
                                             std::array<map_t, kMaxR> const &usable_arr,
                                             std::array<SimpleRotEntry, kMaxR> &entries,
                                             std::vector<std::uint8_t> &visited_order_out)
            {
                for (std::size_t i = 0; i < kMaxR; ++i)
                    entries[i] = SimpleRotEntry{};
                visited_order_out.clear();

                std::uint8_t r0 = start.r;
                if (r0 >= kMaxR)
                    return;
                entries[r0].state = start;
                entries[r0].visited = 1;
                entries[r0].parent_r = kSimpleRotBad;
                entries[r0].op = '\0';
                visited_order_out.push_back(r0);

                for (std::size_t cur = 0; cur < visited_order_out.size(); ++cur)
                {
                    std::uint8_t r = visited_order_out[cur];
                    bb::BBState const &cur_state = entries[r].state;
                    struct EdgeSpec
                    {
                        bb::KickDir dir;
                        char op;
                    };
                    EdgeSpec const edges[3] = {
                        {bb::KickDir::Cw, 'c'},
                        {bb::KickDir::Ccw, 'z'},
                        {bb::KickDir::Opp, 'x'},
                    };
                    for (auto const &e : edges)
                    {
                        auto nb = Helpers::rotate_no_kick_bb(piece_t, e.dir, cur_state);
                        if (!nb)
                            continue;
                        bb::BBState const &ns = *nb;
                        if (ns.r >= kMaxR)
                            continue;
                        if (entries[ns.r].visited)
                            continue;
                        if (!Helpers::usable_at_bb(ns.r, ns.xb, ns.yb, usable_arr))
                            continue;
                        entries[ns.r].state = ns;
                        entries[ns.r].visited = 1;
                        entries[ns.r].parent_r = r;
                        entries[ns.r].op = e.op;
                        visited_order_out.push_back(ns.r);
                    }
                }
            }

            static void emit_simple_drop_states_for_rotation(bb::BBState const &rotation_state,
                                                             std::array<map_t, kMaxR> const &usable_arr,
                                                             std::vector<bb::CellsKey> &emitted_keys,
                                                             std::vector<bb::BBState> &sunk_states)
            {
                auto try_emit = [&](bb::BBState const &s)
                {
                    auto sunk = Helpers::drop_bb_state(s, usable_arr);
                    if (!sunk)
                        return;
                    bb::CellsKey k = Helpers::cells_key_for_state(*sunk);
                    for (bb::CellsKey ek : emitted_keys)
                    {
                        if (ek == k)
                            return;
                    }
                    emitted_keys.push_back(k);
                    sunk_states.push_back(*sunk);
                };
                try_emit(rotation_state);
                bb::BBState scan = rotation_state;
                while (true)
                {
                    scan.xb = static_cast<std::int8_t>(scan.xb - 1);
                    if (!Helpers::usable_at_bb(scan.r, scan.xb, scan.yb, usable_arr))
                        break;
                    try_emit(scan);
                }
                scan = rotation_state;
                while (true)
                {
                    scan.xb = static_cast<std::int8_t>(scan.xb + 1);
                    if (!Helpers::usable_at_bb(scan.r, scan.xb, scan.yb, usable_arr))
                        break;
                    try_emit(scan);
                }
            }

            //=== rotation 上 drop + 横向 L/R 各步 drop, cells_key 去重 ===========
            static void emit_simple_drops_for_rotation(Context &ctx,
                                                       bb::BBState const &rotation_state,
                                                       std::array<map_t, kMaxR> const &usable_arr,
                                                       std::vector<bb::CellsKey> &emitted_keys)
            {
                auto try_emit = [&](bb::BBState const &s)
                {
                    auto sunk = Helpers::drop_bb_state(s, usable_arr);
                    if (!sunk)
                        return;
                    bb::CellsKey k = Helpers::cells_key_for_state(*sunk);
                    for (bb::CellsKey ek : emitted_keys)
                    {
                        if (ek == k)
                            return;
                    }
                    emitted_keys.push_back(k);
                    LandPoint lp;
                    lp.state = *sunk;
                    ctx.land_point_cache_.push_back(lp);
                };
                try_emit(rotation_state);
                bb::BBState scan = rotation_state;
                while (true)
                {
                    scan.xb = static_cast<std::int8_t>(scan.xb - 1);
                    if (!Helpers::usable_at_bb(scan.r, scan.xb, scan.yb, usable_arr))
                        break;
                    try_emit(scan);
                }
                scan = rotation_state;
                while (true)
                {
                    scan.xb = static_cast<std::int8_t>(scan.xb + 1);
                    if (!Helpers::usable_at_bb(scan.r, scan.xb, scan.yb, usable_arr))
                        break;
                    try_emit(scan);
                }
            }

            //=== BBState (T, R, xb, yb) → master x 反推 ========================
            //  公式: mx = xb - origin.x, 与 status_to_bbox_TR 反向公式一致.
            template<char T, std::uint8_t R>
            static void current_master_x_TR(bb::BBState const &s, int &out, bool &ok)
            {
                constexpr auto orig = shape::piece_cells<RuleSpec, T, R>.origin;
                out = static_cast<int>(s.xb) - static_cast<int>(orig.x);
                ok = true;
            }

            template<char T>
            static void current_master_x_T(bb::BBState const &s, int &out, bool &ok)
            {
                std::uint8_t cur_r = s.r;
                [&]<std::size_t... Rs>(std::index_sequence<Rs...>)
                {
                    ((Rs == cur_r
                          ? (current_master_x_TR<T, static_cast<std::uint8_t>(Rs)>(s, out, ok), true)
                          : false) ||
                     ...);
                }(std::make_index_sequence<Helpers::template rcount_v<T>()>{});
            }

            template<char T, std::uint8_t R>
            static void current_master_y_TR(bb::BBState const &s, int &out, bool &ok)
            {
                constexpr auto orig = shape::piece_cells<RuleSpec, T, R>.origin;
                out = static_cast<int>(s.yb) + static_cast<int>(orig.y);
                ok = true;
            }

            template<char T>
            static void current_master_y_T(bb::BBState const &s, int &out, bool &ok)
            {
                std::uint8_t cur_r = s.r;
                [&]<std::size_t... Rs>(std::index_sequence<Rs...>)
                {
                    ((Rs == cur_r
                          ? (current_master_y_TR<T, static_cast<std::uint8_t>(Rs)>(s, out, ok), true)
                          : false) ||
                     ...);
                }(std::make_index_sequence<Helpers::template rcount_v<T>()>{});
            }

            static int current_master_y(bb::BBState const &s, char piece_t)
            {
                int my_out = 0;
                bool ok = false;
                [&]<std::size_t... Is>(std::index_sequence<Is...>)
                {
                    (((!ok && piece_info_t::type_at(Is) == piece_t)
                          ? (current_master_y_T<piece_info_t::type_at(Is)>(s, my_out, ok), true)
                          : false),
                     ...);
                }(std::make_index_sequence<kPieceCount>{});
                return my_out;
            }

            static int current_master_x(bb::BBState const &s, char piece_t)
            {
                int mx_out = 0;
                bool ok = false;
                [&]<std::size_t... Is>(std::index_sequence<Is...>)
                {
                    (((!ok && piece_info_t::type_at(Is) == piece_t)
                          ? (current_master_x_T<piece_info_t::type_at(Is)>(s, mx_out, ok), true)
                          : false),
                     ...);
                }(std::make_index_sequence<kPieceCount>{});
                return mx_out;
            }
        };
    } // namespace movegen

    template<class SpinHook, class RuleSpec, class Policy = void>
    using SimpleStrategy = movegen::SimpleStrategy<SpinHook, RuleSpec, Policy>;
} // namespace m_tetris2

//=== Search tag: TetrisEngine 第三模板参数的"位板版"用法 ===================
// 与 oracle 的 search_simple::Search 同形 — TetrisEngine<Rule, AI,
// simple::Search> 单参数即可装配. SimpleStrategy 当前唯一搭配的 hook 是
// NoSpinHook, 这里仅留 enable 占位字段, 默认/自定义 Config 都解析为同一
// 套 hook 选型, 与历史调用面 byte-equivalent.
//commit (seven-namespace): namespace simple 从 m_tetris2::simple 提到顶层 simple,
//  与 path / aspin / tspin / cautious / simulate / tag 形态统一; 与 oracle
//  search_simple_oracle / 客户端 simple::Search 调用面对称.
namespace simple
{
    struct DefaultConfig
    {
    };

    template<class Config = DefaultConfig>
    struct SearchWith
    {
        template<class RuleType>
        using type = m_tetris2::movegen::Searcher<
            m_tetris2::SimpleStrategy,
            m_tetris2::NoSpinHook,
            typename RuleType::rule_spec,
            typename m_tetris2::detail::policy_or<Config>::type>;

        template<class NewPolicy>
        struct PolicyOverrideConfig : Config { using PolicyType = NewPolicy; };

        template<class NewPolicy>
        using rebind_policy = SearchWith<PolicyOverrideConfig<NewPolicy>>;
    };

    using Search = SearchWith<>;
} // namespace simple

#endif // TETRIS_AI_RUNNER_SEARCH_SIMPLE_H_
