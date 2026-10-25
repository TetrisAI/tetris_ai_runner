// amini_witness_test: amini::Search on rule_io against the independent TETR.IO model (_amini_reference.h).
//
// For every board (34 diff fixtures + the aspin extras + hand-made spin setups), every piece, every key set
// (1g, 1g without 'd', 1g without 180, 20g) and every spin rule (T-spins, T-spins+, all-mini, all-mini+):
//   1. completeness/soundness: the LandPoint set {(cells, class)} equals the reference's realizable set
//      (None if realizable, plus the best spin class);
//   2. exact witness: make_path(lp) replayed by the reference from its own spawn locks exactly lp's cells with
//      exactly lp.type, every key moves the piece, and only allowed keys appear.
// The reference spawns independently (flat row 21), so (2) also proves that the engine's spawn State agrees.

#include "_amini_reference.h"
#include "_diff_harness.h"

#include "../src/rule_io.h"
#include "../src/search_amini.h"
#include "../src/search_path.h"

#include <cstdio>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace
{
    using Rule = rule_io::TetrisRule;
    using Spec = Rule::rule_spec;
    using H = m_tetris2::Helpers<Spec>;
    using Searcher = amini::Search::type<Rule>;
    using LandPoint = search_amini::Search::TetrisNodeWithAMiniType;
    using Config = search_amini::Search::Config;

    struct Mode
    {
        char const *name;
        Config config;
    };

    // amini_ref boards have 1 = occupied; TetrisMap stores 0 = occupied.
    amini_ref::Board to_ref(tetris_diff::NewMap const &m)
    {
        amini_ref::Board b;
        for (int y = 0; y < amini_ref::kH; ++y)
            b.rows[y] = static_cast<std::uint16_t>(m.occupied_row(y));
        return b;
    }

    tetris_diff::NewMap to_engine(amini_ref::Board const &b)
    {
        tetris_diff::NewMap m{};
        for (int y = 0; y < amini_ref::kH; ++y)
            m.set_row(y, static_cast<tetris_diff::NewMap::row_t>(tetris_diff::NewMap::row_full ^ b.rows[y]));
        return m;
    }

    amini_ref::Cells cells_of(LandPoint const &lp)
    {
        m_tetris2::CellsKey k = H::cells_key_for_state(lp.state);
        return {k.c[0], k.c[1], k.c[2], k.c[3]};
    }

    std::string format(amini_ref::Cells const &c)
    {
        std::string s;
        for (int v : c)
            s += "(" + std::to_string(v % amini_ref::kW) + "," + std::to_string(v / amini_ref::kW) + ")";
        return s;
    }

    bool key_allowed(Config const &cfg, char key)
    {
        switch (key)
        {
        case 'l':
        case 'r':
        case 'z':
        case 'c':
        case 'D':
            return true;
        case 'L':
        case 'R':
            return cfg.allow_LR;
        case 'd':
            return cfg.allow_d && !cfg.is_20g;
        case 'x':
            return cfg.allow_180;
        }
        return false;
    }

    struct Stats
    {
        int cases = 0;
        int landpoints = 0;
        int spins = 0;
        int failures = 0;
    };

    amini_ref::Rule ref_rule(search_amini::Search::SpinRule rule)
    {
        using SpinRule = search_amini::Search::SpinRule;
        switch (rule)
        {
        case SpinRule::TSpins: return amini_ref::Rule::TSpins;
        case SpinRule::TSpinsPlus: return amini_ref::Rule::TSpinsPlus;
        case SpinRule::AllMini: return amini_ref::Rule::AllMini;
        case SpinRule::AllMiniPlus: return amini_ref::Rule::AllMiniPlus;
        }
        return amini_ref::Rule::AllMiniPlus;
    }

    void check_case(Stats &stats, Mode const &mode, char const *rule_name, std::string const &board_name,
                    amini_ref::Board const &ref_board, char piece)
    {
        ++stats.cases;
        tetris_diff::NewMap board = to_engine(ref_board);
        Searcher searcher;
        searcher.init(&mode.config);
        auto sp = Spec::spawn(piece, Spec::width, Spec::height);
        m_tetris2::State spawn = H::state_from_status(piece, 0, sp.first, sp.second);
        std::vector<LandPoint> lps = *searcher.search(board, spawn, 0);

        auto fail = [&](std::string const &what)
        {
            ++stats.failures;
            if (stats.failures <= 60)
                std::fprintf(stderr, "[FAIL] mode=%s rule=%s board=%s piece=%c %s\n", mode.name, rule_name,
                             board_name.c_str(), piece, what.c_str());
        };

        std::set<std::pair<amini_ref::Cells, int>> engine;
        for (LandPoint const &lp : lps)
        {
            if (!engine.insert({cells_of(lp), lp.type}).second)
                fail("duplicate LandPoint " + format(cells_of(lp)) + " type=" + std::to_string(lp.type));
        }
        amini_ref::Keys keys{mode.config.allow_180, mode.config.allow_d};
        std::set<std::pair<amini_ref::Cells, int>> expected =
            amini_ref::realizable(ref_board, piece, mode.config.is_20g, keys, ref_rule(mode.config.spin_rule));
        for (auto const &e : expected)
            if (!engine.count(e))
                fail("missing " + format(e.first) + " type=" + std::to_string(e.second));
        for (auto const &e : engine)
            if (!expected.count(e))
                fail("unrealizable " + format(e.first) + " type=" + std::to_string(e.second));

        for (LandPoint const &lp : lps)
        {
            ++stats.landpoints;
            if (lp.type != search_amini::Search::None)
                ++stats.spins;
            std::vector<char> path = searcher.make_path(spawn, lp, board);
            std::string keys_str(path.begin(), path.end());
            std::optional<amini_ref::Game> game =
                amini_ref::spawn(ref_board, piece, mode.config.is_20g, ref_rule(mode.config.spin_rule));
            if (!game)
            {
                fail("reference spawn blocked");
                continue;
            }
            bool replay_ok = true;
            for (char key : path)
            {
                if (!key_allowed(mode.config, key) || !amini_ref::press(*game, key))
                {
                    fail("path \"" + keys_str + "\" key '" + std::string(1, key) + "' is not a move");
                    replay_ok = false;
                    break;
                }
            }
            if (!replay_ok)
                continue;
            auto [cells, spin] = amini_ref::lock(*game);
            if (cells != cells_of(lp) || static_cast<int>(spin) != static_cast<int>(lp.type))
                fail("path \"" + keys_str + "\" locks " + format(cells) + " type=" + std::to_string(spin) +
                     ", want " + format(cells_of(lp)) + " type=" + std::to_string(lp.type));
        }
    }
} // namespace

