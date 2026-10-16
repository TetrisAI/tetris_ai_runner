// ai_zzz_io_spin_slots_test: the spin-slot detector (src/ai_zzz_io_spin_slots.h) against the reference model
// (_amini_reference.h) under every spinbonuses rule, with and without 180s.
//
//   1. definition: on the hand fixtures and seeded random S2-like boards the detector reports exactly the spins the
//      reference finds by hard-dropping each rotation from the spawn rows and rotating once (grounded, >= 1 line);
//   2. soundness: each reported slot is in amini_ref::realizable (no soft drop) with at least the reported class;
//   3. find_spin_slots keeps the best slot per piece; hand expectations, expected misses, rule, pocket, cavity and
//      roof cases.

#include "_amini_reference.h"

#include "../src/rule_io.h"
#include "../src/ai_zzz_io_spin_slots.h"

#include <algorithm>
#include <bit>
#include <climits>
#include <cstdio>
#include <map>
#include <random>
#include <set>
#include <string>
#include <vector>

namespace
{
    using Spec = rule_io::TetrisRule::rule_spec;
    using H = m_tetris2::Helpers<Spec>;
    using MapT = H::map_t;
    using SpinRule = search_amini::Search::SpinRule;
    using amini_ref::Board;
    using amini_ref::Cells;
    using amini_ref::Piece;
    using Slots = std::array<ai_zzz::SpinSlot, m_tetris2::ai_rule_spec::piece_count<Spec>()>;

    constexpr std::array<std::pair<SpinRule, amini_ref::Rule>, 4> kRules{{
        {SpinRule::TSpins, amini_ref::Rule::TSpins},
        {SpinRule::TSpinsPlus, amini_ref::Rule::TSpinsPlus},
        {SpinRule::AllMini, amini_ref::Rule::AllMini},
        {SpinRule::AllMiniPlus, amini_ref::Rule::AllMiniPlus},
    }};
    char const *const kRuleNames[] = {"T-spins", "T-spins+", "all-mini", "all-mini+"};
    constexpr std::uint16_t kFullRow = (1u << amini_ref::kW) - 1;

    int g_failures = 0;

    void expect(bool ok, std::string const &what)
    {
        if (ok)
            return;
        ++g_failures;
        std::fprintf(stderr, "[FAIL] %s\n", what.c_str());
    }

    struct Found
    {
        char piece;
        Cells cells;
        int spin;
        int lines;

        auto operator<=>(Found const &) const = default;
    };

    MapT to_engine(Board const &b)
    {
        MapT m{};
        for (int y = 0; y < amini_ref::kH; ++y)
            m.set_row(y, b.rows[y]);
        return m;
    }

    std::string describe(Found const &f)
    {
        std::string s = std::string(1, f.piece) + " class " + std::to_string(f.spin) + " lines " +
                        std::to_string(f.lines) + " cells";
        for (int c : f.cells)
            s += " (" + std::to_string(c % amini_ref::kW) + "," + std::to_string(c / amini_ref::kW) + ")";
        return s;
    }

    template<SpinRule Rule>
    std::set<Found> detect(Board const &b, bool allow_180)
    {
        std::set<Found> out;
        ai_zzz::for_each_spin_slot<Spec, m_tetris2::AMiniRuleHook<Rule>>(
            to_engine(b), allow_180,
            [&](std::size_t i, m_tetris2::State const &s, io_rules::Spin spin, int lines)
            {
                m_tetris2::CellsKey k = H::cells_key_for_state(s);
                out.insert({m_tetris2::ai_rule_spec::piece_type_at<Spec>(i), {k.c[0], k.c[1], k.c[2], k.c[3]}, static_cast<int>(spin), lines});
            });
        return out;
    }

    std::set<Found> detect(SpinRule rule, Board const &b, bool allow_180)
    {
        switch (rule)
        {
        case SpinRule::TSpins: return detect<SpinRule::TSpins>(b, allow_180);
        case SpinRule::TSpinsPlus: return detect<SpinRule::TSpinsPlus>(b, allow_180);
        case SpinRule::AllMini: return detect<SpinRule::AllMini>(b, allow_180);
        case SpinRule::AllMiniPlus: return detect<SpinRule::AllMiniPlus>(b, allow_180);
        }
        return {};
    }

