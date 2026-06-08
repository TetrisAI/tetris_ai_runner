#ifndef TETRIS_AI_RUNNER_SEARCH_SIMULATE_H_
#define TETRIS_AI_RUNNER_SEARCH_SIMULATE_H_

//==========================================================================
// SimulateStrategy<SpinHook, RuleSpec>: 位板 simulate-style search strategy.
//
// 与旧 MoveGenSearch<RuleSpec, SimulateNoSpinHook> 在 search()/make_path()
// 上的行为完全等价. 与 PathStrategy 的关键差异 (源自 oracle search_simulate
// 与 search_path 在 make_path 上的两点行为差):
//
//   1. 入口快路径用 sunk-起点 cells_key == cells_key(land_point) 判定空 path
//      (与 oracle search_simulate.cpp:18 的 node->drop(map)->index_filtered
//       == land_point->index_filtered 严格等价).
//   2. 1g 邻居集 = x z c l r L R d D — 没有 rotate_move 的二级 X/Z/C, 也
//      不消费 disable_d 阶段; 旋转邻居用 rotate_no_kick_bb (无 wall-kick).
//   3. 20g 邻居集 = x z c l r L R, 入队前**不** drop, 命中谓词把 child sink
//      到地面再比 cells_key (与 oracle search_simulate.cpp:51-150 的 lazy
//      drop 严格对应).
//
// search() 主体仍委托位板 MoveGen<RuleSpec, T, SpinHook>::generate (与
// PathStrategy 同形, 因 SpinHook=NoSpinHook 时 active_for_piece 全 false,
// 不写 spin 字段, 只 push_back 节点).
//
// 1g make_path 复用公共组件 detail::common::MakePath1gDedup (Path / Simulate
// 共享); 20g make_path 用本地 SimulateMakePath20gDedup (set 二态, 与 path
// 20g 同形, 但不带"set + usable 拆分", 因为 simulate 20g neighbor 入队前
// **不** drop, dedup 仅看 PathMark version).
//==========================================================================

#include "bb_bfs_engine.h"
#include "bb_state.h"
#include "movegen_context.h"
#include "movegen_hook.h"
#include "movegen_searcher.h"
#include "movegen_strategy.h"
#include "tetris_core.h"
#include "tetris_movegen.h"
#include "tetris_rule_spec.h"
#include "tetris_shape.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <type_traits>
#include <vector>

namespace m_tetris2
{
    namespace movegen
    {
        //=== Simulate strategy 私有 mixin: SpinHook 依赖字段 ===================
        // 与 detail::path::ExtrasMixin 同形, 但 simulate 通常用 NoSpin Hook,
        // hook_state 是空 struct, [[no_unique_address]] 让其 0 字节占位.
        // 持: TetrisContext / Config / LandPoint cache / hook_state.
        //commit 3: 落到 detail::simulate:: 子命名空间, 标注 "SimulateStrategy
        //  私有实现细节, 第三方扩展不应直接消费".
        namespace detail
        {
            namespace simulate
            {
                template<class SpinHook, class RuleSpec>
                struct ExtrasMixin
                {
                    using Config = typename SpinHook::Config;
                    using LandPoint = typename SpinHook::LandPoint;

                    Config const *config_ = nullptr;
                    std::vector<LandPoint> land_point_cache_{};
                };
            } // namespace simulate
        } // namespace detail

        template<class SpinHook, class RuleSpec, class Policy = void>
        struct SimulateStrategy
        {
        private:
            using Helpers = bb::Helpers<RuleSpec>;
            using map_t = typename Helpers::map_t;
            using PathMark = typename Helpers::PathMark;
            using piece_info_t = typename Helpers::piece_info_t;

            static constexpr int kW = Helpers::kW;
            static constexpr int kH = Helpers::kH;
            static constexpr int kMaxR = Helpers::kMaxR;
            static constexpr int kPieceCount = Helpers::kPieceCount;

        public:
            using LandPoint = typename SpinHook::LandPoint;
            using Config = typename SpinHook::Config;

            // simulate 不用 StateNodeLutMixin: BBState 直接通过 build_state_from_master
            // 构造, 不需要 BBState->TetrisNode 反查.
            using Context = MoveGenContext<RuleSpec,
                                           BfsQueueMixin<RuleSpec>,
                                           detail::simulate::ExtrasMixin<SpinHook, RuleSpec>>;

            //=== init ==========================================================
            static void init(Context &ctx, Config const *config)
            {
                ctx.config_ = config;
            }

