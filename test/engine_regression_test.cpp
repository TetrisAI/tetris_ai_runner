#include "../src/tetris_core.h"
#include "../src/search_tspin.h"
#include "../src/search_aspin.h"
#include "../src/search_amini.h"
#include "../src/search_path.h"
#include "../src/movegen_hook.h"
#include "../src/ai_zzz.h"
#include "../src/rule_srs.h"
#include "../src/rule_toj.h"
#include "../src/rule_botris.h"
#include "../src/rule_io.h"

#include <cstdio>
#include <iterator>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

using m_tetris2::TetrisBlockStatus;

namespace
{
    int g_failures = 0;

    void expect(bool ok, char const *what)
    {
        if (!ok)
        {
            ++g_failures;
            std::printf("FAIL: %s\n", what);
        }
    }

    int const kComboTable[] = {0, 0, 1, 1, 1, 2, 2, 3, 3, 4, 4, 4, 5};
    int const kComboTableMax = int(std::size(kComboTable)) - 1;

    // TSD slot centred at (4, 1) under the overhang at (3, 2): only reachable by rotation.
    template<class Map>
    Map tsd_board()
    {
        Map map{};
        map.set_row(0, 0x3EF);
        map.set_row(1, 0x3C7);
        map.set_row(2, 0x3CF);
        return map;
    }

    template<class Spec>
    TetrisBlockStatus spawn_of(char t)
    {
        auto [x, y] = Spec::spawn(t, Spec::width, Spec::height);
        return TetrisBlockStatus(t, static_cast<int8_t>(x), static_cast<int8_t>(y), 0);
    }

    // Wraps a 4-arg AI and checks every AIEnv against the queue actually left after the placement.
    template<class Base>
    class QueueProbe : public Base
    {
    public:
        std::string pieces;
        mutable size_t calls = 0;
        mutable size_t mismatches = 0;

        typename Base::Status get(typename Base::Result const &result, size_t depth, typename Base::Status const &status, m_tetris2::AIEnv const &env) const
        {
            ++calls;
            size_t drawn = depth + 1 + (env.hold != ' ' ? 1 : 0);
            std::string_view expected = drawn < pieces.size() ? std::string_view(pieces).substr(drawn) : std::string_view();
            std::string_view seen = env.length == 0 ? std::string_view() : std::string_view(env.next, env.length);
            if (seen != expected && ++mismatches <= 3)
            {
                std::printf("  env mismatch depth=%zu node=%c hold=%c is_hold=%d next=\"%.*s\" expected=\"%.*s\"\n",
                            depth, env.node, env.hold, int(env.is_hold), int(seen.size()), seen.data(), int(expected.size()), expected.data());
            }
            return Base::get(result, depth, status, env);
        }
    };

    template<class Engine>
    void configure_host(Engine &engine)
    {
        engine.ai_config()->table = kComboTable;
        engine.ai_config()->table_max = kComboTableMax;
        *engine.status() = {};
        engine.update();
    }

    template<class Engine>
    void check_queue_probe(Engine &engine, char const *label)
    {
        auto const *ai = engine.ai();
        std::printf("%s: %zu get calls, %zu env mismatches\n", label, ai->calls, ai->mismatches);
        expect(ai->calls > 0, label);
        expect(ai->mismatches == 0, label);
    }

    template<class Engine>
    bool is_tsd(Engine &engine, typename Engine::LandPoint const &lp, typename Engine::map_t map)
    {
        size_t clear = engine.attach(lp, map);
        return lp.state.t == 'T' && clear == 2 && map.row(0) == 0x3CF && map.row(1) == 0;
    }

