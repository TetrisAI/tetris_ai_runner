#ifndef TETRIS_AI_RUNNER_SEARCH_PATH_H_
#define TETRIS_AI_RUNNER_SEARCH_PATH_H_

//==========================================================================
// PathStrategy<SpinHook, RuleSpec>: 位板 path search strategy.
//
// 与旧 MoveGenSearch<RuleSpec, Hook> 在 PathHook (NoSpinHook) / TSpinHook /
// ASpinHook / CautiousHook 下的行为完全一致:
//   - search(): 1g 路径走 MoveGen<RuleSpec, T, Hook>::generate, 每个 piece
//     按 RuleSpec::ops 分发; 20g 路径走 run_piece_20g 位板 BFS, 同形分发.
//   - make_path(): 1g 走 BFS + PathMark + 末段 wall-kick 重放; 20g 邻居入队
//     时已 drop, 末段 wall-kick + drop + cells_key 比对.
//
// 封装 1g / 20g 路径搜索与 make_path 所需的静态算法组件。
//==========================================================================

#include "movegen_bfs.h"
#include "movegen_state.h"
#include "integer_utils.h"
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
        //=== Path strategy 私有 mixin: SpinHook 依赖字段 =======================
        // 持: TetrisContext / Config / LandPoint cache.
        // PathStrategy 私有 mixin，保存搜索配置与落点缓存。
        namespace detail
        {
            namespace path
            {
                template<class SpinHook, class RuleSpec>
                struct ExtrasMixin
                {
                    using Config = typename SpinHook::Config;
                    using LandPoint = typename SpinHook::LandPoint;

                    Config const *config_ = nullptr;
                    std::vector<LandPoint> land_point_cache_{};
                };
            } // namespace path
        } // namespace detail

        template<class SpinHook, class RuleSpec, class Policy = void>
        struct PathStrategy
        {
        private:
            using Helpers = m_tetris2::Helpers<RuleSpec>;
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

            using Context = MoveGenContext<RuleSpec,
                                           BfsQueueMixin<RuleSpec>,
                                           detail::path::ExtrasMixin<SpinHook, RuleSpec>>;

            //=== init ==========================================================
            static void init(Context &ctx, Config const *config)
            {
                ctx.config_ = config;
            }

            //=== search (State spawn 主重载) =================================
            // 不依赖 oracle 指针图。spawn 合法性（是否被场地 blocked）由
            // MoveGen::generate 内部处理：若 spawn 位置被占用，generate 直接
            // 返回而不调 collect，land_point_cache_ 保持空，与原 node->check()
            // 提前 return 语义等价，无需在此重复检查。
            template<class MapIn>
            static std::vector<LandPoint> const *
            search(Context &ctx, MapIn const &map, State const &spawn, std::size_t depth)
            {
                ctx.land_point_cache_.clear();
                if (spawn.t == 0)
                {
                    return &ctx.land_point_cache_;
                }
                map_t board = Helpers::build_board(map);
                if (SpinHook::config_is_20g(ctx.config_))
                {
                    char t20 = static_cast<char>(spawn.t);
                    bool dispatched = false;
                    [&]<std::size_t... Is>(std::index_sequence<Is...>)
                    {
                        (((!dispatched && piece_info_t::type_at(Is) == t20)
                              ? (run_piece_20g<piece_info_t::type_at(Is)>(ctx, board, spawn, depth), dispatched = true)
                              : false),
                         ...);
                    }(std::make_index_sequence<kPieceCount>{});
                    if (!dispatched)
                    {
                        assert(false && "PathStrategy::search: piece type outside RuleSpec in 20g path");
                    }
                    return &ctx.land_point_cache_;
                }
                char t = static_cast<char>(spawn.t);
                bool dispatched = false;
                [&]<std::size_t... Is>(std::index_sequence<Is...>)
                {
                    (((!dispatched && piece_info_t::type_at(Is) == t)
                          ? (run_piece<piece_info_t::type_at(Is)>(ctx, board, spawn, depth), dispatched = true)
                          : false),
                     ...);
                }(std::make_index_sequence<kPieceCount>{});
                if (!dispatched)
                {
                    assert(false && "PathStrategy::search: piece type outside RuleSpec in non-20g path");
                }
                return &ctx.land_point_cache_;
            }

            //=== search_eval ===================================================
            // 直接把落点回调给 on_land，并在编译期展开 <T, R>。
            //==================================================================
            template<class EvalCallback>
            static void search_eval(Context &ctx, TetrisMap<RuleSpec::width, RuleSpec::height> const &board,
                                    State const &spawn, std::size_t depth,
                                    EvalCallback &on_land)
            {
                if (SpinHook::config_is_20g(ctx.config_))
                {
                    char t20 = static_cast<char>(spawn.t);
                    bool dispatched = false;
                    [&]<std::size_t... Is>(std::index_sequence<Is...>)
                    {
                        (((!dispatched && piece_info_t::type_at(Is) == t20)
                              ? (run_piece_20g<piece_info_t::type_at(Is), EvalCallback>(
                                     ctx, board, spawn, depth, &on_land),
                                 dispatched = true)
                              : false),
                         ...);
                    }(std::make_index_sequence<kPieceCount>{});
                    if (!dispatched)
                    {
                        assert(false && "PathStrategy::search_eval: piece type outside RuleSpec in 20g path");
                    }
                    return;
                }
                char t = static_cast<char>(spawn.t);
                bool dispatched = false;
                [&]<std::size_t... Is>(std::index_sequence<Is...>)
                {
                    (((!dispatched && piece_info_t::type_at(Is) == t)
                          ? (run_piece<piece_info_t::type_at(Is), EvalCallback>(
                                 ctx, board, spawn, depth, &on_land),
                             dispatched = true)
                          : false),
                     ...);
                }(std::make_index_sequence<kPieceCount>{});
                if (!dispatched)
                {
                    assert(false && "PathStrategy::search_eval: piece type outside RuleSpec in non-20g path");
                }
            }

            //=== make_path =====================================================
            static std::vector<char>
            make_path(Context &ctx, State const &spawn,
                      LandPoint const &land_point,
                      TetrisMap<RuleSpec::width, RuleSpec::height> const &board)
            {
                if (SpinHook::config_is_20g(ctx.config_))
                {
                    return make_path_20g_native(ctx, spawn, land_point, board);
                }
                return make_path_1g_native(ctx, spawn, land_point, board);
            }

        private:
            //==================================================================
            //=== run_piece (1g 路径, 走 MoveGen<RuleSpec, T, Hook>::generate) ==
            //
            // EvalCallback（可选）：若传入非 void 类型，则在产生落点时以编译期 <T,R>
            //   调用 on_land.template operator()<T, R>(node_ex)，跳过 push_back；
            //   传入 void（默认）时退化为原有 push_back 行为。
            //==================================================================
            template<char T, class EvalCallback = void>
            static void run_piece(Context &ctx, map_t const &board, State const &start, std::size_t depth,
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
                    State st_bb{static_cast<std::uint8_t>(T), lp.r, 0, 0};
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
                                // 把 (last_x, last_y, last_r) 折成 State 直接透传，
                                // 无需经过 state_to_node 查表。
                                State const *last_state_ptr = nullptr;
                                State last_bb{};
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
                            // 编译期旋转态已知时直接回调，否则写入落点缓存。
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
                movegen::MoveGen<RuleSpec, T, SpinHook>::generate(board, start, collect);
            }

            //==================================================================
            //=== run_piece_20g (20g 路径, 位板 BFS) ============================
            //==================================================================
            struct Run20gMarkSlot
            {
                std::uint8_t visited;
                std::uint8_t action;
                std::uint8_t parent_r;
                std::int8_t parent_xb;
                std::int8_t parent_yb;
            };

            template<char T, bool EnableT>
            struct Run20gDedup
            {
                std::vector<Run20gMarkSlot> *mark;

                EnqueueDecision try_admit(State const &s, State const *parent, char action)
                {
                    if (s.xb < 0 || s.xb >= kW || s.yb < 0 || s.yb >= kH || s.r >= kMaxR)
                        return EnqueueDecision::Skip;
                    std::size_t i = (static_cast<std::size_t>(s.r) * kH + static_cast<std::size_t>(s.yb)) * kW + static_cast<std::size_t>(s.xb);
                    Run20gMarkSlot &slot = (*mark)[i];
                    if (parent == nullptr)
                    {
                        slot.visited = 1;
                        slot.action = ' ';
                        slot.parent_r = 0xFFu;
                        slot.parent_xb = 0;
                        slot.parent_yb = 0;
                        return EnqueueDecision::MarkAndEnqueue;
                    }
                    bool is_kick = (action == 'x' || action == 'z' || action == 'c');
                    if (slot.visited == 0)
                    {
                        slot.visited = 1;
                        if constexpr (EnableT)
                        {
                            slot.action = static_cast<std::uint8_t>(action);
                            slot.parent_r = parent->r;
                            slot.parent_xb = parent->xb;
                            slot.parent_yb = parent->yb;
                        }
                        return EnqueueDecision::MarkAndEnqueue;
                    }
                    if (is_kick)
                    {
                        if constexpr (EnableT)
                        {
                            if (slot.action == ' ')
                            {
                                slot.action = static_cast<std::uint8_t>(action);
                                slot.parent_r = parent->r;
                                slot.parent_xb = parent->xb;
                                slot.parent_yb = parent->yb;
                                return EnqueueDecision::MarkOnly;
                            }
                        }
                    }
                    return EnqueueDecision::Skip;
                }
            };

            template<char T>
            struct Run20gNeighbors
            {
                std::array<map_t, kMaxR> const *usable_arr;
                bool allow_180;

                template<class Emit>
                void expand(State const &s, Emit emit)
                {
                    auto drop_res = Helpers::drop(s, *usable_arr);
                    if (!drop_res)
                        return;
                    State ss = *drop_res;
                    {
                        State n = ss;
                        n.yb = static_cast<std::int8_t>(n.yb - 1);
                        if (Helpers::usable_at(n.r, n.xb, n.yb, *usable_arr))
                            emit(n, ' ');
                    }
                    {
                        State n = ss;
                        n.xb = static_cast<std::int8_t>(n.xb - 1);
                        if (Helpers::usable_at(n.r, n.xb, n.yb, *usable_arr))
                            emit(n, ' ');
                    }
                    {
                        State n = ss;
                        n.xb = static_cast<std::int8_t>(n.xb + 1);
                        if (Helpers::usable_at(n.r, n.xb, n.yb, *usable_arr))
                            emit(n, ' ');
                    }
                    if (allow_180)
                    {
                        auto wk = Helpers::first_kick(T, KickDir::Opp, ss, *usable_arr);
                        if (wk)
                            emit(*wk, 'x');
                    }
                    {
                        auto wk = Helpers::first_kick(T, KickDir::Ccw, ss, *usable_arr);
                        if (wk)
                            emit(*wk, 'z');
                    }
                    {
                        auto wk = Helpers::first_kick(T, KickDir::Cw, ss, *usable_arr);
                        if (wk)
                            emit(*wk, 'c');
                    }
                }
            };

            template<char T, bool EnableT, class EvalCallback = void>
            struct Run20gVisitor
            {
                Context *ctx;
                std::array<map_t, kMaxR> const *usable_arr;
                std::vector<Run20gMarkSlot> *mark;
                std::vector<CellsKey> *emitted_keys;
                std::size_t depth;
                // 20g 路径按 RotState 中的位图结果补全落点元数据。
                using RotStateT = typename SpinHook::template RotState<map_t, kMaxR>;
                RotStateT const *rot_state;
                // 非缓存模式下保存 EvalCallback 指针，命中落点时直接回调。
                [[no_unique_address]] std::conditional_t<
                    std::is_void_v<EvalCallback>, std::monostate, EvalCallback *> on_land{};

                bool on_pop(State const &s)
                {
                    auto drop_res = Helpers::drop(s, *usable_arr);
                    if (!drop_res)
                        return true;
                    State ss = *drop_res;
                    if (Helpers::usable_at(ss.r, ss.xb, ss.yb - 1, *usable_arr))
                        return true;
                    CellsKey k = Helpers::cells_key_for_state(ss);
                    for (CellsKey ek : *emitted_keys)
                        if (ek == k)
                            return true;
                    emitted_keys->push_back(k);

                    LandPoint node_ex{};
                    node_ex.state = ss;
                    if constexpr (EnableT)
                    {
                        std::size_t mi = (static_cast<std::size_t>(ss.r) * kH + static_cast<std::size_t>(ss.yb)) * kW + static_cast<std::size_t>(ss.xb);
                        Run20gMarkSlot const &slot = (*mark)[mi];
                        State const *last_state_ptr = nullptr;
                        State pst{};
                        char action = ' ';
                        if (slot.visited && slot.parent_r != 0xFFu)
                        {
                            pst = {static_cast<std::uint8_t>(T),
                                   slot.parent_r,
                                   slot.parent_xb,
                                   slot.parent_yb};
                            last_state_ptr = &pst;
                            action = static_cast<char>(slot.action);
                        }
                        SpinHook::template apply_emit_20g<map_t, kMaxR>(
                            node_ex, last_state_ptr, action, depth, ctx->config_,
                            *rot_state, *usable_arr, ss.r, ss.xb, ss.yb);
                    }
                    // 20g 落点按运行时旋转态分发到对应的编译期回调。
                    if constexpr (!std::is_void_v<EvalCallback>)
                    {
                        [&]<std::size_t... Rs>(std::index_sequence<Rs...>)
                        {
                            ((Rs == ss.r
                                  ? (on_land->template operator()<T, static_cast<std::uint8_t>(Rs)>(node_ex),
                                     true)
                                  : false) ||
                             ...);
                        }(std::make_index_sequence<Helpers::template rcount_v<T>()>{});
                    }
                    else
                    {
                        ctx->land_point_cache_.push_back(node_ex);
                    }
                    return true;
                }

                bool on_admit(State const &, char, EnqueueDecision)
                {
                    return true;
                }
            };

            template<char T, class EvalCallback = void>
            static void run_piece_20g(Context &ctx, map_t const &board, State const &start, std::size_t depth,
                                      EvalCallback *on_land = nullptr)
            {
                constexpr bool EnableT = []() constexpr {
                    if constexpr (requires { SpinHook::template active_for_piece<T, Policy>; })
                        return SpinHook::template active_for_piece<T, Policy>;
                    else
                        return SpinHook::template active_for_piece<T>;
                }();
                std::array<map_t, kMaxR> usable_arr{};
                Helpers::build_usable_for_piece(T, board, usable_arr);

                // RotState 在 BFS 前统一初始化，供命中阶段读取。
                //  corners3_arr (≥3 corner 占用位图, 由 board 决定) 与 blocked_arr
                //  (4 邻 OR 取反位图, 由 usable_arr 决定). apply_emit_20g 仅做
                //  位板查表, 不再触碰 master 节点 / TetrisMap 字段.
                //  EnableT=false 路径下 RotState 退化为空 struct, on_init_rotations /
                //  compute_mini_blocked_arr 同为空体, 整段被 if constexpr 裁掉.
                typename SpinHook::template RotState<map_t, kMaxR> rot_state{};
                if constexpr (EnableT)
                {
                    SpinHook::template on_init_rotations<RuleSpec, T, map_t, kMaxR>(board, rot_state);
                    SpinHook::template compute_mini_blocked_arr<RuleSpec, T, map_t, kMaxR>(usable_arr, rot_state);
                }
                auto sunk_opt = Helpers::drop(start, usable_arr);
                if (!sunk_opt)
                    return;
                State start_state = *sunk_opt;

                std::vector<Run20gMarkSlot> mark(static_cast<std::size_t>(kMaxR) * kW * kH,
                                                 Run20gMarkSlot{0, ' ', 0xFFu, 0, 0});
                std::vector<CellsKey> emitted_keys;
                emitted_keys.reserve(32);

                Run20gDedup<T, EnableT> dedup{&mark};
                Run20gNeighbors<T> neighbors{&usable_arr, SpinHook::config_allow_180(ctx.config_)};
                Run20gVisitor<T, EnableT, EvalCallback> visitor{
                    &ctx, &usable_arr, &mark, &emitted_keys, depth, &rot_state};
                if constexpr (!std::is_void_v<EvalCallback>)
                    visitor.on_land = on_land;

                std::vector<State> queue_buffer;
                queue_buffer.reserve(64);
                run_bfs(start_state, neighbors, dedup, visitor, queue_buffer);
            }

            //==================================================================
            //=== make_path 1g 三件套 ==========================================
            //==================================================================
            using MakePath1gDedup = detail::common::MakePath1gDedup<RuleSpec>;

            struct MakePath1gNeighbors
            {
                char piece_t;
                bool allow_180;
                bool allow_LR;
                bool allow_D;
                bool allow_d;
                bool allow_rotate_move;
                bool disable_d;
                std::array<map_t, kMaxR> const *usable_arr;

                template<class Emit>
                void expand(State const &cur, Emit emit)
                {
                    if (disable_d)
                    {
                        auto D = Helpers::drop(cur, *usable_arr);
                        if (D)
                            emit(*D, 'D');
                        return;
                    }
                    if (allow_180)
                    {
                        auto wk = Helpers::first_kick(piece_t, KickDir::Opp, cur, *usable_arr);
                        if (wk)
                            emit(*wk, 'x');
                    }
                    {
                        auto wk = Helpers::first_kick(piece_t, KickDir::Ccw, cur, *usable_arr);
                        if (wk)
                            emit(*wk, 'z');
                    }
                    {
                        auto wk = Helpers::first_kick(piece_t, KickDir::Cw, cur, *usable_arr);
                        if (wk)
                            emit(*wk, 'c');
                    }
                    {
                        State nL = cur;
                        nL.xb = static_cast<std::int8_t>(nL.xb - 1);
                        EnqueueDecision dL = emit(nL, 'l');
                        if (dL == EnqueueDecision::MarkAndEnqueue && allow_rotate_move)
                        {
                            if (allow_180)
                            {
                                auto rn = Helpers::rotate_no_kick_bb(piece_t, KickDir::Opp, nL);
                                if (rn)
                                    emit(*rn, 'X', &nL);
                            }
                            {
                                auto rn = Helpers::rotate_no_kick_bb(piece_t, KickDir::Ccw, nL);
                                if (rn)
                                    emit(*rn, 'Z', &nL);
                            }
                            {
                                auto rn = Helpers::rotate_no_kick_bb(piece_t, KickDir::Cw, nL);
                                if (rn)
                                    emit(*rn, 'C', &nL);
                            }
                        }
                    }
                    {
                        State nR = cur;
                        nR.xb = static_cast<std::int8_t>(nR.xb + 1);
                        EnqueueDecision dR = emit(nR, 'r');
                        if (dR == EnqueueDecision::MarkAndEnqueue && allow_rotate_move)
                        {
                            if (allow_180)
                            {
                                auto rn = Helpers::rotate_no_kick_bb(piece_t, KickDir::Opp, nR);
                                if (rn)
                                    emit(*rn, 'X', &nR);
                            }
                            {
                                auto rn = Helpers::rotate_no_kick_bb(piece_t, KickDir::Ccw, nR);
                                if (rn)
                                    emit(*rn, 'Z', &nR);
                            }
                            {
                                auto rn = Helpers::rotate_no_kick_bb(piece_t, KickDir::Cw, nR);
                                if (rn)
                                    emit(*rn, 'C', &nR);
                            }
                        }
                    }
                    if (allow_LR)
                    {
                        State nL_state = cur;
                        nL_state.xb = static_cast<std::int8_t>(nL_state.xb - 1);
                        if (Helpers::usable_at(nL_state.r, nL_state.xb, nL_state.yb, *usable_arr))
                        {
                            while (true)
                            {
                                State try_state = nL_state;
                                try_state.xb = static_cast<std::int8_t>(try_state.xb - 1);
                                if (!Helpers::usable_at(try_state.r, try_state.xb, try_state.yb, *usable_arr))
                                    break;
                                nL_state = try_state;
                            }
                            emit(nL_state, 'L');
                        }
                    }
                    if (allow_LR)
                    {
                        State nR_state = cur;
                        nR_state.xb = static_cast<std::int8_t>(nR_state.xb + 1);
                        if (Helpers::usable_at(nR_state.r, nR_state.xb, nR_state.yb, *usable_arr))
                        {
                            while (true)
                            {
                                State try_state = nR_state;
                                try_state.xb = static_cast<std::int8_t>(try_state.xb + 1);
                                if (!Helpers::usable_at(try_state.r, try_state.xb, try_state.yb, *usable_arr))
                                    break;
                                nR_state = try_state;
                            }
                            emit(nR_state, 'R');
                        }
                    }
                    if (allow_d)
                    {
                        State nd = cur;
                        nd.yb = static_cast<std::int8_t>(nd.yb - 1);
                        EnqueueDecision dd = emit(nd, 'd');
                        if (dd == EnqueueDecision::MarkAndEnqueue && allow_D)
                        {
                            auto D = Helpers::drop(cur, *usable_arr);
                            if (D)
                                emit(*D, 'D');
                        }
                    }
                    else if (allow_D)
                    {
                        auto D = Helpers::drop(cur, *usable_arr);
                        if (D)
                            emit(*D, 'D');
                    }
                }
            };

            struct MakePath1gVisitor
            {
                CellsKey index;
                CellsKey index_landpoint;
                // TSpin 路径使用统一的 last-rotate 判定接口。
                //  单一 requires_last_rotate (= lp_requires_last_rotate(cfg, lp)),
                //  与 strategy 入口处一次解算的 requires_last 同语义. 命中第二条
                //  谓词由两条 AND 合并为单条件.
                bool requires_last_rotate;
                State found{};
                bool hit = false;

                bool check_hit(State const &s) const
                {
                    CellsKey k = Helpers::cells_key_for_state(s);
                    if (k == index)
                        return true;
                    if (requires_last_rotate && k == index_landpoint)
                        return true;
                    return false;
                }

                bool on_pop(State const &)
                {
                    return true;
                }

                bool on_admit(State const &s, char, EnqueueDecision)
                {
                    if (check_hit(s))
                    {
                        found = s;
                        hit = true;
                        return false;
                    }
                    return true;
                }
            };

            //==================================================================
            //=== make_path 20g 三件套 =========================================
            //==================================================================
            struct MakePath20gDedup
            {
                PathMark *path_mark;

                EnqueueDecision try_admit(State const &s, State const *parent, char action)
                {
                    if (s.xb < 0 || s.xb >= kW || s.yb < 0 || s.yb >= kH || s.r >= kMaxR)
                        return EnqueueDecision::Skip;
                    typename PathMark::PrevKey pk;
                    if (parent == nullptr)
                    {
                        //起点协议: prev = self.
                        pk = typename PathMark::PrevKey{static_cast<std::uint8_t>(s.r),
                                                        static_cast<std::int8_t>(s.xb),
                                                        static_cast<std::int8_t>(s.yb)};
                    }
                    else
                    {
                        pk = typename PathMark::PrevKey{static_cast<std::uint8_t>(parent->r),
                                                        static_cast<std::int8_t>(parent->xb),
                                                        static_cast<std::int8_t>(parent->yb)};
                    }
                    if (!path_mark->set_bbox(static_cast<int>(s.r),
                                             static_cast<int>(s.xb),
                                             static_cast<int>(s.yb),
                                             pk,
                                             action))
                        return EnqueueDecision::Skip;
                    return EnqueueDecision::MarkAndEnqueue;
                }
            };

            struct MakePath20gNeighbors
            {
                char piece_t;
                bool allow_180;
                bool allow_LR;
                std::array<map_t, kMaxR> const *usable_arr;

                template<class Emit>
                void expand(State const &cur, Emit emit)
                {
                    auto try_drop_emit = [&](State const &pre_drop, char action)
                    {
                        auto sunk = Helpers::drop(pre_drop, *usable_arr);
                        if (sunk)
                            emit(*sunk, action);
                    };
                    if (allow_180)
                    {
                        auto wk = Helpers::first_kick(piece_t, KickDir::Opp, cur, *usable_arr);
                        if (wk)
                            try_drop_emit(*wk, 'x');
                    }
                    {
                        auto wk = Helpers::first_kick(piece_t, KickDir::Ccw, cur, *usable_arr);
                        if (wk)
                            try_drop_emit(*wk, 'z');
                    }
                    {
                        auto wk = Helpers::first_kick(piece_t, KickDir::Cw, cur, *usable_arr);
                        if (wk)
                            try_drop_emit(*wk, 'c');
                    }
                    {
                        State nL_state = cur;
                        nL_state.xb = static_cast<std::int8_t>(nL_state.xb - 1);
                        if (Helpers::usable_at(nL_state.r, nL_state.xb, nL_state.yb, *usable_arr))
                            try_drop_emit(nL_state, 'l');
                    }
                    {
                        State nR_state = cur;
                        nR_state.xb = static_cast<std::int8_t>(nR_state.xb + 1);
                        if (Helpers::usable_at(nR_state.r, nR_state.xb, nR_state.yb, *usable_arr))
                            try_drop_emit(nR_state, 'r');
                    }
                    if (allow_LR)
                    {
                        State scan = cur;
                        State try_state = scan;
                        try_state.xb = static_cast<std::int8_t>(try_state.xb - 1);
                        if (Helpers::usable_at(try_state.r, try_state.xb, try_state.yb, *usable_arr))
                        {
                            bool got_terminal = false;
                            State terminal_state = scan;
                            while (true)
                            {
                                State cand = scan;
                                cand.xb = static_cast<std::int8_t>(cand.xb - 1);
                                if (!Helpers::usable_at(cand.r, cand.xb, cand.yb, *usable_arr))
                                    break;
                                auto cand_sunk = Helpers::drop(cand, *usable_arr);
                                if (!cand_sunk)
                                    break;
                                got_terminal = true;
                                terminal_state = *cand_sunk;
                                scan = terminal_state;
                            }
                            if (got_terminal)
                                emit(terminal_state, 'L');
                        }
                    }
                    if (allow_LR)
                    {
                        State scan = cur;
                        State try_state = scan;
                        try_state.xb = static_cast<std::int8_t>(try_state.xb + 1);
                        if (Helpers::usable_at(try_state.r, try_state.xb, try_state.yb, *usable_arr))
                        {
                            bool got_terminal = false;
                            State terminal_state = scan;
                            while (true)
                            {
                                State cand = scan;
                                cand.xb = static_cast<std::int8_t>(cand.xb + 1);
                                if (!Helpers::usable_at(cand.r, cand.xb, cand.yb, *usable_arr))
                                    break;
                                auto cand_sunk = Helpers::drop(cand, *usable_arr);
                                if (!cand_sunk)
                                    break;
                                got_terminal = true;
                                terminal_state = *cand_sunk;
                                scan = terminal_state;
                            }
                            if (got_terminal)
                                emit(terminal_state, 'R');
                        }
                    }
                }
            };

            struct MakePath20gVisitor
            {
                CellsKey index_key;
                CellsKey index_landpoint;
                // 命中条件与 1g visitor 保持同一套判定接口。
                bool requires_last_rotate;
                State found{};
                bool hit = false;

                bool check_hit(State const &s) const
                {
                    CellsKey k = Helpers::cells_key_for_state(s);
                    if (k == index_key)
                        return true;
                    if (requires_last_rotate && k == index_landpoint)
                        return true;
                    return false;
                }

                bool on_pop(State const &)
                {
                    return true;
                }

                bool on_admit(State const &s, char, EnqueueDecision)
                {
                    if (check_hit(s))
                    {
                        found = s;
                        hit = true;
                        return false;
                    }
                    return true;
                }
            };

            //==================================================================
            //=== make_path 1g / 20g 入口 ======================================
            //==================================================================
            static std::vector<char>
            make_path_1g_native(Context &ctx, State const &spawn,
                                LandPoint const &land_point,
                                TetrisMap<RuleSpec::width, RuleSpec::height> const &board)
            {
                char piece_t = static_cast<char>(spawn.t);
                if (piece_info_t::index_of(piece_t) < 0)
                {
                    assert(false && "PathStrategy::make_path_1g_native: piece type outside RuleSpec");
                    return std::vector<char>();
                }
                if (!SpinHook::lp_requires_last_rotate(ctx.config_, land_point) &&
                    Helpers::cells_key_for_state(spawn) ==
                        Helpers::cells_key_for_state(land_point.state))
                {
                    return std::vector<char>();
                }
                bool allow_180 = SpinHook::config_allow_180(ctx.config_);
                bool allow_LR = SpinHook::config_allow_LR(ctx.config_);
                bool allow_D = SpinHook::config_allow_D(ctx.config_);
                State const *land_last = SpinHook::get_last_state(land_point);
                // 是否需要最后一次旋转由 lp_requires_last_rotate 统一判定。
                //  index 选择保持原语义: TSpin 命中且 land_last 可用时, BFS 终点
                //  指向 last 节点 cells_key (旋转一步落到 lp.state); 其余路径
                //  (含 ASpin/None) 指向 lp.state cells_key.
                bool requires_last = SpinHook::lp_requires_last_rotate(ctx.config_, land_point);
                CellsKey index = (!requires_last || land_last == nullptr)
                                         ? Helpers::cells_key_for_state(land_point.state)
                                         : Helpers::cells_key_for_state(*land_last);
                CellsKey index_landpoint = Helpers::cells_key_for_state(land_point.state);
                std::array<map_t, kMaxR> usable_arr{};
                Helpers::build_usable_for_piece(piece_t, board, usable_arr);

                // PathMark 由 make_path 局部栈对象管理。
                //  入口栈分配, 解锁后续并发. 默认构造 (used_ 全 false) 即合法
                //  空表; 仅 disable_d 重试需要在 while 顶部 clear 复位 (复用同一
                //  对象, build_path 闭包捕获 &pm).
                PathMark pm;

                auto build_path = [&pm, &land_point, allow_180, &usable_arr, piece_t,
                                   &index_landpoint, land_last](State const &cur_state) -> std::vector<char>
                {
                    CellsKey cur_key = Helpers::cells_key_for_state(cur_state);
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
                    if (cur_key != index_landpoint)
                    {
                        State last_state = *land_last;
                        if (allow_180 &&
                            Helpers::try_kick_chain_to(piece_t, KickDir::Opp, last_state, index_landpoint, usable_arr))
                        {
                            path.push_back('x');
                            return path;
                        }
                        if (Helpers::try_kick_chain_to(piece_t, KickDir::Ccw, last_state, index_landpoint, usable_arr))
                        {
                            path.push_back('z');
                            return path;
                        }
                        if (Helpers::try_kick_chain_to(piece_t, KickDir::Cw, last_state, index_landpoint, usable_arr))
                        {
                            path.push_back('c');
                            return path;
                        }
                    }
                    return path;
                };

                bool disable_d = false;
                // 是否需要最后一次旋转由 lp_requires_last_rotate 统一判定。
                //  此处 "非 T-spin landpoint" 走开盘 spawn-fast-drop 启发式 —
                //  TSpin 命中且 cfg->last_rotate 时不可走 (会丢失 last 旋转步).
                //  ASpin / NoSpin / Cautious 现在都享受这一优化，旧版 ASpin 路径曾被错误排除。
                if (!requires_last)
                {
                    int spawn_low = Helpers::spawn_min_cell_y_dispatch_state(spawn);
                    int roof = Helpers::board_roof(board);
                    if (spawn_low >= roof && Helpers::open_state(land_point.state, board))
                        disable_d = true;
                }

                while (true)
                {
                    pm.clear();
                    State entry_state = spawn;
                    // 第二条命中谓词处理目标 cells_key 已匹配的 TSpin 入口。
                    //  从 (!is_landpoint_none(lp) && config_last_rotate(cfg)) 合并为
                    //  lp_requires_last_rotate(cfg, lp) — 后者已内联 last_rotate
                    //  检查与 lp.type 检查.
                    if (Helpers::index_filtered_eq_state(entry_state, index) ||
                        (requires_last &&
                         Helpers::cells_key_for_state(entry_state) == index_landpoint))
                    {
                        path_mark_set_state_root(pm, entry_state, '\0');
                        return build_path(entry_state);
                    }
                    MakePath1gDedup dedup{&pm, &usable_arr};
                    MakePath1gNeighbors neighbors{piece_t,
                                                  allow_180,
                                                  allow_LR,
                                                  allow_D,
                                                  SpinHook::config_allow_d(ctx.config_),
                                                  SpinHook::config_allow_rotate_move(ctx.config_),
                                                  disable_d,
                                                  &usable_arr};
                    MakePath1gVisitor visitor{index,
                                              index_landpoint,
                                              requires_last,
                                              State{},
                                              false};
                    ctx.node_search_path_.clear();
                    run_bfs(entry_state, neighbors, dedup, visitor, ctx.node_search_path_);
                    if (visitor.hit)
                    {
                        return build_path(visitor.found);
                    }
                    if (disable_d)
                        disable_d = false;
                    else
                        break;
                }
                return std::vector<char>();
            }

            static std::vector<char>
            make_path_20g_native(Context &ctx, State const &spawn,
                                 LandPoint const &land_point,
                                 TetrisMap<RuleSpec::width, RuleSpec::height> const &board)
            {
                char piece_t = static_cast<char>(spawn.t);
                if (piece_info_t::index_of(piece_t) < 0)
                {
                    assert(false && "PathStrategy::make_path_20g_native: piece type outside RuleSpec");
                    return std::vector<char>();
                }
                std::array<map_t, kMaxR> usable_arr{};
                Helpers::build_usable_for_piece(piece_t, board, usable_arr);
                State entry_state = spawn;
                auto start_sunk_opt = Helpers::drop(entry_state, usable_arr);
                if (!start_sunk_opt)
                    return std::vector<char>();
                State s0 = *start_sunk_opt;
                CellsKey index_landpoint = Helpers::cells_key_for_state(land_point.state);
                State const *land_last = SpinHook::get_last_state(land_point);
                // 20g 路径同样通过 lp_requires_last_rotate 统一判定是否需要最后一次旋转。
                //  is_landpoint_none + config_last_rotate 双查询, ASpin 起点==终点
                //  也享受短路.
                bool requires_last = SpinHook::lp_requires_last_rotate(ctx.config_, land_point);
                CellsKey index_key = (!requires_last || land_last == nullptr)
                                             ? index_landpoint
                                             : Helpers::cells_key_for_state(*land_last);
                bool allow_180 = SpinHook::config_allow_180(ctx.config_);
                bool allow_LR = SpinHook::config_allow_LR(ctx.config_);
                if (!requires_last &&
                    Helpers::cells_key_for_state(s0) == index_landpoint)
                {
                    return std::vector<char>();
                }

                // 与 1g 路径相同，PathMark 使用局部栈对象。
                PathMark pm;

                auto build_path = [&pm, &land_point, allow_180, &usable_arr, piece_t,
                                   &index_landpoint, land_last](State const &cur_state) -> std::vector<char>
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
                    CellsKey cur_key = Helpers::cells_key_for_state(cur_state);
                    if (cur_key != index_landpoint)
                    {
                        State last_state = *land_last;
                        auto try_kick_chain_to_drop = [&](KickDir dir) -> bool
                        {
                            auto hit = Helpers::first_kick(piece_t, dir, last_state, usable_arr);
                            if (!hit)
                                return false;
                            auto sunk = Helpers::drop(*hit, usable_arr);
                            if (!sunk)
                                return false;
                            return Helpers::cells_key_for_state(*sunk) == index_landpoint;
                        };
                        if (allow_180 && try_kick_chain_to_drop(KickDir::Opp))
                        {
                            path.push_back('x');
                            return path;
                        }
                        if (try_kick_chain_to_drop(KickDir::Ccw))
                        {
                            path.push_back('z');
                            return path;
                        }
                        if (try_kick_chain_to_drop(KickDir::Cw))
                        {
                            path.push_back('c');
                            return path;
                        }
                    }
                    return path;
                };

                auto check_hit_state = [&](State const &cand) -> bool
                {
                    CellsKey ck = Helpers::cells_key_for_state(cand);
                    if (ck == index_key)
                        return true;
                    if (requires_last && ck == index_landpoint)
                        return true;
                    return false;
                };

                ctx.node_search_path_.clear();
                if (check_hit_state(s0))
                {
                    return build_path(s0);
                }

                MakePath20gDedup dedup{&pm};
                MakePath20gNeighbors neighbors{piece_t, allow_180, allow_LR, &usable_arr};
                MakePath20gVisitor visitor{index_key,
                                           index_landpoint,
                                           requires_last,
                                           State{},
                                           false};
                run_bfs(s0, neighbors, dedup, visitor, ctx.node_search_path_);
                if (visitor.hit)
                {
                    return build_path(visitor.found);
                }
                return std::vector<char>();
            }

            //=== 起点 root mark 写入 (与旧 path_mark_set_state_root 同形) ========
            // 直接接受调用方提供的 PathMark 引用。
            static bool path_mark_set_state_root(PathMark &pm, State const &child, char op)
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

    //=== 顶层 alias: 让上层直接用 m_tetris2::PathStrategy 而不必拼 movegen:: 前缀.
    // Policy 第三参默认 void, 与 movegen::PathStrategy 保持一致, 同时满足
    // Searcher<template<class,class,class> Strategy, ...> 的模板模板参数约束.
    template<class SpinHook, class RuleSpec, class Policy = void>
    using PathStrategy = movegen::PathStrategy<SpinHook, RuleSpec, Policy>;
} // namespace m_tetris2

