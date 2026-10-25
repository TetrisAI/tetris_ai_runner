// io_league_test: the evaluation machinery shared by io_cmaes and io_eval — results independent of thread count and
// order, interruption, paired statistics over clusters, rank correlation, seed streams, pace text.

#include "../src/io_league.h"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <random>
#include <string>
#include <vector>

namespace
{
    // A run repeats exactly on any machine: game i's result depends neither on the thread count nor on the play order.
    void results_do_not_depend_on_threads_or_order()
    {
        io_sim::MatchConfig base;
        base.budget.nodes = 64;
        base.max_frames = 20 * io_sim::kFramesPerSecond;
        std::vector<io_league::Param> policies{*io_league::builtin_policy("default"), *io_league::builtin_policy("spiker"),
                                               *io_league::builtin_policy("staller")};
        std::vector<io_league::Game> games{
            {0, 1, io_sim::Pace{3.0}, io_sim::Pace{3.0}, 11},
            {1, 0, io_sim::Pace{2.0, io_sim::Cadence::Burst}, io_sim::Pace{4.0}, 12},
            {2, 0, io_sim::Pace{5.0}, io_sim::Pace{2.5, io_sim::Cadence::Burst}, 13},
            {0, 2, io_sim::Pace{1.5, io_sim::Cadence::Burst}, io_sim::Pace{3.0}, 14},
            {1, 2, io_sim::Pace{4.0}, io_sim::Pace{4.0}, 15},
        };
        std::vector<io_league::Played> one, many;
        std::vector<std::size_t> const reversed{4, 3, 2, 1, 0};
        assert(io_league::play(policies, games, {}, base, 1, {}, one) == games.size());
        assert(io_league::play(policies, games, reversed, base, 3, {}, many) == games.size());
        for (std::size_t i = 0; i < games.size(); ++i)
        {
            auto const &a = one[i].result;
            auto const &b = many[i].result;
            assert(one[i].done && many[i].done);
            assert(a.loser == b.loser && a.frames == b.frames && a.margin == b.margin && a.score == b.score);
            for (std::size_t seat = 0; seat < 2; ++seat)
                assert(a.stats[seat].pieces == b.stats[seat].pieces && a.stats[seat].lines_sent == b.stats[seat].lines_sent);
        }

        std::vector<io_league::Played> none;
        assert(io_league::play(policies, games, {}, base, 2, []
                               { return true; }, none) == 0);
        assert(none.size() == games.size() && !none[0].done);
    }

    // Clusters (games sharing a seed) weigh equally whatever their size; the standard error is taken across clusters.
    void statistics_average_within_clusters_first()
    {
        std::vector<double> const a{1, 1, 0, 0, 1, 1};
        std::vector<double> const b(6, 0.0);
        std::vector<int> const cluster{0, 0, 1, 1, 2, 2};
        auto const d = io_league::paired_difference(a, b, cluster);
        assert(d.clusters == 3 && std::abs(d.mean - 2.0 / 3) < 1e-12 && std::abs(d.se - 1.0 / 3) < 1e-12);

        std::vector<double> const x{1, 0, 0, 0};
        std::vector<int> const c{7, 9, 9, 9};
        auto const m = io_league::clustered_mean(x, c);
        assert(m.clusters == 2 && std::abs(m.mean - 0.5) < 1e-12);

        std::vector<double> const up{1, 2, 2, 3};
        std::vector<double> const down{3, 2, 2, 1};
        std::vector<double> const flat{5, 5, 5, 5};
        assert(std::abs(io_league::spearman(up, up) - 1) < 1e-12 && std::abs(io_league::spearman(up, down) + 1) < 1e-12);
        assert(io_league::spearman(up, flat) == 0);
    }

    void seed_streams_are_disjoint()
    {
        std::mt19937_64 rng(5);
        for (int i = 0; i < 1000; ++i)
        {
            std::int64_t const t = io_league::training_seed(rng);
            std::int64_t const v = io_league::panel_seed(io_league::SeedStream::Validation, i);
            std::int64_t const s = io_league::panel_seed(io_league::SeedStream::Test, i);
            assert(t >= 1 && t < (std::int64_t{1} << 30));
            assert(v >= (std::int64_t{1} << 30) && v < (std::int64_t{3} << 29));
            assert(s >= (std::int64_t{3} << 29) && s <= 2147483646);
        }
        assert(io_league::panel_seed(io_league::SeedStream::Test, 3) == io_league::panel_seed(io_league::SeedStream::Test, 3));
    }

    // The checkpoint stores paces and numbers as text, so resuming needs an exact round trip.
    void paces_round_trip()
    {
        auto const p = io_league::parse_pace("2.5b");
        assert(p && p->pps == 2.5 && p->cadence == io_sim::Cadence::Burst && io_league::format_pace(*p) == "2.5b");
        auto const q = io_league::parse_pace("5.25");
        assert(q && q->cadence == io_sim::Cadence::Constant && io_league::format_pace(*q) == "5.25");
        assert(!io_league::parse_pace("") && !io_league::parse_pace("b") && !io_league::parse_pace("0") && !io_league::parse_pace("2x"));
        assert(!io_league::parse_paces("1.5b,,3") && io_league::parse_paces("1.5b,3")->size() == 2);
        double const third = 1.0 / 3;
        assert(std::stod(io_league::format_number(third)) == third && io_league::format_number(0.15) == "0.15");
    }
}

int main()
{
    results_do_not_depend_on_threads_or_order();
    statistics_average_within_clusters_first();
    seed_streams_are_disjoint();
    paces_round_trip();
    std::puts("io_league_test: ok");
    return 0;
}