    void toj_v08_picks_tsd_and_sees_queue()
    {
        using Engine = m_tetris2::TetrisEngine<rule_toj::TetrisRule, QueueProbe<ai_zzz::TOJ_v08>, tspin::Search>;
        using Spec = rule_toj::TetrisRule::rule_spec;
        Engine engine;
        expect(engine.prepare(Spec::width, Spec::height), "toj prepare");
        configure_host(engine);
        engine.ai()->pieces = "TIOLJSZ";
        auto map = tsd_board<Engine::map_t>();
        auto result = engine.run_hold(map, spawn_of<Spec>('T'), ' ', true, "IOLJSZ", 6, 300);
        check_queue_probe(engine, "TOJ_v08 env queue");
        expect(!result.change_hold, "TOJ_v08 keeps T");
        expect(result.target.type == ai_zzz::TOJ_v08::TSpinType::TSpin, "TOJ_v08 target is a T-spin");
        auto before = map;
        expect(is_tsd(engine, result.target, map), "TOJ_v08 target is the TSD");
        size_t clear = engine.attach(result.target, map);
        expect(clear == 2, "attach returns cleared lines");
        expect(map != before && map.row(0) == 0x3CF && map.row(1) == 0, "attach mutates the caller's map");
    }

    void botris_sees_queue()
    {
        using Engine = m_tetris2::TetrisEngine<rule_botris::TetrisRule, QueueProbe<ai_zzz::Botris>, aspin::Search>;
        using Spec = rule_botris::TetrisRule::rule_spec;
        Engine engine;
        expect(engine.prepare(Spec::width, Spec::height), "botris prepare");
        configure_host(engine);
        engine.ai()->pieces = "TIOLJSZ";
        auto map = tsd_board<Engine::map_t>();
        auto result = engine.run_hold(map, spawn_of<Spec>('T'), ' ', true, "IOLJSZ", 6, 300);
        check_queue_probe(engine, "Botris env queue");
        expect(result.target != nullptr, "Botris finds a move");
    }

    template<class Rule>
    void check_t_spin_types(char const *rule_name)
    {
        using Engine = m_tetris2::TetrisEngine<Rule, ai_zzz::TOJ_v08, tspin::Search>;
        using Spec = typename Rule::rule_spec;
        Engine engine;
        expect(engine.prepare(Spec::width, Spec::height), "tspin prepare");
        for (bool is_20g : {false, true})
        {
            engine.search_config()->is_20g = is_20g;
            std::vector<typename Engine::LandPoint> lps;
            typename Engine::map_t empty{};
            engine.search(spawn_of<Spec>('T'), empty, lps);
            size_t spins = 0;
            for (auto const &lp : lps)
            {
                spins += lp.type != ai_zzz::TOJ_v08::TSpinType::None;
            }
            std::printf("%s %s: empty board %zu T landings, %zu spin-typed\n", rule_name, is_20g ? "20g" : "1g", lps.size(), spins);
            expect(!lps.empty() && spins == 0, "empty board has no 3-corner T landing");

            if (is_20g)
            {
                continue;
            }
            auto map = tsd_board<typename Engine::map_t>();
            engine.search(spawn_of<Spec>('T'), map, lps);
            size_t tsd = 0;
            for (auto const &lp : lps)
            {
                if (is_tsd(engine, lp, map))
                {
                    ++tsd;
                    expect(lp.is_last_rotate && lp.type == ai_zzz::TOJ_v08::TSpinType::TSpin, "TSD landing is typed TSpin");
                }
            }
            expect(tsd == 1, "TSD landing found once");
        }
    }

    template<class Spec, std::uint8_t R, class Map>
    void check_corners_for_rotation(Map const &board, char const *label)
    {
        using Helpers = m_tetris2::Helpers<Spec>;
        typename m_tetris2::TSpinHook::template RotState<Map, Helpers::kMaxR> rs{};
        m_tetris2::TSpinHook::on_init_rotations<Spec, 'T', Map, Helpers::kMaxR>(board, rs);
        constexpr auto pivot = m_tetris2::shape::piece_cells<Spec, 'T', R>.pivot;
        auto occupied = [&](int x, int y)
        {
            return x < 0 || x >= Map::width || y < 0 || y >= Map::height || board.get(x, y);
        };
        size_t wrong = 0;
        for (int y = 0; y < Map::height; ++y)
        {
            for (int x = 0; x < Map::width; ++x)
            {
                int cx = x + pivot.x;
                int cy = y + pivot.y;
                int corners = occupied(cx - 1, cy - 1) + occupied(cx + 1, cy - 1) + occupied(cx - 1, cy + 1) + occupied(cx + 1, cy + 1);
                wrong += rs.corners3_arr[R].get(x, y) != (corners >= 3);
            }
        }
        expect(wrong == 0, label);
    }