            //=== search ========================================================
            // simulate 当前唯一消费者是 NoSpinHook (active_for_piece 全 false,
            // 不写 spin/last 元数据), search() 仅调 MoveGen<>::generate 把
            // landings 转 LandPoint 入 cache; 不需要 20g 专属位板 BFS (即使
            // SimulateNoSpinHook 的 config_is_20g 恒 false, 也保留这条 dispatch
            // 让自定义 hook 自由扩展).
            static std::vector<LandPoint> const *
            search(Context &ctx, TetrisMap const &map, bb::BBState const &spawn, std::size_t depth)
            {
                ctx.land_point_cache_.clear();
                map_t board = Helpers::build_board(map);
                char t = static_cast<char>(spawn.t);
                if (piece_info_t::index_of(t) < 0)
                {
                    assert(false && "SimulateStrategy::search: piece type outside RuleSpec");
                    return &ctx.land_point_cache_;
                }
                bool dispatched = false;
                [&]<std::size_t... Is>(std::index_sequence<Is...>)
                {
                    (((!dispatched && piece_info_t::type_at(Is) == t)
                          ? (run_piece<piece_info_t::type_at(Is)>(ctx, board, depth), dispatched = true)
                          : false),
                     ...);
                }(std::make_index_sequence<kPieceCount>{});
                if (!dispatched)
                {
                    assert(false && "SimulateStrategy::search: piece type outside RuleSpec");
                }
                return &ctx.land_point_cache_;
            }

            //=== search_eval ===================================================
            // Phase 2 推式接口：与 PathStrategy::search_eval 同形，参见其注释。
            //==================================================================
            template<class EvalCallback>
            static void search_eval(Context &ctx, Map<RuleSpec::width, RuleSpec::height> const &board,
                                    bb::BBState const &spawn, std::size_t depth,
                                    EvalCallback &on_land)
            {
                char t = static_cast<char>(spawn.t);
                bool dispatched = false;
                [&]<std::size_t... Is>(std::index_sequence<Is...>)
                {
                    (((!dispatched && piece_info_t::type_at(Is) == t)
                          ? (run_piece<piece_info_t::type_at(Is), EvalCallback>(
                                 ctx, board, depth, &on_land),
                             dispatched = true)
                          : false),
                     ...);
                }(std::make_index_sequence<kPieceCount>{});
                if (!dispatched)
                {
                    assert(false && "SimulateStrategy::search_eval: piece type outside RuleSpec");
                }
            }

            //=== make_path =====================================================
            static std::vector<char>
            make_path(Context &ctx, bb::BBState const &spawn,
                      LandPoint const &land_point,
                      Map<RuleSpec::width, RuleSpec::height> const &board)
            {
                if (SpinHook::config_is_20g(ctx.config_))
                {
                    return make_path_simulate_20g_native(ctx, spawn, land_point, board);
                }
                return make_path_simulate_1g_native(ctx, spawn, land_point, board);
            }

