#ifndef TETRIS_AI_RUNNER_AI_ZZZ_IO_SPIN_SLOTS_H_
#define TETRIS_AI_RUNNER_AI_ZZZ_IO_SPIN_SLOTS_H_

#include "tetris_core.h"
#include "ai_rule_spec_utils.h"
#include "io_rules.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cassert>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <utility>

// Spin slots: line-clearing placements a piece enters with a spin by spawn -> rotate -> shift -> hard drop ->
// one rotation. The predecessor s' of a slot s is a hard-drop landing (grounded, nothing above any of its cells)
// whose first legal rotation test lands on s; the class is the search's own classify_rotation. Tucks, soft-drop
// stops and multi-rotation entries are not searched, so a slot is always realizable but not every spin is a slot.
namespace ai_zzz
{
    struct SpinSlot
    {
        io_rules::Spin spin = io_rules::Spin::None; // None: no slot for this piece
        std::uint8_t lines = 0;
    };

    namespace spin_slots_detail
    {
        // One rotation's cells relative to its State corner (xb, yb): row masks, and the same rows packed into 16-bit
        // lanes (lane j = row j) to test against Board windows. origin converts to master coordinates, which the kick
        // offsets apply to.
        struct Footprint
        {
            std::array<std::uint32_t, 4> rows{};
            std::array<int, 4> counts{};
            std::uint64_t cells = 0;
            int h = 0;
            int w = 0;
            int origin_x = 0;
            int origin_y = 0;
        };

        template<class Spec, char P, std::uint8_t R>
        consteval Footprint make_footprint()
        {
            constexpr auto cells = m_tetris2::shape::piece_cells<Spec, P, R>;
            Footprint f{};
            for (std::size_t i = 0; i < cells.count; ++i)
            {
                f.rows.at(cells.cells[i].y) |= std::uint32_t{1} << cells.cells[i].x;
                ++f.counts.at(cells.cells[i].y);
                f.cells |= std::uint64_t{1} << (16 * cells.cells[i].y + cells.cells[i].x);
                f.h = std::max(f.h, cells.cells[i].y + 1);
                f.w = std::max(f.w, cells.cells[i].x + 1);
            }
            f.origin_x = cells.origin.x;
            f.origin_y = cells.origin.y;
            return f;
        }

        template<class Spec, char P, std::uint8_t R>
        inline constexpr Footprint kFootprint = make_footprint<Spec, P, R>();

        // Four cells, each row one run: what the gap-row filter in Board::load relies on.
        consteval bool tetromino_rows(Footprint const &f)
        {
            int cells = 0;
            for (int j = 0; j < f.h; ++j)
            {
                if (f.rows[j] == 0)
                    return false;
                std::uint32_t const run = f.rows[j] >> std::countr_zero(f.rows[j]);
                if ((run & (run + 1)) != 0)
                    return false;
                cells += f.counts[j];
            }
            return cells == 4;
        }

        template<class Spec, char P>
        inline constexpr int kRotations = static_cast<int>(m_tetris2::shape::rotation_count<Spec, P>);

        template<class Spec, char P>
        consteval bool rotates()
        {
            return []<std::size_t... Rs>(std::index_sequence<Rs...>)
            {
                using namespace m_tetris2;
                return ((shape::target_cw<Spec, P, Rs> != kOpRotateNone ||
                         shape::target_ccw<Spec, P, Rs> != kOpRotateNone ||
                         shape::target_opp<Spec, P, Rs> != kOpRotateNone) ||
                        ...);
            }(std::make_index_sequence<kRotations<Spec, P>>{});
        }

        // Lowest row any rotation of P occupies at the spawn position. It also checks what makes the spawn ->
        // rotate -> shift part of a predecessor's path need nothing but empty rows from there up: CW presses reach
        // every rotation and every rotation at the spawn position is inside the board, so with those rows empty each
        // plain rotation and each shift succeeds; and that P is a tetromino with single-run rows (-1 otherwise).
        template<class Spec, char P>
        consteval int spawn_floor_of()
        {
            constexpr auto sp = Spec::spawn(P, Spec::width, Spec::height);
            int lo = INT_MAX;
            bool ok = true;
            [&]<std::size_t... Rs>(std::index_sequence<Rs...>)
            {
                constexpr int kR = kRotations<Spec, P>;
                ((lo = std::min(lo, sp.second - kFootprint<Spec, P, Rs>.origin_y),
                  ok = ok && tetromino_rows(kFootprint<Spec, P, Rs>) &&
                       m_tetris2::shape::target_cw<Spec, P, Rs> == (Rs + 1) % kR &&
                       sp.first + kFootprint<Spec, P, Rs>.origin_x >= 0 &&
                       sp.first + kFootprint<Spec, P, Rs>.origin_x + kFootprint<Spec, P, Rs>.w <= Spec::width &&
                       sp.second - kFootprint<Spec, P, Rs>.origin_y + kFootprint<Spec, P, Rs>.h <= Spec::height),
                 ...);
            }(std::make_index_sequence<kRotations<Spec, P>>{});
            return ok ? lo : -1;
        }

