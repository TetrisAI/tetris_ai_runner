#ifndef TETRIS_AI_RUNNER_SEARCH_AMINI_PHASE1_
#define TETRIS_AI_RUNNER_SEARCH_AMINI_PHASE1_

#include "tetris_core.h"
#include "movegen_state.h"

#include <cstddef>
#include <cstdint>

namespace search_amini
{
    // TETR.IO spin classes (rules.md §6). Full is only possible for T.
    class Search
    {
    public:
        enum AMiniType : std::uint8_t
        {
            None,
            Mini,
            Full
        };
        // TETR.IO `spinbonuses` (tetrio.js IsTSpin). The T 3-corner rule (front corners / listed kick index 3
        // -> Full, else Mini) applies under every rule; they differ in which immobile pieces score a Mini:
        //   TSpins      "T-spins":    none.
        //   TSpinsPlus  "T-spins+":   T only.
        //   AllMini     "all-mini":   every piece but T.
        //   AllMiniPlus "all-mini+":  every piece.
        enum class SpinRule : std::uint8_t
        {
            TSpins,
            TSpinsPlus,
            AllMini,
            AllMiniPlus
        };
        // Keys the executor can press; search and make_path both honour them.
        //   allow_d = false: soft drop cannot stop mid-air (instant soft drop), only 'D' to the floor.
        //   allow_LR = false: make_path spells DAS moves as repeated l/r.
        struct Config
        {
            bool allow_180 = true;
            bool allow_d = true;
            bool allow_LR = true;
            bool is_20g = false;
            SpinRule spin_rule = SpinRule::AllMiniPlus;
        };
        // One landing per realizable class at the same cells: a landing reachable both with and without
        // a qualifying final rotation is emitted twice (None + best spin class).
        struct TetrisNodeWithAMiniType
        {
            m_tetris2::State state{};
            AMiniType type = None;

            bool operator==(TetrisNodeWithAMiniType const &other) const
            {
                return state.t == other.state.t && state.r == other.state.r && state.xb == other.state.xb &&
                       state.yb == other.state.yb && type == other.type;
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
    };
} // namespace search_amini

template<>
struct m_tetris2::SpinTypePolicy<search_amini::Search::AMiniType>
{
    using type = m_tetris2::ActiveAllPolicy;
};

#endif // TETRIS_AI_RUNNER_SEARCH_AMINI_PHASE1_

// AMiniStrategy needs the movegen machinery that search_path.h assembles; search_path.h re-includes this header
// once it is complete.
#if !defined(TETRIS_AI_RUNNER_SEARCH_AMINI_PHASE2_) && defined(TETRIS_AI_RUNNER_SEARCH_PATH_TAGS_READY_)
#define TETRIS_AI_RUNNER_SEARCH_AMINI_PHASE2_

#include <algorithm>
#include <array>
#include <optional>
#include <vector>

namespace m_tetris2
{
    namespace movegen
    {
        namespace detail
        {
            namespace amini
            {
                struct Landing
                {
                    CellsKey cells;
                    State state;
                    bool none;
                    search_amini::Search::AMiniType spin;
                };

                template<class SpinHook>
                struct ExtrasMixin
                {
                    typename SpinHook::Config const *config_ = nullptr;
                    std::vector<typename SpinHook::LandPoint> land_point_cache_{};
                    std::vector<Landing> landings_{};
                };
            } // namespace amini
        } // namespace detail

        // Spin-class-exact search for TETR.IO all-mini+ (SpinHook = AMiniHook).
        //   search: 1g uses the bitboard MoveGen; 20g uses the same state BFS as make_path.
        //   make_path: shortest key sequence whose replay ends on lp's cells with exactly lp.type, i.e. a spin
        //   path ends with the classifying rotation and a None path never ends with a qualifying rotation.
        //   Every entry point starts from the engine's spawn State (master coordinates already converted).
        template<class SpinHook, class RuleSpec, class Policy = void>
        struct AMiniStrategy
        {
        private:
            using Helpers = m_tetris2::Helpers<RuleSpec>;
            using map_t = typename Helpers::map_t;
            using usable_t = std::array<map_t, Helpers::kMaxR>;
            using PathMark = typename Helpers::PathMark;
            using AMiniType = search_amini::Search::AMiniType;
            using SpinRule = search_amini::Search::SpinRule;
            using Landing = detail::amini::Landing;