        private:
            //=== run_piece (与 PathStrategy::run_piece 同形, 不写 spin 元数据) ===
            //
            // EvalCallback（可选）：与 PathStrategy::run_piece 同形，参见其注释。
            template<char T, class EvalCallback = void>
            static void run_piece(Context &ctx, map_t const &board, std::size_t depth,
                                  EvalCallback *on_land = nullptr)
            {
                constexpr bool EnableMini = []() constexpr {
                    if constexpr (requires { SpinHook::template active_for_piece<T, Policy>; })
                        return SpinHook::template active_for_piece<T, Policy>;
                    else
                        return SpinHook::template active_for_piece<T>;
                }();
                std::array<map_t, kMaxR> usable_arr{};
                auto collect = [&](LandingPosT<SpinHook> lp)
                {
                    //commit (remove-context): 与 search_path::run_piece 同构, 不再走
                    //  ctx.context_->get(status), 改 BBState + state_to_node.
                    bb::BBState st_bb{static_cast<std::uint8_t>(T), lp.r, 0, 0};
                    bool emitted = false;
                    [&]<std::size_t... Rs>(std::index_sequence<Rs...>)
                    {
                        ((Rs == lp.r ? ([&]() {
                            st_bb = Helpers::template build_state_from_master<
                                T, static_cast<std::uint8_t>(Rs)>(lp.x, lp.y);
                            LandPoint node_ex{};
                            node_ex.state = st_bb;
                            if constexpr (EnableMini)
                            {
                                // 与 PathStrategy::run_piece 同形，last 直接用 BBState 透传。
                                bb::BBState const *last_state_ptr = nullptr;
                                bb::BBState last_bb{};
                                if constexpr (SpinHook::resolves_last_1g)
                                {
                                    if (SpinHook::payload_has_last_rot(lp.extra))
                                    {
                                        std::uint8_t lr = SpinHook::payload_last_r(lp.extra);
                                        int lx = SpinHook::payload_last_x(lp.extra);
                                        int ly = SpinHook::payload_last_y(lp.extra);
                                        last_bb = {static_cast<std::uint8_t>(T), lr, 0, 0};
                                        [&]<std::size_t... Rs2>(std::index_sequence<Rs2...>)
                                        {
                                            ((Rs2 == lr
                                                  ? (last_bb = Helpers::template build_state_from_master<
                                                         T, static_cast<std::uint8_t>(Rs2)>(lx, ly),
                                                     true)
                                                  : false) ||
                                             ...);
                                        }(std::make_index_sequence<Helpers::template rcount_v<T>()>{});
                                        last_state_ptr = &last_bb;
                                    }
                                }
                                SpinHook::template apply_emit_1g<map_t, kMaxR>(
                                    node_ex, lp.extra, depth, ctx.config_, last_state_ptr,
                                    usable_arr, lp.r, /*xb*/ -1, /*yb*/ -1);
                            }
                            else
                            {
                                (void)depth;
                            }
                            // Phase 2: 编译期 R 已知，直接触发 EvalCallback，跳过 push_back。
                            // TODO(multi-thread): 多线程路径在此构造 PendingTask，参见 search_path.h。
                            if constexpr (!std::is_void_v<EvalCallback>)
                            {
                                on_land->template operator()<T, static_cast<std::uint8_t>(Rs)>(node_ex);
                            }
                            else
                            {
                                ctx.land_point_cache_.push_back(node_ex);
                            }
                            emitted = true;
                        }(), true) : false) || ...);
                    }(std::make_index_sequence<Helpers::template rcount_v<T>()>{});
                    (void)emitted;
                };
                constexpr auto sp_run = RuleSpec::spawn(T, kW, kH);
                movegen::MoveGen<RuleSpec, T, SpinHook>::generate(
                    board, sp_run.first, sp_run.second, collect);
            }

            //==================================================================
            //=== Simulate 1g 三件套 ===========================================
            //==================================================================
            // 1g make_path dedup 复用 detail::common::MakePath1gDedup (set + usable
            // 三态), 与 oracle "node_mark_.set + child->check" 完全等价.
            using MakePath1gDedup = detail::common::MakePath1gDedup<RuleSpec>;

            // Simulate1gNeighbors: 邻居顺序与 oracle search_simulate.cpp:158-285
            // (1g 分支) 严格一致: x → z → c → l → r → L → R → d → D (D 嵌套
            // 在 d). 入队前不 drop. 旋转邻居用 rotate_no_kick_bb (无 kick).
            struct Simulate1gNeighbors
            {
                char piece_t;
                std::array<map_t, kMaxR> const *usable_arr;

                template<class Emit>
                void expand(bb::BBState const &cur, Emit emit)
                {
                    {
                        auto wk = Helpers::rotate_no_kick_bb(piece_t, bb::KickDir::Opp, cur);
                        if (wk)
                            emit(*wk, 'x');
                    }
                    {
                        auto wk = Helpers::rotate_no_kick_bb(piece_t, bb::KickDir::Ccw, cur);
                        if (wk)
                            emit(*wk, 'z');
                    }
                    {
                        auto wk = Helpers::rotate_no_kick_bb(piece_t, bb::KickDir::Cw, cur);
                        if (wk)
                            emit(*wk, 'c');
                    }
                    {
                        bb::BBState nL = cur;
                        nL.xb = static_cast<std::int8_t>(nL.xb - 1);
                        emit(nL, 'l');
                    }
                    {
                        bb::BBState nR = cur;
                        nR.xb = static_cast<std::int8_t>(nR.xb + 1);
                        emit(nR, 'r');
                    }
                    {
                        bb::BBState nL_state = cur;
                        nL_state.xb = static_cast<std::int8_t>(nL_state.xb - 1);
                        if (Helpers::usable_at_bb(nL_state.r, nL_state.xb, nL_state.yb, *usable_arr))
                        {
                            while (true)
                            {
                                bb::BBState try_state = nL_state;
                                try_state.xb = static_cast<std::int8_t>(try_state.xb - 1);
                                if (!Helpers::usable_at_bb(try_state.r, try_state.xb, try_state.yb, *usable_arr))
                                    break;
                                nL_state = try_state;
                            }
                            emit(nL_state, 'L');
                        }
                    }
                    {
                        bb::BBState nR_state = cur;
                        nR_state.xb = static_cast<std::int8_t>(nR_state.xb + 1);
                        if (Helpers::usable_at_bb(nR_state.r, nR_state.xb, nR_state.yb, *usable_arr))
                        {
                            while (true)
                            {
                                bb::BBState try_state = nR_state;
                                try_state.xb = static_cast<std::int8_t>(try_state.xb + 1);
                                if (!Helpers::usable_at_bb(try_state.r, try_state.xb, try_state.yb, *usable_arr))
                                    break;
                                nR_state = try_state;
                            }
                            emit(nR_state, 'R');
                        }
                    }
                    {
                        bb::BBState nd = cur;
                        nd.yb = static_cast<std::int8_t>(nd.yb - 1);
                        bb::EnqueueDecision dd = emit(nd, 'd');
                        if (dd != bb::EnqueueDecision::Skip)
                        {
                            auto D = Helpers::drop_bb_state(cur, *usable_arr);
                            if (D)
                                emit(*D, 'D');
                        }
                    }
                }
            };

