#ifndef TETRIS_AI_RUNNER_EXAMPLES_DUMMY_STRATEGY_H_
#define TETRIS_AI_RUNNER_EXAMPLES_DUMMY_STRATEGY_H_

//==========================================================================
// DummyStrategy<SpinHook, RuleSpec>: minimal "spawn-row drop only" strategy.
//
// Purpose:
//   Serves as the canonical template for third-party SearchStrategy authors.
//   Implements the full strategy contract (Context alias, LandPoint / Config
//   aliases, init / search / make_path static entry points) using only the
//   smallest combination of mixins that the contract actually requires.
//
// Behavior (intentionally minimal — NOT semantically correct for play):
//   - search(map, spawn_node, depth):
//         Iterate the spawn-row land-point list (TetrisContext-built table
//         hanging off `spawn_node->land_point`), drop each rotation onto the
//         board, dedup by absolute cell signature (bb::CellsKey), and emit
//         distinct landings into ctx.land_point_cache_.
//   - make_path(map, src_node, land_point):
//         Always returns "D" (a single hard drop). Sufficient for any landing
//         whose path is "spawn -> hard drop", incorrect otherwise.
//
// This file is referenced from `docs/extending_search.md` as the worked
// example. The companion compile-only target `dummy_strategy_compile_check`
// instantiates `Searcher<DummyStrategy, NoSpinHook, rule_spec>` to keep the
// example wired to the live API surface.
//==========================================================================

#include "bb_state.h"
#include "movegen_context.h"
#include "movegen_hook.h"
#include "movegen_strategy.h"
#include "tetris_core.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace m_tetris2
{
    namespace movegen
    {
        //=== DummyStrategy private mixin (SpinHook-dependent fields) ===========
        // This mirrors the layout of detail::path::ExtrasMixin so that any
        // SpinHook providing Config / LandPoint aliases works out of the box.
        // Lives in `detail::dummy::` so it does not leak into the public
        // `m_tetris::movegen::` namespace.
        namespace detail
        {
            namespace dummy
            {
                template<class SpinHook, class RuleSpec>
                struct ExtrasMixin
                {
                    using Config = typename SpinHook::Config;
                    using LandPoint = typename SpinHook::LandPoint;

                    TetrisContext const *context_ = nullptr;
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
            using Helpers = bb::Helpers<RuleSpec>;
            using map_t = typename Helpers::map_t;
            using piece_info_t = typename Helpers::piece_info_t;

            static constexpr int kMaxR = Helpers::kMaxR;
            static constexpr int kPieceCount = Helpers::kPieceCount;

        public:
            using LandPoint = typename SpinHook::LandPoint;
            using Config = typename SpinHook::Config;

            //=== Strategy contract: Context type alias ========================
            // DummyStrategy only needs land_point_cache_ and context_/config_
            // fields; no BBState->TetrisNode LUT is required because search()
            // constructs LandPoint directly from BBState.
            using Context = MoveGenContext<RuleSpec,
                                           detail::dummy::ExtrasMixin<SpinHook, RuleSpec>>;

            //=== Strategy contract: init ======================================
            // Cache the TetrisContext / Config pair.
            static void init(Context &ctx, TetrisContext const *context, Config const *config)
            {
                ctx.context_ = context;
                ctx.config_ = config;
            }

            //=== Strategy contract: search ====================================
            // Iterate `node->land_point` (spawn-row rotation table) and emit
            // one landing per distinct cell signature.
            static std::vector<LandPoint> const *
            search(Context &ctx, TetrisMap const &map, TetrisNode const *node, std::size_t depth)
            {
                (void)depth;
                ctx.land_point_cache_.clear();
                if (!node || !node->check(map))
                    return &ctx.land_point_cache_;
                if (piece_info_t::index_of(node->status.t) < 0)
                    return &ctx.land_point_cache_;
                map_t board = Helpers::build_board(map);
                std::array<map_t, kMaxR> usable_arr{};
                Helpers::build_usable_for_piece(node->status.t, board, usable_arr);

                std::vector<bb::CellsKey> emitted_keys;
                emitted_keys.reserve(8);

                if (node->land_point == nullptr)
                    return &ctx.land_point_cache_;
                for (auto cit = node->land_point->begin(); cit != node->land_point->end(); ++cit)
                {
                    TetrisNode const *spawn_rot = *cit;
                    if (!spawn_rot)
                        continue;
                    bb::BBState rot_state = Helpers::state_from_node(spawn_rot);
                    auto sunk = Helpers::drop_bb_state(rot_state, usable_arr);
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
                    LandPoint lp(*sunk);
                    ctx.land_point_cache_.push_back(lp);
                }
                return &ctx.land_point_cache_;
            }

            //=== Strategy contract: make_path =================================
            // Always emits a single 'D' (hard drop). Correct only when the
            // landing was reached by spawn -> drop; this strategy is for
            // documentation / sanity-check purposes only.
            static std::vector<char>
            make_path(Context &ctx, bb::BBState const &spawn,
                      LandPoint const &land_point,
                      Map<RuleSpec::width, RuleSpec::height> const &board)
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
