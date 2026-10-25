// io_eval: paired evaluation of ai_zzz::IO parameter sets in simulated TETR.IO Season 2 matches. Every policy plays
// the same panel on the same test seeds, so differences between policies are paired game by game.

#include "io_league.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace
{
    using io_league::Game;
    using io_league::Param;
    using io_sim::Pace;

    std::atomic<bool> g_interrupted{false};

    void on_signal(int)
    {
        g_interrupted.store(true);
    }

    struct Options
    {
        std::vector<std::string> policies;
        std::string opponents = "default,spiker,staller";
        std::string own = "1.5b,2.5b,3.5b";
        std::string ratios = "1,1.5,2";
        io_sim::Cadence opponent_cadence = io_sim::Cadence::Constant;
        bool legacy = false;
        int seeds = 16;
        int seed_offset = 0;
        int h2h = 0;
        int nodes = io_sim::SearchBudget{}.nodes;
        int think_ms = 0;
        int next_count = io_sim::MatchConfig{}.next_count;
        double max_minutes = io_sim::MatchConfig{}.max_frames / (60.0 * io_sim::kFramesPerSecond);
        int workers = std::max(1, static_cast<int>(std::thread::hardware_concurrency()) - 2);
        std::string csv;
    };

    void print_usage()
    {
        Options d;
        std::cout << "usage: io_eval [options] POLICY...\n"
                     "Every POLICY (default, spiker, staller or an IO param file) plays the same panel on the same test seeds,\n"
                     "every seed on both seats; prints per-group scores and all pairwise paired differences (cluster = group x\n"
                     "seed, 95% interval = 1.96 standard errors).\n"
                     "\n"
                     "  --opponents LIST   panel opponents (default "
                  << d.opponents << ")\n"
                  << "  --own LIST         own paces, \"b\" = the bot client's burst cadence (default " << d.own << ")\n"
                  << "  --ratios LIST      opponent pace = own pps x ratio (default " << d.ratios << ")\n"
                  << "  --opponent-cadence constant|burst  (default constant)\n"
                     "  --legacy           the old trainer's panel instead: own 4 pps against default@4, spiker@4, staller@6\n"
                  << "  --seeds N          seeds per group (default " << d.seeds << ")\n"
                  << "  --seed-offset K    first test-seed index (default 0); a new offset draws a fresh sample\n"
                     "  --h2h N            also play every pair of POLICY head to head: N seeds per own pace, ratio 1, both seats\n"
                  << "  --nodes N          positions scored per placement (default " << d.nodes << ")\n"
                  << "  --think-ms MS      wall-clock budget per placement instead of --nodes (not reproducible)\n"
                     "  --next N           preview pieces (default "
                  << d.next_count << ")\n"
                  << "  --max-minutes M    game time limit before adjudication (default " << d.max_minutes << ")\n"
                  << "  --workers N        parallel match threads (default " << d.workers << " = cores - 2)\n"
                  << "  --csv FILE         write every game\n";
    }

    bool parse_options(int argc, char const *argv[], Options &o)
    {
        for (int i = 1; i < argc; ++i)
        {
            std::string_view arg = argv[i];
            if (arg == "--help" || arg == "-h")
            {
                print_usage();
                std::exit(0);
            }
            if (arg == "--legacy")
            {
                o.legacy = true;
                continue;
            }
            if (!arg.starts_with("--"))
            {
                o.policies.emplace_back(arg);
                continue;
            }
            if (i + 1 >= argc)
            {
                std::cerr << "missing value for " << arg << "\n";
                return false;
            }
            char const *value = argv[++i];
            if (arg == "--opponents")
                o.opponents = value;
            else if (arg == "--own")
                o.own = value;
            else if (arg == "--ratios")
                o.ratios = value;
            else if (arg == "--opponent-cadence" && std::string_view(value) == "constant")
                o.opponent_cadence = io_sim::Cadence::Constant;
            else if (arg == "--opponent-cadence" && std::string_view(value) == "burst")
                o.opponent_cadence = io_sim::Cadence::Burst;
            else if (arg == "--seeds")
                o.seeds = std::atoi(value);
            else if (arg == "--seed-offset")
                o.seed_offset = std::atoi(value);
            else if (arg == "--h2h")
                o.h2h = std::atoi(value);
            else if (arg == "--nodes")
                o.nodes = std::atoi(value);
            else if (arg == "--think-ms")
                o.think_ms = std::atoi(value);
            else if (arg == "--next")
                o.next_count = std::atoi(value);
            else if (arg == "--max-minutes")
                o.max_minutes = std::atof(value);
            else if (arg == "--workers")
                o.workers = std::atoi(value);
            else if (arg == "--csv")
                o.csv = value;
            else
            {
                std::cerr << "unknown option " << arg << " " << value << "\n";
                return false;
            }
        }
        if (o.policies.empty() || o.seeds < 1 || o.seed_offset < 0 || o.h2h < 0 || o.nodes < 1 || o.think_ms < 0 || o.next_count < 1 ||
            !(o.max_minutes > 0) || o.workers < 1)
        {
            std::cerr << "invalid options (see --help)\n";
            return false;
        }
        return true;
    }

    struct Group
    {
        int opponent; // index into the opponent list
        Pace own;
        Pace opponent_pace;
        double ratio;
        std::string label;
    };

    struct Entry // one panel or head-to-head game of a subject
    {
        int subject;
        int group; // panel group, or -1 for head to head
        int rival; // head to head: the other subject
        int cluster;
        int seat;
    };

    std::string format_row(std::string_view label, std::vector<double> const &values, char const *format)
    {
        char cell[32];
        std::string row(label);
        row.resize(std::max<std::size_t>(row.size() + 1, 30), ' ');
        for (double v : values)
        {
            std::snprintf(cell, sizeof cell, format, v);
            row += cell;
        }
        return row;
    }
}