    template<class Rule>
    void check_corner_tables(char const *rule_name)
    {
        using Spec = typename Rule::rule_spec;
        using Map = typename m_tetris2::Helpers<Spec>::map_t;
        std::printf("%s: corners3 vs brute force\n", rule_name);
        for (Map const &board : {Map{}, tsd_board<Map>()})
        {
            [&]<std::size_t... Rs>(std::index_sequence<Rs...>)
            {
                (check_corners_for_rotation<Spec, std::uint8_t(Rs)>(board, "corners3 counts occupied or out-of-bounds corners"), ...);
            }(std::make_index_sequence<m_tetris2::shape::rotation_count<Spec, 'T'>>{});
        }
    }

    // Minimal AI with iterate(): exercises the virtual-piece check on the held tail.
    class IterateProbe
    {
    public:
        struct Config
        {
        };
        using Result = int;
        struct Status
        {
            double value;
            bool operator<(Status const &other) const
            {
                return value < other.value;
            }
        };
        template<class Spec>
        void init(Config const *)
        {
        }
        std::string ai_name() const
        {
            return "IterateProbe";
        }
        template<class Details>
        Result eval(m_tetris2::TetrisNode<Details, std::monostate> const &, typename Details::map_type const &map, typename Details::map_type const &, size_t clear) const
        {
            return int(clear) * 100 - map.popcount();
        }
        Status get(Result const &result, size_t, Status const &status, m_tetris2::AIEnv const &) const
        {
            return {status.value + result};
        }
        Status iterate(Status const **status, size_t length) const
        {
            Status best{-1e9};
            for (size_t i = 0; i < length; ++i)
            {
                if (status[i] != nullptr && best < *status[i])
                {
                    best = *status[i];
                }
            }
            return best;
        }
    };

    void iterate_with_held_tail()
    {
        using Engine = m_tetris2::TetrisEngine<rule_srs::TetrisRule, IterateProbe, path::Search>;
        using Spec = rule_srs::TetrisRule::rule_spec;
        Engine engine;
        expect(engine.prepare(Spec::width, Spec::height), "iterate prepare");
        Engine::map_t map{};
        auto result = engine.run_hold(map, spawn_of<Spec>('T'), 'I', true, "O?", 2, 1000);
        std::printf("iterate + held tail: target %c\n", result.target != nullptr ? char(result.target.state.t) : '-');
        expect(result.target != nullptr, "iterate AI with held tail finds a move");
    }

    // Prefers the leftmost landing, so it walks around any wall the active piece can actually pass.
    class LeftmostProbe
    {
    public:
        struct Config
        {
        };
        using Result = int;
        struct Status
        {
            double value;
            bool operator<(Status const &other) const
            {
                return value < other.value;
            }
        };
        template<class Spec>
        void init(Config const *)
        {
        }
        std::string ai_name() const
        {
            return "LeftmostProbe";
        }
        template<class Details>
        Result eval(m_tetris2::TetrisNode<Details, std::monostate> const &lp, typename Details::map_type const &, typename Details::map_type const &, size_t) const
        {
            return -int(lp.x);
        }
        Status get(Result const &result, size_t, Status const &status) const
        {
            return {status.value + result};
        }
    };