            // Simulate1gVisitor: 命中谓词 = cells_key(child) == index_landpoint.
            struct Simulate1gVisitor
            {
                bb::CellsKey index_landpoint;
                bb::BBState found{};
                bool hit = false;

                bool on_pop(bb::BBState const &)
                {
                    return true;
                }

                bool on_admit(bb::BBState const &s, char, bb::EnqueueDecision)
                {
                    bb::CellsKey k = Helpers::cells_key_for_state(s);
                    if (k == index_landpoint)
                    {
                        found = s;
                        hit = true;
                        return false;
                    }
                    return true;
                }
            };

            //==================================================================
            //=== Simulate 20g 三件套 ==========================================
            //==================================================================
            // Simulate20gNeighbors: 邻居顺序与 oracle search_simulate.cpp:48-150
            // 一致 — x → z → c → l → r → L → R, 没有 d/D. 入队前不 drop.
            // 旋转邻居用 rotate_no_kick_bb. L/R 多步与 1g 同形.
            struct Simulate20gNeighbors
            {
                char piece_t;
                std::array<map_t, kMaxR> const *usable_arr;

                template<class Emit>
                void expand(bb::BBState const &cur, Emit emit)
                {
                    {
                        auto wk = Helpers::rotate_no_kick_bb(piece_t, bb::KickDir::Opp, cur);
                        if (wk)
                            emit(*wk, 'x');
                    }
                    {
                        auto wk = Helpers::rotate_no_kick_bb(piece_t, bb::KickDir::Ccw, cur);
                        if (wk)
                            emit(*wk, 'z');
                    }
                    {
                        auto wk = Helpers::rotate_no_kick_bb(piece_t, bb::KickDir::Cw, cur);
                        if (wk)
                            emit(*wk, 'c');
                    }
                    {
                        bb::BBState nL = cur;
                        nL.xb = static_cast<std::int8_t>(nL.xb - 1);
                        emit(nL, 'l');
                    }
                    {
                        bb::BBState nR = cur;
                        nR.xb = static_cast<std::int8_t>(nR.xb + 1);
                        emit(nR, 'r');
                    }
                    {
                        bb::BBState nL_state = cur;
                        nL_state.xb = static_cast<std::int8_t>(nL_state.xb - 1);
                        if (Helpers::usable_at_bb(nL_state.r, nL_state.xb, nL_state.yb, *usable_arr))
                        {
                            while (true)
                            {
                                bb::BBState try_state = nL_state;
                                try_state.xb = static_cast<std::int8_t>(try_state.xb - 1);
                                if (!Helpers::usable_at_bb(try_state.r, try_state.xb, try_state.yb, *usable_arr))
                                    break;
                                nL_state = try_state;
                            }
                            emit(nL_state, 'L');
                        }
                    }
                    {
                        bb::BBState nR_state = cur;
                        nR_state.xb = static_cast<std::int8_t>(nR_state.xb + 1);
                        if (Helpers::usable_at_bb(nR_state.r, nR_state.xb, nR_state.yb, *usable_arr))
                        {
                            while (true)
                            {
                                bb::BBState try_state = nR_state;
                                try_state.xb = static_cast<std::int8_t>(try_state.xb + 1);
                                if (!Helpers::usable_at_bb(try_state.r, try_state.xb, try_state.yb, *usable_arr))
                                    break;
                                nR_state = try_state;
                            }
                            emit(nR_state, 'R');
                        }
                    }
                }
            };