int main(int argc, char const *argv[])
{
    Options options;
    if (!parse_options(argc, argv, options))
        return 2;
    std::signal(SIGINT, on_signal);
    std::signal(SIGTERM, on_signal);

    std::vector<Param> policies;
    for (auto const &spec : options.policies)
    {
        auto p = io_league::load_policy(spec);
        if (!p)
        {
            std::cerr << "cannot read policy " << spec << "\n";
            return 2;
        }
        policies.push_back(*p);
    }
    int const subjects = static_cast<int>(policies.size());

    std::vector<std::string> opponent_names;
    std::vector<Group> groups;
    if (options.legacy)
    {
        opponent_names = {"default", "spiker", "staller"};
        groups = {{0, Pace{4}, Pace{4}, 1, "default 4 vs 4"}, {1, Pace{4}, Pace{4}, 1, "spiker 4 vs 4"}, {2, Pace{4}, Pace{6}, 1.5, "staller 4 vs 6"}};
    }
    else
    {
        opponent_names = io_league::split(options.opponents, ',');
        auto own = io_league::parse_paces(options.own);
        auto ratios = io_league::parse_numbers(options.ratios);
        if (opponent_names.empty() || !own || !ratios)
        {
            std::cerr << "invalid --opponents, --own or --ratios\n";
            return 2;
        }
        for (int o = 0; o < static_cast<int>(opponent_names.size()); ++o)
        {
            for (Pace const &p : *own)
            {
                for (double r : *ratios)
                {
                    Pace const opp{p.pps * r, options.opponent_cadence};
                    groups.push_back({o, p, opp, r, opponent_names[static_cast<std::size_t>(o)] + " " + io_league::format_pace(p) + " vs " + io_league::format_pace(opp)});
                }
            }
        }
    }
    for (auto const &name : opponent_names)
    {
        auto p = io_league::load_policy(name);
        if (!p)
        {
            std::cerr << "cannot read opponent " << name << "\n";
            return 2;
        }
        policies.push_back(*p);
    }

    io_sim::MatchConfig base;
    base.next_count = options.next_count;
    base.budget = {options.nodes, options.think_ms};
    base.max_frames = static_cast<int>(options.max_minutes * 60 * io_sim::kFramesPerSecond);
    std::vector<Pace> h2h_paces;
    for (auto const &g : groups)
    {
        if (std::none_of(h2h_paces.begin(), h2h_paces.end(), [&](Pace const &p)
                         { return p.pps == g.own.pps && p.cadence == g.own.cadence; }))
            h2h_paces.push_back(g.own);
    }
    for (auto const &g : groups)
    {
        io_sim::MatchConfig check = base;
        check.pace = {g.own, g.opponent_pace};
        if (!io_sim::fits_pending_capacity(check))
        {
            std::cerr << "group " << g.label << " can deliver more garbage packets than io_sim::kPendingCapacity; lower --max-minutes\n";
            return 2;
        }
    }

    int const seeds = options.seeds;
    int const group_count = static_cast<int>(groups.size());
    std::vector<Game> games;
    std::vector<Entry> entries;
    for (int i = 0; i < subjects; ++i)
    {
        for (int g = 0; g < group_count; ++g)
        {
            for (int s = 0; s < seeds; ++s)
            {
                int const cluster = g * seeds + s;
                std::int64_t const seed = io_league::panel_seed(io_league::SeedStream::Test, options.seed_offset + cluster);
                int const opponent = subjects + groups[static_cast<std::size_t>(g)].opponent;
                for (int seat = 0; seat < 2; ++seat)
                {
                    Pace const &own = groups[static_cast<std::size_t>(g)].own;
                    Pace const &opp = groups[static_cast<std::size_t>(g)].opponent_pace;
                    games.push_back(seat == 0 ? Game{i, opponent, own, opp, seed} : Game{opponent, i, opp, own, seed});
                    entries.push_back({i, g, -1, cluster, seat});
                }
            }
        }
    }
    constexpr int kHeadToHeadSeedBase = 1 << 20;
    for (int i = 0; i < subjects; ++i)
    {
        for (int j = i + 1; j < subjects; ++j)
        {
            for (int p = 0; p < static_cast<int>(h2h_paces.size()); ++p)
            {
                for (int t = 0; t < options.h2h; ++t)
                {
                    int const cluster = p * options.h2h + t;
                    std::int64_t const seed = io_league::panel_seed(io_league::SeedStream::Test, options.seed_offset + kHeadToHeadSeedBase + cluster);
                    Pace const &pace = h2h_paces[static_cast<std::size_t>(p)];
                    for (int seat = 0; seat < 2; ++seat)
                    {
                        games.push_back(seat == 0 ? Game{i, j, pace, pace, seed} : Game{j, i, pace, pace, seed});
                        entries.push_back({i, -1, j, cluster, seat});
                    }
                }
            }
        }
    }

    for (int i = 0; i < subjects; ++i)
        std::printf("P%d = %s\n", i + 1, options.policies[static_cast<std::size_t>(i)].c_str());
    std::printf("panel: %d groups x %d seeds x 2 seats = %d games per policy%s; nodes %d, think_ms %d, next %d, max %.1f min, %d workers; "
                "%zu games\n",
                group_count, seeds, group_count * seeds * 2, options.legacy ? " (legacy)" : "", options.nodes, options.think_ms, options.next_count,
                options.max_minutes, options.workers, games.size());
    std::fflush(stdout);

    std::atomic<std::size_t> started{0};
    std::size_t const total = games.size();
    auto stop = [&]
    {
        if (g_interrupted.load())
            return true;
        std::size_t const k = started.fetch_add(1) + 1;
        if (k <= total && k % std::max<std::size_t>(1, total / 20) == 0)
            std::fprintf(stderr, "started %zu/%zu\n", k, total);
        return false;
    };
    std::vector<io_league::Played> played;
    std::size_t const done = io_league::play(policies, games, {}, base, options.workers, stop, played);
    if (done != games.size())
    {
        std::fprintf(stderr, "interrupted after %zu/%zu games\n", done, games.size());
        return 1;
    }

    if (!options.csv.empty())
    {
        std::ofstream out(options.csv);
        io_league::write_csv_header(out);
        for (std::size_t k = 0; k < games.size(); ++k)
        {
            Entry const &e = entries[k];
            std::string const subject = "P" + std::to_string(e.subject + 1);
            std::string const opponent = e.group >= 0 ? opponent_names[static_cast<std::size_t>(groups[static_cast<std::size_t>(e.group)].opponent)]
                                                      : "P" + std::to_string(e.rival + 1);
            io_league::write_csv_row(out, e.group >= 0 ? "eval" : "h2h", 0, subject, opponent, games[k], e.seat, played[k]);
        }
    }

    // Panel scores in a common order per subject: entries of subject i are contiguous and identically laid out.
    std::size_t const per_subject = static_cast<std::size_t>(group_count) * static_cast<std::size_t>(seeds) * 2;
    std::vector<std::vector<double>> scores(static_cast<std::size_t>(subjects));
    std::vector<int> cluster(per_subject);
    std::vector<int> timeouts(static_cast<std::size_t>(subjects), 0);
    for (std::size_t k = 0; k < static_cast<std::size_t>(subjects) * per_subject; ++k)
    {
        Entry const &e = entries[k];
        scores[static_cast<std::size_t>(e.subject)].push_back(io_league::seat_score(played[k].result, e.seat));
        cluster[k % per_subject] = e.cluster;
        timeouts[static_cast<std::size_t>(e.subject)] += played[k].result.loser < 0;
    }

    std::string columns(30, ' ');
    for (int i = 0; i < subjects; ++i)
    {
        char cell[16];
        std::snprintf(cell, sizeof cell, "%8s", ("P" + std::to_string(i + 1)).c_str());
        columns += cell;
    }
    std::puts(columns.c_str());
    auto mean_where = [&](auto predicate)
    {
        std::vector<double> means;
        for (int i = 0; i < subjects; ++i)
        {
            double sum = 0;
            int n = 0;
            for (std::size_t k = 0; k < per_subject; ++k)
            {
                if (predicate(entries[k]))
                {
                    sum += scores[static_cast<std::size_t>(i)][k];
                    ++n;
                }
            }
            means.push_back(n > 0 ? sum / n : 0);
        }
        return means;
    };
    for (int g = 0; g < group_count; ++g)
        std::puts(format_row(groups[static_cast<std::size_t>(g)].label, mean_where([&](Entry const &e)
                                                                                   { return e.group == g; }),
                             "%8.3f")
                      .c_str());
    if (!options.legacy)
    {
        std::vector<double> ratio_values;
        std::vector<std::string> own_labels;
        for (auto const &g : groups)
        {
            if (std::find(ratio_values.begin(), ratio_values.end(), g.ratio) == ratio_values.end())
                ratio_values.push_back(g.ratio);
            if (std::find(own_labels.begin(), own_labels.end(), io_league::format_pace(g.own)) == own_labels.end())
                own_labels.push_back(io_league::format_pace(g.own));
        }
        for (double r : ratio_values)
            std::puts(format_row("ratio " + io_league::format_number(r), mean_where([&](Entry const &e)
                                                                                    { return groups[static_cast<std::size_t>(e.group)].ratio == r; }),
                                 "%8.3f")
                          .c_str());
        for (auto const &label : own_labels)
            std::puts(format_row("own " + label, mean_where([&](Entry const &e)
                                                            { return io_league::format_pace(groups[static_cast<std::size_t>(e.group)].own) == label; }),
                                 "%8.3f")
                          .c_str());
    }
    std::vector<double> all, all_se, timeout_share;
    for (int i = 0; i < subjects; ++i)
    {
        auto const e = io_league::clustered_mean(scores[static_cast<std::size_t>(i)], cluster);
        all.push_back(e.mean);
        all_se.push_back(e.se);
        timeout_share.push_back(100.0 * timeouts[static_cast<std::size_t>(i)] / static_cast<double>(per_subject));
    }
    std::puts(format_row("all", all, "%8.3f").c_str());
    std::puts(format_row("  standard error", all_se, "%8.3f").c_str());
    std::puts(format_row("time-limit games %", timeout_share, "%8.1f").c_str());

    if (subjects > 1)
    {
        std::puts("paired differences (cluster = group x seed), 95% interval:");
        for (int i = 0; i < subjects; ++i)
        {
            for (int j = i + 1; j < subjects; ++j)
            {
                auto const d = io_league::paired_difference(scores[static_cast<std::size_t>(j)], scores[static_cast<std::size_t>(i)], cluster);
                std::printf("  P%d - P%d  %+.4f  [%+.4f, %+.4f]  z %+.2f  (%d clusters)\n", j + 1, i + 1, d.mean, d.mean - 1.96 * d.se, d.mean + 1.96 * d.se,
                            d.se > 0 ? d.mean / d.se : 0.0, d.clusters);
            }
        }
    }
    if (options.h2h > 0 && subjects > 1)
    {
        std::printf("head to head (ratio 1, %d seeds per own pace, both seats), score of the first:\n", options.h2h);
        for (int i = 0; i < subjects; ++i)
        {
            for (int j = i + 1; j < subjects; ++j)
            {
                std::vector<double> x;
                std::vector<int> c;
                for (std::size_t k = static_cast<std::size_t>(subjects) * per_subject; k < games.size(); ++k)
                {
                    Entry const &e = entries[k];
                    if (e.subject == i && e.rival == j)
                    {
                        x.push_back(io_league::seat_score(played[k].result, e.seat));
                        c.push_back(e.cluster);
                    }
                }
                auto const e = io_league::clustered_mean(x, c);
                std::printf("  P%d vs P%d  %.4f  [%.4f, %.4f]  (%zu games)\n", i + 1, j + 1, e.mean, e.mean - 1.96 * e.se, e.mean + 1.96 * e.se, x.size());
            }
        }
    }
    return 0;
}