int main()
{
    std::vector<std::pair<std::string, amini_ref::Board>> boards;
    for (tetris_diff::Fixture const &fx : tetris_diff::build_all_aspin_fixtures())
        boards.emplace_back(fx.name, to_ref(fx.board));
    for (auto const &[name, board] : amini_ref::hand_fixtures())
        boards.emplace_back(name, board);

    Mode const modes[] = {
        {"1g", Config{}},
        {"1g-no-d", Config{.allow_180 = true, .allow_d = false, .allow_LR = true, .is_20g = false}},
        {"1g-no-180-no-LR", Config{.allow_180 = false, .allow_d = true, .allow_LR = false, .is_20g = false}},
        {"20g", Config{.allow_180 = true, .allow_d = true, .allow_LR = true, .is_20g = true}},
    };

    std::pair<char const *, search_amini::Search::SpinRule> const rules[] = {
        {"T-spins", search_amini::Search::SpinRule::TSpins},
        {"T-spins+", search_amini::Search::SpinRule::TSpinsPlus},
        {"all-mini", search_amini::Search::SpinRule::AllMini},
        {"all-mini+", search_amini::Search::SpinRule::AllMiniPlus},
    };

    Stats stats;
    for (auto const &[rule_name, rule] : rules)
        for (Mode mode : modes)
        {
            mode.config.spin_rule = rule;
            for (auto const &[name, board] : boards)
                for (char piece : std::string("OITLJSZ"))
                    check_case(stats, mode, rule_name, name, board, piece);
        }

    std::fprintf(stderr, "# amini_witness: boards=%zu cases=%d landpoints=%d spin_landpoints=%d failures=%d\n",
                 boards.size(), stats.cases, stats.landpoints, stats.spins, stats.failures);
    return stats.failures == 0 ? 0 : 1;
}
