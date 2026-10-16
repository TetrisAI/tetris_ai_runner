// io_rules_test: TETR.IO Season 2 rules arithmetic against .research/tetrio-s2-rules/rules.md (§3, §7, §8, §9)
// and reference vectors produced by Triangle.js (@haelp/teto RNG / Bag7).

#include "../src/io_rules.h"

#include <array>
#include <cmath>
#include <cstdio>
#include <string>

using namespace io_rules;

namespace
{
    int g_failures = 0;

    void expect(bool ok, std::string const &what)
    {
        if (!ok)
        {
            ++g_failures;
            std::fprintf(stderr, "FAIL: %s\n", what.c_str());
        }
    }

    Options const kTL = Options::tetra_league();

    Attack resolve(Placement const &p, int btb, int combo, double m = 1, Options const &o = kTL)
    {
        return resolve_attack(o, Counters{btb, combo}, p, m);
    }

    void test_combo_table()
    {
        struct Row
        {
            char const *name;
            Placement placement;
            int btb_before;
            std::array<int, 12> expected;
        };
        std::array<Row, 13> const rows = {{
            {"single", {1, Spin::None, false, false}, 0, {0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2}},
            {"mini single", {1, Spin::Mini, false, false}, 0, {0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2}},
            {"double", {2, Spin::None, false, false}, 0, {1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3}},
            {"mini double", {2, Spin::Mini, false, false}, 0, {1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3}},
            {"B2B mini single", {1, Spin::Mini, false, false}, 1, {1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3}},
            {"triple", {3, Spin::None, false, false}, 0, {2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7}},
            {"TSS", {1, Spin::Full, false, false}, 0, {2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7}},
            {"B2B mini double", {2, Spin::Mini, false, false}, 1, {2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7}},
            {"quad", {4, Spin::None, false, false}, 0, {4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15}},
            {"TSD", {2, Spin::Full, false, false}, 0, {4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15}},
            {"B2B quad", {4, Spin::None, false, false}, 1, {5, 6, 7, 8, 10, 11, 12, 13, 15, 16, 17, 18}},
            {"TST", {3, Spin::Full, false, false}, 0, {6, 7, 9, 10, 12, 13, 15, 16, 18, 19, 21, 22}},
            {"B2B TST", {3, Spin::Full, false, false}, 1, {7, 8, 10, 12, 14, 15, 17, 19, 21, 22, 24, 26}},
        }};
        for (Row const &row : rows)
        {
            for (int c = 1; c <= 12; ++c)
            {
                Attack const a = resolve(row.placement, row.btb_before, c - 1);
                expect(a.lines == row.expected[c - 1] && a.counters.combo == c,
                       std::string("combo table ") + row.name + " c=" + std::to_string(c) + ": got " + std::to_string(a.lines));
            }
        }
        expect(resolve({2, Spin::Full, false, false}, 1, 1).lines == 6, "B2B TSD at 1 COMBO = (4+1)*1.25 -> 6");
    }

    void test_b2b_counters()
    {
        Attack const tsd = resolve({2, Spin::Full, false, false}, 0, 0);
        expect(tsd.counters.btb == 1 && tsd.lines == 4, "first B2B clear sets btb 1 without bonus");
        Attack const mini = resolve({1, Spin::Mini, false, false}, 3, 0);
        expect(mini.counters.btb == 4 && !mini.chain_broken, "mini spin adds B2B");
        expect(mini.lines == 1, "B2B bonus applies to a mini single (base 0)");
        expect(resolve({1, Spin::Mini, false, false}, 3, 0, 1.48).lines == 1, "B2B mini single at M=1.48 -> floor(1.48)");
        expect(resolve({2, Spin::Mini, false, false}, 3, 0, 1.96).lines == 3, "B2B mini double at M=1.96 -> floor(3.92)");
        expect(resolve({1, Spin::Mini, false, false}, 0, 0).lines == 0, "mini single without B2B sends 0");
        Attack const zero_spin = resolve({0, Spin::Full, false, false}, 6, 3);
        expect(zero_spin.counters.btb == 6 && zero_spin.counters.combo == 0 && zero_spin.lines == 0 && !zero_spin.blocks,
               "zero-line spin keeps B2B, resets combo, does not block");
        Attack const nothing = resolve({0, Spin::None, false, false}, 6, 3);
        expect(nothing.counters.btb == 6 && !nothing.chain_broken && !nothing.blocks, "no clear never breaks B2B");
        Attack const broken = resolve({2, Spin::None, false, false}, 3, 0);
        expect(broken.counters.btb == 0 && broken.chain_broken && broken.total() == 1, "plain double breaks B2B x2 without surge");
    }

