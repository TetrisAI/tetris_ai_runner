#include "io_sim.h"

#include <algorithm>
#include <cassert>
#include <cmath>

namespace io_sim
{
    namespace
    {
        using Spec = rule_io::TetrisRule::rule_spec;
        using Helpers = m_tetris2::Helpers<Spec>;

        constexpr int kHeight = Map::height;

        // botconnect Minion::next_frame (can_burst): pressure shortens the interval by kBurstSensitivity * fpp per 6
        // units, never below kBurstMinSpacing * fpp; the catch-up term is capped at kBurstMaxInfluency * fpp.
        constexpr double kBurstSensitivity = 0.2;
        constexpr double kBurstMinSpacing = 0.6;
        constexpr double kBurstMaxInfluency = 0.5;
    }

    Side::Side(io_rules::Options const &rules, std::int64_t seed, int next_count)
        : rules_(&rules), bag_(seed), holes_(seed, Map::width), next_count_(next_count)
    {
        queue_.reserve(static_cast<std::size_t>(next_count_) + 2);
        refill();
    }

    void Side::refill()
    {
        while (queue_.size() < static_cast<std::size_t>(next_count_) + 2)
            queue_.push_back(bag_.next());
    }

    std::optional<m_tetris2::TetrisBlockStatus> Side::spawn(char piece) const
    {
        auto [x, y] = Spec::spawn(piece, Map::width, Map::height);
        std::array<Map, Helpers::kMaxR> usable;
        Helpers::build_usable_for_piece(piece, board, usable);
        auto status_at = [&](int row)
        { return m_tetris2::TetrisBlockStatus(piece, static_cast<std::int8_t>(x), static_cast<std::int8_t>(row), 0); };
        if (Helpers::check_T(piece, x, y, 0, usable))
            return status_at(y);
        if (!rules_->clutch || !last_was_clear)
            return std::nullopt;
        for (int row = y + 1; row < kHeight; ++row)
        {
            if (Helpers::check_T(piece, x, row, 0, usable))
                return status_at(row);
        }
        return std::nullopt;
    }

    char Side::take(bool use_hold)
    {
        char played = queue_.front();
        if (use_hold)
        {
            if (hold == ' ')
            {
                hold = queue_[0];
                played = queue_[1];
                queue_.erase(queue_.begin(), queue_.begin() + 2);
            }
            else
            {
                played = hold;
                hold = queue_[0];
                queue_.erase(queue_.begin());
            }
        }
        else
        {
            queue_.erase(queue_.begin());
        }
        refill();
        return played;
    }

    void Side::sync(int frame)
    {
        pending.advance(frame - queue_frame_);
        queue_frame_ = frame;
    }

    void Side::receive(int amount, int frame)
    {
        sync(frame);
        assert(pending.entries().size() < kPendingCapacity);
        pending.push(amount, rules_->garbagespeed);
    }

    bool Side::push_garbage_row(int hole)
    {
        // dev uses 0=occupied/1=empty：顶行非空即无法上推，垃圾行仅洞位为 1。
        if (board.row(kHeight - 1) != Map::row_full)
            return false;
        board = board.shifted<0, 1>();
        board.set_row(0, static_cast<Map::row_t>(Map::row_t(1) << hole));
        garbage_rows = std::min(garbage_rows + 1, kHeight);
        return true;
    }

    LockResult Side::lock(Engine const &engine, LandPoint const &lp, int frame)
    {
        sync(frame);
        LockResult r;
        ++pieces_placed;

        Map piece{};
        engine.attach(lp, piece);
        for (int y = 0; y < kHeight; ++y)
        {
            // dev board is 0=occupied; compare explicit occupied views with the 1=piece mask.
            if ((board.occupied_row(y) | piece.occupied_row(y)) == Map::row_full)
            {
                ++r.lines;
                r.garbage_lines += y < garbage_rows;
            }
        }
        [[maybe_unused]] std::size_t cleared = engine.attach(lp, board);
        assert(static_cast<int>(cleared) == r.lines);
        garbage_rows -= r.garbage_lines;

        io_rules::Placement placement{r.lines, static_cast<io_rules::Spin>(lp.type), r.lines > 0 && board.none(), r.garbage_lines > 0};
        double multiplier = io_rules::garbage_multiplier(*rules_, frame);
        r.attack = io_rules::resolve_attack(*rules_, counters, placement, multiplier);
        counters = r.attack.counters;

        std::array<std::int8_t, kHeight> hole_columns{};
        int cap = io_rules::garbage_cap(*rules_, frame);
        r.exchange = io_rules::exchange(*rules_, r.attack, pending, pieces_placed, lines_sent, cap, &holes_, std::span<std::int8_t>(hole_columns));
        assert(r.exchange.tanked <= kHeight);
        for (int i = 0; i < r.exchange.tanked && !r.dead; ++i)
        {
            r.dead = !push_garbage_row(hole_columns[i]);
            r.tanked += !r.dead;
        }
        last_was_clear = r.lines > 0;
        last_tanked = r.tanked;

        stats.pieces = pieces_placed;
        stats.lines_cleared += r.lines;
        stats.attack += r.attack.total();
        stats.lines_sent = lines_sent;
        stats.garbage_cancelled += r.exchange.cancelled;
        stats.garbage_tanked += r.tanked;
        stats.spins += lp.type != search_amini::Search::None && r.lines > 0;
        int surge = r.attack.surge[0] + r.attack.surge[1] + r.attack.surge[2];
        stats.surges += surge > 0;
        stats.surge_lines += surge;
        stats.all_clears += placement.all_clear;
        stats.max_btb = std::max(stats.max_btb, counters.btb);
        return r;
    }