        template<class Spec>
        consteval int spawn_floor()
        {
            int lo = INT_MAX;
            [&]<std::size_t... Is>(std::index_sequence<Is...>)
            {
                ((rotates<Spec, m_tetris2::ai_rule_spec::piece_type_at<Spec>(Is)>()
                      ? (lo = std::min(lo, spawn_floor_of<Spec, m_tetris2::ai_rule_spec::piece_type_at<Spec>(Is)>()))
                      : 0),
                 ...);
            }(std::make_index_sequence<m_tetris2::ai_rule_spec::piece_count<Spec>()>{});
            return lo;
        }

        // Rows are stored with a wall column on each side (board column x is bit x + 1) and full floor rows below
        // row 0, so out-of-board cells count as occupied. A window packs rows y..y+3 into 16-bit lanes (lane j = row
        // y + j): a footprint at column x tests against it with one shift and one AND.
        template<class Spec>
        struct Board
        {
            using Helpers = m_tetris2::Helpers<Spec>;
            using Map = typename Helpers::map_t;
            static constexpr int kWidth = static_cast<int>(Spec::width);
            static_assert(kWidth <= 13);
            static constexpr std::uint32_t kCols = ((std::uint32_t{1} << kWidth) - 1) << 1;
            static constexpr std::uint32_t kWalls = 1 | (std::uint32_t{1} << (kWidth + 1));
            static constexpr std::uint32_t kSolid = kCols | kWalls;
            static constexpr int kRoof = spawn_floor<Spec>();
            static_assert(kRoof > 0, "rotating pieces must be tetrominoes whose spawn rotations are inside the board "
                                     "and reachable by CW presses");
            // Floor rows below row 0, enough for the 4 rows a gap-row neighbourhood reaches down.
            static constexpr int kBelow = 4;
            // Rows are read up to 4 above the top.
            static constexpr int kRows = kBelow + kRoof + 4;

            // A row whose gaps (one run of 1-4 cells, board columns) one piece row can fill.
            struct GapRow
            {
                int y;
                int count;
                std::uint32_t gaps;
            };

            Map const &map;
            int top = -1;
            std::array<std::uint32_t, kRows> rows;
            std::array<std::uint64_t, kRows> windows;
            std::array<std::uint64_t, kRows> covered_windows; // cells with an occupied cell at or above them
            std::array<GapRow, kRoof> gap_rows;
            int gap_row_count = 0;

            explicit Board(Map const &m) : map(m) {}

            // False when the stack reaches the roof or no row can be completed by one row of one piece (an empty
            // board has max_y 0 and a row 0 with no cell).
            bool load()
            {
                top = map.max_y();
                if (top >= kRoof)
                    return false;
                for (int i = 0; i < kBelow; ++i)
                    rows[i] = kSolid;
                for (int y = 0; y <= top; ++y)
                    rows[y + kBelow] = (static_cast<std::uint32_t>(map.row(y)) << 1) | kWalls;
                for (int i = top + 1 + kBelow; i <= top + kBelow + 4; ++i)
                    rows[i] = kWalls;
                for (int y = 0; y <= top; ++y)
                {
                    // Piece rows are single runs of cells (adding the lowest gap clears the run and sets the bit
                    // above it), and a tetromino with cells off this row reaches them vertically through a gap.
                    std::uint32_t const gaps = ~row(y) & kCols;
                    std::uint32_t const low = gaps & (0u - gaps);
                    std::uint32_t const end = gaps + low;
                    bool const up = (gaps & ~row(y + 1)) != 0;
                    bool const down = (gaps & ~row(y - 1)) != 0;
                    gap_rows[gap_row_count] = {y, std::countr_zero(end) - std::countr_zero(low), gaps >> 1};
                    gap_row_count +=
                        gaps != 0 && (end & gaps) == 0 && end <= (low << 4) && (end == (low << 4) || up || down);
                }
                if (gap_row_count == 0)
                    return false;
                std::uint64_t window = 0;
                std::uint64_t covered = 0;
                std::uint32_t above = 0;
                for (int i = top + 1 + kBelow + 3; i >= 0; --i)
                {
                    above |= rows[i];
                    window = (window << 16) | rows[i];
                    covered = (covered << 16) | above;
                    windows[i] = window;
                    covered_windows[i] = covered;
                }
                return true;
            }

