// io_pso: particle swarm tuning of ai_zzz::IO parameters with simulated TETR.IO Season 2 Tetra League matches.

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
#include <limits>
#include <mutex>
#include <numeric>
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

    constexpr std::string_view kCheckpointMagic = "io_pso_checkpoint_v1";
    constexpr double kInertia = 0.7298;
    constexpr double kCognitive = 1.49618;
    constexpr double kSocial = 1.49618;
    constexpr double kStallerChainScale = 4;

    std::atomic<bool> g_interrupted{false};

    void on_signal(int)
    {
        g_interrupted.store(true);
    }

    struct Options
    {
        int workers = std::max(1, static_cast<int>(std::thread::hardware_concurrency()) - 2);
        double hours = 6;
        int population = 16;
        int pop_opponents = 4;
        int seeds = 3;
        double spread = 0.15;
        double pps = 4.0;
        int next_count = 10;
        int nodes = io_sim::SearchBudget{}.nodes;
        int think_ms = 0;
        double max_minutes = 8;
        std::uint64_t seed = 1;
        std::string out = "io_pso_out";
        std::string init;
        bool resume = false;
    };

    void print_usage()
    {
        Options d;
        std::cout << "usage: io_pso [options]\n"
                     "PSO over ai_zzz::IO::param_fields() with simulated TETR.IO S2 Tetra League matches.\n"
                     "Every generation evaluates each particle's position and its personal best against the same\n"
                     "opponents and seeds (frozen anchors + the best personal bests of the swarm); every pairing\n"
                     "plays each seed twice with seats swapped. Fitness = mean score (win 1, loss 0, time limit:\n"
                     "adjudicated, see io_sim.h). The run checkpoints after every generation.\n"
                     "Games per generation = 2 * population * (anchors + pop-opponents) * seeds * 2 (half in generation 0).\n"
                     "\n"
                     "  --workers N        parallel match threads (default "
                  << d.workers
                  << " = cores - 2)\n"
                     "  --hours H          wall-clock budget; no generation starts that would end past it (default "
                  << d.hours
                  << ")\n"
                     "  --population N     particles (default "
                  << d.population
                  << ")\n"
                     "  --pop-opponents K  best personal bests used as opponents each generation (default "
                  << d.pop_opponents
                  << ")\n"
                     "  --seeds S          seeds per pairing per generation, each played on both seats (default "
                  << d.seeds
                  << ")\n"
                     "  --spread F         initial position spread around the start params, fraction of each bound range (default "
                  << d.spread
                  << ")\n"
                     "  --pps X            pieces per second of both players (default "
                  << d.pps
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
                     "  --out DIR          output directory: checkpoint.txt, io_pso.log, best.params, best_params.inc (default "
                  << d.out
                  << ")\n"
                     "  --init FILE        IO param file used as swarm centre and as an extra frozen anchor\n"
                     "  --resume           continue from DIR/checkpoint.txt\n"
                     "  --help             this text\n"
                     "\n"
                     "Frozen anchors: in-code IO defaults, spiker (b2b = surge = 0: no B2B chain value), staller (4x the\n"
                     "default b2b and surge weights), plus --init when given. SIGINT/SIGTERM stop after the running\n"
                     "games; the last finished generation stays checkpointed.\n";
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
            else if (arg == "--population")
                o.population = std::atoi(value);
            else if (arg == "--pop-opponents")
                o.pop_opponents = std::atoi(value);
            else if (arg == "--seeds")
                o.seeds = std::atoi(value);
            else if (arg == "--spread")
                o.spread = std::atof(value);
            else if (arg == "--pps")
                o.pps = std::atof(value);
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
        if (o.workers < 1 || o.population < 2 || o.pop_opponents < 0 || o.pop_opponents >= o.population || o.seeds < 1 || o.pps <= 0 ||
            o.next_count < 1 || o.nodes < 1 || o.think_ms < 0 || o.max_minutes <= 0 || o.hours <= 0)
        {
            std::cerr << "invalid option value (see --help)\n";
            return false;
        }
        return true;
    }

    std::span<ai_zzz::IO::ParamField const> fields()
    {
        return ai_zzz::IO::param_fields();
    }

    std::vector<double> to_vector(Param const &p)
    {
        std::vector<double> v;
        v.reserve(fields().size());
        for (auto const &f : fields())
            v.push_back(p.*f.member);
        return v;
    }

    Param to_param(std::vector<double> const &v)
    {
        Param p;
        for (std::size_t i = 0; i < fields().size(); ++i)
            p.*fields()[i].member = v[i];
        return p;
    }

    struct Particle
    {
        std::vector<double> x, v, p;
        double p_fitness = -1;
    };

    struct Swarm
    {
        int generation = 0;
        std::mt19937_64 rng;
        std::vector<Particle> particles;
        std::vector<double> g;
        double g_fitness = -1;
        std::uint64_t games = 0;
        double game_seconds = 0;
        double wall_seconds = 0;
    };

    void init_swarm(Swarm &s, Options const &o, Param const &centre)
    {
        s.rng.seed(o.seed);
        auto c = to_vector(centre);
        std::normal_distribution<double> gauss(0, 1);
        s.particles.resize(static_cast<std::size_t>(o.population));
        for (std::size_t i = 0; i < s.particles.size(); ++i)
        {
            auto &pt = s.particles[i];
            pt.x = c;
            pt.v.assign(c.size(), 0);
            for (std::size_t d = 0; d < c.size(); ++d)
            {
                auto const &f = fields()[d];
                double range = f.upper - f.lower;
                if (i > 0)
                    pt.x[d] = std::clamp(c[d] + gauss(s.rng) * o.spread * range, f.lower, f.upper);
                pt.v[d] = std::uniform_real_distribution<double>(-f.max_velocity, f.max_velocity)(s.rng);
            }
            pt.p = pt.x;
        }
        s.g = c;
    }

    void write_vector(std::ostream &out, std::string_view tag, std::vector<double> const &v)
    {
        out << tag;
        for (double d : v)
            out << ' ' << d;
        out << '\n';
    }

    bool read_vector(std::istream &in, std::string_view tag, std::vector<double> &v)
    {
        std::string t;
        if (!(in >> t) || t != tag)
            return false;
        for (double &d : v)
        {
            if (!(in >> d))
                return false;
        }
        return true;
    }

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

    bool save_checkpoint(std::filesystem::path const &path, Swarm const &s)
    {
        std::ostringstream out;
        out << std::setprecision(17);
        out << kCheckpointMagic << '\n';
        out << "fields " << fields().size();
        for (auto const &f : fields())
            out << ' ' << f.name;
        out << '\n';
        out << "generation " << s.generation << '\n';
        out << "games " << s.games << '\n';
        out << "game_seconds " << s.game_seconds << '\n';
        out << "wall_seconds " << s.wall_seconds << '\n';
        out << "rng " << s.rng << '\n';
        out << "particles " << s.particles.size() << '\n';
        for (auto const &pt : s.particles)
        {
            write_vector(out, "x", pt.x);
            write_vector(out, "v", pt.v);
            write_vector(out, "p", pt.p);
            out << "p_fitness " << pt.p_fitness << '\n';
        }
        write_vector(out, "g", s.g);
        out << "g_fitness " << s.g_fitness << '\n';
        return write_atomically(path, out.str());
    }

    bool load_checkpoint(std::filesystem::path const &path, Swarm &s)
    {
        std::ifstream in(path);
        std::string word;
        std::size_t count = 0;
        if (!(in >> word) || word != kCheckpointMagic || !(in >> word >> count) || word != "fields" || count != fields().size())
            return false;
        for (auto const &f : fields())
        {
            if (!(in >> word) || word != f.name)
                return false;
        }
        auto expect = [&](std::string_view tag, auto &value)
        { return (in >> word) && word == tag && (in >> value); };
        std::size_t particles = 0;
        if (!expect("generation", s.generation) || !expect("games", s.games) || !expect("game_seconds", s.game_seconds) ||
            !expect("wall_seconds", s.wall_seconds) || !expect("rng", s.rng) || !expect("particles", particles))
            return false;
        s.particles.assign(particles, Particle{});
        for (auto &pt : s.particles)
        {
            pt.x.resize(count);
            pt.v.resize(count);
            pt.p.resize(count);
            if (!read_vector(in, "x", pt.x) || !read_vector(in, "v", pt.v) || !read_vector(in, "p", pt.p) || !expect("p_fitness", pt.p_fitness))
                return false;
        }
        s.g.resize(count);
        return read_vector(in, "g", s.g) && expect("g_fitness", s.g_fitness);
    }

    std::string cpp_initializer(std::vector<double> const &v, double fitness, int generation)
    {
        std::ostringstream out;
        out << std::setprecision(10);
        out << "// io_pso best params: generation " << generation << ", fitness " << fitness << "\n";
        out << "ai_zzz::IO::Param param;\n";
        for (std::size_t i = 0; i < v.size(); ++i)
            out << "param." << fields()[i].name << " = " << v[i] << ";\n";
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
    };

    std::vector<Anchor> make_anchors(Param const *init)
    {
        std::vector<Anchor> anchors;
        anchors.push_back({"default", Param{}});
        Param spiker;
        spiker.b2b = 0;
        spiker.surge = 0;
        anchors.push_back({"spiker", spiker});
        Param staller;
        staller.b2b *= kStallerChainScale;
        staller.surge *= kStallerChainScale;
        anchors.push_back({"staller", staller});
        if (init != nullptr)
            anchors.push_back({"init", *init});
        return anchors;
    }

    struct Job
    {
        int entity;
        int opponent;
        std::int64_t seed;
        int seat;
    };

    struct Tally
    {
        double score = 0;
        int games = 0;
    };

    struct GenerationOutcome
    {
        std::vector<Tally> tallies;
        std::uint64_t games = 0;
        double game_seconds = 0;
        double pieces = 0;
        int time_limit_games = 0;
        bool complete = false;
    };

    // Plays every job; entity/opponent indices refer to `params`. Stops early (complete = false) on interrupt or deadline.
    GenerationOutcome play_generation(std::vector<Param> const &params, std::size_t entities, std::vector<Job> const &jobs,
                                      io_sim::MatchConfig const &config, int workers, Clock::time_point deadline)
    {
        GenerationOutcome outcome;
        outcome.tallies.resize(entities);
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
                Param const &self = params[static_cast<std::size_t>(job.entity)];
                Param const &other = params[static_cast<std::size_t>(job.opponent)];
                std::array<Param, 2> seats = job.seat == 0 ? std::array<Param, 2>{self, other} : std::array<Param, 2>{other, self};
                io_sim::MatchResult r = io_sim::play_match(config, seats, job.seed);
                double score = job.seat == 0 ? r.score : 1 - r.score;
                std::lock_guard lock(mutex);
                auto &tally = outcome.tallies[static_cast<std::size_t>(job.entity)];
                tally.score += score;
                ++tally.games;
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

    void move_particle(Particle &pt, std::vector<double> const &g, std::mt19937_64 &rng)
    {
        std::uniform_real_distribution<double> unit(0, 1);
        for (std::size_t d = 0; d < pt.x.size(); ++d)
        {
            auto const &f = fields()[d];
            double v = kInertia * pt.v[d] + kCognitive * unit(rng) * (pt.p[d] - pt.x[d]) + kSocial * unit(rng) * (g[d] - pt.x[d]);
            v = std::clamp(v, -f.max_velocity, f.max_velocity);
            double x = pt.x[d] + v;
            if (x < f.lower || x > f.upper)
            {
                x = std::clamp(x, f.lower, f.upper);
                v = 0;
            }
            pt.x[d] = x;
            pt.v[d] = v;
        }
    }

    std::string format(char const *fmt, auto... args)
    {
        char buffer[512];
        std::snprintf(buffer, sizeof(buffer), fmt, args...);
        return buffer;
    }
}

