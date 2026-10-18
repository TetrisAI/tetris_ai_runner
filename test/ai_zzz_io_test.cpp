// ai_zzz_io_test: host-visible behaviour of ai_zzz::IO without a search — spawn-overlap death (rules.md §2),
// the param text format, how spin slots are discounted by when their piece can be played, certain kills (IOLethal),
// and which status fields the transposition key covers.

#include "../src/rule_io.h"
#include "../src/ai_zzz_io.h"
#include "../src/ai_zzz_io_lethal.h"

#include <array>
#include <cstdio>
#include <span>
#include <string>
#include <string_view>

namespace
{
    using Spec = rule_io::TetrisRule::rule_spec;
    using Map = m_tetris2::TetrisMap<Spec::width, Spec::height>;
    using Spin = ai_zzz::IO::EvalSpinType;

    int g_failures = 0;

    void expect(bool ok, char const *what)
    {
        if (!ok)
        {
            ++g_failures;
            std::fprintf(stderr, "FAIL: %s\n", what);
        }
    }

    // Locks an O (no line clear) with its bottom-left cell at (x, row).
    ai_zzz::IO::Result place_o(ai_zzz::IO const &ai, Map const &src, int x, int row, Map &after)
    {
        using Det = m_tetris2::TetrisNodeDetails<Spec, 'O', 0>;
        m_tetris2::TetrisNode<Det, Spin> node;
        node.x = static_cast<std::int8_t>(x);
        node.y = static_cast<std::int8_t>(row + m_tetris2::TetrisNodeBase<Det>::kOriginY);
        node.spin = Spin::None;
        after = src;
        for (int dy = 0; dy < 2; ++dy)
        {
            for (int dx = 0; dx < 2; ++dx)
                after.set(x + dx, row + dy);
        }
        return ai.eval(node, after, src, 0);
    }

    bool dies(ai_zzz::IO const &ai, ai_zzz::IO::Config const &config, Map const &board, int pending_rows)
    {
        std::array<ai_zzz::IO::PendingGarbage, 1> const pending = {{{pending_rows, 0}}};
        ai_zzz::IO::RootState root;
        root.pieces_placed = 30;
        root.lines_sent = 30;
        root.pending = std::span<ai_zzz::IO::PendingGarbage const>(pending.data(), pending_rows > 0 ? 1 : 0);
        Map after;
        ai_zzz::IO::Result const result = place_o(ai, board, 0, 3, after);
        char next[] = "T";
        m_tetris2::AIEnv const env{next, 1, 'O', ' ', false};
        return ai.get(result, 0, ai_zzz::IO::make_root_status(root, config), env).death;
    }

    Map stack_with_column(int column, int top_row)
    {
        Map board;
        for (int y = 0; y < 3; ++y)
            board.set_row(y, static_cast<Map::row_t>(Map::row_full & ~(1u << 9)));
        for (int y = 3; y <= top_row; ++y)
            board.set(column, y);
        return board;
    }

    void test_spawn_death()
    {
        ai_zzz::IO ai;
        ai_zzz::IO::Config config;
        ai.init<Spec>(&config);

        Map overhang = stack_with_column(4, 2);
        overhang.set(4, 25);
        overhang.set(5, 25);
        expect(!dies(ai, config, overhang, 0), "blocks above the spawn rows are not a spawn overlap");

        expect(dies(ai, config, stack_with_column(4, 21), 0), "a stack in the T spawn cells is death");
        expect(!dies(ai, config, stack_with_column(4, 19), 1), "one tanked row under a stack ending at row 19 leaves the spawn free");
        expect(dies(ai, config, stack_with_column(4, 19), 2), "two tanked rows push row 19 into the T spawn");

        Map side = stack_with_column(4, 2);
        side.set(3, 19);
        expect(!dies(ai, config, side, 1), "an overhang at (3,19) stays below the T spawn after one row");
        expect(dies(ai, config, side, 2), "two rows lift (3,19) into the T spawn cell (3,21)");
        expect(!dies(ai, config, side, 3), "three rows lift (3,19) to (3,22), outside the T spawn cells");
        expect(!dies(ai, config, stack_with_column(4, 2), 8), "a full cap of garbage under a low stack is not death");
    }