            template<char T>
            static constexpr bool kSpin = SpinHook::template active_for_piece<T, Policy>;

        public:
            using LandPoint = typename SpinHook::LandPoint;
            using Config = typename SpinHook::Config;
            using Context = MoveGenContext<RuleSpec, BfsQueueMixin<RuleSpec>, detail::amini::ExtrasMixin<SpinHook>>;

            static void init(Context &ctx, Config const *config)
            {
                ctx.config_ = config;
            }

            template<class MapIn>
            static std::vector<LandPoint> const *search(Context &ctx, MapIn const &map, State const &spawn,
                                                        std::size_t depth)
            {
                ctx.land_point_cache_.clear();
                auto collect = [&]<char T, std::uint8_t R>(LandPoint const &lp)
                {
                    ctx.land_point_cache_.push_back(lp);
                };
                search_eval(ctx, Helpers::build_board(map), spawn, depth, collect);
                return &ctx.land_point_cache_;
            }

            template<class EvalCallback>
            static void search_eval(Context &ctx, map_t const &board, State const &spawn, std::size_t /*depth*/,
                                    EvalCallback &on_land)
            {
                dispatch_piece(spawn.t, [&]<char T>()
                               {
                    Config const &cfg = config(ctx);
                    dispatch_rule(cfg.spin_rule, [&]<class RuleHook>()
                                  {
                        if (cfg.is_20g)
                            explore_landings<T, RuleHook>(ctx, board, spawn, on_land);
                        else
                            generate_1g<T, RuleHook>(board, spawn, MoveSet{cfg.allow_180, cfg.allow_d}, on_land); }); });
            }

            static std::vector<char> make_path(Context &ctx, State const &spawn, LandPoint const &land_point,
                                               map_t const &board)
            {
                std::vector<char> path;
                dispatch_piece(land_point.state.t, [&]<char T>()
                               { dispatch_rule(config(ctx).spin_rule, [&]<class RuleHook>()
                                               { path = make_path_piece<T, RuleHook>(ctx, spawn, land_point, board); }); });
                return path;
            }

        private:
            struct Env
            {
                map_t const &board;
                usable_t usable;
                Config cfg;
            };

            static Config const &config(Context const &ctx)
            {
                static constexpr Config kDefault{};
                return ctx.config_ != nullptr ? *ctx.config_ : kDefault;
            }

            template<class Fn>
            static void dispatch_piece(std::uint8_t t, Fn &&fn)
            {
                [&]<std::size_t... Is>(std::index_sequence<Is...>)
                {
                    ((Helpers::piece_info_t::type_at(Is) == static_cast<char>(t)
                          ? (fn.template operator()<Helpers::piece_info_t::type_at(Is)>(), true)
                          : false) ||
                     ...);
                }(std::make_index_sequence<Helpers::kPieceCount>{});
            }

            // The classifying hook for the configured TETR.IO spin rule.
            template<class Fn>
            static void dispatch_rule(SpinRule rule, Fn &&fn)
            {
                switch (rule)
                {
                case SpinRule::TSpins:
                    return fn.template operator()<typename SpinHook::template ForRule<SpinRule::TSpins>>();
                case SpinRule::TSpinsPlus:
                    return fn.template operator()<typename SpinHook::template ForRule<SpinRule::TSpinsPlus>>();
                case SpinRule::AllMini:
                    return fn.template operator()<typename SpinHook::template ForRule<SpinRule::AllMini>>();
                case SpinRule::AllMiniPlus:
                    return fn.template operator()<typename SpinHook::template ForRule<SpinRule::AllMiniPlus>>();
                }
            }

            template<char T, class Fn>
            static void dispatch_rotation(std::uint8_t r, Fn &&fn)
            {
                [&]<std::size_t... Rs>(std::index_sequence<Rs...>)
                {
                    ((Rs == r ? (fn.template operator()<static_cast<std::uint8_t>(Rs)>(), true) : false) || ...);
                }(std::make_index_sequence<Helpers::template rcount_v<T>()>{});
            }