            // Simulate20gVisitor: 命中谓词需要 lazy drop —
            // oracle: child->drop(map)->index_filtered == index, 等价位板侧
            // cells_key_for_state(*drop_bb_state(child, usable)) == index_landpoint.
            struct Simulate20gVisitor
            {
                bb::CellsKey index_landpoint;
                std::array<map_t, kMaxR> const *usable_arr;
                bb::BBState found{};
                bool hit = false;

                bool on_pop(bb::BBState const &)
                {
                    return true;
                }

                bool on_admit(bb::BBState const &s, char, bb::EnqueueDecision)
                {
                    auto sunk = Helpers::drop_bb_state(s, *usable_arr);
                    if (!sunk)
                        return true;
                    bb::CellsKey k = Helpers::cells_key_for_state(*sunk);
                    if (k == index_landpoint)
                    {
                        found = s;
                        hit = true;
                        return false;
                    }
                    return true;
                }
            };

            //==================================================================
            //=== Simulate 1g / 20g 入口 =======================================
            //==================================================================
            static std::vector<char>
            make_path_simulate_1g_native(Context &ctx, bb::BBState const &spawn,
                                         LandPoint const &land_point,
                                         Map<RuleSpec::width, RuleSpec::height> const &board)
            {
                char piece_t = static_cast<char>(spawn.t);
                if (piece_info_t::index_of(piece_t) < 0)
                {
                    assert(false && "SimulateStrategy::make_path_simulate_1g_native: piece type outside RuleSpec");
                    return std::vector<char>();
                }
                std::array<map_t, kMaxR> usable_arr{};
                Helpers::build_usable_for_piece(piece_t, board, usable_arr);
                bb::CellsKey index_landpoint = Helpers::cells_key_for_state(land_point.state);
                bb::BBState entry_state = spawn;
                {
                    auto sunk_start = Helpers::drop_bb_state(entry_state, usable_arr);
                    if (sunk_start && Helpers::cells_key_for_state(*sunk_start) == index_landpoint)
                        return std::vector<char>();
                }
                //commit B (PathMark 栈化): ctx.path_mark_ 字段已移除, 改栈分配; 默认构造即空表.
                PathMark pm;
                auto build_path = [&pm](bb::BBState const &cur_state) -> std::vector<char>
                {
                    std::vector<char> path;
                    int r = static_cast<int>(cur_state.r);
                    int xb = static_cast<int>(cur_state.xb);
                    int yb = static_cast<int>(cur_state.yb);
                    while (true)
                    {
                        auto result = pm.get_bbox(r, xb, yb);
                        //prev == cur 即起点 (self prev 协议), 终止回溯.
                        if (static_cast<int>(result.first.r) == r &&
                            static_cast<int>(result.first.xb) == xb &&
                            static_cast<int>(result.first.yb) == yb)
                            break;
                        path.push_back(result.second);
                        r = static_cast<int>(result.first.r);
                        xb = static_cast<int>(result.first.xb);
                        yb = static_cast<int>(result.first.yb);
                    }
                    std::reverse(path.begin(), path.end());
                    return path;
                };
                if (Helpers::cells_key_for_state(entry_state) == index_landpoint)
                {
                    path_mark_set_state_root(pm, entry_state, '\0');
                    return build_path(entry_state);
                }
                Simulate1gNeighbors neighbors{piece_t, &usable_arr};
                MakePath1gDedup dedup{&pm, &usable_arr};
                Simulate1gVisitor visitor{index_landpoint, bb::BBState{}, false};
                ctx.node_search_path_.clear();
                bb::run_bb_bfs(entry_state, neighbors, dedup, visitor, ctx.node_search_path_);
                if (visitor.hit)
                    return build_path(visitor.found);
                return std::vector<char>();
            }