    void test_params()
    {
        ai_zzz::IO::Param param;
        param.roof = 7;
        expect(ai_zzz::IO::parse_params("roof = 3 # comment\n\n surge=4.5\n", param) && param.roof == 3 && param.surge == 4.5, "valid text applies every line");

        ai_zzz::IO::Param const before = param;
        expect(!ai_zzz::IO::parse_params("roof = 9\nbogus = 1\n", param), "unknown name fails");
        expect(!ai_zzz::IO::parse_params("roof = 9\nsurge = 1x\n", param), "malformed number fails");
        expect(param.roof == before.roof && param.surge == before.surge, "a failed parse leaves params unchanged");

        ai_zzz::IO::Param round_trip;
        round_trip.roof = 0;
        expect(ai_zzz::IO::parse_params(ai_zzz::IO::format_params(param), round_trip), "formatted params parse back");
        bool same = true;
        for (ai_zzz::IO::ParamField const &field : ai_zzz::IO::param_fields())
            same = same && round_trip.*(field.member) == param.*(field.member);
        expect(same, "format/parse round trip is exact");
    }

    double value_at_btb(ai_zzz::IO::Config const &config, int btb)
    {
        ai_zzz::IO ai;
        ai.init<Spec>(&config);
        Map after;
        ai_zzz::IO::Result const result = place_o(ai, Map{}, 0, 0, after);
        ai_zzz::IO::RootState root;
        root.btb = btb;
        char next[] = "T";
        m_tetris2::AIEnv const env{next, 1, 'O', ' ', false};
        return ai.get(result, 0, ai_zzz::IO::make_root_status(root, config), env).value;
    }

    void test_chain_value()
    {
        ai_zzz::IO::Config charging;
        expect(value_at_btb(charging, 21) > value_at_btb(charging, 6) && value_at_btb(charging, 6) > value_at_btb(charging, 3), "with charging the chain value grows with B2B");

        ai_zzz::IO::Config flat;
        flat.rules.b2bcharging = false;
        expect(value_at_btb(flat, 1) > value_at_btb(flat, 0), "without charging an active chain has value");
        expect(value_at_btb(flat, 1) == value_at_btb(flat, 3) && value_at_btb(flat, 3) == value_at_btb(flat, 21), "without charging the chain value does not grow with B2B");
    }

    void test_kill_credit()
    {
        ai_zzz::IO ai;
        ai_zzz::IO::Config config;
        config.opponent.lethal_sent = 1; // sending nothing kills
        ai.init<Spec>(&config);
        Map after;
        ai_zzz::IO::Result const result = place_o(ai, Map{}, 0, 0, after);
        char next[] = "T";
        m_tetris2::AIEnv const env{next, 1, 'O', ' ', false};
        ai_zzz::IO::Status const root = ai_zzz::IO::make_root_status({}, config);
        expect(ai.get(result, 0, root, env).kill, "a root placement sending a lethal amount kills");
        expect(!ai.get(result, 1, root, env).kill, "only the root placement's garbage counts for a kill");
    }

    // No piece can clear a line: every hole is covered, the top row has one at each wall.
    Map sealed_stack(int top)
    {
        Map board;
        for (int y = 0; y < top; ++y)
            board.set_row(y, static_cast<Map::row_t>(Map::row_full & ~(1u << (y % 2 == 0 ? 9 : 8))));
        board.set_row(top, static_cast<Map::row_t>(Map::row_full & ~(1u | (1u << 9))));
        return board;
    }

    std::uint64_t lethal(Map const &board, std::string_view pieces, std::span<ai_zzz::IO::PendingGarbage const> pending = {})
    {
        ai_zzz::IOLethal search;
        ai_zzz::IOLethal::Opponent const opponent{board, 0, pieces, ' ', true, {}, 20, pending};
        return search.lethal_sent(opponent, io_rules::Options::tetra_league(), 0, 0);
    }

    void test_lethal()
    {
        // Columns 3-5 reach row 16, so 5+ rows lift them into the T spawn at row 21.
        expect(lethal(sealed_stack(16), "TTTTTTTT") == ~std::uint64_t(0) << 5, "a sealed stack dies to 5 or more rows");
        std::array<ai_zzz::IO::PendingGarbage, 1> const queued = {{{3, 0}}};
        expect(lethal(sealed_stack(16), "TTTTTTTT", queued) == ~std::uint64_t(0) << 2, "garbage already queued enters first");

        Map well;
        for (int y = 0; y <= 16; ++y)
            well.set_row(y, static_cast<Map::row_t>(Map::row_full & ~(1u << 9)));
        expect(lethal(well, "IIIIIIII") == 0, "an I in an open well digs out of any amount");

        // Only 2 rows lift (3,19) into the T spawn.
        Map overhang = sealed_stack(2);
        overhang.set(3, 19);
        expect(lethal(overhang, "TTTTTTTT") == std::uint64_t(1) << 2, "a kill can need an exact amount");
    }

