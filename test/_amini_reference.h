// _amini_reference.h — independent model of TETR.IO piece mechanics for the amini tests.
//
// Written from the y-down tables of .research/tetrio-s2-rules/rules.md (§2 shapes and spawn, §4 SRS+ kicks,
// §6 all-mini+ spin detection) without any engine code, so the engine's rule_io / AMiniHook / AMiniStrategy
// can be checked against it.

#ifndef TETRIS_AI_RUNNER_TESTS_AMINI_REFERENCE_H_
#define TETRIS_AI_RUNNER_TESTS_AMINI_REFERENCE_H_

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace amini_ref
{
    constexpr int kW = 10;
    constexpr int kH = 40;

    enum Spin : int
    {
        None = 0,
        Mini = 1,
        Full = 2
    };

    // y-up: rows[0] is the bottom row, bit x is column x.
    struct Board
    {
        std::array<std::uint16_t, kH> rows{};

        bool occupied(int x, int y) const
        {
            return x < 0 || x >= kW || y < 0 || y >= kH || ((rows[y] >> x) & 1) != 0;
        }
    };

    inline Board from_rows(std::initializer_list<char const *> rows_bottom_up)
    {
        Board b;
        int y = 0;
        for (char const *row : rows_bottom_up)
        {
            for (int x = 0; x < kW && row[x] != '\0'; ++x)
                if (row[x] == '#')
                    b.rows[y] |= static_cast<std::uint16_t>(1u << x);
            ++y;
        }
        return b;
    }

    // Offsets relative to the anchor, y-DOWN as printed in the spec.
    struct Offset
    {
        int x, y;
    };

    inline std::array<Offset, 4> shape(char t, int r)
    {
        switch (t)
        {
        case 'T':
        {
            static constexpr std::array<std::array<Offset, 4>, 4> s{{{{{0, -1}, {-1, 0}, {0, 0}, {1, 0}}},
                                                                     {{{0, -1}, {0, 0}, {1, 0}, {0, 1}}},
                                                                     {{{-1, 0}, {0, 0}, {1, 0}, {0, 1}}},
                                                                     {{{0, -1}, {-1, 0}, {0, 0}, {0, 1}}}}};
            return s[r];
        }
        case 'S':
        {
            static constexpr std::array<std::array<Offset, 4>, 4> s{{{{{0, -1}, {1, -1}, {-1, 0}, {0, 0}}},
                                                                     {{{0, -1}, {0, 0}, {1, 0}, {1, 1}}},
                                                                     {{{0, 0}, {1, 0}, {-1, 1}, {0, 1}}},
                                                                     {{{-1, -1}, {-1, 0}, {0, 0}, {0, 1}}}}};
            return s[r];
        }
        case 'Z':
        {
            static constexpr std::array<std::array<Offset, 4>, 4> s{{{{{-1, -1}, {0, -1}, {0, 0}, {1, 0}}},
                                                                     {{{1, -1}, {0, 0}, {1, 0}, {0, 1}}},
                                                                     {{{-1, 0}, {0, 0}, {0, 1}, {1, 1}}},
                                                                     {{{0, -1}, {-1, 0}, {0, 0}, {-1, 1}}}}};
            return s[r];
        }
        case 'L':
        {
            static constexpr std::array<std::array<Offset, 4>, 4> s{{{{{1, -1}, {-1, 0}, {0, 0}, {1, 0}}},
                                                                     {{{0, -1}, {0, 0}, {0, 1}, {1, 1}}},
                                                                     {{{-1, 0}, {0, 0}, {1, 0}, {-1, 1}}},
                                                                     {{{-1, -1}, {0, -1}, {0, 0}, {0, 1}}}}};
            return s[r];
        }
        case 'J':
        {
            static constexpr std::array<std::array<Offset, 4>, 4> s{{{{{-1, -1}, {-1, 0}, {0, 0}, {1, 0}}},
                                                                     {{{0, -1}, {1, -1}, {0, 0}, {0, 1}}},
                                                                     {{{-1, 0}, {0, 0}, {1, 0}, {1, 1}}},
                                                                     {{{0, -1}, {0, 0}, {-1, 1}, {0, 1}}}}};
            return s[r];
        }
        case 'I':
        {
            static constexpr std::array<std::array<Offset, 4>, 4> s{{{{{-1, 0}, {0, 0}, {1, 0}, {2, 0}}},
                                                                     {{{1, -1}, {1, 0}, {1, 1}, {1, 2}}},
                                                                     {{{-1, 1}, {0, 1}, {1, 1}, {2, 1}}},
                                                                     {{{0, -1}, {0, 0}, {0, 1}, {0, 2}}}}};
            return s[r];
        }
        default:
            return {{{0, -1}, {1, -1}, {0, 0}, {1, 0}}};
        }
    }

    // Listed kicks (the plain rotation is tested before them), y-DOWN as printed in the spec.
    inline std::vector<Offset> kicks(char t, int from, int to)
    {
        int const key = from * 10 + to;
        if (t == 'O')
            return {};
        if (t == 'I')
        {
            switch (key)
            {
            case 1: return {{1, 0}, {-2, 0}, {-2, 1}, {1, -2}};
            case 10: return {{-1, 0}, {2, 0}, {-1, 2}, {2, -1}};
            case 12: return {{-1, 0}, {2, 0}, {-1, -2}, {2, 1}};
            case 21: return {{-2, 0}, {1, 0}, {-2, -1}, {1, 2}};
            case 23: return {{2, 0}, {-1, 0}, {2, -1}, {-1, 2}};
            case 32: return {{1, 0}, {-2, 0}, {1, -2}, {-2, 1}};
            case 30: return {{1, 0}, {-2, 0}, {1, 2}, {-2, -1}};
            case 3: return {{-1, 0}, {2, 0}, {2, 1}, {-1, -2}};
            case 2: return {{0, -1}};
            case 20: return {{0, 1}};
            case 13: return {{1, 0}};
            case 31: return {{-1, 0}};
            }
            return {};
        }
        switch (key)
        {
        case 1: return {{-1, 0}, {-1, -1}, {0, 2}, {-1, 2}};
        case 10: return {{1, 0}, {1, 1}, {0, -2}, {1, -2}};
        case 12: return {{1, 0}, {1, 1}, {0, -2}, {1, -2}};
        case 21: return {{-1, 0}, {-1, -1}, {0, 2}, {-1, 2}};
        case 23: return {{1, 0}, {1, -1}, {0, 2}, {1, 2}};
        case 32: return {{-1, 0}, {-1, 1}, {0, -2}, {-1, -2}};
        case 30: return {{-1, 0}, {-1, 1}, {0, -2}, {-1, -2}};
        case 3: return {{1, 0}, {1, -1}, {0, 2}, {1, 2}};
        case 2: return {{0, -1}, {1, -1}, {-1, -1}, {1, 0}, {-1, 0}};
        case 20: return {{0, 1}, {-1, 1}, {1, 1}, {-1, 0}, {1, 0}};
        case 13: return {{1, 0}, {1, -2}, {1, -1}, {0, -2}, {0, -1}};
        case 31: return {{-1, 0}, {-1, -2}, {-1, -1}, {0, -2}, {0, -1}};
        }
        return {};
    }

    // Anchor position in y-up board coordinates.
    struct Piece
    {
        char t;
        int r;
        int x, y;

        bool operator==(Piece const &) const = default;
    };

    using Cells = std::array<int, 4>; // sorted y * kW + x

    inline Cells cells(Piece const &p)
    {
        Cells c{};
        std::array<Offset, 4> s = shape(p.t, p.r);
        for (int i = 0; i < 4; ++i)
            c[i] = (p.y - s[i].y) * kW + (p.x + s[i].x);
        std::sort(c.begin(), c.end());
        return c;
    }

    inline bool fits(Board const &b, Piece const &p)
    {
        for (Offset o : shape(p.t, p.r))
            if (b.occupied(p.x + o.x, p.y - o.y))
                return false;
        return true;
    }

    inline Piece moved(Piece p, int dx, int dy)
    {
        p.x += dx;
        p.y += dy;
        return p;
    }

    // TETR.IO `spinbonuses` values the engine supports.
    enum class Rule
    {
        TSpins,     // "T-spins"
        TSpinsPlus, // "T-spins+"
        AllMini,    // "all-mini"
        AllMiniPlus // "all-mini+"
    };

    // tetrio.js IsTSpin / rules.md §6; `kick` is TETR.IO's stored kick index (0 for the plain rotation).
    inline Spin classify(Board const &b, Piece const &p, int kick, Rule rule)
    {
        bool const grounded = !fits(b, moved(p, 0, -1));
        bool const is_t = p.t == 'T';
        // "T-spins"/"T-spins+" list only T in spinbonuses_rules[rule].types; other pieces never spin.
        if (!is_t && (rule == Rule::TSpins || rule == Rule::TSpinsPlus))
            return None;
        // The 3-corner test (only T is in types_mini, and only T reaches it under these rules).
        if (is_t && grounded)
        {
            // y-down corner offsets and the rotations for which each corner is a front corner.
            static constexpr std::array<Offset, 4> corner{{{-1, -1}, {1, -1}, {1, 1}, {-1, 1}}};
            static constexpr std::array<std::array<int, 2>, 4> front_for{{{3, 0}, {0, 1}, {1, 2}, {2, 3}}};
            int n = 0;
            int f = 0;
            for (int i = 0; i < 4; ++i)
            {
                if (!b.occupied(p.x + corner[i].x, p.y - corner[i].y))
                    continue;
                ++n;
                if (front_for[i][0] == p.r || front_for[i][1] == p.r)
                    ++f;
            }
            if (n >= 3)
                return f == 2 || kick == 3 ? Full : Mini;
        }
        // "T-spins" returns the corner result alone; "all-mini" returns it for T before the immobility test.
        if (rule == Rule::TSpins || (is_t && rule == Rule::AllMini))
            return None;
        bool const immobile = grounded && !fits(b, moved(p, -1, 0)) && !fits(b, moved(p, 1, 0)) &&
                              !fits(b, moved(p, 0, 1));
        return immobile ? Mini : None;
    }

    struct Game
    {
        Board board;
        Piece piece;
        Spin spin = None;
        bool g20 = false;
        Rule rule = Rule::AllMiniPlus;
    };

    inline void gravity(Game &g)
    {
        if (!g.g20)
            return;
        bool fell = false;
        while (fits(g.board, moved(g.piece, 0, -1)))
        {
            g.piece = moved(g.piece, 0, -1);
            fell = true;
        }
        if (fell)
            g.spin = None;
    }

    // Spawn anchor (4, 21) y-up: the flat row is board row 21. A blocked spawn tops out.
    inline std::optional<Game> spawn(Board const &b, char t, bool g20, Rule rule)
    {
        Game g{b, Piece{t, 0, 4, 21}, None, g20, rule};
        if (!fits(b, g.piece))
            return std::nullopt;
        gravity(g);
        return g;
    }

    inline bool shift(Game &g, int dx, int dy)
    {
        Piece n = moved(g.piece, dx, dy);
        if (!fits(g.board, n))
            return false;
        g.piece = n;
        g.spin = None;
        gravity(g);
        return true;
    }

    // First legal test of a rotation; `kick` is TETR.IO's stored index (0 also for the plain rotation).
    struct Rotation
    {
        Piece piece;
        int kick;
    };

    inline std::optional<Rotation> try_rotate(Board const &b, Piece const &p, int to)
    {
        Piece n = p;
        n.r = to;
        if (fits(b, n))
            return Rotation{n, 0};
        std::vector<Offset> list = kicks(p.t, p.r, to);
        for (int i = 0; i < static_cast<int>(list.size()); ++i)
        {
            Piece k = moved(n, list[i].x, -list[i].y);
            if (fits(b, k))
                return Rotation{k, i};
        }
        return std::nullopt;
    }

    inline bool rotate(Game &g, int to)
    {
        std::optional<Rotation> rot = try_rotate(g.board, g.piece, to);
        if (!rot)
            return false;
        g.piece = rot->piece;
        g.spin = classify(g.board, rot->piece, rot->kick, g.rule);
        gravity(g);
        return true;
    }

    // Applies one key; returns false when the key changes nothing.
    inline bool press(Game &g, char key)
    {
        switch (key)
        {
        case 'l': return shift(g, -1, 0);
        case 'r': return shift(g, 1, 0);
        case 'd': return shift(g, 0, -1);
        case 'L':
        case 'R':
        case 'D':
        {
            int dx = (key == 'R') - (key == 'L');
            int dy = -(key == 'D');
            bool any = false;
            while (shift(g, dx, dy))
                any = true;
            return any;
        }
        case 'z': return rotate(g, (g.piece.r + 3) % 4);
        case 'c': return rotate(g, (g.piece.r + 1) % 4);
        case 'x': return rotate(g, (g.piece.r + 2) % 4);
        }
        return false;
    }

    // Hard drop. TETR.IO does not recompute the spin, and a spin only exists while the piece rests.
    inline std::pair<Cells, Spin> lock(Game const &g)
    {
        Piece p = g.piece;
        while (fits(g.board, moved(p, 0, -1)))
            p = moved(p, 0, -1);
        assert(p == g.piece || g.spin == None);
        return {cells(p), g.spin};
    }

    struct Keys
    {
        bool allow_180 = true;
        bool allow_d = true;
    };

    // Every (cells, class) the piece can lock with, reduced like the engine's LandPoints:
    // None if realizable plus the best realizable spin class.
    inline std::set<std::pair<Cells, int>> realizable(Board const &b, char t, bool g20, Keys keys, Rule rule)
    {
        std::set<std::pair<Cells, int>> out;
        std::optional<Game> start = spawn(b, t, g20, rule);
        if (!start)
            return out;
        std::string alphabet = keys.allow_d ? "lrdzc" : "lrDzc";
        if (keys.allow_180)
            alphabet += 'x';
        auto encode = [](Game const &g)
        {
            return ((g.piece.r * kH + g.piece.y + 4) * (kW + 8) + g.piece.x + 4) * 3 + g.spin;
        };
        std::vector<bool> seen(4 * (kH + 8) * (kW + 8) * 3, false);
        std::vector<Game> queue{*start};
        seen[encode(*start)] = true;
        std::vector<std::pair<Cells, Spin>> locks;
        for (std::size_t head = 0; head < queue.size(); ++head)
        {
            Game const g = queue[head];
            locks.push_back(lock(g));
            for (char key : alphabet)
            {
                Game n = g;
                if (!press(n, key) || seen[encode(n)])
                    continue;
                seen[encode(n)] = true;
                queue.push_back(n);
            }
        }
        std::sort(locks.begin(), locks.end());
        for (std::size_t i = 0; i < locks.size();)
        {
            std::size_t j = i;
            bool none = false;
            int best = None;
            for (; j < locks.size() && locks[j].first == locks[i].first; ++j)
            {
                if (locks[j].second == None)
                    none = true;
                best = std::max<int>(best, locks[j].second);
            }
            if (none)
                out.insert({locks[i].first, None});
            if (best != None)
                out.insert({locks[i].first, best});
            i = j;
        }
        return out;
    }

    // Hand-made spin setups (rows bottom-up) shared by the amini tests.
    inline std::vector<std::pair<std::string, Board>> hand_fixtures()
    {
        return {
            // T r2 can only rotate in under the overhang at (3,2): 3 corners, both front corners -> Full.
            {"tsd", from_rows({"####.#####", "###...####", "####......"})},
            // T r0 at anchor (4,3) + CW only fits with listed kick 3 (-1,-2): front (4,0) is empty -> Full only
            // through the index-3 upgrade.
            {"tst_upgrade", from_rows({"###..#####", "###..#####", "###.######", "###.......", "####......"})},
            {"tst_upgrade_mirror", from_rows({"#####..###", "#####..###", "######.###", ".......###", "......####"})},
            // T r1 at anchor (4,1) + 180 only fits with listed kick 3 (0,+2 up) into r3 at (4,3).
            {"t_180_upgrade", from_rows({"####.#####", "####..####", "####.#####", "###..#####", "###..#####"})},
            // T r0 at (4,1) is immobile with only the two floor corners occupied -> Mini under all-mini+.
            {"t_immobile", from_rows({"#########.", "###...####", "###...####", "....#....."})},
            // T r0 reaching (2,0) by the second listed kick: 3 corners, one front corner -> Mini.
            {"t_mini", from_rows({"..........", "#..#......"})},
            // The flat spawn row is 21: (2,21) blocks T/S/Z/L/J/I from shifting left at the spawn row.
            {"spawn_ceiling", from_rows({"#########.", "#########.", "#########.", "#########.", "#########.",
                                         "#########.", "#########.", "#########.", "#########.", "#########.",
                                         "#########.", "#########.", "#########.", "#########.", "#########.",
                                         "#########.", "#########.", "#########.", "#########.", "#########.",
                                         "#########.", "..#......."})},
            // The flat spawn row is 21: pieces pass over (2,20); a spawn one row lower could not.
            {"spawn_floor", from_rows({"#########.", "#########.", "#########.", "#########.", "#########.",
                                       "#########.", "#########.", "#########.", "#########.", "#########.",
                                       "#########.", "#########.", "#########.", "#########.", "#########.",
                                       "#########.", "#########.", "#########.", "#########.", "#########.",
                                       "..#......."})},
            {"i_spin", from_rows({"#########.", "#########.", "#########.", "#########.", "########..",
                                  "#.........", "#........."})},
            {"sz_pockets", from_rows({"##.######.", "#..#####..", "#.####.##.", "####..####", "......####"})},
            {"lj_pockets", from_rows({"#.######.#", "#...##...#", "####..####", "####..####", ".........."})},
        };
    }
} // namespace amini_ref

#endif // TETRIS_AI_RUNNER_TESTS_AMINI_REFERENCE_H_