            std::uint32_t row(int y) const
            {
                return rows[y + kBelow];
            }

            // Valid for -1 <= y <= top + 1.
            std::uint64_t window(int y) const
            {
                return windows[y + kBelow];
            }

            std::uint64_t covered_window(int y) const
            {
                return covered_windows[y + kBelow];
            }
        };

        template<class Spec, class RuleHook, class Fn>
        struct Scanner
        {
            using B = Board<Spec>;
            using Helpers = typename B::Helpers;
            using Map = typename B::Map;
            using AMiniType = typename RuleHook::AMiniType;
            static_assert(static_cast<int>(AMiniType::None) == static_cast<int>(io_rules::Spin::None) &&
                          static_cast<int>(AMiniType::Mini) == static_cast<int>(io_rules::Spin::Mini) &&
                          static_cast<int>(AMiniType::Full) == static_cast<int>(io_rules::Spin::Full));
            static constexpr std::size_t kPieces = m_tetris2::ai_rule_spec::piece_count<Spec>();

            struct Context
            {
                B const &board;
                bool allow_180;
                Fn &fn;
                std::array<Map, Helpers::kMaxR> usable;
                std::size_t usable_piece = kPieces; // piece the usable maps are built for
            };

            using Search = void (*)(Context &, int xb, int yb, int lines);

            // Rotation R of a piece with its row `row` on a gap row, in wall coordinates; only the predecessor search
            // is compiled per piece and rotation.
            struct Candidate
            {
                std::uint64_t cells = 0;
                std::uint64_t corners = 0; // T: corners around the pivot (3 occupied spin)
                std::uint64_t lanes = 0;   // 0x8000 in each lane the piece spans
                Search search = nullptr;
                std::uint8_t row = 0;
                std::uint8_t shift = 0; // lowest column of row `row`
                bool immobile_mini = false;
            };

            // Only a T scores without immobility (the corner test); others need the rule's immobile Mini.
            template<char P>
            static constexpr bool kScanned = rotates<Spec, P>() && (P == 'T' || RuleHook::template kImmobileMini<P>);

            // Candidates with C cells in their gap row. Their cells span columns dx_min .. dx_min + width - 1
            // relative to the lowest gap.
            struct Candidates
            {
                std::array<Candidate, 64> items{};
                int count = 0;
                int dx_min = 0;
                int width = 1;
            };

            // Calls fn(dx, dy) for each cell of c relative to the lowest gap of its gap row.
            template<class CellFn>
            static constexpr void for_each_cell(Candidate const &c, CellFn &&fn)
            {
                for (int bit = 0; bit < 64; ++bit)
                    if ((c.cells >> bit) & 1)
                        fn(bit % 16 - 1 - c.shift, bit / 16 - c.row);
            }