    Slots best_slots(SpinRule rule, Board const &b, bool allow_180)
    {
        MapT const m = to_engine(b);
        switch (rule)
        {
        case SpinRule::TSpins:
            return ai_zzz::find_spin_slots<Spec, m_tetris2::AMiniRuleHook<SpinRule::TSpins>>(m, allow_180);
        case SpinRule::TSpinsPlus:
            return ai_zzz::find_spin_slots<Spec, m_tetris2::AMiniRuleHook<SpinRule::TSpinsPlus>>(m, allow_180);
        case SpinRule::AllMini:
            return ai_zzz::find_spin_slots<Spec, m_tetris2::AMiniRuleHook<SpinRule::AllMini>>(m, allow_180);
        case SpinRule::AllMiniPlus:
            return ai_zzz::find_spin_slots<Spec, m_tetris2::AMiniRuleHook<SpinRule::AllMiniPlus>>(m, allow_180);
        }
        return {};
    }

    int piece_index(char t)
    {
        return m_tetris2::ai_rule_spec::piece_index<Spec>(t);
    }

    bool below_roof(Board const &b)
    {
        for (int y = ai_zzz::kSpinSlotRoof<Spec>; y < amini_ref::kH; ++y)
            if (b.rows[y] != 0)
                return false;
        return true;
    }

    int lines_of(Board const &b, Cells const &cells)
    {
        Board after = b;
        for (int c : cells)
            after.rows[c / amini_ref::kW] |= static_cast<std::uint16_t>(1u << (c % amini_ref::kW));
        int n = 0;
        for (std::uint16_t row : after.rows)
            n += row == kFullRow;
        return n;
    }

    Piece hard_drop(Board const &b, Piece p)
    {
        while (amini_ref::fits(b, amini_ref::moved(p, 0, -1)))
            p = amini_ref::moved(p, 0, -1);
        return p;
    }

    // The definition, built from reference primitives only: every rotation at every column of the spawn rows,
    // hard-dropped, then rotated once; kept when it rests, clears a line and classifies as a spin. O never rotates
    // in the engine (rule_io), so it has no slots.
    std::set<Found> drop_then_rotate(Board const &b, bool allow_180, amini_ref::Rule rule)
    {
        std::set<Found> out;
        for (char t : std::string("IJLSZT"))
            for (int r = 0; r < 4; ++r)
                for (int x = -2; x < amini_ref::kW + 2; ++x)
                {
                    Piece const spawn{t, r, x, 21};
                    if (!amini_ref::fits(b, spawn))
                        continue;
                    Piece const from = hard_drop(b, spawn);
                    for (int turns : {1, 3, 2})
                    {
                        if (turns == 2 && !allow_180)
                            continue;
                        std::optional<amini_ref::Rotation> rot = amini_ref::try_rotate(b, from, (r + turns) % 4);
                        if (!rot || amini_ref::fits(b, amini_ref::moved(rot->piece, 0, -1)))
                            continue;
                        Cells const cells = amini_ref::cells(rot->piece);
                        int const lines = lines_of(b, cells);
                        int const spin = amini_ref::classify(b, rot->piece, rot->kick, rule);
                        if (lines > 0 && spin != amini_ref::None)
                            out.insert({t, cells, spin, lines});
                    }
                }
        return out;
    }

    // Contract order: most base attack, then more lines, then the higher class.
    Slots best_of(std::set<Found> const &found)
    {
        Slots best{};
        auto key = [](int spin, int lines)
        { return std::array<int, 3>{io_rules::kBaseAttack[lines][spin], lines, spin}; };
        for (Found const &f : found)
        {
            ai_zzz::SpinSlot &slot = best[piece_index(f.piece)];
            if (key(f.spin, f.lines) > key(static_cast<int>(slot.spin), slot.lines))
                slot = {static_cast<io_rules::Spin>(f.spin), static_cast<std::uint8_t>(f.lines)};
        }
        return best;
    }

    bool same(Slots const &a, Slots const &b)
    {
        for (std::size_t i = 0; i < a.size(); ++i)
            if (a[i].spin != b[i].spin || a[i].lines != b[i].lines)
                return false;
        return true;
    }

    struct Stats
    {
        int boards = 0;
        int boards_with_slot = 0;
        int slots = 0;
        std::map<char, int> per_piece;
    };