    double value_with_s_slot(ai_zzz::IO const &ai, ai_zzz::IO::Config const &config, std::string_view next, char hold)
    {
        Map after;
        ai_zzz::IO::Result result = place_o(ai, Map{}, 0, 0, after);
        result.spin_slots = {};
        for (std::size_t i = 0; i < m_tetris2::ai_rule_spec::piece_count<Spec>(); ++i)
        {
            if (m_tetris2::ai_rule_spec::piece_type_at<Spec>(i) == 'S')
                result.spin_slots[i] = {io_rules::Spin::Mini, 1};
        }
        m_tetris2::AIEnv const env{next.data(), next.size(), 'O', hold, false};
        return ai.get(result, 0, ai_zzz::IO::make_root_status({}, config), env).value;
    }

    void test_spin_slot_availability()
    {
        ai_zzz::IO ai;
        ai_zzz::IO::Config config;
        config.param.slot_b2b = 1;
        config.param.slot_attack = 0;
        config.param.slot_decay = 0.5;
        ai.init<Spec>(&config);

        double const next = value_with_s_slot(ai, config, "SZT", ' ');
        double const third = value_with_s_slot(ai, config, "ZTSLS", ' ');
        double const unseen = value_with_s_slot(ai, config, "ZTL", ' ');
        expect(next > third && third > unseen, "a slot is worth more the sooner its piece comes");
        expect(value_with_s_slot(ai, config, "ZTL", 'S') == next, "the held piece is as available as the next one");
        expect(value_with_s_slot(ai, config, "ZTSSSSSSSS", ' ') == third, "the first occurrence in the preview counts");

        config.param.slot_b2b = 0;
        expect(value_with_s_slot(ai, config, "SZT", ' ') == value_with_s_slot(ai, config, "ZTL", ' '), "zero slot weights ignore slots");
    }

    // The beam expands one node per key at each depth, so the key must ignore the score accumulators and split on
    // everything that shapes later placements.
    void test_transposition_key()
    {
        using Status = ai_zzz::IO::Status;
        std::array<ai_zzz::IO::PendingGarbage, 1> const pending = {{{3, 20}}};
        ai_zzz::IO::RootState root;
        root.btb = 2;
        root.combo = 1;
        root.pending = pending;
        Status const base = ai_zzz::IO::make_root_status(root, {});

        Status scored = base;
        scored.like += 5;
        scored.value += 100;
        scored.sent += 2;
        scored.surge_sent += 1;
        scored.cancelled += 1;
        scored.b2b_gained += 1;
        scored.max_combo += 1;
        expect(scored.transposition_key() == base.transposition_key(), "score accumulators keep the transposition key");

        for (int Status::*counter : {&Status::btb, &Status::combo, &Status::lines_sent, &Status::garbage_rows, &Status::map_rise})
        {
            Status other = base;
            ++(other.*counter);
            expect(other.transposition_key() != base.transposition_key(), "B2B, combo, lines sent, garbage rows and tanked rows split the key");
        }
        for (bool Status::*flag : {&Status::death, &Status::kill})
        {
            Status other = base;
            other.*flag = true;
            expect(other.transposition_key() != base.transposition_key(), "death and kill split the transposition key");
        }
        Status more_garbage = base;
        more_garbage.pending.push(1, 40);
        expect(more_garbage.transposition_key() != base.transposition_key(), "more pending garbage splits the transposition key");
    }
}

int main()
{
    test_spawn_death();
    test_params();
    test_chain_value();
    test_kill_credit();
    test_lethal();
    test_spin_slot_availability();
    test_transposition_key();
    if (g_failures != 0)
    {
        std::fprintf(stderr, "%d failure(s)\n", g_failures);
        return 1;
    }
    std::puts("ai_zzz_io_test: all checks passed");
    return 0;
}
