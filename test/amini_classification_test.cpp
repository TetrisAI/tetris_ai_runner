// amini_classification_test: fixed TETR.IO spin outcomes for rule_io + AMiniRuleHook + amini::Search under each
// spinbonuses rule (T-spins, T-spins+, all-mini, all-mini+).
//
//   1. spawn: every piece's engine spawn State covers the reference spawn cells (flat row 21).
//   2. rotation table: hand-checked rotations (start, key) -> (end, TETR.IO kick index, class per rule); the
//      engine's first_kick_hit + AMiniRuleHook::classify_rotation and the reference (_amini_reference.h) must match.
//   3. search: the LandPoint classes amini::Search emits at a few hand-checked cells, per rule.

#include "_amini_reference.h"

#include "../src/rule_io.h"
#include "../src/search_amini.h"
#include "../src/search_path.h"

#include <cstdio>
#include <set>
#include <string>
#include <vector>

namespace
{
    using Rule = rule_io::TetrisRule;
    using Spec = Rule::rule_spec;
    using H = m_tetris2::Helpers<Spec>;
    using MapT = H::map_t;
    using Searcher = amini::Search::type<Rule>;
    using amini_ref::Board;
    using amini_ref::Cells;
    using amini_ref::Piece;
    using amini_ref::Spin;

    int g_failures = 0;

    void expect(bool ok, std::string const &what)
    {
        if (ok)
            return;
        ++g_failures;
        std::fprintf(stderr, "[FAIL] %s\n", what.c_str());
    }

    // amini_ref boards have 1 = occupied; TetrisMap stores 0 = occupied.
    MapT to_engine(Board const &b)
    {
        MapT m{};
        for (int y = 0; y < amini_ref::kH; ++y)
            m.set_row(y, static_cast<MapT::row_t>(MapT::row_full ^ b.rows[y]));
        return m;
    }

    Cells engine_cells(m_tetris2::State const &s)
    {
        m_tetris2::CellsKey k = H::cells_key_for_state(s);
        return {k.c[0], k.c[1], k.c[2], k.c[3]};
    }

    // The engine State covering the same cells: rule_io keeps TETR.IO's rotation numbering and its State is the
    // bottom-left corner of the cells' bounding box.
    m_tetris2::State engine_state(Piece const &p)
    {
        Cells c = amini_ref::cells(p);
        int x = amini_ref::kW;
        int y = amini_ref::kH;
        for (int v : c)
        {
            x = std::min(x, v % amini_ref::kW);
            y = std::min(y, v / amini_ref::kW);
        }
        return {static_cast<std::uint8_t>(p.t), static_cast<std::uint8_t>(p.r), static_cast<std::int8_t>(x),
                static_cast<std::int8_t>(y)};
    }

    using SpinRule = search_amini::Search::SpinRule;
    // Every spin rule, in the order of RotationCase::spin.
    constexpr std::array<std::pair<SpinRule, amini_ref::Rule>, 4> kRules{{
        {SpinRule::TSpins, amini_ref::Rule::TSpins},
        {SpinRule::TSpinsPlus, amini_ref::Rule::TSpinsPlus},
        {SpinRule::AllMini, amini_ref::Rule::AllMini},
        {SpinRule::AllMiniPlus, amini_ref::Rule::AllMiniPlus},
    }};
    char const *const kRuleNames[] = {"T-spins", "T-spins+", "all-mini", "all-mini+"};

    template<SpinRule Rule, char... Ts>
    Spin engine_classify(char t, MapT const &board, std::array<MapT, H::kMaxR> const &usable,
                         m_tetris2::State const &to, std::uint8_t test)
    {
        Spin out = amini_ref::None;
        ((t == Ts ? (out = static_cast<Spin>(
                         m_tetris2::AMiniRuleHook<Rule>::template classify_rotation<Spec, Ts>(board, usable, to, test)),
                     true)
                  : false) ||
         ...);
        return out;
    }

    Spin engine_classify(SpinRule rule, char t, MapT const &board, std::array<MapT, H::kMaxR> const &usable,
                         m_tetris2::State const &to, std::uint8_t test)
    {
        switch (rule)
        {
        case SpinRule::TSpins:
            return engine_classify<SpinRule::TSpins, 'T', 'S', 'Z', 'L', 'J', 'I', 'O'>(t, board, usable, to, test);
        case SpinRule::TSpinsPlus:
            return engine_classify<SpinRule::TSpinsPlus, 'T', 'S', 'Z', 'L', 'J', 'I', 'O'>(t, board, usable, to,
                                                                                            test);
        case SpinRule::AllMini:
            return engine_classify<SpinRule::AllMini, 'T', 'S', 'Z', 'L', 'J', 'I', 'O'>(t, board, usable, to, test);
        case SpinRule::AllMiniPlus:
            return engine_classify<SpinRule::AllMiniPlus, 'T', 'S', 'Z', 'L', 'J', 'I', 'O'>(t, board, usable, to,
                                                                                             test);
        }
        return amini_ref::None;
    }