            template<char T, class RuleHook, class EvalCallback>
            static void generate_1g(map_t const &board, State const &spawn, MoveSet moves, EvalCallback &on_land)
            {
                auto collect = [&](auto const &lp)
                {
                    dispatch_rotation<T>(lp.r, [&]<std::uint8_t R>()
                                         {
                        LandPoint node{};
                        node.state = Helpers::template build_state_from_master<T, R>(lp.x, lp.y);
                        if constexpr (kSpin<T>)
                            node.type = static_cast<AMiniType>(lp.extra.type);
                        on_land.template operator()<T, R>(node); });
                };
                if constexpr (kSpin<T>)
                    MoveGen<RuleSpec, T, RuleHook>::generate(board, spawn, collect, moves);
                else
                    MoveGen<RuleSpec, T, NoHook>::generate(board, spawn, collect, moves);
            }

            template<char T>
            static Env make_env(Context const &ctx, map_t const &board)
            {
                Env env{board, {}, config(ctx)};
                Helpers::template build_usable_T<T>(board, env.usable);
                return env;
            }

            static bool fits(Env const &env, State const &s)
            {
                return Helpers::usable_at(s.r, s.xb, s.yb, env.usable);
            }

            static bool grounded(Env const &env, State s)
            {
                --s.yb;
                return !fits(env, s);
            }

            // Match MoveGen::generate: if the given start cannot enter as-is, the search is dead.
            static std::optional<State> start_state(Env const &env, State spawn)
            {
                if (!fits(env, spawn))
                    return std::nullopt;
                return env.cfg.is_20g ? Helpers::drop(spawn, env.usable) : spawn;
            }

            // Every key that moves the piece from `from`, with the spin class TETR.IO stores for it.
            template<char T, class RuleHook, class Fn>
            static void for_each_arrival(Env const &env, State const &from, Fn &&fn)
            {
                auto settle = [&](State s)
                {
                    return env.cfg.is_20g ? *Helpers::drop(s, env.usable) : s;
                };
                auto shift = [&](State s, int dx) -> std::optional<State>
                {
                    s.xb = static_cast<std::int8_t>(s.xb + dx);
                    if (!fits(env, s))
                        return std::nullopt;
                    return settle(s);
                };
                for (int dx : {-1, 1})
                {
                    std::optional<State> step = shift(from, dx);
                    if (!step)
                        continue;
                    fn(*step, dx < 0 ? 'l' : 'r', AMiniType::None);
                    if (!env.cfg.allow_LR)
                        continue;
                    State wall = *step;
                    while (std::optional<State> next = shift(wall, dx))
                        wall = *next;
                    if (wall.xb != step->xb || wall.yb != step->yb)
                        fn(wall, dx < 0 ? 'L' : 'R', AMiniType::None);
                }
                if (!env.cfg.is_20g && !grounded(env, from))
                {
                    State floor = *Helpers::drop(from, env.usable);
                    if (env.cfg.allow_d)
                    {
                        State down = from;
                        --down.yb;
                        fn(down, 'd', AMiniType::None);
                        if (floor.yb != down.yb)
                            fn(floor, 'D', AMiniType::None);
                    }
                    else
                        fn(floor, 'D', AMiniType::None);
                }
                auto rotate = [&](KickDir dir, char key)
                {
                    std::optional<KickHit> hit = Helpers::first_kick_hit(T, dir, from, env.usable);
                    if (!hit)
                        return;
                    AMiniType type = AMiniType::None;
                    if constexpr (kSpin<T>)
                        type = RuleHook::template classify_rotation<RuleSpec, T>(env.board, env.usable, hit->state,
                                                                                 hit->test);
                    fn(settle(hit->state), key, type);
                };
                rotate(KickDir::Ccw, 'z');
                rotate(KickDir::Cw, 'c');
                if (env.cfg.allow_180)
                    rotate(KickDir::Opp, 'x');
            }

            static typename PathMark::PrevKey prev_key(State const &s)
            {
                return {s.r, s.xb, s.yb};
            }

            // BFS over piece positions in key order; stops as soon as on_arrival returns false.
            template<char T, class RuleHook, class OnArrival>
            static void explore(Context &ctx, Env const &env, State const &start, PathMark &mark,
                                OnArrival &&on_arrival)
            {
                std::vector<State> &queue = ctx.node_search_path_;
                queue.clear();
                mark.set_bbox(start.r, start.xb, start.yb, prev_key(start), ' ');
                queue.push_back(start);
                for (std::size_t head = 0; head < queue.size(); ++head)
                {
                    State const from = queue[head];
                    bool stop = false;
                    for_each_arrival<T, RuleHook>(env, from, [&](State const &to, char key, AMiniType type)
                                                  {
                        if (stop)
                            return;
                        if (!on_arrival(from, to, key, type))
                        {
                            stop = true;
                            return;
                        }
                        if (mark.set_bbox(to.r, to.xb, to.yb, prev_key(from), key))
                            queue.push_back(to); });
                    if (stop)
                        return;
                }
            }

