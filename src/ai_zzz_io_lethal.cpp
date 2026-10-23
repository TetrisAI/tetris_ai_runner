#include "ai_zzz_io_lethal.h"

#include <algorithm>

namespace ai_zzz
{
    namespace
    {
        using Helpers = m_tetris2::Helpers<IOLethal::Spec>;
        using Map = IOLethal::Map;

        constexpr auto kSpawnCells = IO::make_spawn_cells<IOLethal::Spec>();
        constexpr int kMaxSent = 64;
        constexpr std::uint64_t kAll = ~std::uint64_t(0);
        constexpr int kMaxStalls = 2;
        constexpr int kMaxSearches = 512;
        // Bounds the opponent's attack multiplier over its replies.
        constexpr int kReplyFrames = 600;
        constexpr int kActivateAll = 1 << 20;

        // Bit t: tanking t rows blocks the spawn. Garbage rows count as free (unknown holes).
        std::uint64_t tank_blocked(Map const &board, char piece, int cap)
        {
            IO::SpawnCells const &cells = kSpawnCells[m_tetris2::ai_rule_spec::piece_index<IOLethal::Spec>(piece)];
            std::uint64_t blocked = 0;
            for (int c = 0; c < cells.count; ++c)
            {
                for (int t = 0; t <= std::min<int>(cap, cells.y[c]); ++t)
                    blocked |= std::uint64_t(board.get(cells.x[c], cells.y[c] - t)) << t;
            }
            return blocked;
        }

        bool spawn_blocked(Map const &board, char piece)
        {
            return (tank_blocked(board, piece, 0) & 1) != 0;
        }
    }

    IOLethal::IOLethal()
    {
        search_.init(&config_);
    }

    std::uint64_t IOLethal::lethal_sent(Opponent const &opponent, io_rules::Options const &rules, int frame, int free_placements)
    {
        // The opener's double cancel would make the result depend on our garbage.
        if (opponent.pieces_placed < rules.openerphase || opponent.pending.size() > kQueueCapacity || opponent.pieces.empty())
            return 0;
        config_.allow_180 = rules.allow180;
        config_.spin_rule = IO::search_spin_rule(rules.spinbonuses).rule;
        rules_ = &rules;
        pieces_ = opponent.pieces;
        can_hold_ = opponent.can_hold;
        pieces_placed_ = opponent.pieces_placed;
        free_ = std::max(0, free_placements);
        horizon_ = free_ + kMaxStalls + 1;
        cap_ = std::min(io_rules::garbage_cap(rules, frame), kMaxSent - 1);
        multiplier_ = io_rules::garbage_multiplier(rules, frame + kReplyFrames);
        searches_ = 0;
        if (moves_.size() < static_cast<std::size_t>(horizon_))
            moves_.resize(static_cast<std::size_t>(horizon_));

        Node root{opponent.board, {}, opponent.counters, opponent.garbage_rows, 0, 0, opponent.hold};
        for (IO::PendingGarbage const &g : opponent.pending)
            root.queue.push(g.amount, g.frames_until_active);
        return ~survivors(root, 0);
    }

    std::uint64_t IOLethal::survivors(Node node, int j)
    {
        if (j == horizon_ || node.next >= pieces_.size())
            return kAll;
        if (j == free_)
            node.queue.advance(kActivateAll);
        char const current = pieces_[node.next];
        // Clutch-raised piece: not modelled.
        if (spawn_blocked(node.board, current))
            return kAll;
        std::uint64_t const survive = place(node, j, current, node.next + 1, node.hold);
        if (survive == kAll || (j == 0 && !can_hold_))
            return survive;
        bool const empty = node.hold == ' ';
        if (empty && node.next + 1 >= pieces_.size())
            return survive;
        char const played = empty ? pieces_[node.next + 1] : node.hold;
        if (spawn_blocked(node.board, played))
            return survive;
        return survive | place(node, j, played, empty ? node.next + 2 : node.next + 1, current);
    }

    std::uint64_t IOLethal::place(Node const &node, int j, char piece, std::size_t next, char hold)
    {
        if (next >= pieces_.size() || ++searches_ > kMaxSearches)
            return kAll;
        auto const [x, y] = Spec::spawn(piece, Spec::width, Spec::height);
        std::vector<LandPoint> &moves = moves_[static_cast<std::size_t>(j)];
        moves = *search_.search(node.board, m_tetris2::TetrisBlockStatus(piece, static_cast<std::int8_t>(x), static_cast<std::int8_t>(y), 0), 0);
        // Non-clearing placements first: they usually end the sequence without a search.
        std::uint64_t survive = 0;
        for (LandPoint const &lp : moves)
        {
            Map after = node.board;
            if (Helpers::attach_to_map(lp.state, after) != 0)
                continue;
            survive |= tank(node, j, after, next, hold);
            if (survive == kAll)
                return kAll;
        }
        for (LandPoint const &lp : moves)
        {
            Map after = node.board;
            int const lines = static_cast<int>(Helpers::attach_to_map(lp.state, after));
            if (lines == 0)
                continue;
            survive |= clear(node, j, lp, after, lines, next, hold);
            if (survive == kAll)
                return kAll;
        }
        return survive;
    }

    std::uint64_t IOLethal::tank(Node const &node, int j, Map const &after, std::size_t next, char hold)
    {
        char const next_piece = pieces_[next];
        if (j < free_)
        {
            Node child{after, node.queue, {node.counters.btb, 0}, node.garbage_rows, node.ours_cancelled, next, hold};
            int const rows = child.queue.tank(*rules_, cap_);
            if (((tank_blocked(after, next_piece, rows) >> rows) & 1) != 0)
                return 0;
            // Old garbage entered with unknown holes: not modelled.
            return rows > 0 ? kAll : survivors(child, j + 1);
        }
        std::uint64_t const blocked = tank_blocked(after, next_piece, cap_);
        int const queued = node.queue.total();
        std::uint64_t survive = 0;
        for (int x = 0; x < kMaxSent; ++x)
        {
            int const rows = std::min(cap_, queued + std::max(0, x - node.ours_cancelled));
            survive |= ((~blocked >> rows) & 1) << x;
        }
        return survive;
    }

    std::uint64_t IOLethal::clear(Node const &node, int j, LandPoint const &lp, Map const &after, int lines, std::size_t next, char hold)
    {
        Map piece{};
        Helpers::attach_to_map(lp.state, piece);
        int garbage_cleared = 0;
        for (int y = 0; y < node.garbage_rows; ++y)
            garbage_cleared += (node.board.occupied_row(y) | piece.occupied_row(y)) == Map::row_full;
        io_rules::Placement const placement{lines, static_cast<io_rules::Spin>(lp.type), after.none(), garbage_cleared > 0};
        io_rules::Attack const attack = io_rules::resolve_attack(*rules_, node.counters, placement, multiplier_);
        Node child{after, node.queue, attack.counters, node.garbage_rows - garbage_cleared, node.ours_cancelled, next, hold};
        int lines_sent = 0; // opener only
        child.ours_cancelled += io_rules::exchange(*rules_, attack, child.queue, pieces_placed_ + j + 1, lines_sent, cap_).sent;
        return survivors(child, j + 1);
    }
}