    void check_spawn()
    {
        for (char t : std::string("OITLJSZ"))
        {
            auto sp = Spec::spawn(t, Spec::width, Spec::height);
            m_tetris2::State s = H::state_from_status(t, 0, sp.first, sp.second);
            std::optional<amini_ref::Game> ref = amini_ref::spawn(Board{}, t, false, amini_ref::Rule::AllMiniPlus);
            expect(ref && engine_cells(s) == amini_ref::cells(ref->piece), std::string("spawn cells of ") + t);
        }
    }

    struct RotationCase
    {
        char const *name;
        Board board;
        Piece from;
        char key;
        Piece to;
        int kick;
        std::array<Spin, 4> spin; // per kRules
    };

    void check_rotation(RotationCase const &c)
    {
        int turns = 3;
        if (c.key == 'c')
            turns = 1;
        else if (c.key == 'x')
            turns = 2;
        int const to_r = (c.from.r + turns) % 4;

        expect(amini_ref::fits(c.board, c.from), std::string(c.name) + ": start does not fit");
        std::optional<amini_ref::Rotation> ref = amini_ref::try_rotate(c.board, c.from, to_r);
        expect(ref && ref->piece == c.to && ref->kick == c.kick, std::string(c.name) + ": reference kick");

        MapT board = to_engine(c.board);
        std::array<MapT, H::kMaxR> usable{};
        H::build_usable_for_piece(c.from.t, board, usable);
        m_tetris2::KickDir dir = c.key == 'c'   ? m_tetris2::KickDir::Cw
                                 : c.key == 'z' ? m_tetris2::KickDir::Ccw
                                                : m_tetris2::KickDir::Opp;
        std::optional<m_tetris2::KickHit> hit = H::first_kick_hit(c.from.t, dir, engine_state(c.from), usable);
        expect(hit && engine_cells(hit->state) == amini_ref::cells(c.to), std::string(c.name) + ": engine kick target");
        expect(hit && (hit->test == 0 ? 0 : hit->test - 1) == c.kick, std::string(c.name) + ": engine kick index");

        for (std::size_t i = 0; i < kRules.size(); ++i)
        {
            std::string const name = std::string(c.name) + " [" + kRuleNames[i] + "]";
            expect(ref && amini_ref::classify(c.board, ref->piece, ref->kick, kRules[i].second) == c.spin[i],
                   name + ": reference class");
            if (hit)
                expect(engine_classify(kRules[i].first, c.from.t, board, usable, hit->state, hit->test) == c.spin[i],
                       name + ": engine class");
        }
    }

    std::set<int> classes_at(Board const &board, char t, Cells const &cells, SpinRule rule)
    {
        Searcher searcher;
        search_amini::Search::Config config{};
        config.spin_rule = rule;
        searcher.init(&config);
        auto sp = Spec::spawn(t, Spec::width, Spec::height);
        std::set<int> out;
        for (auto const &lp : *searcher.search(to_engine(board), H::state_from_status(t, 0, sp.first, sp.second), 0))
            if (engine_cells(lp.state) == cells)
                out.insert(lp.type);
        return out;
    }

    Board fixture(std::string const &name)
    {
        for (auto const &[n, b] : amini_ref::hand_fixtures())
            if (n == name)
                return b;
        expect(false, "unknown fixture " + name);
        return {};
    }

    Cells cells_at(std::initializer_list<std::pair<int, int>> xy)
    {
        Cells c{};
        int i = 0;
        for (auto [x, y] : xy)
            c[i++] = y * amini_ref::kW + x;
        std::sort(c.begin(), c.end());
        return c;
    }
} // namespace