    void test_surge_split()
    {
        std::array<std::pair<int, std::array<int, 3>>, 5> const cases = {{
            {4, {1, 1, 2}},
            {5, {2, 2, 1}},
            {7, {2, 2, 3}},
            {8, {3, 3, 2}},
            {10, {3, 3, 4}},
        }};
        for (auto const &[amount, parts] : cases)
            expect(split_surge(amount) == parts, "surge split S=" + std::to_string(amount));
    }

    void test_surge_amount()
    {
        std::array<double, 4> const multipliers = {1.0, garbage_multiplier(kTL, 4 * 3600), garbage_multiplier(kTL, 5 * 3600), 2.2};
        for (double m : multipliers)
        {
            for (int n = 0; n <= 120; ++n)
            {
                Attack const a = resolve({1, Spin::None, false, false}, n + 1, 0, m);
                int const surge = a.surge[0] + a.surge[1] + a.surge[2];
                int const expected = n >= 4 ? static_cast<int>(std::floor(n * m)) : 0;
                expect(surge == expected && a.surge == (expected > 0 ? split_surge(expected) : std::array<int, 3>{}),
                       "surge at B2B x" + std::to_string(n) + " M=" + std::to_string(m) + ": got " + std::to_string(surge));
                expect(a.counters.btb == 0 && a.chain_broken, "a plain single breaks the chain");
                expect(a.lines == 0, "the breaking single sends no B2B bonus");
            }
        }

        Options preset = kTL;
        preset.b2bcharge_base = 0;
        preset.allclear_b2b_sends = false;
        preset.allclear_b2b_dupes = true;
        for (int n = 0; n <= 40; ++n)
        {
            Attack const a = resolve({2, Spin::None, false, false}, n + 1, 0, 1, preset);
            int const surge = a.surge[0] + a.surge[1] + a.surge[2];
            expect(surge == (n >= 4 ? n - 3 : 0), "client preset surge at B2B x" + std::to_string(n));
        }

        Attack const kept = resolve({2, Spin::Full, false, false}, 50, 0);
        expect(kept.surge == std::array<int, 3>{} && kept.counters.btb == 51, "a B2B clear never releases surge");
    }

    void test_all_clear()
    {
        Attack const single = resolve({1, Spin::None, true, false}, 0, 0);
        expect(single.counters.btb == 1 && !single.chain_broken, "PC single adds one B2B and does not break");
        expect(single.lines == 0 && single.all_clear == 5 && single.total() == 5, "PC attack is a separate floor(5M)");
        expect(resolve({1, Spin::None, true, false}, 0, 0, 1.48).all_clear == 7, "PC attack at M=1.48 -> floor(7.4)");

        Attack const quad = resolve({4, Spin::None, true, false}, 3, 0);
        expect(quad.counters.btb == 4, "PC quad adds +1 B2B, never +2");
        expect(quad.lines == 5 && quad.all_clear == 5, "PC quad gets the B2B bonus on its line attack");

        Attack const b2b_single = resolve({1, Spin::None, true, false}, 2, 0);
        expect(b2b_single.counters.btb == 3 && b2b_single.lines == 1, "B2B bonus on the PC clear (allclear_b2b_sends)");

        Options no_sends = kTL;
        no_sends.allclear_b2b_sends = false;
        expect(resolve({1, Spin::None, true, false}, 2, 0, 1, no_sends).lines == 0, "allclear_b2b_sends=false: PC-only B2B sends no bonus");
        expect(resolve({4, Spin::None, true, false}, 2, 0, 1, no_sends).lines == 4, "allclear_b2b_sends=false: a PC quad's merged B2B sends no bonus either");

        Attack const charged = resolve({1, Spin::None, true, false}, 9, 0);
        expect(charged.surge == std::array<int, 3>{} && charged.counters.btb == 10, "PC keeps a charged chain");
    }

