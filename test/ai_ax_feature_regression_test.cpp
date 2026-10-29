#include "ai_ax.h"
#include "rule_srs.h"

#include <array>
#include <cstdio>
#include <stdexcept>

namespace
{
    using Map = m_tetris2::TetrisMap<10, 40>;
    struct CheckedMap : Map
    {
        row_t occupied_row(int y) const
        {
            if (y < 0 || y >= height) throw std::out_of_range("ai_ax row access");
            return Map::occupied_row(y);
        }
    };

    // Cell-by-cell reference, independent of the evaluator's bit masks.
    double reference(Map const &m)
    {
        int roof = 0;
        for (int y = 0; y < 12; ++y)
            for (int x = 0; x < 10; ++x)
                if (m.get(x, y)) roof = y + 1;
        int ct = 2 * (40 - roof), rt = 0;
        int holes = 0, hole_lines = 0, hole_depth = 0, well_depth = 0;
        int highest_hole = -1, hole_piece = 0;
        std::array<bool, 10> covered{}, highest_columns{};
        std::array<int, 10> hole_run{}, well_run{};
        if (roof > 0)
            for (int x = 0; x < 10; ++x)
                rt += !m.get(x, 0) + m.get(x, roof - 1);
        for (int y = 0; y < roof; ++y)
        {
            ct += !m.get(0, y) + !m.get(9, y);
            for (int x = 1; x < 10; ++x)
                ct += m.get(x - 1, y) != m.get(x, y);
            if (y > 0)
                for (int x = 0; x < 10; ++x)
                    rt += m.get(x, y - 1) != m.get(x, y);
        }
        for (int y = roof - 1; y >= 0; --y)
        {
            std::array<bool, 10> line_holes{};
            bool has_hole = false;
            for (int x = 0; x < 10; ++x)
            {
                bool occupied = m.get(x, y);
                covered[x] = covered[x] || occupied;
                line_holes[x] = covered[x] && !occupied;
                holes += line_holes[x];
                has_hole = has_hole || line_holes[x];
                if (line_holes[x]) hole_depth += ++hole_run[x];
                else hole_run[x] = 0;
                bool left = x == 0 || m.get(x - 1, y);
                bool right = x == 9 || m.get(x + 1, y);
                if (!occupied && left && right) well_depth += ++well_run[x];
                else well_run[x] = 0;
            }
            if (has_hole)
            {
                ++hole_lines;
                if (highest_hole < 0)
                {
                    highest_hole = y;
                    highest_columns = line_holes;
                }
            }
        }
        for (int y = highest_hole + 1; highest_hole >= 0 && y < roof; ++y)
        {
            int blocking = 0;
            for (int x = 0; x < 10; ++x)
                blocking += highest_columns[x] && m.get(x, y);
            if (blocking == 0) break;
            hole_piece += (y + 1) * blocking;
        }
        return -ct * 80 - rt * 80 - holes * 60 - hole_lines * 380
               - well_depth * 100 - hole_depth * 40 - hole_piece * 5;
    }
}

int main()
{
    ai_ax::AI ai;
    ai.init<rule_srs::TetrisRule::rule_spec>();
    int failures = 0, cases = 0;
    auto check = [&](CheckedMap const &m, char const *name)
    {
        ++cases;
        try
        {
            double got = ai.eval(m).map, expected = reference(m);
            if (got != expected)
            {
                if (failures < 4) std::printf("FAIL %s: got=%.17g expected=%.17g\n", name, got, expected);
                ++failures;
            }
        }
        catch (std::out_of_range const &e)
        {
            std::printf("FAIL %s: %s\n", name, e.what());
            ++failures;
        }
    };
    check(CheckedMap{}, "empty");
    CheckedMap covered;
    covered.set(4, 1); covered.set(4, 2);
    check(covered, "two blocks above a hole");
    CheckedMap split;
    split.set(4, 1); split.set(5, 2);
    check(split, "hole cover run stops at empty intersection");
    unsigned state = 0x6ac1395u;
    for (int i = 0; i < 4096; ++i)
    {
        CheckedMap m;
        int height = i % 12 + 1;
        for (int y = 0; y < height; ++y)
            for (int x = 0; x < 10; ++x)
            {
                state ^= state << 13; state ^= state >> 17; state ^= state << 5;
                if (state % 5 < 3) m.set(x, y);
            }
        check(m, "seeded low board");
    }
    std::printf("ai_ax feature regression: cases=%d failures=%d\n", cases, failures);
    return failures != 0;
}
