// io_sim_test: TETR.IO S2 match simulator — shared bags, garbage bookkeeping, surge delivery, death, adjudication.

#include "../src/io_sim.h"

#include <bit>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <vector>

namespace
{
    using io_sim::LandPoint;
    using io_sim::Map;
    using io_sim::Side;

    io_rules::Options const kRules = io_rules::Options::tetra_league();

    io_sim::MatchConfig quick_config()
    {
        io_sim::MatchConfig config;
        config.budget.nodes = 1;
        return config;
    }

    // First landing of `piece` on `side`'s board accepted by `accept(lp, board_after, lines)`.
    template<class Accept>
    LandPoint find_landing_where(io_sim::Engine &engine, Side const &side, char piece, Accept accept)
    {
        auto spawn = side.spawn(piece);
        assert(spawn);
        std::vector<LandPoint> landings;
        engine.search(*spawn, side.board, landings);
        for (auto const &lp : landings)
        {
            Map board = side.board;
            int lines = static_cast<int>(engine.attach(lp, board));
            if (accept(lp, board, lines))
                return lp;
        }
        assert(false && "no such landing");
        return {};
    }

    LandPoint find_landing(io_sim::Engine &engine, Side const &side, char piece, search_amini::Search::AMiniType type, int lines)
    {
        return find_landing_where(engine, side, piece, [&](LandPoint const &lp, Map const &, int cleared)
                                  { return lp.type == type && cleared == lines; });
    }

    void fill_rows(Side &side, int rows)
    {
        for (int y = 0; y < rows; ++y)
            side.board.set_row(y, static_cast<Map::row_t>(Map::row_full & ~(Map::row_t(1) << 9)));
    }

    void same_seed_same_bags()
    {
        constexpr std::int64_t kSeed = 882087139;
        io_rules::Bag7 bag(kSeed);
        Side a(kRules, kSeed, 5), b(kRules, kSeed, 5);
        for (int bag_index = 0; bag_index < 10; ++bag_index)
        {
            int seen = 0;
            for (int i = 0; i < 7; ++i)
            {
                char expected = bag.next();
                assert(a.current() == expected && b.current() == expected);
                assert(a.preview()[0] == b.preview()[0]);
                seen |= 1 << (std::string_view("ZLOSIJT").find(expected));
                a.take(false);
                b.take(false);
            }
            assert(seen == 0x7f);
        }
    }

    void surge_reaches_opponent()
    {
        io_sim::Engine engine;
        engine.prepare(Map::width, Map::height);
        Side sender(kRules, 5, 5), receiver(kRules, 5, 5);
        sender.board.set_row(0, static_cast<Map::row_t>(Map::row_full & ~Map::row_t(0b1111)));
        sender.board.set(9, 1); // keeps the clear from being an all clear
        sender.counters.btb = 9;

        io_sim::LockResult surge = sender.lock(engine, find_landing(engine, sender, 'I', search_amini::Search::None, 1), 30);
        auto const &sent = surge.exchange;
        assert(surge.attack.chain_broken && surge.attack.lines == 0);
        assert(sent.part_count == 3 && sent.parts[0] == 3 && sent.parts[1] == 3 && sent.parts[2] == 2);
        assert(sender.counters.btb == 0 && sender.lines_sent == 8 && sender.stats.surges == 1 && sender.stats.surge_lines == 8);
        for (int i = 0; i < sent.part_count; ++i)
            receiver.receive(sent.parts[i], 30);
        assert(receiver.pending.total() == 8 && receiver.pending.entries().size() == 3);
        for (auto const &entry : receiver.pending.entries())
            assert(entry.frames_until_active == kRules.garbagespeed);

        io_sim::LockResult early = receiver.lock(engine, find_landing(engine, receiver, 'O', search_amini::Search::None, 0), 30 + kRules.garbagespeed - 1);
        assert(early.tanked == 0 && receiver.garbage_rows == 0);
        io_sim::LockResult tank = receiver.lock(engine, find_landing(engine, receiver, 'O', search_amini::Search::None, 0), 30 + kRules.garbagespeed);
        assert(tank.tanked == 8 && receiver.garbage_rows == 8 && receiver.pending.total() == 0);
        assert(receiver.stats.garbage_tanked == 8);
        auto hole = [&](int y)
        { return std::countr_one(static_cast<unsigned>(receiver.board.row(y))); };
        for (int y = 0; y < 8; ++y)
            assert(std::popcount(static_cast<unsigned>(receiver.board.row(y))) == Map::width - 1);
        // The first entry entered first, so it ends up on top; each entry keeps one hole column.
        assert(hole(0) == hole(1));
        assert(hole(2) == hole(3) && hole(3) == hole(4));
        assert(hole(5) == hole(6) && hole(6) == hole(7));
    }