    // Definition, soundness, find_spin_slots and T-only rules on one board, for every rule and 180 setting.
    void check_board(std::string const &name, Board const &b, Stats &stats)
    {
        bool const roof_ok = below_roof(b);
        bool any = false;
        for (std::size_t ri = 0; ri < kRules.size(); ++ri)
            for (bool allow_180 : {true, false})
            {
                auto const [rule, ref_rule] = kRules[ri];
                std::string const tag = name + " [" + kRuleNames[ri] + (allow_180 ? ", 180]" : ", no 180]");
                std::set<Found> const found = detect(rule, b, allow_180);
                std::set<Found> const want = roof_ok ? drop_then_rotate(b, allow_180, ref_rule) : std::set<Found>{};
                expect(found == want, tag + ": detector == drop-then-rotate reference");
                for (Found const &f : found)
                    if (!want.count(f))
                        std::fprintf(stderr, "  extra   %s\n", describe(f).c_str());
                for (Found const &f : want)
                    if (!found.count(f))
                        std::fprintf(stderr, "  missing %s\n", describe(f).c_str());
                expect(same(best_slots(rule, b, allow_180), best_of(found)), tag + ": find_spin_slots keeps the best");

                std::map<char, std::set<std::pair<Cells, int>>> realizable;
                for (Found const &f : found)
                {
                    expect(f.lines >= 1 && f.lines == lines_of(b, f.cells), tag + ": lines of " + describe(f));
                    if ((rule == SpinRule::TSpins || rule == SpinRule::TSpinsPlus) && f.piece != 'T')
                        expect(false, tag + ": non-T slot under a T-only rule: " + describe(f));
                    if (!realizable.count(f.piece))
                        realizable[f.piece] =
                            amini_ref::realizable(b, f.piece, false, {allow_180, false}, ref_rule);
                    int best = -1;
                    for (auto const &[cells, spin] : realizable[f.piece])
                        if (cells == f.cells)
                            best = std::max(best, spin);
                    expect(best >= f.spin, tag + ": realizable " + describe(f));
                }
                if (rule == SpinRule::AllMiniPlus && allow_180)
                {
                    stats.slots += static_cast<int>(found.size());
                    any = !found.empty();
                    for (Found const &f : found)
                        ++stats.per_piece[f.piece];
                }
            }
        ++stats.boards;
        stats.boards_with_slot += any;
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

    bool reported(std::set<Found> const &found, char piece, Cells const &cells)
    {
        for (Found const &f : found)
            if (f.piece == piece && f.cells == cells)
                return true;
        return false;
    }

    int best_realizable(Board const &b, char piece, Cells const &cells)
    {
        int best = -1;
        for (auto const &[c, spin] : amini_ref::realizable(b, piece, false, {true, false}, amini_ref::Rule::AllMiniPlus))
            if (c == cells)
                best = std::max(best, spin);
        return best;
    }

    int holes(Board const &b)
    {
        int n = 0;
        std::uint16_t cover = 0;
        for (int y = amini_ref::kH - 1; y >= 0; --y)
        {
            n += std::popcount(static_cast<std::uint16_t>(cover & ~b.rows[y] & kFullRow));
            cover |= b.rows[y];
        }
        return n;
    }

    // S2-like boards: 0-6 garbage rows with one hole each (the hole column moves with probability 0.3 per row), then
    // 3-27 random pieces hard-dropped with full rows cleared. Unscored boards take the lowest of 1-4 random (rotation,
    // column) tries; scored boards the best of all hard drops by 2 * top row + 4 * new holes + noise in [0, 16).
    // Dropping stops once the stack reaches row 15, so every board stays below the roof (row 19).
    Board random_board(std::mt19937 &rng, bool scored)
    {
        Board b;
        int const garbage = static_cast<int>(rng() % 7);
        int hole = static_cast<int>(rng() % amini_ref::kW);
        for (int y = 0; y < garbage; ++y)
        {
            if (rng() % 10 < 3)
                hole = static_cast<int>(rng() % amini_ref::kW);
            b.rows[y] = static_cast<std::uint16_t>(kFullRow & ~(1u << hole));
        }
        int const drops = 3 + static_cast<int>(rng() % 25);
        for (int d = 0; d < drops; ++d)
        {
            char const t = "OITLJSZ"[rng() % 7];
            std::optional<Piece> best;
            int best_score = INT_MAX;
            auto consider = [&](Piece const &spawn, bool noisy)
            {
                if (!amini_ref::fits(b, spawn))
                    return;
                Piece const p = hard_drop(b, spawn);
                Cells const cells = amini_ref::cells(p);
                int score = cells[3] / amini_ref::kW;
                if (noisy)
                {
                    Board after = b;
                    for (int c : cells)
                        after.rows[c / amini_ref::kW] |= static_cast<std::uint16_t>(1u << (c % amini_ref::kW));
                    score = 2 * score + 4 * (holes(after) - holes(b)) + static_cast<int>(rng() % 16);
                }
                if (score < best_score)
                {
                    best = p;
                    best_score = score;
                }
            };
            if (scored)
            {
                for (int r = 0; r < 4; ++r)
                    for (int x = -2; x < amini_ref::kW + 2; ++x)
                        consider({t, r, x, 21}, true);
            }
            else
            {
                int const tries = 1 + static_cast<int>(rng() % 4);
                for (int k = 0; k < tries; ++k)
                    consider({t, static_cast<int>(rng() % 4), static_cast<int>(rng() % amini_ref::kW), 21}, false);
            }
            if (!best)
                continue;
            for (int c : amini_ref::cells(*best))
                b.rows[c / amini_ref::kW] |= static_cast<std::uint16_t>(1u << (c % amini_ref::kW));
            Board cleared;
            int y = 0;
            for (std::uint16_t row : b.rows)
                if (row != kFullRow)
                    cleared.rows[y++] = row;
            b = cleared;
            int top = 0;
            for (int r = 0; r < amini_ref::kH; ++r)
                if (b.rows[r] != 0)
                    top = r;
            if (top >= 15)
                break;
        }
        return b;
    }

    // Every line-clearing spin realizable on the hand fixtures (all-mini+, 180s, no soft drop; O excluded since the
    // engine never rotates it) and whether the detector finds it.
    void check_fixture_recall()
    {
        using amini_ref::Full;
        using amini_ref::Mini;
        struct Expected
        {
            char const *fixture;
            Found slot;
            char const *miss; // why the detector cannot see it; nullptr when found
        };
        char const *const tuck = "tuck: every predecessor sits under the overhang above the column";
        char const *const roof = "the stack reaches the roof (row 19)";
        Expected const table[] = {
            {"tsd", {'T', cells_at({{4, 0}, {3, 1}, {4, 1}, {5, 1}}), Full, 2}, nullptr},
            {"tsd", {'T', cells_at({{4, 0}, {3, 1}, {4, 1}, {4, 2}}), Full, 1}, nullptr},
            {"tsd", {'T', cells_at({{3, 1}, {4, 1}, {5, 1}, {4, 2}}), Mini, 1}, nullptr},
            // T r1 in column 4 is where the drop itself lands; it only spins in by rotating back out of the TSD.
            {"tsd", {'T', cells_at({{4, 0}, {4, 1}, {5, 1}, {4, 2}}), Mini, 1}, "second rotation"},
            {"tst_upgrade", {'T', cells_at({{3, 0}, {3, 1}, {4, 1}, {3, 2}}), Full, 2}, tuck},
            {"tst_upgrade", {'S', cells_at({{4, 0}, {3, 1}, {4, 1}, {3, 2}}), Mini, 2}, tuck},
            {"tst_upgrade", {'L', cells_at({{3, 2}, {3, 3}, {4, 3}, {5, 3}}), Mini, 1}, tuck},
            {"tst_upgrade", {'I', cells_at({{3, 0}, {3, 1}, {3, 2}, {3, 3}}), Mini, 1}, tuck},
            {"tst_upgrade_mirror", {'T', cells_at({{6, 0}, {5, 1}, {6, 1}, {6, 2}}), Full, 2}, tuck},
            {"tst_upgrade_mirror", {'Z', cells_at({{5, 0}, {5, 1}, {6, 1}, {6, 2}}), Mini, 2}, tuck},
            {"tst_upgrade_mirror", {'J', cells_at({{6, 2}, {4, 3}, {5, 3}, {6, 3}}), Mini, 1}, tuck},
            {"tst_upgrade_mirror", {'I', cells_at({{6, 0}, {6, 1}, {6, 2}, {6, 3}}), Mini, 1}, tuck},
            {"t_180_upgrade", {'L', cells_at({{4, 1}, {5, 1}, {4, 2}, {4, 3}}), Mini, 2}, nullptr},
            {"t_immobile", {'L', cells_at({{3, 1}, {3, 2}, {4, 2}, {5, 2}}), Mini, 1}, nullptr},
            {"t_immobile", {'J', cells_at({{5, 1}, {3, 2}, {4, 2}, {5, 2}}), Mini, 1}, nullptr},
            {"spawn_ceiling", {'T', cells_at({{9, 20}, {8, 21}, {9, 21}, {9, 22}}), Mini, 1}, roof},
            {"spawn_floor", {'T', cells_at({{9, 19}, {8, 20}, {9, 20}, {9, 21}}), Mini, 1}, roof},
        };
        for (auto const &[name, b] : amini_ref::hand_fixtures())
        {
            std::set<Found> realizable;
            for (char t : std::string("TSZLJI"))
                for (auto const &[cells, spin] :
                     amini_ref::realizable(b, t, false, {true, false}, amini_ref::Rule::AllMiniPlus))
                    if (spin != amini_ref::None && lines_of(b, cells) > 0)
                        realizable.insert({t, cells, spin, lines_of(b, cells)});
            std::set<Found> listed;
            std::set<Found> const found = detect(SpinRule::AllMiniPlus, b, true);
            for (Expected const &e : table)
            {
                if (name != e.fixture)
                    continue;
                listed.insert(e.slot);
                if (e.miss == nullptr)
                    expect(found.count(e.slot) != 0, name + ": finds " + describe(e.slot));
                else
                    expect(!reported(found, e.slot.piece, e.slot.cells),
                           name + ": expected miss (" + e.miss + ") " + describe(e.slot));
            }
            expect(realizable == listed, name + ": the table lists every realizable line-clearing spin");
            for (Found const &f : realizable)
                if (!listed.count(f))
                    std::fprintf(stderr, "  unlisted %s\n", describe(f).c_str());
        }
    }

    void check_rule_cases()
    {
        using amini_ref::from_rows;
        Cells const tsd_cells = cells_at({{3, 1}, {4, 1}, {5, 1}, {4, 0}});
        int const t = piece_index('T');

        Slots const tsd = best_slots(SpinRule::AllMiniPlus, fixture("tsd"), true);
        expect(tsd[t].spin == io_rules::Spin::Full && tsd[t].lines == 2, "tsd: the best T slot is the TSD");
        for (auto const &[rule, ref_rule] : kRules)
            expect(best_slots(rule, fixture("tsd"), false)[t].spin == io_rules::Spin::Full, "tsd: T Full, every rule");

        // A pocket that clears nothing: the TSD is realizable but column 9 stays open in rows 0-1.
        Board const pocket = from_rows({"####.####.", "###...###.", "####......"});
        expect(best_realizable(pocket, 'T', tsd_cells) == amini_ref::Full, "pocket: the TSD is realizable");
        for (auto const &[rule, ref_rule] : kRules)
            expect(detect(rule, pocket, true).empty(), "pocket: nothing reported");

        // A sealed cavity: the TSD cells fit and would clear two rows, but row 2 covers them.
        Board const cavity = from_rows({"####.#####", "###...####", "######...."});
        expect(best_realizable(cavity, 'T', tsd_cells) == -1, "cavity: the TSD is unreachable");
        for (auto const &[rule, ref_rule] : kRules)
            expect(detect(rule, cavity, true).empty(), "cavity: nothing reported");

        // The roof: row 19 is the lowest row of I r1/r3 at the spawn position.
        static_assert(ai_zzz::kSpinSlotRoof<Spec> == 19);
        Board high = fixture("tsd");
        high.rows[18] = 1;
        expect(best_slots(SpinRule::AllMiniPlus, high, true)[t].spin == io_rules::Spin::Full, "roof: row 18 is free");
        high.rows[19] = 1;
        expect(detect(SpinRule::AllMiniPlus, high, true).empty(), "roof: row 19 disables the detector");
    }
} // namespace

int main()
{
    check_fixture_recall();
    check_rule_cases();

    Stats fixtures;
    for (auto const &[name, b] : amini_ref::hand_fixtures())
        check_board(name, b, fixtures);

    Stats random;
    std::mt19937 rng(20261015);
    for (int i = 0; i < 800; ++i)
        check_board("random #" + std::to_string(i), random_board(rng, i % 2 == 1), random);
    // The corpus must exercise the detector for every piece that rotates, not only empty answers.
    expect(random.boards_with_slot >= 40, "random boards with a slot");
    for (char t : std::string("TSZLJI"))
        expect(random.per_piece[t] > 0, std::string("random slots of ") + t);

    std::fprintf(stderr,
                 "# ai_zzz_io_spin_slots: fixtures %d/%d boards with slots, random %d/%d boards %d slots "
                 "(all-mini+, 180); failures=%d\n",
                 fixtures.boards_with_slot, fixtures.boards, random.boards_with_slot, random.boards, random.slots,
                 g_failures);
    return g_failures == 0 ? 0 : 1;
}