//==========================================================================
// 顶层 namespace path: TetrisEngine 第三模板参数的"位板版"用法.
//
//   * path::DefaultConfig: HookType = m_tetris2::NoSpinHook (无 spin 元数据).
//   * path::SearchWith<Cfg>: Cfg 提供 HookType (trait 字段) 选 SpinHook;
//     未指定 HookType 时回退到 NoSpinHook. Cfg 的其他常量字段 (allow_180 等)
//     仅作为与 oracle 对称的形态装饰, path::SearchWith 模板内不消费它们,
//     运行时配置由 SpinHook::Config 实例 (TetrisEngine status_config_) 决定.
//   * path::Search = SearchWith<DefaultConfig>.
//
// aspin / tspin / cautious 在各自头文件 (search_aspin.h / search_tspin.h /
// search_cautious.h) 内定义同形态的 Default/SearchWith/Search, 走
// path::detail::merge 把用户 Cfg 与各 namespace 自家的 DefaultConfig 合并.
//==========================================================================
namespace path
{
    namespace detail
    {
        //HookType 探测: 若 Cfg 暴露 HookType type alias 则取之, 否则回退.
        template<class C, class Fallback, class = void>
        struct hook_or
        {
            using type = Fallback;
        };
        template<class C, class Fallback>
        struct hook_or<C, Fallback, std::void_t<typename C::HookType>>
        {
            using type = typename C::HookType;
        };