            template<int C>
            static consteval Candidates make_candidates()
            {
                Candidates t;
                auto add = [&]<std::size_t Index, std::uint8_t R>()
                {
                    constexpr char kPiece = m_tetris2::ai_rule_spec::piece_type_at<Spec>(Index);
                    constexpr Footprint f = kFootprint<Spec, kPiece, R>;
                    for (int i = 0; i < f.h; ++i)
                    {
                        if (f.counts[i] != C)
                            continue;
                        Candidate c;
                        c.cells = f.cells << 1;
                        c.row = static_cast<std::uint8_t>(i);
                        c.shift = static_cast<std::uint8_t>(std::countr_zero(f.rows[i]));
                        for (int j = 0; j < f.h; ++j)
                            c.lanes |= std::uint64_t{0x8000} << (16 * j);
                        if constexpr (kPiece == 'T')
                        {
                            // Corners around the pivot, in the window one row below the corner (yb - 1).
                            constexpr auto pivot = m_tetris2::shape::piece_cells<Spec, 'T', R>.pivot;
                            static_assert(pivot.x >= 0 && pivot.y >= 0 && pivot.y <= 1);
                            for (int dy : {0, 2})
                                for (int dx : {0, 2})
                                    c.corners |= std::uint64_t{1} << (16 * (pivot.y + dy) + pivot.x + dx);
                        }
                        c.immobile_mini = RuleHook::template kImmobileMini<kPiece>;
                        c.search = &search<Index, R>;
                        t.items.at(t.count++) = c;
                    }
                };
                [&]<std::size_t... Is>(std::index_sequence<Is...>)
                {
                    auto piece = [&]<std::size_t Index>()
                    {
                        constexpr char kPiece = m_tetris2::ai_rule_spec::piece_type_at<Spec>(Index);
                        if constexpr (kScanned<kPiece>)
                            [&]<std::size_t... Rs>(std::index_sequence<Rs...>)
                            {
                                (add.template operator()<Index, static_cast<std::uint8_t>(Rs)>(), ...);
                            }(std::make_index_sequence<kRotations<Spec, kPiece>>{});
                    };
                    (piece.template operator()<Is>(), ...);
                }(std::make_index_sequence<kPieces>{});
                int lo = 0;
                int hi = 0;
                for (int k = 0; k < t.count; ++k)
                    for_each_cell(t.items[k], [&](int dx, int)
                                  {
                                      lo = std::min(lo, dx);
                                      hi = std::max(hi, dx); });
                t.dx_min = lo;
                t.width = hi - lo + 1;
                return t;
            }

            template<int C>
            static constexpr Candidates kCandidates = make_candidates<C>();

            // Rows around a gap row (relative -4 .. 3) that candidate cells (-3 .. 3) and the cells under them cover.
            static constexpr int kDyMin = -4;
            static constexpr int kSpan = 8;

            // Per neighbouring row and its occupancy over the candidates' columns: the candidates that collide with
            // it, and those it supports (an occupied cell right under one of their cells). OR-ing one entry per row
            // gives exactly the candidates that fit and are grounded; walls and the floor rows make a candidate that
            // leaves the board collide, since a tetromino crossing the board edge covers the column or row next to it.
            template<int C>
            struct Lookup
            {
                static constexpr int kWidth = kCandidates<C>.width;
                static_assert(kWidth <= 8);
                std::array<std::array<std::uint64_t, (1u << kWidth)>, kSpan> conflict{};
                std::array<std::array<std::uint64_t, (1u << kWidth)>, kSpan> support{};
            };

            template<int C>
            static consteval Lookup<C> make_lookup()
            {
                Lookup<C> l;
                Candidates const &t = kCandidates<C>;
                for (int k = 0; k < t.count; ++k)
                    for_each_cell(t.items[k], [&](int dx, int dy)
                                  {
                                      for (unsigned m = 0; m < (1u << Lookup<C>::kWidth); ++m)
                                      {
                                          if (((m >> (dx - t.dx_min)) & 1) == 0)
                                              continue;
                                          l.conflict.at(dy - kDyMin)[m] |= std::uint64_t{1} << k;
                                          l.support.at(dy - 1 - kDyMin)[m] |= std::uint64_t{1} << k;
                                      } });
                return l;
            }

            template<int C>
            static constexpr Lookup<C> kLookup = make_lookup<C>();

            static constexpr std::uint64_t kSolidLanes = 0x0001000100010001ull * B::kSolid;
            static constexpr std::uint64_t kCarryLanes = 0x0001000100010001ull * 0x7fff;
            static constexpr std::uint64_t kHighLanes = 0x0001000100010001ull * 0x8000;

            static void run(B const &board, bool allow_180, Fn &fn)
            {
                Context ctx{board, allow_180, fn, {}};
                for (int g = 0; g < board.gap_row_count; ++g)
                {
                    typename B::GapRow const &row = board.gap_rows[g];
                    switch (row.count)
                    {
                    case 1: scan<1>(ctx, row); break;
                    case 2: scan<2>(ctx, row); break;
                    case 3: scan<3>(ctx, row); break;
                    default: scan<4>(ctx, row); break;
                    }
                }
            }

