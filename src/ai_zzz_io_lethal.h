#pragma once

#include "tetris_core.h"
#include "search_amini.h"
#include "search_path.h"
#include "rule_io.h"
#include "io_rules.h"
#include "ai_zzz_io.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

// Certain kills: the opponent dies on every reply sequence of its known pieces. Anything unmodelled counts as survival.
namespace ai_zzz
{
    class IOLethal
    {
    public:
        using Spec = rule_io::TetrisRule::rule_spec;
        using Map = m_tetris2::Helpers<Spec>::map_t;

        struct Opponent
        {
            Map board;
            int garbage_rows = 0;
            std::string_view pieces; // piece in play, then the queue
            char hold = ' ';
            bool can_hold = true;
            io_rules::Counters counters;
            int pieces_placed = 0;
            std::span<IO::PendingGarbage const> pending;
        };

        IOLethal();
        IOLethal(IOLethal const &) = delete;
        IOLethal &operator=(IOLethal const &) = delete;

        // Bit X: sending exactly X lines with the root placement kills. Not monotone in X.
        std::uint64_t lethal_sent(Opponent const &opponent, io_rules::Options const &rules, int frame, int free_placements);

    private:
        using Search = m_tetris2::TetrisEngine<rule_io::TetrisRule, IO, amini::Search>::TetrisSearch;
        using LandPoint = Search::LandPoint;
        static constexpr std::size_t kQueueCapacity = 32;
        using Queue = io_rules::PendingQueue<kQueueCapacity>;

        struct Node
        {
            Map board;
            Queue queue; // garbage queued before ours
            io_rules::Counters counters;
            int garbage_rows;
            int ours_cancelled;
            std::size_t next;
            char hold;
        };

        // Bit X: the opponent survives X lines.
        std::uint64_t survivors(Node node, int j);
        std::uint64_t place(Node const &node, int j, char piece, std::size_t next, char hold);
        std::uint64_t tank(Node const &node, int j, Map const &after, std::size_t next, char hold);
        std::uint64_t clear(Node const &node, int j, LandPoint const &lp, Map const &after, int lines, std::size_t next, char hold);

        search_amini::Search::Config config_;
        Search search_;
        std::vector<std::vector<LandPoint>> moves_;

        io_rules::Options const *rules_ = nullptr;
        std::string_view pieces_;
        bool can_hold_ = true;
        int pieces_placed_ = 0;
        int free_ = 0;
        int horizon_ = 0;
        int cap_ = 0;
        double multiplier_ = 1;
        int searches_ = 0;
    };
}