    void block_out_and_clutch()
    {
        Side side(kRules, 11, 5);
        fill_rows(side, 26);
        assert(!side.spawn('T'));
        side.last_was_clear = true;
        auto raised = side.spawn('T');
        assert(raised);
        io_rules::Options no_clutch = kRules;
        no_clutch.clutch = false;
        Side strict(no_clutch, 11, 5);
        strict.board = side.board;
        strict.last_was_clear = true;
        assert(!strict.spawn('T'));
    }

    // A blocked spawn is a garbage smash exactly when the previous placement tanked garbage (rules.md §2).
    void death_reason()
    {
        io_sim::Engine engine;
        engine.prepare(Map::width, Map::height);
        auto plain_o = [](int x, int y)
        { return [=](LandPoint const &lp, Map const &after, int lines)
          { return lp.type == search_amini::Search::None && lines == 0 && after.get(x, y); }; };

        Side stacked(kRules, 3, 5);
        fill_rows(stacked, 20);
        io_sim::LockResult top_out = stacked.lock(engine, find_landing_where(engine, stacked, 'O', plain_o(4, 21)), 60);
        assert(top_out.tanked == 0 && !top_out.dead);
        assert(!stacked.spawn('T') && stacked.spawn_failure() == io_sim::EndReason::BlockOut);

        Side smashed(kRules, 3, 5);
        fill_rows(smashed, 19);
        smashed.receive(3, 0);
        io_sim::LockResult tank = smashed.lock(engine, find_landing_where(engine, smashed, 'O', plain_o(0, 19)), kRules.garbagespeed);
        assert(tank.tanked == 3 && !tank.dead && smashed.last_tanked == 3);
        assert(!smashed.spawn('T') && smashed.spawn_failure() == io_sim::EndReason::GarbageSmash);
    }

    // Every received packet stays a separate entry with its own activation time (no merging, rules.md §9).
    void pending_entries_are_exact()
    {
        Side side(kRules, 1, 5);
        constexpr int kPackets = 100;
        for (int i = 0; i < kPackets; ++i)
            side.receive(1, i);
        auto entries = side.pending.entries();
        assert(static_cast<int>(entries.size()) == kPackets && side.pending.total() == kPackets);
        for (int i = 0; i < kPackets; ++i)
            assert(entries[i].amount == 1 && entries[i].frames_until_active == kRules.garbagespeed - (kPackets - 1 - i));

        io_sim::MatchConfig config;
        assert(io_sim::fits_pending_capacity(config));
        config.pps = {2.0, 1000.0};
        assert(!io_sim::fits_pending_capacity(config));
    }