            template<int C>
            static void scan(Context &ctx, typename B::GapRow const &g)
            {
                constexpr Candidates const &t = kCandidates<C>;
                if constexpr (t.count != 0)
                {
                    static_assert(t.dx_min >= -4);
                    constexpr Lookup<C> const &l = kLookup<C>;
                    constexpr std::uint64_t kMask = (std::uint64_t{1} << Lookup<C>::kWidth) - 1;
                    B const &b = ctx.board;
                    int const lo = std::countr_zero(g.gaps);
                    // Rows are shifted left by 4 so the shift stays non-negative; bit 0 of a row is board column -1.
                    int const shift = lo + 1 + t.dx_min + 4;
                    std::uint64_t conflict = 0;
                    std::uint64_t support = 0;
                    for (int r = 0; r < kSpan; ++r)
                    {
                        std::uint64_t const m = ((std::uint64_t{b.row(g.y + kDyMin + r)} << 4) >> shift) & kMask;
                        conflict |= l.conflict[r][m];
                        support |= l.support[r][m];
                    }
                    for (std::uint64_t alive = support & ~conflict; alive != 0; alive &= alive - 1)
                        place(ctx, t.items[std::countr_zero(alive)], g, lo);
                }
            }

            // A candidate that fits and is grounded. Its piece row covers exactly the gaps; a placement completing
            // several rows is taken from the lowest one. The spin test is the part of RuleHook::classify_rotation
            // that depends on neither the predecessor nor the kick.
            static void place(Context &ctx, Candidate const &c, typename B::GapRow const &g, int lo)
            {
                B const &b = ctx.board;
                int const xb = lo - c.shift;
                int const yb = g.y - c.row;
                assert(xb >= 0 && yb >= 0);
                assert(((c.cells >> (16 * c.row + 1) << xb) & 0xffff) == g.gaps);
                std::uint64_t const cells = c.cells << xb;
                std::uint64_t const here = b.window(yb);
                std::uint64_t const below = b.window(yb - 1);
                assert((cells & here) == 0 && (cells & below) != 0);
                if (std::popcount((c.corners << xb) & below) < 3 &&
                    !(c.immobile_mini && ((cells >> 1) & here) != 0 && ((cells << 1) & here) != 0 &&
                      (cells & b.window(yb + 1)) != 0))
                    return;
                std::uint64_t const open = ~(cells | here) & kSolidLanes;
                std::uint64_t const full = ~((open + kCarryLanes) & kHighLanes) & c.lanes;
                if ((full & ((std::uint64_t{1} << (16 * c.row)) - 1)) != 0)
                    return;
                c.search(ctx, xb, yb, std::popcount(full));
            }

            template<std::size_t Index, std::uint8_t R>
            static void search(Context &ctx, int xb, int yb, int lines)
            {
                constexpr char kPiece = m_tetris2::ai_rule_spec::piece_type_at<Spec>(Index);
                m_tetris2::State const s{static_cast<std::uint8_t>(kPiece), R, static_cast<std::int8_t>(xb),
                                         static_cast<std::int8_t>(yb)};
                [&]<std::size_t... Ss>(std::index_sequence<Ss...>)
                {
                    (from_rotation<Index, static_cast<std::uint8_t>(Ss), R>(ctx, s, lines), ...);
                }(std::make_index_sequence<kRotations<Spec, kPiece>>{});
            }

            template<std::size_t Index, std::uint8_t S, std::uint8_t R>
            static void from_rotation(Context &ctx, m_tetris2::State const &s, int lines)
            {
                using namespace m_tetris2;
                constexpr char kPiece = ai_rule_spec::piece_type_at<Spec>(Index);
                if constexpr (shape::target_cw<Spec, kPiece, S> == R)
                    from_kicks<Index, S, R, KickDir::Cw, shape::wk_cw<Spec, kPiece, S>>(ctx, s, lines);
                if constexpr (shape::target_ccw<Spec, kPiece, S> == R)
                    from_kicks<Index, S, R, KickDir::Ccw, shape::wk_ccw<Spec, kPiece, S>>(ctx, s, lines);
                if constexpr (shape::target_opp<Spec, kPiece, S> == R)
                    if (ctx.allow_180)
                        from_kicks<Index, S, R, KickDir::Opp, shape::wk_opp<Spec, kPiece, S>>(ctx, s, lines);
            }

