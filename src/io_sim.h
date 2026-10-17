#pragma once

// TETR.IO Season 2 (Tetra League) 1v1 match simulator for two ai_zzz::IO bots.
//
// Time model: player s locks its k-th piece at frame floor(k * 60 / pps[s]) and plans the next piece at that same
// frame (ARE 0), so a plan sees the opponent's state at spawn time and the lock resolves against the incoming queue
// as it is at lock time. The AI evaluates its root placement at that placement's lock frame: it receives copies of
// the pending timers projected to the lock frame; the simulator's own queue only advances when time does. Locks are
// processed in frame order, seat 0 first on ties. Attacks reach the opponent's queue instantly and become active
// `garbagespeed` frames later; with instant delivery nothing is ever in flight, so zero passthrough reduces to
// cancelling against the own queue. All attack and garbage arithmetic comes from io_rules (rules.md §7-§9).
// Each plan gets the opponent's kill mask (IOLethal).
//
// Time limit: at `max_frames` the match is adjudicated by a continuous margin (seat 0 minus seat 1) of
//     health = (20 - stack height) - pending garbage + releasable surge (io_rules::surge_amount at the end frame)
// and seat 0 scores 0.5 + 0.45 * tanh(margin / 10). A kill always scores 1 / 0, so any kill beats any timeout.

#include "tetris_core.h"
#include "search_amini.h"
#include "search_path.h"
#include "rule_io.h"
#include "io_rules.h"
#include "ai_zzz_io.h"
#include "ai_zzz_io_lethal.h"

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>

namespace io_sim
{
    using Engine = m_tetris2::TetrisEngine<rule_io::TetrisRule, ai_zzz::IO, amini::Search>;
    using Map = Engine::map_t;
    using LandPoint = Engine::LandPoint;

    inline constexpr int kFramesPerSecond = 60;
    inline constexpr int kVisibleRows = 20;
    // Exact incoming-garbage storage: io_rules::PendingQueue merges entries once full, so the capacity must cover every
    // packet a match can deliver (see fits_pending_capacity).
    inline constexpr std::size_t kPendingCapacity = 16384;
    using PendingQueue = io_rules::PendingQueue<kPendingCapacity>;

    struct SearchBudget
    {
        int iterations = 64; // beam widenings per placement, deterministic; ~15 ms on an M4 Pro core (Release)
        int think_ms = 0;    // > 0: wall-clock budget per placement instead (not reproducible)
    };

    struct MatchConfig
    {
        io_rules::Options rules = io_rules::Options::tetra_league();
        std::array<double, 2> pps{2.0, 2.0};
        int next_count = 5;
        SearchBudget budget;
        int max_frames = 8 * 60 * kFramesPerSecond;
    };

    // Every opponent lock sends at most ExchangeResult::parts.size() packets, so this bounds the incoming entries.
    bool fits_pending_capacity(MatchConfig const &config);

    struct PlayerStats
    {
        int pieces = 0;
        int lines_cleared = 0;
        int attack = 0; // generated, before cancelling
        int lines_sent = 0;
        int garbage_cancelled = 0;
        int garbage_tanked = 0;
        int spins = 0;
        int surges = 0;
        int surge_lines = 0;
        int all_clears = 0;
        int max_btb = 0;
    };

    enum class EndReason : std::uint8_t
    {
        BlockOut,
        GarbageSmash,
        TimeLimit
    };

    struct MatchResult
    {
        int loser = -1; // -1: adjudicated at the time limit
        EndReason reason = EndReason::TimeLimit;
        int frames = 0;
        double margin = 0; // seat 0 minus seat 1 (always computed at the end)
        double score = 0.5; // seat 0
        std::array<PlayerStats, 2> stats;
    };

    struct LockResult
    {
        int lines = 0;
        int garbage_lines = 0;
        io_rules::Attack attack;
        io_rules::ExchangeResult exchange; // parts[] are the entries sent to the opponent
        int tanked = 0;                    // rows that entered the board (fewer than exchange.tanked on garbage smash)
        bool dead = false;                 // garbage smash while tanking
    };

    // One player's rules state: board, piece queue, hold, counters and incoming garbage.
    class Side
    {
    public:
        Side(io_rules::Options const &rules, std::int64_t seed, int next_count);

        char current() const
        {
            return queue_.front();
        }
        std::span<char const> preview() const
        {
            return {queue_.data() + 1, static_cast<std::size_t>(next_count_)};
        }
        // Spawn position of `piece`, raised by a clutch clear when allowed; nullopt = the spawn is blocked.
        std::optional<m_tetris2::TetrisBlockStatus> spawn(char piece) const;
        // Why a blocked spawn kills (rules.md §2): garbagesmash when the previous placement tanked garbage.
        EndReason spawn_failure() const
        {
            return last_tanked > 0 ? EndReason::GarbageSmash : EndReason::BlockOut;
        }
        // Removes the piece the engine plays (with hold: the held or the first preview piece) from the queue.
        char take(bool use_hold);
        void receive(int amount, int frame);
        // Advances incoming-garbage activation timers to `frame`.
        void sync(int frame);
        LockResult lock(Engine const &engine, LandPoint const &lp, int frame);
        int stack_height() const;
        double health(int frame) const;

        Map board{};
        int garbage_rows = 0;
        io_rules::Counters counters;
        int pieces_placed = 0;
        int lines_sent = 0;
        bool last_was_clear = false;
        int last_tanked = 0;
        char hold = ' ';
        PendingQueue pending;
        PlayerStats stats;

    private:
        bool push_garbage_row(int hole);
        void refill();

        io_rules::Options const *rules_;
        io_rules::Bag7 bag_;
        io_rules::HoleColumns holes_;
        std::vector<char> queue_;
        int next_count_;
        int queue_frame_ = 0;
    };

    class Match
    {
    public:
        Match(MatchConfig const &config, std::array<ai_zzz::IO::Param, 2> const &params, std::int64_t seed);
        Match(Match const &) = delete; // sides point into config_
        Match &operator=(Match const &) = delete;

        // Processes the next lock in frame order; false once the match is over.
        bool step();
        bool finished() const
        {
            return finished_;
        }
        MatchResult const &result() const
        {
            return result_;
        }
        Side const &side(int seat) const
        {
            return sides_[seat];
        }

    private:
        int lock_frame(int seat, int ahead = 0) const;
        std::uint64_t lethal_sent(int seat, int root_lock_frame);
        bool plan(int seat, int frame);
        void finish(int loser, EndReason reason, int frame);

        MatchConfig config_;
        std::array<ai_zzz::IO::Param, 2> params_;
        std::array<std::unique_ptr<Engine>, 2> engines_;
        std::array<Side, 2> sides_;
        std::array<LandPoint, 2> planned_{};
        std::vector<ai_zzz::IO::PendingGarbage> projected_pending_;
        ai_zzz::IOLethal lethal_;
        std::string opponent_pieces_;
        std::vector<ai_zzz::IO::PendingGarbage> opponent_pending_;
        std::array<int, 2> placements_{};
        MatchResult result_;
        bool finished_ = false;
    };

    MatchResult play_match(MatchConfig const &config, std::array<ai_zzz::IO::Param, 2> const &params, std::int64_t seed);

    double adjudicated_score(double margin);
}