            static std::vector<char> trace(PathMark const &mark, State s)
            {
                std::vector<char> keys;
                for (;;)
                {
                    auto [prev, key] = mark.get_bbox(s.r, s.xb, s.yb);
                    if (prev.r == s.r && prev.xb == s.xb && prev.yb == s.yb)
                        break;
                    keys.push_back(key);
                    s.r = prev.r;
                    s.xb = prev.xb;
                    s.yb = prev.yb;
                }
                std::reverse(keys.begin(), keys.end());
                return keys;
            }

            template<char T, class RuleHook, class EvalCallback>
            static void explore_landings(Context &ctx, map_t const &board, State const &spawn,
                                         EvalCallback &on_land)
            {
                Env const env = make_env<T>(ctx, board);
                std::optional<State> start = start_state(env, spawn);
                if (!start)
                    return;
                std::vector<Landing> &landings = ctx.landings_;
                landings.clear();
                auto record = [&](State const &s, AMiniType type)
                {
                    if (!grounded(env, s))
                        return;
                    CellsKey cells = Helpers::cells_key_for_state(s);
                    auto it = std::find_if(landings.begin(), landings.end(),
                                           [&](Landing const &l)
                                           { return l.cells == cells; });
                    if (it == landings.end())
                        it = landings.insert(landings.end(), Landing{cells, s, false, AMiniType::None});
                    if (type == AMiniType::None)
                        it->none = true;
                    else
                        it->spin = std::max(it->spin, type);
                };
                record(*start, AMiniType::None);
                PathMark mark{};
                explore<T, RuleHook>(ctx, env, *start, mark, [&](State const &, State const &to, char, AMiniType type)
                                     {
                    record(to, type);
                    return true; });
                for (Landing const &l : landings)
                {
                    dispatch_rotation<T>(l.state.r, [&]<std::uint8_t R>()
                                         {
                        if (l.none)
                            on_land.template operator()<T, R>(LandPoint{l.state, AMiniType::None});
                        if (l.spin != AMiniType::None)
                            on_land.template operator()<T, R>(LandPoint{l.state, l.spin}); });
                }
            }

            template<char T, class RuleHook>
            static std::vector<char> make_path_piece(Context &ctx, State const &spawn, LandPoint const &land_point,
                                                     map_t const &board)
            {
                Env const env = make_env<T>(ctx, board);
                std::optional<State> start = start_state(env, spawn);
                if (!start)
                    return {};
                CellsKey const target = Helpers::cells_key_for_state(land_point.state);
                auto reaches = [&](State const &s, AMiniType type)
                {
                    return (!kSpin<T> || type == land_point.type) && Helpers::cells_key_for_state(s) == target;
                };
                if (reaches(*start, AMiniType::None))
                    return {};
                std::vector<char> path;
                PathMark mark{};
                explore<T, RuleHook>(ctx, env, *start, mark,
                                     [&](State const &from, State const &to, char key, AMiniType type)
                                     {
                    if (!reaches(to, type))
                        return true;
                    path = trace(mark, from);
                    path.push_back(key);
                    return false; });
                return path;
            }
        };
    } // namespace movegen
} // namespace m_tetris2

namespace amini
{
    struct DefaultConfig
    {
        using HookType = m_tetris2::AMiniHook;
    };

    template<class Config = DefaultConfig>
    struct SearchWith
    {
        template<class RuleType>
        using type = m_tetris2::movegen::Searcher<
            m_tetris2::movegen::AMiniStrategy,
            typename path::detail::merge<DefaultConfig, Config>::HookType,
            typename RuleType::rule_spec,
            typename path::detail::merge<DefaultConfig, Config>::PolicyType>;

        template<class NewPolicy>
        struct PolicyOverrideConfig : Config
        {
            using PolicyType = NewPolicy;
        };

        template<class NewPolicy>
        using rebind_policy = SearchWith<PolicyOverrideConfig<NewPolicy>>;
    };

    using Search = SearchWith<>;
} // namespace amini

#endif