            static std::vector<char>
            make_path_simulate_20g_native(Context &ctx, bb::BBState const &spawn,
                                          LandPoint const &land_point,
                                          Map<RuleSpec::width, RuleSpec::height> const &board)
            {
                char piece_t = static_cast<char>(spawn.t);
                if (piece_info_t::index_of(piece_t) < 0)
                {
                    assert(false && "SimulateStrategy::make_path_simulate_20g_native: piece type outside RuleSpec");
                    return std::vector<char>();
                }
                std::array<map_t, kMaxR> usable_arr{};
                Helpers::build_usable_for_piece(piece_t, board, usable_arr);
                bb::CellsKey index_landpoint = Helpers::cells_key_for_state(land_point.state);
                bb::BBState entry_state = spawn;
                {
                    auto sunk_start = Helpers::drop_bb_state(entry_state, usable_arr);
                    if (sunk_start && Helpers::cells_key_for_state(*sunk_start) == index_landpoint)
                        return std::vector<char>();
                }
                //commit B (PathMark 栈化): ctx.path_mark_ 字段已移除, 改栈分配; 默认构造即空表.
                PathMark pm;
                auto build_path = [&pm](bb::BBState const &cur_state) -> std::vector<char>
                {
                    std::vector<char> path;
                    int r = static_cast<int>(cur_state.r);
                    int xb = static_cast<int>(cur_state.xb);
                    int yb = static_cast<int>(cur_state.yb);
                    while (true)
                    {
                        auto result = pm.get_bbox(r, xb, yb);
                        //prev == cur 即起点 (self prev 协议), 终止回溯.
                        if (static_cast<int>(result.first.r) == r &&
                            static_cast<int>(result.first.xb) == xb &&
                            static_cast<int>(result.first.yb) == yb)
                            break;
                        path.push_back(result.second);
                        r = static_cast<int>(result.first.r);
                        xb = static_cast<int>(result.first.xb);
                        yb = static_cast<int>(result.first.yb);
                    }
                    std::reverse(path.begin(), path.end());
                    return path;
                };
                Simulate20gNeighbors neighbors{piece_t, &usable_arr};
                MakePath1gDedup dedup{&pm, &usable_arr};
                Simulate20gVisitor visitor{index_landpoint, &usable_arr, bb::BBState{}, false};
                ctx.node_search_path_.clear();
                bb::run_bb_bfs(entry_state, neighbors, dedup, visitor, ctx.node_search_path_);
                if (visitor.hit)
                    return build_path(visitor.found);
                return std::vector<char>();
            }

            //=== 起点 root mark 写入 ============================================
            //commit B: 接受栈分配的 PathMark 引用, 不再依赖 ctx.path_mark_.
            static bool path_mark_set_state_root(PathMark &pm, bb::BBState const &child, char op)
            {
                if (child.xb < 0 || child.xb >= kW || child.yb < 0 || child.yb >= kH)
                    return false;
                //起点协议: prev = self.
                typename PathMark::PrevKey pk{static_cast<std::uint8_t>(child.r),
                                              static_cast<std::int8_t>(child.xb),
                                              static_cast<std::int8_t>(child.yb)};
                return pm.set_bbox(static_cast<int>(child.r),
                                   static_cast<int>(child.xb),
                                   static_cast<int>(child.yb),
                                   pk,
                                   op);
            }
        };
    } // namespace movegen

    template<class SpinHook, class RuleSpec, class Policy = void>
    using SimulateStrategy = movegen::SimulateStrategy<SpinHook, RuleSpec, Policy>;
} // namespace m_tetris2

//=== Search tag: TetrisEngine 第三模板参数的"位板版"用法 ===================
// 与 oracle 的 search_simulate::Search 同形 — TetrisEngine<Rule, AI,
// simulate::Search> 单参数即可装配. SimulateStrategy 当前唯一搭配的 hook 是
// NoSpinHook (active_for_piece 全 false), 这里 Config trait 仅保留 enable
// 占位以与 path::SearchWith 形态一致.
//commit (seven-namespace): namespace simulate 从 m_tetris2::simulate 提到顶层
//  simulate, 与 path / aspin / tspin / cautious / simple / tag 形态统一.
namespace simulate
{
    struct DefaultConfig
    {
    };

    template<class Config = DefaultConfig>
    struct SearchWith
    {
        template<class RuleType>
        using type = m_tetris2::movegen::Searcher<
            m_tetris2::SimulateStrategy,
            m_tetris2::NoSpinHook,
            typename RuleType::rule_spec,
            typename m_tetris2::detail::policy_or<Config>::type>;

        template<class NewPolicy>
        struct PolicyOverrideConfig : Config { using PolicyType = NewPolicy; };

        template<class NewPolicy>
        using rebind_policy = SearchWith<PolicyOverrideConfig<NewPolicy>>;
    };

    using Search = SearchWith<>;
} // namespace simulate

#endif // TETRIS_AI_RUNNER_SEARCH_SIMULATE_H_
