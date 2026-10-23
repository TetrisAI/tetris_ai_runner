// io_cmaes: sep-CMA-ES tuning of ai_zzz::IO parameters with simulated TETR.IO Season 2 Tetra League matches.

#include "io_cmaes.h"
#include "io_sim.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <numeric>
#include <optional>
#include <random>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace
{
    using Param = ai_zzz::IO::Param;
    using Clock = std::chrono::steady_clock;

    constexpr std::string_view kCheckpointMagic = "io_cmaes_checkpoint_v1";
    constexpr double kStallerChainScale = 4;
    constexpr double kStallerPps = 6;

    std::atomic<bool> g_interrupted{false};

    void on_signal(int)
    {
        g_interrupted.store(true);
    }

    std::span<ai_zzz::IO::ParamField const> fields()
    {
        return ai_zzz::IO::param_fields();
    }

    struct Options
    {
        int workers = std::max(1, static_cast<int>(std::thread::hardware_concurrency()) - 2);
        double hours = 6;
        int lambda = io_cmaes::SepCmaes::default_lambda(fields().size());
        int seeds = 3;
        double sigma = 0.15;
        double pps = io_sim::MatchConfig{}.pps[0];
        std::optional<double> default_pps; // unset: --pps
        std::optional<double> spiker_pps;  // unset: --pps
        double staller_pps = kStallerPps;
        int next_count = io_sim::MatchConfig{}.next_count;
        int nodes = io_sim::SearchBudget{}.nodes;
        int think_ms = 0;
        double max_minutes = io_sim::MatchConfig{}.max_frames / (60.0 * io_sim::kFramesPerSecond);
        std::uint64_t seed = 1;
        std::string out = "io_cmaes_out";
        std::string init;
        bool resume = false;
    };

    void print_usage()
    {
        Options d;
        std::cout << "usage: io_cmaes [options]\n"
                     "sep-CMA-ES over ai_zzz::IO::param_fields() with simulated TETR.IO S2 Tetra League matches.\n"
                     "Each parameter is searched in units of its bound range and mirrored back into its bounds. Every\n"
                     "generation samples lambda candidates; each plays the same opponents (the current distribution mean\n"
                     "and the frozen anchors) on the same seeds, every seed twice with seats swapped. Fitness = mean score\n"
                     "(win 1, loss 0, time limit: adjudicated, see io_sim.h); only the ranking updates the distribution.\n"
                     "best.params is the distribution mean; the run checkpoints after every generation.\n"
                     "Games per generation = lambda * (1 + anchors) * seeds * 2.\n"
                     "\n"
                     "  --workers N        parallel match threads (default "
                  << d.workers
                  << " = cores - 2)\n"
                     "  --hours H          wall-clock budget; no generation starts that would end past it (default "
                  << d.hours
                  << ")\n"
                     "  --lambda N         candidates per generation; the best N/2 are recombined (default "
                  << d.lambda
                  << " = 4 + floor(3 ln dims))\n"
                     "  --seeds S          seeds per opponent per generation, each played on both seats (default "
                  << d.seeds
                  << ")\n"
                     "  --sigma F          initial step size, fraction of each bound range (default "
                  << d.sigma
                  << ")\n"
                     "  --pps X            pieces per second of the candidates, the mean and --init (default "
                  << d.pps
                  << ")\n"
                     "  --default-pps X    pieces per second of the default anchor (default: --pps)\n"
                     "  --spiker-pps X     pieces per second of the spiker anchor (default: --pps)\n"
                     "  --staller-pps X    pieces per second of the staller anchor (default "
                  << d.staller_pps
                  << ")\n"
                     "  --next N           preview pieces visible to the AI (default "
                  << d.next_count
                  << ")\n"
                     "  --nodes N          deterministic think budget: positions scored per placement (default "
                  << d.nodes
                  << ")\n"
                     "  --think-ms MS      wall-clock think budget per placement instead of --nodes\n"
                     "  --max-minutes M    game time limit before adjudication (default "
                  << d.max_minutes
                  << ")\n"
                     "  --seed N           master random seed (default "
                  << d.seed
                  << ")\n"
                     "  --out DIR          output directory: checkpoint.txt, io_cmaes.log, best.params, best_params.inc (default "
                  << d.out
                  << ")\n"
                     "  --init FILE        IO param file used as the initial mean and as an extra frozen anchor\n"
                     "  --resume           continue from DIR/checkpoint.txt (its distribution replaces --lambda, --sigma and\n"
                     "                     the --init mean)\n"
                     "  --help             this text\n"
                     "\n"
                     "Frozen anchors: in-code IO defaults, spiker (b2b = surge = 0: no B2B chain value), staller (4x the\n"
                     "default b2b and surge weights), each at its own --*-pps, plus --init when given. SIGINT/SIGTERM stop\n"
                     "after the running games; the last finished generation stays checkpointed.\n";
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
            if (arg == "--resume")
            {
                o.resume = true;
                continue;
            }
            if (i + 1 >= argc)
            {
                std::cerr << "missing value for " << arg << "\n";
                return false;
            }
            char const *value = argv[++i];
            if (arg == "--workers")
                o.workers = std::atoi(value);
            else if (arg == "--hours")
                o.hours = std::atof(value);
            else if (arg == "--lambda")
                o.lambda = std::atoi(value);
            else if (arg == "--seeds")
                o.seeds = std::atoi(value);
            else if (arg == "--sigma")
                o.sigma = std::atof(value);
            else if (arg == "--pps")
                o.pps = std::atof(value);
            else if (arg == "--default-pps")
                o.default_pps = std::atof(value);
            else if (arg == "--spiker-pps")
                o.spiker_pps = std::atof(value);
            else if (arg == "--staller-pps")
                o.staller_pps = std::atof(value);
            else if (arg == "--next")
                o.next_count = std::atoi(value);
            else if (arg == "--nodes")
                o.nodes = std::atoi(value);
            else if (arg == "--think-ms")
                o.think_ms = std::atoi(value);
            else if (arg == "--max-minutes")
                o.max_minutes = std::atof(value);
            else if (arg == "--seed")
                o.seed = std::strtoull(value, nullptr, 10);
            else if (arg == "--out")
                o.out = value;
            else if (arg == "--init")
                o.init = value;
            else
            {
                std::cerr << "unknown option " << arg << "\n";
                return false;
            }
        }
        if (o.workers < 1 || o.lambda < 2 || o.seeds < 1 || !(o.sigma > 0) || o.pps <= 0 || o.default_pps.value_or(o.pps) <= 0 ||
            o.spiker_pps.value_or(o.pps) <= 0 || o.staller_pps <= 0 || o.next_count < 1 || o.nodes < 1 || o.think_ms < 0 || o.max_minutes <= 0 ||
            o.hours <= 0)
        {
            std::cerr << "invalid option value (see --help)\n";
            return false;
        }
        return true;
    }

    std::vector<double> to_genotype(Param const &p)
    {
        std::vector<double> y;
        y.reserve(fields().size());
        for (auto const &f : fields())
            y.push_back(std::clamp((p.*f.member - f.lower) / (f.upper - f.lower), 0.0, 1.0));
        return y;
    }

    Param to_param(std::vector<double> const &y)
    {
        Param p;
        for (std::size_t i = 0; i < fields().size(); ++i)
        {
            auto const &f = fields()[i];
            p.*f.member = f.lower + (f.upper - f.lower) * io_cmaes::mirror(y[i]);
        }
        return p;
    }

    struct Run
    {
        std::mt19937_64 rng;
        std::uint64_t games = 0;
        double game_seconds = 0;
        double wall_seconds = 0;
    };

    bool write_atomically(std::filesystem::path const &path, std::string const &content)
    {
        auto tmp = path;
        tmp += ".tmp";
        {
            std::ofstream out(tmp, std::ios::trunc);
            out << content;
            if (!out.flush())
                return false;
        }
        std::error_code ec;
        std::filesystem::rename(tmp, path, ec);
        return !ec;
    }

    bool save_checkpoint(std::filesystem::path const &path, Run const &run, io_cmaes::SepCmaes const &es)
    {
        std::ostringstream out;
        out << std::setprecision(17);
        out << kCheckpointMagic << '\n';
        out << "fields " << fields().size();
        for (auto const &f : fields())
            out << ' ' << f.name;
        out << '\n';
        out << "games " << run.games << '\n';
        out << "game_seconds " << run.game_seconds << '\n';
        out << "wall_seconds " << run.wall_seconds << '\n';
        out << "rng " << run.rng << '\n';
        es.save(out);
        return write_atomically(path, out.str());
    }

    std::optional<io_cmaes::SepCmaes> load_checkpoint(std::filesystem::path const &path, Run &run)
    {
        std::ifstream in(path);
        std::string word;
        std::size_t count = 0;
        if (!(in >> word) || word != kCheckpointMagic || !(in >> word >> count) || word != "fields" || count != fields().size())
            return std::nullopt;
        for (auto const &f : fields())
        {
            if (!(in >> word) || word != f.name)
                return std::nullopt;
        }
        auto expect = [&](std::string_view tag, auto &value)
        { return (in >> word) && word == tag && (in >> value); };
        if (!expect("games", run.games) || !expect("game_seconds", run.game_seconds) || !expect("wall_seconds", run.wall_seconds) ||
            !expect("rng", run.rng))
            return std::nullopt;
        auto es = io_cmaes::SepCmaes::load(in);
        if (!es || es->dimension() != fields().size())
            return std::nullopt;
        return es;
    }

    std::string cpp_initializer(std::vector<double> const &mean, int generation)
    {
        Param const p = to_param(mean);
        std::ostringstream out;
        out << std::setprecision(10);
        out << "// io_cmaes distribution mean after generation " << generation << "\n";
        out << "ai_zzz::IO::Param param;\n";
        for (auto const &f : fields())
            out << "param." << f.name << " = " << p.*f.member << ";\n";
        return out.str();
    }

    class Log
    {
    public:
        explicit Log(std::filesystem::path const &path) : file_(path, std::ios::app) {}
        void line(std::string const &text)
        {
            std::lock_guard lock(mutex_);
            std::cout << text << std::endl;
            file_ << text << std::endl;
        }

    private:
        std::mutex mutex_;
        std::ofstream file_;
    };

    struct Anchor
    {
        std::string name;
        Param param;
        double pps;
    };

    std::vector<Anchor> make_anchors(Param const *init, Options const &options)
    {
        std::vector<Anchor> anchors;
        anchors.push_back({"default", Param{}, options.default_pps.value_or(options.pps)});
        Param spiker;
        spiker.b2b = 0;
        spiker.surge = 0;
        anchors.push_back({"spiker", spiker, options.spiker_pps.value_or(options.pps)});
        Param staller;
        staller.b2b *= kStallerChainScale;
        staller.surge *= kStallerChainScale;
        anchors.push_back({"staller", staller, options.staller_pps});
        if (init != nullptr)
            anchors.push_back({"init", *init, options.pps});
        return anchors;
    }

    struct Job
    {
        int candidate;
        int opponent;
        std::int64_t seed;
        int seat;
    };

    struct Tally
    {
        double score = 0;
        int games = 0;

        double mean() const
        {
            return score / games;
        }
    };

    struct GenerationOutcome
    {
        std::vector<Tally> candidates;
        std::vector<Tally> opponents; // the candidates' scores against each opponent
        std::uint64_t games = 0;
        double game_seconds = 0;
        double pieces = 0;
        int time_limit_games = 0;
        bool complete = false;
    };

    // Plays every job; candidates play at config.pps, opponents at their own pps. Stops early (complete = false) on
    // interrupt or deadline.
    GenerationOutcome play_generation(std::vector<Param> const &candidates, std::vector<Anchor> const &opponents, std::vector<Job> const &jobs,
                                      io_sim::MatchConfig const &config, int workers, Clock::time_point deadline)
    {
        GenerationOutcome outcome;
        outcome.candidates.resize(candidates.size());
        outcome.opponents.resize(opponents.size());
        std::atomic<std::size_t> next{0};
        std::mutex mutex;
        auto work = [&]
        {
            for (;;)
            {
                if (g_interrupted.load() || Clock::now() > deadline)
                    return;
                std::size_t index = next.fetch_add(1);
                if (index >= jobs.size())
                    return;
                Job const &job = jobs[index];
                Param const &self = candidates[static_cast<std::size_t>(job.candidate)];
                Anchor const &other = opponents[static_cast<std::size_t>(job.opponent)];
                std::array<Param, 2> seats = job.seat == 0 ? std::array<Param, 2>{self, other.param} : std::array<Param, 2>{other.param, self};
                io_sim::MatchConfig match = config;
                match.pps[1 - job.seat] = other.pps;
                io_sim::MatchResult r = io_sim::play_match(match, seats, job.seed);
                double score = job.seat == 0 ? r.score : 1 - r.score;
                std::lock_guard lock(mutex);
                for (Tally *tally : {&outcome.candidates[static_cast<std::size_t>(job.candidate)], &outcome.opponents[static_cast<std::size_t>(job.opponent)]})
                {
                    tally->score += score;
                    ++tally->games;
                }
                ++outcome.games;
                outcome.game_seconds += static_cast<double>(r.frames) / io_sim::kFramesPerSecond;
                outcome.pieces += (r.stats[0].pieces + r.stats[1].pieces) / 2.0;
                outcome.time_limit_games += r.loser < 0;
            }
        };
        std::vector<std::thread> threads;
        for (int i = 0; i < workers; ++i)
            threads.emplace_back(work);
        for (auto &t : threads)
            t.join();
        outcome.complete = outcome.games == jobs.size();
        return outcome;
    }

    std::string format(char const *fmt, auto... args)
    {
        int const size = std::snprintf(nullptr, 0, fmt, args...);
        std::string text(static_cast<std::size_t>(size), '\0');
        std::snprintf(text.data(), text.size() + 1, fmt, args...);
        return text;
    }
}