    int Side::stack_height() const
    {
        return board.none() ? 0 : board.max_y() + 1;
    }

    double Side::health(int frame) const
    {
        double multiplier = io_rules::garbage_multiplier(*rules_, frame);
        return kVisibleRows - stack_height() - pending.total() + io_rules::surge_amount(*rules_, counters.btb, multiplier);
    }

    double adjudicated_score(double margin)
    {
        return 0.5 + 0.45 * std::tanh(margin / 10);
    }

    bool fits_pending_capacity(MatchConfig const &config)
    {
        constexpr double kPacketsPerLock = std::tuple_size_v<decltype(io_rules::ExchangeResult::parts)>;
        for (Pace const &pace : config.pace)
        {
            double const max_pps = pace.cadence == Cadence::Burst ? pace.pps / kBurstMinSpacing : pace.pps;
            if (std::floor(config.max_frames * max_pps / kFramesPerSecond) * kPacketsPerLock >= kPendingCapacity)
                return false;
        }
        return true;
    }

    Match::Match(MatchConfig const &config, std::array<ai_zzz::IO::Param, 2> const &params, std::int64_t seed)
        : config_(config), params_(params),
          sides_{Side(config_.rules, seed, config_.next_count), Side(config_.rules, seed, config_.next_count)}
    {
        assert(fits_pending_capacity(config_));
        for (int seat = 0; seat < 2; ++seat)
        {
            engines_[seat] = std::make_unique<Engine>();
            auto *search = engines_[seat]->search_config();
            search->allow_180 = config_.rules.allow180;
            search->allow_d = false; // the live executors soft drop instantly (same as io_uds)
            search->allow_LR = true;
            search->is_20g = false;
            engines_[seat]->prepare(Map::width, Map::height);
        }
        for (int seat = 0; seat < 2; ++seat)
        {
            if (config_.pace[seat].cadence == Cadence::Burst)
                schedule_burst(seat, 0);
        }
        for (int seat = 0; seat < 2 && !finished_; ++seat)
        {
            if (!plan(seat, 0))
                finish(seat, EndReason::BlockOut, 0);
        }
    }

    int Match::lock_frame(int seat, int ahead) const
    {
        Pace const &pace = config_.pace[seat];
        if (pace.cadence == Cadence::Burst)
            return burst_lock_[seat] + static_cast<int>(std::floor(ahead * kFramesPerSecond / pace.pps + 1e-9));
        return static_cast<int>(std::floor((placements_[seat] + 1 + ahead) * kFramesPerSecond / pace.pps + 1e-9));
    }

    // The client computes the next hard drop right after the previous one, from the board, the queued garbage and the
    // combo at that moment; its two catch-up branches both add fpp - average.
    void Match::schedule_burst(int seat, int last_lock)
    {
        double const fpp = kFramesPerSecond / config_.pace[seat].pps;
        Side const &me = sides_[seat];
        double const average = static_cast<double>(last_lock) / std::max(1, me.pieces_placed);
        influency_[seat] = std::min(influency_[seat] + fpp - average, fpp * kBurstMaxInfluency);
        int rows = 0;
        for (int y = 0; y < kHeight; ++y)
            rows += me.board.occupied_row(y) != 0;
        double const pressure = rows + me.pending.total() + 2.0 * me.counters.combo;
        double const frame = last_lock + fpp + influency_[seat] - pressure / 6 * (fpp * kBurstSensitivity);
        burst_lock_[seat] = static_cast<int>(std::floor(std::max(frame, last_lock + fpp * kBurstMinSpacing)));
    }