    void test_special_bonus()
    {
        double const m = garbage_multiplier(kTL, 4 * 3600);
        expect(resolve({2, Spin::Full, false, true}, 0, 0, m).lines == 6, "TSD through garbage at M=1.48: floor(5.92)+1");
        expect(resolve({2, Spin::Full, false, true}, 0, 0, 1.96).lines == 8, "special bonus is added after the multiplier");
        expect(resolve({4, Spin::None, false, true}, 0, 0).lines == 5, "quad through garbage +1");
        expect(resolve({1, Spin::Mini, false, true}, 0, 0).lines == 1, "all-spin mini single through garbage +1");
        expect(resolve({3, Spin::None, false, true}, 0, 0).lines == 2, "plain triple through garbage gets no bonus");
        expect(resolve({2, Spin::Full, false, false}, 0, 0).lines == 4, "no bonus without a garbage row");
        Options off = kTL;
        off.garbagespecialbonus = false;
        expect(resolve({4, Spin::None, false, true}, 0, 0, 1, off).lines == 4, "garbagespecialbonus=false");
    }

    void test_multiplier_and_cap()
    {
        expect(garbage_multiplier(kTL, 0) == 1 && garbage_multiplier(kTL, 10800) == 1, "M = 1 until 3:00");
        expect(std::abs(garbage_multiplier(kTL, 4 * 3600) - 1.48) < 1e-12, "M(4:00) = 1.48");
        expect(std::abs(garbage_multiplier(kTL, 5 * 3600) - 1.96) < 1e-12, "M(5:00) = 1.96");
        expect(std::abs(garbage_multiplier(kTL, 6 * 3600) - 2.44) < 1e-12, "M(6:00) = 2.44");
        expect(garbage_cap(kTL, 0) == 8 && garbage_cap(kTL, 100000) == 8, "TL cap stays 8");
    }

    void test_queue()
    {
        int sent_total = 0;
        PendingQueue<8> opener;
        opener.push(10, 0);
        FightResult f = opener.fight(kTL, 3, 5, sent_total);
        expect(f.cancelled == 6 && f.sent == 0 && opener.total() == 4, "opener cancels double");
        f = opener.fight(kTL, 3, 5, sent_total);
        expect(f.cancelled == 4 && f.sent == 0 && opener.empty(), "opener extra stops at an empty queue");

        PendingQueue<8> small;
        small.push(2, 0);
        sent_total = 0;
        f = small.fight(kTL, 4, 3, sent_total);
        expect(f.cancelled == 2 && f.sent == 2 && sent_total == 2, "opener double-cancel never adds to sent");

        PendingQueue<8> late;
        late.push(10, 0);
        sent_total = 0;
        f = late.fight(kTL, 3, 15, sent_total);
        expect(f.cancelled == 3 && late.total() == 7, "no double cancel after piece 14");

        PendingQueue<8> behind;
        behind.push(3, 0);
        sent_total = 5;
        f = behind.fight(kTL, 2, 4, sent_total);
        expect(f.cancelled == 2 && behind.total() == 1, "no double cancel when pending < sent");

        PendingQueue<8> capped;
        capped.push(5, 0);
        capped.push(7, 0);
        expect(capped.tank(kTL, garbage_cap(kTL, 0)) == 8 && capped.total() == 4, "at most 8 lines enter per placement");
        expect(capped.tank(kTL, 8) == 4 && capped.empty(), "the split entry enters on the next tank");

        PendingQueue<8> travelling;
        travelling.push(4, kTL.garbagespeed);
        expect(travelling.tank(kTL, 8) == 0, "inactive garbage does not enter");
        travelling.advance(kTL.garbagespeed - 1);
        expect(travelling.tank(kTL, 8) == 0 && travelling.active_total() == 0, "still travelling one frame before activation");
        travelling.advance(1);
        expect(travelling.active_total() == 4 && travelling.tank(kTL, 8) == 4, "active after garbagespeed frames");

        PendingQueue<8> mixed;
        mixed.push(2, 0);
        mixed.push(3, 10);
        sent_total = 0;
        f = mixed.fight(kTL, 4, 30, sent_total);
        expect(f.cancelled == 4 && mixed.total() == 1, "inactive garbage can be cancelled");

        PendingQueue<2> full;
        full.push(1, 0);
        full.push(1, 5);
        full.push(3, 9);
        expect(full.total() == 5 && full.entries().size() == 2 && full.entries()[1].amount == 4, "a full queue merges into its last entry");
    }