int main(int argc, char const *argv[])
{
    Options options;
    if (!parse_options(argc, argv, options))
        return 2;
    std::filesystem::path out_dir = options.out;
    std::filesystem::create_directories(out_dir);
    Log log(out_dir / "io_cmaes.log");
    std::signal(SIGINT, on_signal);
    std::signal(SIGTERM, on_signal);

    Param init_param;
    bool has_init = !options.init.empty();
    if (has_init && !ai_zzz::IO::load_params(options.init, init_param))
    {
        std::cerr << "cannot read param file " << options.init << "\n";
        return 2;
    }
    std::vector<Anchor> anchors = make_anchors(has_init ? &init_param : nullptr, options);

    Run run;
    run.rng.seed(options.seed);
    auto checkpoint_path = out_dir / "checkpoint.txt";
    std::optional<io_cmaes::SepCmaes> es;
    if (options.resume)
        es = load_checkpoint(checkpoint_path, run);
    else
        es.emplace(to_genotype(has_init ? init_param : Param{}), options.sigma, options.lambda);
    if (!es)
    {
        std::cerr << "cannot resume from " << checkpoint_path << " (missing, corrupt, or different param fields)\n";
        return 2;
    }

    io_sim::MatchConfig config;
    config.pps = {options.pps, options.pps};
    config.next_count = options.next_count;
    config.budget = {options.nodes, options.think_ms};
    config.max_frames = static_cast<int>(options.max_minutes * 60 * io_sim::kFramesPerSecond);
    std::string anchor_list;
    for (auto const &anchor : anchors)
    {
        io_sim::MatchConfig pairing = config;
        pairing.pps[1] = anchor.pps;
        if (!io_sim::fits_pending_capacity(pairing))
        {
            std::cerr << "the --*-pps options and --max-minutes allow more garbage packets than io_sim::kPendingCapacity\n";
            return 2;
        }
        anchor_list += format(" %s@%.2f", anchor.name.c_str(), anchor.pps);
    }

    log.line(format("io_cmaes %s: generation %d, lambda %d, mu %d, %zu dims, sigma %.4f, pps %.2f, anchors%s, seeds %d, workers %d, next %d, "
                    "budget %s %d, max %.1f min, budget %.2f h, out %s",
                    options.resume ? "resume" : "start", es->generation(), es->lambda(), es->mu(), fields().size(), es->sigma(), options.pps,
                    anchor_list.c_str(), options.seeds, options.workers, options.next_count, options.think_ms > 0 ? "ms" : "nodes",
                    options.think_ms > 0 ? options.think_ms : options.nodes, options.max_minutes, options.hours, options.out.c_str()));

    auto start = Clock::now();
    auto deadline = start + std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(options.hours * 3600));
    double last_generation_seconds = 0;
    while (!g_interrupted.load())
    {
        auto gen_start = Clock::now();
        if (gen_start + std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(last_generation_seconds)) > deadline)
            break;

        std::vector<std::vector<double>> candidates = es->ask(run.rng);
        std::vector<Param> candidate_params;
        candidate_params.reserve(candidates.size());
        for (auto const &x : candidates)
            candidate_params.push_back(to_param(x));
        std::vector<Anchor> opponents{{"mean", to_param(es->mean()), options.pps}};
        opponents.insert(opponents.end(), anchors.begin(), anchors.end());

        std::vector<std::int64_t> seeds(static_cast<std::size_t>(options.seeds));
        for (auto &seed : seeds)
            seed = static_cast<std::int64_t>(std::uniform_int_distribution<std::uint32_t>(1, 2147483646u)(run.rng));

        std::vector<Job> jobs;
        for (int c = 0; c < static_cast<int>(candidates.size()); ++c)
            for (int o = 0; o < static_cast<int>(opponents.size()); ++o)
                for (auto seed : seeds)
                    for (int seat = 0; seat < 2; ++seat)
                        jobs.push_back({c, o, seed, seat});
        std::shuffle(jobs.begin(), jobs.end(), run.rng);

        GenerationOutcome outcome = play_generation(candidate_params, opponents, jobs, config, options.workers, deadline);
        double gen_seconds = std::chrono::duration<double>(Clock::now() - gen_start).count();
        if (!outcome.complete)
        {
            log.line(format("generation %d aborted after %llu/%zu games (%s)%s", es->generation(), static_cast<unsigned long long>(outcome.games),
                            jobs.size(), g_interrupted.load() ? "interrupted" : "time budget",
                            es->generation() > 0 ? format("; checkpoint keeps generation %d", es->generation() - 1).c_str() : ""));
            break;
        }

        std::vector<double> fitness;
        fitness.reserve(candidates.size());
        for (auto const &t : outcome.candidates)
            fitness.push_back(t.mean());
        std::vector<std::size_t> order(candidates.size());
        std::iota(order.begin(), order.end(), std::size_t{0});
        std::stable_sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b)
                         { return fitness[a] > fitness[b]; });
        double const sigma = es->sigma();
        auto const [min_c, max_c] = std::ranges::minmax(es->variances());
        es->tell(candidates, order);

        run.games += outcome.games;
        run.game_seconds += outcome.game_seconds;
        run.wall_seconds += gen_seconds;
        last_generation_seconds = gen_seconds;
        int const generation = es->generation() - 1;

        bool saved = save_checkpoint(checkpoint_path, run, *es) && ai_zzz::IO::save_params((out_dir / "best.params").string(), to_param(es->mean())) &&
                     write_atomically(out_dir / "best_params.inc", cpp_initializer(es->mean(), generation));
        std::string versus;
        for (std::size_t o = 0; o < opponents.size(); ++o)
            versus += format(" %s %.3f", opponents[o].name.c_str(), outcome.opponents[o].mean());
        double games = static_cast<double>(outcome.games);
        log.line(format("gen %d | sigma %.4f, axis %.4f-%.4f | best %.4f | avg %.4f | vs%s | games %.0f in %.1f min | %.0f games/h | avg game "
                        "%.1f s, %.1f pieces/player | time-limit %.1f%% | total %.2f h, %llu games%s",
                        generation, sigma, sigma * std::sqrt(min_c), sigma * std::sqrt(max_c), fitness[order.front()],
                        std::accumulate(fitness.begin(), fitness.end(), 0.0) / static_cast<double>(fitness.size()), versus.c_str(), games, gen_seconds / 60,
                        games * 3600 / gen_seconds, outcome.game_seconds / games, outcome.pieces / games, 100.0 * outcome.time_limit_games / games,
                        run.wall_seconds / 3600, static_cast<unsigned long long>(run.games), saved ? "" : " | CHECKPOINT WRITE FAILED"));
    }
    log.line(format("io_cmaes stopped at generation %d; outputs in %s", es->generation(), options.out.c_str()));
    return 0;
}