int main(int argc, char const *argv[])
{
    Options options;
    if (!parse_options(argc, argv, options))
        return 2;
    std::filesystem::path out_dir = options.out;
    std::filesystem::create_directories(out_dir);
    Log log(out_dir / "io_pso.log");
    std::signal(SIGINT, on_signal);
    std::signal(SIGTERM, on_signal);

    Param init_param;
    bool has_init = !options.init.empty();
    if (has_init && !ai_zzz::IO::load_params(options.init, init_param))
    {
        std::cerr << "cannot read param file " << options.init << "\n";
        return 2;
    }
    std::vector<Anchor> anchors = make_anchors(has_init ? &init_param : nullptr);

    Swarm swarm;
    auto checkpoint_path = out_dir / "checkpoint.txt";
    if (options.resume)
    {
        if (!load_checkpoint(checkpoint_path, swarm))
        {
            std::cerr << "cannot resume from " << checkpoint_path << " (missing, corrupt, or different param fields)\n";
            return 2;
        }
    }
    else
    {
        init_swarm(swarm, options, has_init ? init_param : Param{});
    }
    std::size_t population = swarm.particles.size();
    int pop_opponents = std::min<int>(options.pop_opponents, static_cast<int>(population) - 1);

    io_sim::MatchConfig config;
    config.pps = {options.pps, options.pps};
    config.next_count = options.next_count;
    config.budget = {options.nodes, options.think_ms};
    config.max_frames = static_cast<int>(options.max_minutes * 60 * io_sim::kFramesPerSecond);
    if (!io_sim::fits_pending_capacity(config))
    {
        std::cerr << "--pps and --max-minutes allow more garbage packets than io_sim::kPendingCapacity\n";
        return 2;
    }

    log.line(format("io_pso %s: population %zu, %zu dims, anchors %zu, pop opponents %d, seeds %d, workers %d, pps %.2f, next %d, "
                    "budget %s %d, max %.1f min, budget %.2f h, out %s",
                    options.resume ? "resume" : "start", population, fields().size(), anchors.size(), pop_opponents, options.seeds,
                    options.workers, options.pps, options.next_count, options.think_ms > 0 ? "ms" : "nodes",
                    options.think_ms > 0 ? options.think_ms : options.nodes, options.max_minutes, options.hours, options.out.c_str()));

    auto start = Clock::now();
    auto deadline = start + std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(options.hours * 3600));
    double last_generation_seconds = 0;
    while (!g_interrupted.load())
    {
        auto gen_start = Clock::now();
        if (gen_start + std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(last_generation_seconds)) > deadline)
            break;

        // params: [0, P) positions, [P, 2P) personal bests, then anchors.
        std::vector<Param> params;
        params.reserve(2 * population + anchors.size());
        for (auto const &pt : swarm.particles)
            params.push_back(to_param(pt.x));
        for (auto const &pt : swarm.particles)
            params.push_back(to_param(pt.p));
        for (auto const &a : anchors)
            params.push_back(a.param);
        int anchor_base = static_cast<int>(2 * population);

        std::vector<int> ranked(population);
        std::iota(ranked.begin(), ranked.end(), 0);
        std::stable_sort(ranked.begin(), ranked.end(), [&](int a, int b)
                         { return swarm.particles[a].p_fitness > swarm.particles[b].p_fitness; });

        std::vector<std::int64_t> seeds(static_cast<std::size_t>(options.seeds));
        for (auto &seed : seeds)
            seed = static_cast<std::int64_t>(std::uniform_int_distribution<std::uint32_t>(1, 2147483646u)(swarm.rng));

        bool fresh_bests = swarm.generation == 0;
        std::vector<Job> jobs;
        for (int i = 0; i < static_cast<int>(population); ++i)
        {
            std::vector<int> opponents;
            for (int a = 0; a < static_cast<int>(anchors.size()); ++a)
                opponents.push_back(anchor_base + a);
            for (int r = 0, taken = 0; taken < pop_opponents; ++r)
            {
                if (ranked[r] == i)
                    continue;
                opponents.push_back(static_cast<int>(population) + ranked[r]);
                ++taken;
            }
            for (int entity : {i, static_cast<int>(population) + i})
            {
                if (fresh_bests && entity >= static_cast<int>(population))
                    continue;
                for (int opponent : opponents)
                    for (auto seed : seeds)
                        for (int seat = 0; seat < 2; ++seat)
                            jobs.push_back({entity, opponent, seed, seat});
            }
        }
        std::shuffle(jobs.begin(), jobs.end(), swarm.rng);

        GenerationOutcome outcome = play_generation(params, 2 * population, jobs, config, options.workers, deadline);
        double gen_seconds = std::chrono::duration<double>(Clock::now() - gen_start).count();
        if (!outcome.complete)
        {
            log.line(format("generation %d aborted after %llu/%zu games (%s); checkpoint keeps generation %d", swarm.generation,
                            static_cast<unsigned long long>(outcome.games), jobs.size(), g_interrupted.load() ? "interrupted" : "time budget",
                            swarm.generation - 1));
            break;
        }

        auto fitness = [&](int entity)
        {
            auto const &t = outcome.tallies[static_cast<std::size_t>(entity)];
            return t.games > 0 ? t.score / t.games : 0.0;
        };
        double mean_x = 0;
        int best_particle = 0;
        for (int i = 0; i < static_cast<int>(population); ++i)
        {
            auto &pt = swarm.particles[static_cast<std::size_t>(i)];
            double fx = fitness(i);
            double fp = fresh_bests ? fx : fitness(static_cast<int>(population) + i);
            mean_x += fx;
            pt.p_fitness = fp;
            if (fx > fp)
            {
                pt.p = pt.x;
                pt.p_fitness = fx;
            }
            if (pt.p_fitness > swarm.particles[static_cast<std::size_t>(best_particle)].p_fitness)
                best_particle = i;
        }
        mean_x /= static_cast<double>(population);
        swarm.g = swarm.particles[static_cast<std::size_t>(best_particle)].p;
        swarm.g_fitness = swarm.particles[static_cast<std::size_t>(best_particle)].p_fitness;
        for (auto &pt : swarm.particles)
            move_particle(pt, swarm.g, swarm.rng);

        swarm.games += outcome.games;
        swarm.game_seconds += outcome.game_seconds;
        swarm.wall_seconds += gen_seconds;
        ++swarm.generation;
        last_generation_seconds = gen_seconds;

        bool saved = save_checkpoint(checkpoint_path, swarm) && ai_zzz::IO::save_params((out_dir / "best.params").string(), to_param(swarm.g)) &&
                     write_atomically(out_dir / "best_params.inc", cpp_initializer(swarm.g, swarm.g_fitness, swarm.generation - 1));
        double games = static_cast<double>(outcome.games);
        log.line(format("gen %d | best %.4f (particle %d) | mean %.4f | games %.0f in %.1f min | %.0f games/h | avg game %.1f s, %.1f "
                        "pieces/player | time-limit %.1f%% | total %.2f h, %llu games%s",
                        swarm.generation - 1, swarm.g_fitness, best_particle, mean_x, games, gen_seconds / 60, games * 3600 / gen_seconds,
                        outcome.game_seconds / games, outcome.pieces / games, 100.0 * outcome.time_limit_games / games, swarm.wall_seconds / 3600,
                        static_cast<unsigned long long>(swarm.games), saved ? "" : " | CHECKPOINT WRITE FAILED"));
    }
    log.line(format("io_pso stopped at generation %d; best fitness %.4f; outputs in %s", swarm.generation, swarm.g_fitness, options.out.c_str()));
    return 0;
}