    std::uint64_t Match::lethal_sent(int seat, int root_lock_frame)
    {
        int const other = 1 - seat;
        Side const &opponent = sides_[other];
        opponent_pieces_.clear();
        if (planned_[other].state.t != 0)
            opponent_pieces_ += static_cast<char>(planned_[other].state.t);
        opponent_pieces_ += opponent.current();
        auto const preview = opponent.preview();
        opponent_pieces_.append(preview.begin(), preview.end());
        opponent_pending_.clear();
        for (auto const &entry : opponent.pending.entries())
            opponent_pending_.push_back({entry.amount, std::max(0, entry.frames_until_active)});
        int const active_frame = root_lock_frame + config_.rules.garbagespeed;
        int free_placements = 0;
        while (lock_frame(other, free_placements) < active_frame)
            ++free_placements;
        // The planned piece may already have held; allowing hold again only adds replies.
        ai_zzz::IOLethal::Opponent const view{opponent.board, opponent.garbage_rows, opponent_pieces_, opponent.hold, true,
                                              opponent.counters, opponent.pieces_placed, opponent_pending_};
        return lethal_.lethal_sent(view, config_.rules, root_lock_frame, free_placements);
    }

    bool Match::plan(int seat, int frame)
    {
        Side &me = sides_[seat];
        Side const &opponent = sides_[1 - seat];
        Engine &engine = *engines_[seat];
        auto spawn = me.spawn(me.current());
        if (!spawn)
            return false;
        me.sync(frame);

        int root_lock_frame = lock_frame(seat);
        ai_zzz::IO::Config &ai_config = *engine.ai_config();
        ai_config.rules = config_.rules;
        ai_config.pps = config_.pace[seat].pps;
        ai_config.allow_pc = true;
        ai_config.opponent = ai_zzz::IO::OpponentView{true, opponent.counters.btb, lethal_sent(seat, root_lock_frame)};
        ai_config.param = params_[seat];

        projected_pending_.clear();
        for (auto const &entry : me.pending.entries())
            projected_pending_.push_back({entry.amount, std::max(0, entry.frames_until_active - (root_lock_frame - frame))});
        ai_zzz::IO::RootState root{me.counters.btb, me.counters.combo, me.pieces_placed, me.lines_sent, me.garbage_rows, root_lock_frame,
                                   projected_pending_};
        *engine.status() = ai_zzz::IO::make_root_status(root, ai_config);
        engine.update();

        auto preview = me.preview();
        Engine::RunResult result;
        if (config_.budget.think_ms > 0)
        {
            result = engine.run_hold(me.board, *spawn, me.hold, true, preview.data(), preview.size(), config_.budget.think_ms);
        }
        else
        {
            do
            {
                result = engine.run_hold(me.board, *spawn, me.hold, true, preview.data(), preview.size(), 0);
            } while (!engine.complete() && engine.scored() < static_cast<std::size_t>(config_.budget.nodes));
        }
        if (result.target == nullptr)
            return false;
        char played = me.take(result.change_hold);
        if (result.change_hold && !me.spawn(played))
            return false;
        assert(static_cast<char>(result.target.state.t) == played);
        planned_[seat] = result.target;
        return true;
    }

    void Match::finish(int loser, EndReason reason, int frame)
    {
        finished_ = true;
        result_.loser = loser;
        result_.reason = reason;
        result_.frames = frame;
        result_.margin = sides_[0].health(frame) - sides_[1].health(frame);
        if (loser < 0)
            result_.score = adjudicated_score(result_.margin);
        else
            result_.score = loser == 0 ? 0.0 : 1.0;
        result_.stats = {sides_[0].stats, sides_[1].stats};
    }

    bool Match::step()
    {
        if (finished_)
            return false;
        int seat = lock_frame(0) <= lock_frame(1) ? 0 : 1;
        int frame = lock_frame(seat);
        if (frame > config_.max_frames)
        {
            finish(-1, EndReason::TimeLimit, config_.max_frames);
            return false;
        }
        Side &me = sides_[seat];
        LockResult lock = me.lock(*engines_[seat], planned_[seat], frame);
        ++placements_[seat];
        for (int i = 0; i < lock.exchange.part_count; ++i)
            sides_[1 - seat].receive(lock.exchange.parts[i], frame);
        if (lock.dead)
        {
            finish(seat, EndReason::GarbageSmash, frame);
            return false;
        }
        if (config_.pace[seat].cadence == Cadence::Burst)
            schedule_burst(seat, frame);
        if (!plan(seat, frame))
        {
            finish(seat, me.spawn_failure(), frame);
            return false;
        }
        return true;
    }

    MatchResult play_match(MatchConfig const &config, std::array<ai_zzz::IO::Param, 2> const &params, std::int64_t seed)
    {
        auto match = std::make_unique<Match>(config, params, seed); // the pending queues are too large for a thread stack
        while (match->step())
        {
        }
        return match->result();
    }
}
