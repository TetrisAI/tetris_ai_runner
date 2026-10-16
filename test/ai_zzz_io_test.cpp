// ai_zzz_io_test: host-visible behaviour of ai_zzz::IO without a search — spawn-overlap death (rules.md §2)
// and the param text format.

#include "../src/rule_io.h"
#include "../src/ai_zzz_io.h"

#include <array>
#include <cstdio>
#include <span>
#include <string>

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

    double chain_value(ai_zzz::IO::Config const &config, int btb)
    {
        ai_zzz::IO::RootState root;
        root.btb = btb;
        return ai_zzz::IO::make_root_status(root, config).root_chain;
    }

    void test_chain_value()
    {
        ai_zzz::IO::Config charging;
        expect(chain_value(charging, 21) > chain_value(charging, 6) && chain_value(charging, 6) > chain_value(charging, 3), "with charging the chain value grows with B2B");

        ai_zzz::IO::Config flat;
        flat.rules.b2bcharging = false;
        expect(chain_value(flat, 0) == 0 && chain_value(flat, 1) > 0, "without charging an active chain has value");
        expect(chain_value(flat, 1) == chain_value(flat, 3) && chain_value(flat, 3) == chain_value(flat, 21), "without charging the chain value does not grow with B2B");
    }
}

int main()
{
    test_spawn_death();
    test_params();
    test_chain_value();
    if (g_failures != 0)
    {
        std::fprintf(stderr, "%d failure(s)\n", g_failures);
        return 1;
    }
    std::puts("ai_zzz_io_test: all checks passed");
    return 0;
}