    template<class Spec>
    bool path_reaches(TetrisBlockStatus const &from, m_tetris2::State const &target, std::vector<char> const &path, typename m_tetris2::Helpers<Spec>::map_t const &map)
    {
        using H = m_tetris2::Helpers<Spec>;
        std::array<typename H::map_t, H::kMaxR> usable{};
        H::build_usable_for_piece(from.t, map, usable);
        auto cs = H::state_from_status(from.t, static_cast<std::uint8_t>(from.r), from.x, from.y);
        auto shift = [&](int dx, int dy, bool to_wall)
        {
            while (H::usable_at(cs.r, cs.xb + dx, cs.yb + dy, usable))
            {
                cs.xb = static_cast<std::int8_t>(cs.xb + dx);
                cs.yb = static_cast<std::int8_t>(cs.yb + dy);
                if (!to_wall)
                    break;
            }
        };
        auto rotate = [&](m_tetris2::KickDir dir)
        {
            if (auto kicked = H::first_kick(static_cast<char>(cs.t), dir, cs, usable))
                cs = *kicked;
        };
        for (char key : path)
        {
            switch (key)
            {
            case 'l': shift(-1, 0, false); break;
            case 'r': shift(1, 0, false); break;
            case 'd': shift(0, -1, false); break;
            case 'L': shift(-1, 0, true); break;
            case 'R': shift(1, 0, true); break;
            case 'D': shift(0, -1, true); break;
            case 'z': rotate(m_tetris2::KickDir::Ccw); break;
            case 'c': rotate(m_tetris2::KickDir::Cw); break;
            case 'x': rotate(m_tetris2::KickDir::Opp); break;
            default: return false;
            }
        }
        shift(0, -1, true);
        return cs.r == target.r && cs.xb == target.xb && cs.yb == target.yb;
    }

    // A wall right of the spawn's left edge, one row short of the spawn: a piece one row below spawn cannot cross it.
    template<class Rule, class SearchTag>
    void lowered_active_piece_searches_from_its_position(char const *name)
    {
        using Engine = m_tetris2::TetrisEngine<Rule, LeftmostProbe, SearchTag>;
        using Spec = typename Rule::rule_spec;
        std::printf("%s:\n", name);
        Engine engine;
        expect(engine.prepare(Spec::width, Spec::height), "lowered prepare");
        TetrisBlockStatus spawn = spawn_of<Spec>('O');
        TetrisBlockStatus lowered(spawn.t, spawn.x, static_cast<int8_t>(spawn.y - 1), spawn.r);
        auto spawn_state = m_tetris2::Helpers<Spec>::state_from_status('O', 0, spawn.x, spawn.y);
        int wall_x = spawn_state.xb - 1;
        typename Engine::map_t map{};
        for (int y = 0; y < spawn_state.yb; ++y)
        {
            map.set(wall_x, y);
        }
        auto run_from = [&](TetrisBlockStatus const &from, char const *label)
        {
            auto target = engine.run(map, from, "O", 1, 300).target;
            auto path = engine.make_path(from, target, map);
            std::printf("%s: target xb=%d yb=%d path=\"%.*s\"\n", label, target.state.xb, target.state.yb, int(path.size()), path.data());
            expect(target != nullptr && path_reaches<Spec>(from, target.state, path, map), label);
            return target;
        };
        expect(run_from(lowered, "lowered start").state.xb > wall_x, "lowered start stays right of the wall");
        expect(run_from(spawn, "spawn start, same root").state.xb == 0, "spawn start crosses the wall");
        expect(run_from(lowered, "lowered start, same root").state.xb > wall_x, "lowered start after spawn start stays right of the wall");

        auto first = run_from(spawn, "spawn start before placing");
        engine.attach(first, map);
        expect(run_from(lowered, "lowered start, reused child root").state.xb > wall_x, "reused root drops spawn-start children");
    }
} // namespace

int main()
{
    toj_v08_picks_tsd_and_sees_queue();
    botris_sees_queue();
    check_t_spin_types<rule_srs::TetrisRule>("rule_srs");
    check_t_spin_types<rule_toj::TetrisRule>("rule_toj");
    check_t_spin_types<rule_botris::TetrisRule>("rule_botris");
    check_corner_tables<rule_srs::TetrisRule>("rule_srs");
    check_corner_tables<rule_toj::TetrisRule>("rule_toj");
    check_corner_tables<rule_botris::TetrisRule>("rule_botris");
    iterate_with_held_tail();
    lowered_active_piece_searches_from_its_position<rule_srs::TetrisRule, path::Search>("rule_srs path");
    lowered_active_piece_searches_from_its_position<rule_io::TetrisRule, amini::Search>("rule_io amini");
    std::printf("%s (%d failures)\n", g_failures == 0 ? "PASS" : "FAIL", g_failures);
    return g_failures == 0 ? 0 : 1;
}
