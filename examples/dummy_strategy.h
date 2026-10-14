#ifndef TETRIS_AI_RUNNER_EXAMPLES_DUMMY_STRATEGY_H_
#define TETRIS_AI_RUNNER_EXAMPLES_DUMMY_STRATEGY_H_

//==========================================================================
// DummyStrategy<SpinHook, RuleSpec, Policy>: minimal "spawn-row drop only"
// strategy.
//
// Purpose:
//   Serves as the canonical template for third-party SearchStrategy authors.
//   Implements the full strategy contract (Context alias, LandPoint / Config
//   aliases, init / search / search_eval / make_path static entry points)
//   using only the smallest combination of mixins that the contract actually
//   requires.
//
// Behavior (intentionally minimal — NOT semantically correct for play):
//   - search(map, spawn, depth) / search_eval<Cb>(board, spawn, depth, cb):
//         Enumerate all rotation states of `spawn.t` at the spawn row, drop
//         each onto the board via the bitboard usable table, dedup by
//         absolute cell signature (CellsKey), and emit distinct landings
//         (search: into ctx.land_point_cache_; search_eval: through
//         cb.template operator()<T, R>(land_point)).
//   - make_path(spawn, land_point, board):
//         Always returns "D" (a single hard drop). Sufficient for any landing
//         whose path is "spawn -> hard drop", incorrect otherwise.
//
// This file is referenced from `docs/extending_search.md` as the worked
// example. The companion target `dummy_strategy_compile_check` instantiates
// `Searcher<DummyStrategy, NoSpinHook, rule_spec>` to keep the example wired
// to the live API surface.
//==========================================================================