    void outmatched_player_dies()
    {
        io_sim::MatchConfig config = quick_config();
        config.pps = {3.0, 0.5};
        config.max_frames = 4 * 60 * io_sim::kFramesPerSecond;
        auto match = std::make_unique<io_sim::Match>(config, std::array<ai_zzz::IO::Param, 2>{}, 42);
        while (match->step())
        {
        }
        auto const &r = match->result();
        std::printf("outmatched: loser %d reason %d frames %d sent %d/%d\n", r.loser, static_cast<int>(r.reason), r.frames,
                    r.stats[0].lines_sent, r.stats[1].lines_sent);
        assert(r.loser == 1 && r.score == 1.0 && r.frames < config.max_frames);
        Side const &loser = match->side(1);
        if (r.reason == io_sim::EndReason::BlockOut)
            assert(loser.last_tanked == 0);
        else
            assert(r.reason == io_sim::EndReason::GarbageSmash && loser.stats.garbage_tanked > 0);
    }

    void garbage_accounting_is_consistent()
    {
        io_sim::MatchConfig config = quick_config();
        config.pps = {2.5, 2.0};
        config.max_frames = 3 * 60 * io_sim::kFramesPerSecond;
        io_sim::Match match(config, {}, 1234);
        int locks = 0;
        do
        {
            ++locks;
            if (match.finished() && match.result().reason == io_sim::EndReason::GarbageSmash)
                break;
            for (int seat = 0; seat < 2; ++seat)
            {
                Side const &me = match.side(seat);
                Side const &opponent = match.side(1 - seat);
                assert(opponent.lines_sent == me.stats.garbage_cancelled + me.stats.garbage_tanked + me.pending.total());
                assert(me.stats.lines_sent == me.lines_sent && me.stats.attack >= me.lines_sent);
                for (int y = 0; y < me.garbage_rows; ++y)
                    assert(std::popcount(static_cast<unsigned>(me.board.row(y))) == Map::width - 1);
            }
        } while (match.step());
        std::printf("accounting: %d locks, loser %d, sent %d/%d, tanked %d/%d\n", locks, match.result().loser, match.side(0).lines_sent,
                    match.side(1).lines_sent, match.side(0).stats.garbage_tanked, match.side(1).stats.garbage_tanked);
        assert(match.side(0).lines_sent + match.side(1).lines_sent > 0);
    }

    void time_limit_is_adjudicated()
    {
        assert(io_sim::adjudicated_score(0) == 0.5);
        assert(std::abs(io_sim::adjudicated_score(7) + io_sim::adjudicated_score(-7) - 1) < 1e-12);
        assert(io_sim::adjudicated_score(3) > io_sim::adjudicated_score(2));
        assert(io_sim::adjudicated_score(1e9) < 1 && io_sim::adjudicated_score(-1e9) > 0);

        io_sim::MatchConfig config = quick_config();
        config.max_frames = 20 * io_sim::kFramesPerSecond;
        io_sim::Match match(config, {}, 99);
        while (match.step())
        {
        }
        auto const &r = match.result();
        assert(r.loser == -1 && r.reason == io_sim::EndReason::TimeLimit && r.frames == config.max_frames);
        assert(r.margin == match.side(0).health(r.frames) - match.side(1).health(r.frames));
        assert(r.score == io_sim::adjudicated_score(r.margin));
    }

    void same_seed_same_match()
    {
        io_sim::MatchConfig config = quick_config();
        config.max_frames = 60 * io_sim::kFramesPerSecond;
        io_sim::MatchResult a = io_sim::play_match(config, {}, 7);
        io_sim::MatchResult b = io_sim::play_match(config, {}, 7);
        assert(a.frames == b.frames && a.loser == b.loser && a.margin == b.margin);
        for (int seat = 0; seat < 2; ++seat)
        {
            assert(a.stats[seat].pieces == b.stats[seat].pieces && a.stats[seat].lines_sent == b.stats[seat].lines_sent);
            assert(a.stats[seat].garbage_tanked == b.stats[seat].garbage_tanked);
        }
    }
}

int main()
{
    same_seed_same_bags();
    surge_reaches_opponent();
    block_out_and_clutch();
    death_reason();
    pending_entries_are_exact();
    time_limit_is_adjudicated();
    same_seed_same_match();
    garbage_accounting_is_consistent();
    outmatched_player_dies();
    std::puts("io_sim_test: ok");
    return 0;
}