int main()
{
    check_spawn();

    using amini_ref::Full;
    using amini_ref::Mini;
    using amini_ref::None;
    // Expected classes per rule: {T-spins, T-spins+, all-mini, all-mini+}.
    RotationCase const rotations[] = {
        // Plain CW into the TSD slot: corners (3,0) (5,0) (3,2), both front (bottom) corners.
        {"tsd", fixture("tsd"), {'T', 1, 4, 1}, 'c', {'T', 2, 4, 1}, 0, {Full, Full, Full, Full}},
        // Tests (0,0) (-1,0) (-1,+1) (0,-2) fail; listed kick 3 (-1,-2) lands at (3,1): 3 corners, one front
        // corner -> Full only because of the kick index.
        {"tst_upgrade", fixture("tst_upgrade"), {'T', 0, 4, 3}, 'c', {'T', 1, 3, 1}, 3, {Full, Full, Full, Full}},
        {"tst_upgrade_mirror", fixture("tst_upgrade_mirror"), {'T', 0, 5, 3}, 'z', {'T', 3, 6, 1}, 3, {Full, Full, Full, Full}},
        // 180 1->3: listed kick 3 lifts the T by two rows; corners (3,2) (5,2) (5,4), one front corner -> Full.
        {"t_180_upgrade", fixture("t_180_upgrade"), {'T', 1, 4, 1}, 'x', {'T', 3, 4, 3}, 3, {Full, Full, Full, Full}},
        // 180 2->0 with listed kick 0 (0,-1): only the floor corners are occupied, but the T cannot move -> Mini
        // where an immobile T scores ("+" rules).
        {"t_immobile", fixture("t_immobile"), {'T', 2, 4, 2}, 'x', {'T', 0, 4, 1}, 0, {None, Mini, None, Mini}},
        // CCW 1->0 with listed kick 1 (+1,-1): corners (1,-1) (3,-1) (3,1), front (3,1) only -> Mini.
        {"t_mini", fixture("t_mini"), {'T', 1, 1, 1}, 'z', {'T', 0, 2, 0}, 1, {Mini, Mini, Mini, Mini}},
        // Plain 180 1->3: corners (3,0) (5,0) (3,2), both front (left) corners -> Full.
        {"t_180_full", fixture("tsd"), {'T', 1, 4, 1}, 'x', {'T', 3, 4, 1}, 0, {Full, Full, Full, Full}},
        // Non-T pieces only score through immobility, and only under the all-mini rules.
        {"s_mini", fixture("t_mini"), {'S', 1, 1, 1}, 'c', {'S', 2, 1, 1}, 0, {None, None, Mini, Mini}},
        {"z_mini_kicked", fixture("t_mini"), {'Z', 1, 1, 1}, 'c', {'Z', 2, 2, 1}, 0, {None, None, Mini, Mini}},
        {"l_180_mini", fixture("tst_upgrade"), {'L', 0, 4, 3}, 'x', {'L', 2, 4, 3}, 0, {None, None, Mini, Mini}},
        {"j_180_mini", fixture("tst_upgrade_mirror"), {'J', 0, 5, 3}, 'x', {'J', 2, 5, 3}, 0, {None, None, Mini, Mini}},
        // Tests (0,0) (+1,0) (-2,0) fail; listed kick 2 (-2,-1) drops the I into column 3, rows 0-3.
        {"i_mini_kicked", fixture("tst_upgrade"), {'I', 0, 4, 3}, 'c', {'I', 1, 2, 2}, 2, {None, None, Mini, Mini}},
    };
    for (RotationCase const &c : rotations)
        check_rotation(c);

    for (std::size_t i = 0; i < kRules.size(); ++i)
    {
        SpinRule const rule = kRules[i].first;
        std::string const tag = std::string(" [") + kRuleNames[i] + "]";
        expect(classes_at(fixture("tsd"), 'T', cells_at({{3, 1}, {4, 1}, {5, 1}, {4, 0}}), rule) == std::set<int>{2},
               "tsd: only Full" + tag);
        expect(classes_at(fixture("tst_upgrade"), 'T', cells_at({{3, 0}, {3, 1}, {3, 2}, {4, 1}}), rule) ==
                   std::set<int>{2},
               "tst_upgrade: only Full" + tag);
        expect(classes_at(fixture("tst_upgrade_mirror"), 'T', cells_at({{6, 0}, {6, 1}, {6, 2}, {5, 1}}), rule) ==
                   std::set<int>{2},
               "tst_upgrade_mirror: only Full" + tag);
        // T r1 at (4,1) on the TSD board: dropped in it is None, rotated in it is a 3-corner Mini.
        expect(classes_at(fixture("tsd"), 'T', cells_at({{4, 0}, {4, 1}, {5, 1}, {4, 2}}), rule) ==
                   std::set<int>{0, 1},
               "tsd: T r1 both None and Mini" + tag);
        // S r2 in the t_mini corner is immobile: a Mini only under the all-mini rules.
        bool const s_mini =
            classes_at(fixture("t_mini"), 'S', cells_at({{0, 0}, {1, 0}, {1, 1}, {2, 1}}), rule).count(1) != 0;
        expect(s_mini == (rule == SpinRule::AllMini || rule == SpinRule::AllMiniPlus), "t_mini: S Mini" + tag);
    }

    std::fprintf(stderr, "# amini_classification: failures=%d\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}