    void test_exchange()
    {
        PendingQueue<8> queue;
        queue.push(3, 0);
        int sent_total = 0;
        Attack const surge = resolve({1, Spin::None, false, false}, 9, 0);
        ExchangeResult x = exchange(kTL, surge, queue, 40, sent_total, 8);
        expect(x.cancelled == 3 && x.sent == 5 && x.tanked == 0, "each surge part cancels own pending first");
        expect(x.part_count == 2 && x.parts[0] == 3 && x.parts[1] == 2, "sent parts keep send order");

        queue.push(6, 0);
        Attack const single = resolve({1, Spin::None, false, false}, 0, 0);
        x = exchange(kTL, single, queue, 40, sent_total, 8);
        expect(x.tanked == 0 && queue.total() == 6, "combo blocking: a clear never tanks");

        Attack const drop = resolve({0, Spin::None, false, false}, 0, 1);
        x = exchange(kTL, drop, queue, 40, sent_total, 8);
        expect(x.tanked == 6 && queue.empty(), "a non-clearing placement tanks");

        PendingQueue<8> pc_queue;
        pc_queue.push(2, 0);
        sent_total = 0;
        Attack const pc = resolve({4, Spin::None, true, false}, 3, 0);
        x = exchange(kTL, pc, pc_queue, 40, sent_total, 8);
        expect(x.cancelled == 2 && x.sent == 8 && x.part_count == 2 && x.parts[0] == 3 && x.parts[1] == 5, "PC attack is fought after the line attack");
        expect(sent_total == 8, "lines_sent counts lines actually sent");
    }

    void test_rng()
    {
        Rng rng(1);
        expect(rng.next() == 16807 && rng.next() == 282475249 && rng.next() == 1622650073, "Park-Miller sequence from seed 1");
        expect(Rng(2147483647).seed() == 2147483646 && Rng(-5).seed() == 2147483641, "seed normalisation");

        struct Reference
        {
            std::int64_t seed;
            char const *pieces;
            std::array<int, 8> columns;
        };
        // Triangle.js Bag7 / RNG output (floor(nextFloat() * 10) for the columns).
        std::array<Reference, 5> const refs = {{
            {882087139, "JTZOILSIOSLZJTLOITSZJ", {5, 2, 8, 6, 2, 2, 9, 8}},
            {1, "OJILSTZTOLJSIZLISOTZJ", {0, 1, 7, 4, 5, 2, 0, 6}},
            {2147483647, "ZSIOLJTOSIZJLTOSZTIJL", {9, 8, 2, 5, 4, 7, 9, 3}},
            {-5, "SJIZOLTZSOLITJTLOJSIZ", {9, 2, 4, 2, 8, 6, 7, 9}},
            {123456789012, "SLIZTJOOLSJITZSOTJIZL", {4, 8, 5, 1, 7, 7, 1, 1}},
        }};
        for (Reference const &ref : refs)
        {
            Bag7 bag(ref.seed);
            std::string pieces;
            for (int i = 0; i < 21; ++i)
                pieces += bag.next();
            expect(pieces == ref.pieces, "bag for seed " + std::to_string(ref.seed) + ": " + pieces);
            Rng columns(ref.seed);
            bool same = true;
            for (int c : ref.columns)
                same = same && static_cast<int>(columns.next_float() * 10) == c;
            expect(same, "float sequence for seed " + std::to_string(ref.seed));
        }

        // rngex for seed 882087139 yields columns 5,2,8,6,2,2,9,...: the first row rerolls (float 1), the finished
        // entry rerolls (floats 2-3), each later row spends one messiness_inner float, the last entry rerolls (6-7).
        HoleColumns holes(882087139);
        PendingQueue<8> queue;
        queue.push(1, 0);
        queue.push(2, 0);
        std::array<std::int8_t, 8> out{};
        expect(queue.tank(kTL, 8, &holes, out) == 3, "tank with hole columns");
        expect(out[0] == 5 && out[1] == 8 && out[2] == 8 && holes.last_column() == 9, "hole columns follow rngex");
    }
}

int main()
{
    test_combo_table();
    test_b2b_counters();
    test_surge_split();
    test_surge_amount();
    test_all_clear();
    test_special_bonus();
    test_multiplier_and_cap();
    test_queue();
    test_exchange();
    test_rng();
    if (g_failures != 0)
    {
        std::fprintf(stderr, "%d failure(s)\n", g_failures);
        return 1;
    }
    std::puts("io_rules_test: all checks passed");
    return 0;
}