        //PolicyType 探测: 复用 m_tetris2::detail::policy_or (movegen_strategy.h).
        //  此处 type alias 透传, 避免在 path::detail 里重复定义同一个 trait.
        template<class C, class Fallback = void>
        using policy_or = m_tetris2::detail::policy_or<C, Fallback>;

        //merge<Default, Override>: 用户 Cfg 覆盖 Default. HookType 与 PolicyType
        //  均参与合并: 用户若在 Override 中重新定义这两个 alias 会优先生效, 否则
        //  继续走 Default 里的值. 其他字段 (bool 常量) 由继承 Default 透传.
        template<class Default, class Override>
        struct merge : Default
        {
            using HookType = typename hook_or<Override, typename Default::HookType>::type;
            using PolicyType = typename policy_or<Override, typename policy_or<Default>::type>::type;
        };
    } // namespace detail

    struct DefaultConfig
    {
        using HookType = m_tetris2::NoSpinHook;
    };

    template<class Config = DefaultConfig>
    struct SearchWith
    {
        template<class RuleType>
        using type = m_tetris2::movegen::Searcher<
            m_tetris2::PathStrategy,
            typename detail::hook_or<Config, m_tetris2::NoSpinHook>::type,
            typename RuleType::rule_spec,
            typename detail::policy_or<Config>::type>;

        //rebind_policy<P>: 保留原 Config 的 HookType 等字段, 仅覆盖 PolicyType.
        //  TetrisEngine 用此接口向 SearchWith 注入编译期推导的 PolicyType,
        //  而不影响用户已选定的 HookType (正交原则).
        template<class NewPolicy>
        struct PolicyOverrideConfig : Config { using PolicyType = NewPolicy; };

        template<class NewPolicy>
        using rebind_policy = SearchWith<PolicyOverrideConfig<NewPolicy>>;
    };

    using Search = SearchWith<>;
} // namespace path

// search_path.h 的核心类型已定义完成，其他搜索标签可在此之后补全顶层命名空间。
#define TETRIS_AI_RUNNER_SEARCH_PATH_TAGS_READY_

// 重新包含 search_aspin.h / search_tspin.h / search_amini.h，使依赖 PathStrategy 的顶层搜索标签在这里展开。
#include "search_aspin.h"
#include "search_tspin.h"
#include "search_amini.h"

#endif // TETRIS_AI_RUNNER_SEARCH_PATH_H_
