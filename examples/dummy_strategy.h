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
//   - search(map, spawn, depth):
//         Enumerate all rotation states reachable from `spawn` at the spawn
//         row, drop each onto the board via the bitboard usable-row table,
//         dedup by absolute cell signature (bb::CellsKey), and emit distinct
//         landings into ctx.land_point_cache_.
//   - make_path(map, spawn, land_point, board):
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
            // Cache the Config pointer. TetrisContext is no longer needed.
            static void init(Context &ctx, TetrisContext const * /*context*/, Config const *config)
            {
                ctx.config_ = config;
            }

            //=== Strategy contract: search ====================================
            // Enumerate all spawn-row rotation states for `spawn.t`, drop each
            // onto the board via the bitboard usable-row table, dedup by cell
            // signature, and emit distinct landings into ctx.land_point_cache_.
            static std::vector<LandPoint> const *
            search(Context &ctx, TetrisMap const &map, TetrisBlockStatus const &spawn, std::size_t depth)
            {
                (void)depth;
                ctx.land_point_cache_.clear();
                if (piece_info_t::index_of(spawn.t) < 0)
                    return &ctx.land_point_cache_;
                map_t board{};
                using row_t = typename map_t::row_t;
                constexpr row_t row_mask = map_t::row_full;
                int h = map.height < map_t::height ? map.height : map_t::height;
                for (int y = 0; y < h; ++y)
                {
                    std::uint32_t occ = (~map.row[y]) & static_cast<std::uint32_t>(row_mask);
                    board.set_row(y, static_cast<row_t>(occ));
                }
                std::array<map_t, kMaxR> usable_arr{};
                Helpers::build_usable_for_piece(spawn.t, board, usable_arr);

                std::array<map_t, kMaxR> inbounds_arr{};
                Helpers::build_inbounds_for_piece(spawn.t, inbounds_arr);

                bb::BBState entry = Helpers::state_from_status(
                    spawn.t, static_cast<std::uint8_t>(spawn.r), spawn.x, spawn.y);

                if (!Helpers::usable_at_bb(entry.r, entry.xb, entry.yb, usable_arr))
                    return &ctx.land_point_cache_;

                std::vector<bb::BBState> spawn_states;
                Helpers::enumerate_spawn_row_states(spawn.t, entry, inbounds_arr, spawn_states);

                std::vector<bb::CellsKey> emitted_keys;
                emitted_keys.reserve(8);

                for (auto const &rot_state : spawn_states)
                {
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