#include "movegen_state.h"
#include "movegen_context.h"
#include "movegen_hook.h"
#include "movegen_strategy.h"
#include "tetris_core.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace m_tetris2
{
    namespace movegen
    {
        //=== DummyStrategy private mixin (SpinHook-dependent fields) ===========
        // This mirrors the layout of detail::simple::ExtrasMixin so that any
        // SpinHook providing Config / LandPoint aliases works out of the box.
        // Lives in `detail::dummy::` so it does not leak into the public
        // `m_tetris2::movegen::` namespace.
        namespace detail
        {
            namespace dummy
            {
                template<class SpinHook, class RuleSpec>
                struct ExtrasMixin
                {
                    using Config = typename SpinHook::Config;
                    using LandPoint = typename SpinHook::LandPoint;

                    Config const *config_ = nullptr;
                    std::vector<LandPoint> land_point_cache_{};
                };
            } // namespace dummy
        } // namespace detail

        // Policy 第三参占位 (void): DummyStrategy 内部不依赖 Policy,
        // 但 Searcher<template<class,class,class> Strategy, ...> 要求 Strategy
        // 恰好有三个模板参数, 此处加 Policy = void 以满足 ttp 约束.
        template<class SpinHook, class RuleSpec, class Policy = void>
        struct DummyStrategy
        {
        private:
            using Helpers = m_tetris2::Helpers<RuleSpec>;
            using map_t = typename Helpers::map_t;
            using piece_info_t = typename Helpers::piece_info_t;

            static constexpr int kMaxR = Helpers::kMaxR;
            static constexpr int kPieceCount = Helpers::kPieceCount;

            // Shared by search / search_eval: spawn-row states of spawn.t,
            // dropped and deduped by CellsKey, in emission order.
            static void collect_landings(map_t const &board, State const &spawn, std::vector<State> &out)
            {
                out.clear();
                char piece_t = static_cast<char>(spawn.t);
                if (piece_info_t::index_of(piece_t) < 0)
                    return;
                std::array<map_t, kMaxR> usable_arr{};
                Helpers::build_usable_for_piece(piece_t, board, usable_arr);
                if (!Helpers::usable_at(spawn.r, spawn.xb, spawn.yb, usable_arr))
                    return;

                std::array<map_t, kMaxR> inbounds_arr{};
                Helpers::build_inbounds_for_piece(piece_t, inbounds_arr);
                std::vector<State> spawn_states;
                Helpers::enumerate_spawn_row_states(piece_t, spawn, inbounds_arr, spawn_states);

                std::vector<CellsKey> emitted_keys;
                emitted_keys.reserve(spawn_states.size());
                for (State const &rot_state : spawn_states)
                {
                    auto sunk = Helpers::drop(rot_state, usable_arr);
                    if (!sunk)
                        continue;
                    CellsKey k = Helpers::cells_key_for_state(*sunk);
                    bool dup = false;
                    for (CellsKey const &ek : emitted_keys)
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
                    out.push_back(*sunk);
                }
            }

            // Runtime (t, r) -> compile-time <T, R> for the eval callback.
            template<char T, class EvalCallback, class LP, std::size_t... Rs>
            static void dispatch_r(LP const &lp, EvalCallback &on_land, std::index_sequence<Rs...>)
            {
                (void)((static_cast<std::uint8_t>(Rs) == lp.state.r
                            ? (on_land.template operator()<T, static_cast<std::uint8_t>(Rs)>(lp), true)
                            : false) ||
                       ...);
            }

            template<class EvalCallback, class LP>
            static void dispatch_typed(LP const &lp, EvalCallback &on_land)
            {
                char t = static_cast<char>(lp.state.t);
                [&]<std::size_t... Is>(std::index_sequence<Is...>)
                {
                    (void)((piece_info_t::type_at(Is) == t
                                ? (dispatch_r<piece_info_t::type_at(Is)>(lp, on_land, std::make_index_sequence<Helpers::template rcount_v<piece_info_t::type_at(Is)>()>{}), true)
                                : false) ||
                           ...);
                }(std::make_index_sequence<kPieceCount>{});
            }

        public:
            using LandPoint = typename SpinHook::LandPoint;
            using Config = typename SpinHook::Config;

            //=== Strategy contract: Context type alias ========================
            // DummyStrategy only needs land_point_cache_ / config_; no BFS
            // queue is required because it never runs run_bfs.
            using Context = MoveGenContext<RuleSpec,
                                           detail::dummy::ExtrasMixin<SpinHook, RuleSpec>>;

            //=== Strategy contract: init ======================================
            static void init(Context &ctx, Config const *config)
            {
                ctx.config_ = config;
            }

            //=== Strategy contract: search ====================================
            // Searcher::search(map, TetrisBlockStatus, depth) converts the
            // status to a State via Helpers::state_from_status and forwards
            // here. MapIn is TetrisMap<W,H> or anything Helpers::build_board
            // accepts.
            template<class MapIn>
            static std::vector<LandPoint> const *
            search(Context &ctx, MapIn const &map, State const &spawn, std::size_t depth)
            {
                (void)depth;
                ctx.land_point_cache_.clear();
                std::vector<State> landings;
                collect_landings(Helpers::build_board(map), spawn, landings);
                for (State const &s : landings)
                {
                    LandPoint lp;
                    lp.state = s;
                    ctx.land_point_cache_.push_back(lp);
                }
                return &ctx.land_point_cache_;
            }

            //=== Strategy contract: search_eval ===============================
            // Hot path used by TetrisTreeNode::run: every landing is handed to
            // on_land.template operator()<T, R>(land_point) with T / R as
            // compile-time constants, instead of being materialised in a
            // vector.
            template<class EvalCallback>
            static void search_eval(Context &ctx, TetrisMap<RuleSpec::width, RuleSpec::height> const &board,
                                    State const &spawn, std::size_t depth,
                                    EvalCallback &on_land)
            {
                (void)ctx;
                (void)depth;
                std::vector<State> landings;
                collect_landings(board, spawn, landings);
                for (State const &s : landings)
                {
                    LandPoint lp;
                    lp.state = s;
                    dispatch_typed(lp, on_land);
                }
            }

            //=== Strategy contract: make_path =================================
            // Always emits a single 'D' (hard drop). Correct only when the
            // landing was reached by spawn -> drop; this strategy is for
            // documentation / sanity-check purposes only.
            static std::vector<char>
            make_path(Context &ctx, State const &spawn,
                      LandPoint const &land_point,
                      TetrisMap<RuleSpec::width, RuleSpec::height> const &board)
            {
                (void)ctx;
                (void)spawn;
                (void)land_point;
                (void)board;
                std::vector<char> path;
                path.push_back('D');
                return path;
            }
        };
    } // namespace movegen

    template<class SpinHook, class RuleSpec, class Policy = void>
    using DummyStrategy = movegen::DummyStrategy<SpinHook, RuleSpec, Policy>;
} // namespace m_tetris2

#endif // TETRIS_AI_RUNNER_EXAMPLES_DUMMY_STRATEGY_H_