            // Predecessors in rotation S whose test k would land on s: grounded with nothing above any cell, so a
            // hard drop from the spawn rows stops there. The engine decides which test is first legal.
            template<std::size_t Index, std::uint8_t S, std::uint8_t R, m_tetris2::KickDir Dir, class Kicks>
            static void from_kicks(Context &ctx, m_tetris2::State const &s, int lines)
            {
                constexpr char kPiece = m_tetris2::ai_rule_spec::piece_type_at<Spec>(Index);
                constexpr Footprint from = kFootprint<Spec, kPiece, S>;
                constexpr Footprint to = kFootprint<Spec, kPiece, R>;
                B const &b = ctx.board;
                int const mx = s.xb - to.origin_x;
                int const my = s.yb + to.origin_y;
                for (std::size_t k = 0; k <= Kicks::length; ++k)
                {
                    int const dx = k == 0 ? 0 : Kicks::data[2 * k - 2];
                    int const dy = k == 0 ? 0 : Kicks::data[2 * k - 1];
                    int const xb = mx - dx + from.origin_x;
                    int const yb = my - dy - from.origin_y;
                    if (xb < 0 || xb > B::kWidth - from.w || yb < 0 || yb > b.top + 1)
                        continue;
                    std::uint64_t const cells = from.cells << (xb + 1);
                    if ((cells & b.covered_window(yb)) != 0 || (cells & b.window(yb - 1)) == 0)
                        continue;
                    if (ctx.usable_piece != Index)
                    {
                        Helpers::template build_usable_T<kPiece>(b.map, ctx.usable);
                        ctx.usable_piece = Index;
                    }
                    m_tetris2::State const pred{static_cast<std::uint8_t>(kPiece), S, static_cast<std::int8_t>(xb),
                                                static_cast<std::int8_t>(yb)};
                    auto const hit = Helpers::first_kick_hit(kPiece, Dir, pred, ctx.usable);
                    if (!hit || hit->state.r != s.r || hit->state.xb != s.xb || hit->state.yb != s.yb)
                        continue;
                    AMiniType const type =
                        RuleHook::template classify_rotation<Spec, kPiece>(b.map, ctx.usable, hit->state, hit->test);
                    if (type != AMiniType::None)
                        ctx.fn(Index, s, static_cast<io_rules::Spin>(type), lines);
                }
            }
        };
    } // namespace spin_slots_detail

    // Lowest board row the spawn -> rotate -> shift path of some piece occupies; slots are only reported while the
    // board is empty from this row up (row 19 for rule_io: I r1/r3 at the spawn reach down to it).
    template<class Spec>
    inline constexpr int kSpinSlotRoof = spin_slots_detail::Board<Spec>::kRoof;

    // Calls fn(piece_index, slot, spin, lines) once per (slot, predecessor) whose rotation classifies as a spin;
    // piece_index follows m_tetris2::ai_rule_spec::piece_type_at<Spec>.
    template<class Spec, class RuleHook, class Fn>
    void for_each_spin_slot(typename m_tetris2::Helpers<Spec>::map_t const &map, bool allow_180, Fn &&fn)
    {
        spin_slots_detail::Board<Spec> board(map);
        if (board.load())
            spin_slots_detail::Scanner<Spec, RuleHook, std::remove_reference_t<Fn>>::run(board, allow_180, fn);
    }

    // Best slot per piece: most base attack, then more lines, then the higher class.
    template<class Spec, class RuleHook>
    std::array<SpinSlot, m_tetris2::ai_rule_spec::piece_count<Spec>()> find_spin_slots(
        typename m_tetris2::Helpers<Spec>::map_t const &board, bool allow_180)
    {
        std::array<SpinSlot, m_tetris2::ai_rule_spec::piece_count<Spec>()> best{};
        auto key = [](io_rules::Spin spin, int lines)
        {
            return std::array<int, 3>{io_rules::kBaseAttack[lines][static_cast<int>(spin)], lines,
                                      static_cast<int>(spin)};
        };
        for_each_spin_slot<Spec, RuleHook>(board, allow_180,
                                           [&](std::size_t i, m_tetris2::State const &, io_rules::Spin spin, int lines)
                                           {
                                               if (key(spin, lines) > key(best[i].spin, best[i].lines))
                                                   best[i] = {spin, static_cast<std::uint8_t>(lines)};
                                           });
        return best;
    }
} // namespace ai_zzz

#endif // TETRIS_AI_RUNNER_AI_ZZZ_IO_SPIN_SLOTS_H_
