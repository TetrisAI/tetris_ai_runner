// io_cmaes: sep-CMA-ES tuning of ai_zzz::IO parameters in simulated TETR.IO Season 2 Tetra League matches, played under
// the deployed bot client's conditions (own pace from its range, burst cadence, faster opponents) against a league of
// the current mean, frozen anchors and the hall of fame. The output is a champion that beat its predecessor on fresh
// validation games.

#include "io_cmaes.h"
#include "io_league.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <climits>
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
    using io_league::Game;
    using io_sim::Pace;

    constexpr std::string_view kCheckpointMagic = "io_cmaes_checkpoint_v2";
    // The default lambda gives each worker about this many games per generation, which keeps the generation barrier
    // short on any core count.
    constexpr int kGamesPerWorker = 30;
    // Groups per generation double when the moving average of the split-half rank reliability stays below the floor
    // for a whole window.
    constexpr double kReliabilityFloor = 0.3;
    constexpr int kReliabilityWindow = 10;
    constexpr double kReliabilityDecay = 0.2;

    std::atomic<bool> g_interrupted{false};

    void on_signal(int)
    {
        g_interrupted.store(true);
    }

    std::span<ai_zzz::IO::ParamField const> fields()
    {
        return ai_zzz::IO::param_fields();
    }

    // Everything that defines the objective. Stored in the checkpoint; --resume restores it.
    struct Scenario
    {
        std::vector<Pace> own{{1.5, io_sim::Cadence::Burst}, {2.5, io_sim::Cadence::Burst}, {3.5, io_sim::Cadence::Burst}};
        std::vector<double> ratios{1, 1.25, 1.5};
        io_sim::Cadence opponent_cadence = io_sim::Cadence::Burst;
        std::vector<std::string> anchors{"default", "spiker", "staller"};
        std::vector<std::string> freeze{"ratio"};
        int lambda = 0; // 0: derived from --workers when the run starts
        int groups = 12;
        int max_groups = 96;
        double sigma = 0.15;
        int next_count = io_sim::MatchConfig{}.next_count;
        int nodes = io_sim::SearchBudget{}.nodes;
        int think_ms = 0;
        double max_minutes = 8;
        int validate_every = 8;
        int validation_seeds = 8;
        double promote_z = 1.0;
        std::uint64_t seed = 1;
    };

    std::string join(std::vector<std::string> const &items)
    {
        std::string text;
        for (auto const &item : items)
            text += (text.empty() ? "" : ",") + item;
        return text;
    }

    std::string format_scenario(Scenario const &s)
    {
        std::vector<std::string> own, ratios;
        for (auto const &p : s.own)
            own.push_back(io_league::format_pace(p));
        for (double r : s.ratios)
            ratios.push_back(io_league::format_number(r));
        std::ostringstream out;
        out << "own=" << join(own) << " ratios=" << join(ratios)
            << " opponent_cadence=" << (s.opponent_cadence == io_sim::Cadence::Burst ? "burst" : "constant") << " anchors=" << join(s.anchors)
            << " freeze=" << join(s.freeze) << " lambda=" << s.lambda << " groups=" << s.groups << " max_groups=" << s.max_groups
            << " sigma=" << io_league::format_number(s.sigma) << " next=" << s.next_count << " nodes=" << s.nodes << " think_ms=" << s.think_ms
            << " max_minutes=" << io_league::format_number(s.max_minutes) << " validate_every=" << s.validate_every
            << " validation_seeds=" << s.validation_seeds << " promote_z=" << io_league::format_number(s.promote_z) << " seed=" << s.seed;
        return out.str();
    }

    bool set_scenario(Scenario &s, std::string_view key, std::string const &value)
    {
        auto integer = [&](int &out, long min)
        {
            char *end = nullptr;
            long const v = std::strtol(value.c_str(), &end, 10);
            if (value.empty() || *end != '\0' || v < min || v > INT_MAX)
                return false;
            out = static_cast<int>(v);
            return true;
        };
        auto real = [&](double &out, double min)
        {
            char *end = nullptr;
            double const v = std::strtod(value.c_str(), &end);
            if (value.empty() || *end != '\0' || !std::isfinite(v) || !(v >= min))
                return false;
            out = v;
            return true;
        };
        if (key == "own")
        {
            auto v = io_league::parse_paces(value);
            return v && (s.own = *v, true);
        }
        if (key == "ratios")
        {
            auto v = io_league::parse_numbers(value);
            return v && (s.ratios = *v, true);
        }
        if (key == "opponent_cadence")
        {
            if (value != "constant" && value != "burst")
                return false;
            s.opponent_cadence = value == "burst" ? io_sim::Cadence::Burst : io_sim::Cadence::Constant;
            return true;
        }
        if (key == "anchors")
        {
            s.anchors = io_league::split(value, ',');
            return !s.anchors.empty();
        }
        if (key == "freeze")
        {
            s.freeze = io_league::split(value, ',');
            return true;
        }
        if (key == "lambda")
            return integer(s.lambda, 0) && s.lambda != 1;
        if (key == "groups")
            return integer(s.groups, 2);
        if (key == "max_groups")
            return integer(s.max_groups, 2);
        if (key == "sigma")
            return real(s.sigma, std::numeric_limits<double>::min());
        if (key == "next")
            return integer(s.next_count, 1);
        if (key == "nodes")
            return integer(s.nodes, 1);
        if (key == "think_ms")
            return integer(s.think_ms, 0);
        if (key == "max_minutes")
            return real(s.max_minutes, std::numeric_limits<double>::min());
        if (key == "validate_every")
            return integer(s.validate_every, 1);
        if (key == "validation_seeds")
            return integer(s.validation_seeds, 1);
        if (key == "promote_z")
            return real(s.promote_z, 0);
        if (key == "seed")
        {
            char *end = nullptr;
            s.seed = std::strtoull(value.c_str(), &end, 10);
            return !value.empty() && *end == '\0';
        }
        return false;
    }

    std::optional<Scenario> parse_scenario(std::string_view text)
    {
        Scenario s;
        for (auto const &token : io_league::split(text, ' '))
        {
            if (token.empty())
                continue;
            auto const eq = token.find('=');
            if (eq == std::string::npos || !set_scenario(s, std::string_view(token).substr(0, eq), token.substr(eq + 1)))
                return std::nullopt;
        }
        return s;
    }

    struct Options
    {
        int workers = std::max(1, static_cast<int>(std::thread::hardware_concurrency()) - 2);
        double hours = 6;
        int generations = 0;
        std::string out = "io_cmaes_out";
        std::string init;
        std::string hof;
        bool resume = false;
    };

    void print_usage()
    {
        Options d;
        Scenario s;
        std::cout
            << "usage: io_cmaes [options]\n"
               "sep-CMA-ES over ai_zzz::IO::param_fields() with simulated TETR.IO S2 Tetra League matches under the bot client's\n"
               "conditions. Each generation deals --groups league groups (opponent x own pace x ratio; opponents: the current\n"
               "mean, the anchors and the hall of fame) and every candidate plays each dealt group on its own seed, both seats.\n"
               "Fitness = mean score (win 1, loss 0, time limit: adjudicated, see io_sim.h); only the ranking updates the\n"
               "distribution. Every --validate-every generations the mean and the champion play the same fresh panel (anchors x\n"
               "own paces x ratios x --validation-seeds, both seats) and the mean becomes the champion when its paired advantage\n"
               "reaches --promote-z standard errors. Outputs in --out: best.params and best_params.inc (the champion), mean.params\n"
               "(the distribution mean), hof/ (every champion), games.csv (every game), io_cmaes.log, checkpoint.txt.\n"
               "\n"
               "Run:\n"
               "  --workers N          parallel match threads (default "
            << d.workers
            << " = cores - 2); results do not depend on it\n"
               "  --hours H            wall-clock budget of this invocation, 0 = until SIGINT/SIGTERM (default "
            << d.hours
            << ")\n"
               "  --generations N      stop once the run has N generations (default 0 = no limit)\n"
               "  --out DIR            output directory (default "
            << d.out
            << ")\n"
               "  --resume             continue DIR/checkpoint.txt; its scenario is kept, so only --workers, --hours,\n"
               "                       --generations and --out may be given\n"
               "Scenario (new runs only):\n"
               "  --init FILE          IO param file: the initial mean, frozen-field values, first champion and hall-of-fame entry\n"
               "                       (default: the in-code defaults)\n"
               "  --hof DIR            preload every *.params file in DIR into the hall of fame\n"
               "  --own LIST           own paces, \"b\" = the bot client's burst cadence (default "
            << join([&]
                    {
                        std::vector<std::string> v;
                        for (auto const &p : s.own)
                            v.push_back(io_league::format_pace(p));
                        return v; }())
            << ")\n"
               "  --ratios LIST        opponent pace = own pps x ratio (default 1,1.25,1.5)\n"
               "  --opponent-cadence constant|burst  (default burst: ratio-1 groups test equal-cadence strength)\n"
               "  --anchors LIST       frozen opponents: default, spiker (b2b = surge = 0), staller (4x b2b and surge) or param\n"
               "                       files without spaces or commas (default "
            << join(s.anchors)
            << ")\n"
               "  --freeze LIST        fields kept at their initial value (default "
            << join(s.freeze)
            << "; ratio shapes the beam and\n"
               "                       its best value depends on the search budget)\n"
               "  --lambda N           candidates per generation; the best N/2 are recombined (default: max(4 + floor(3 ln dims),\n"
            << "                       " << kGamesPerWorker
            << " x workers / games per candidate))\n"
               "  --groups N           league groups per generation; games per candidate = 2N (default "
            << s.groups
            << ")\n"
               "  --max-groups N       groups double up to N while the split-half reliability of the ranking stays below "
            << kReliabilityFloor << " (default " << s.max_groups
            << ")\n"
               "  --sigma F            initial step size, fraction of each bound range (default "
            << s.sigma
            << ")\n"
               "  --next N             preview pieces (default "
            << s.next_count
            << ")\n"
               "  --nodes N            positions scored per placement (default "
            << s.nodes
            << ")\n"
               "  --think-ms MS        wall-clock budget per placement instead of --nodes (not reproducible)\n"
               "  --max-minutes M      game time limit before adjudication (default "
            << s.max_minutes
            << ")\n"
               "  --validate-every N   generations between validations (default "
            << s.validate_every
            << ")\n"
               "  --validation-seeds N seeds per validation group (default "
            << s.validation_seeds
            << ")\n"
               "  --promote-z Z        promotion threshold in standard errors (default "
            << s.promote_z
            << ")\n"
               "  --seed N             master random seed (default "
            << s.seed << ")\n";
    }

    bool parse_options(int argc, char const *argv[], Options &o, Scenario &s, std::vector<std::string> &new_run_flags)
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
            if (!arg.starts_with("--") || i + 1 >= argc)
            {
                std::cerr << (arg.starts_with("--") ? "missing value for " : "unknown argument ") << arg << "\n";
                return false;
            }
            std::string const value = argv[++i];
            if (arg == "--workers")
                o.workers = std::atoi(value.c_str());
            else if (arg == "--hours")
                o.hours = std::atof(value.c_str());
            else if (arg == "--generations")
                o.generations = std::atoi(value.c_str());
            else if (arg == "--out")
                o.out = value;
            else
            {
                if (arg == "--init")
                    o.init = value;
                else if (arg == "--hof")
                    o.hof = value;
                else
                {
                    std::string key(arg.substr(2));
                    std::replace(key.begin(), key.end(), '-', '_');
                    if (!set_scenario(s, key, value))
                    {
                        std::cerr << "invalid option " << arg << " " << value << " (see --help)\n";
                        return false;
                    }
                }
                new_run_flags.emplace_back(arg);
            }
        }
        if (o.workers < 1 || !(o.hours >= 0) || o.generations < 0 || s.groups > s.max_groups)
        {
            std::cerr << "invalid option value (see --help)\n";
            return false;
        }
        return true;
    }

    struct HofEntry
    {
        int generation; // -1: the initial parameters or a preloaded file
        Param param;
    };

    // The whole run state; checkpointed after every generation.
    struct State
    {
        Scenario scenario;
        std::vector<std::size_t> active; // searched fields: all minus the frozen ones
        Param initial;
        std::vector<Param> anchors;
        std::vector<HofEntry> hof;
        std::mt19937_64 rng;
        int groups = 0;
        double reliability = 0; // moving average of the split-half reliability
        int reliability_count = 0;
        int grown_at = 0;
        std::vector<int> deck;
        std::size_t dealt = 0;
        Param champion;
        int champion_generation = -1;
        int validations = 0; // completed validations; each one plays a fresh panel
        std::uint64_t games = 0;
        double game_seconds = 0;
        double wall_seconds = 0;
    };

    std::optional<std::vector<std::size_t>> active_fields(std::vector<std::string> const &freeze)
    {
        for (auto const &name : freeze)
        {
            if (std::none_of(fields().begin(), fields().end(), [&](auto const &f)
                             { return f.name == name; }))
                return std::nullopt;
        }
        std::vector<std::size_t> active;
        for (std::size_t i = 0; i < fields().size(); ++i)
        {
            if (std::find(freeze.begin(), freeze.end(), fields()[i].name) == freeze.end())
                active.push_back(i);
        }
        return active;
    }

    std::vector<double> to_genotype(State const &st, Param const &p)
    {
        std::vector<double> y;
        for (std::size_t i : st.active)
        {
            auto const &f = fields()[i];
            y.push_back(std::clamp((p.*f.member - f.lower) / (f.upper - f.lower), 0.0, 1.0));
        }
        return y;
    }

    Param to_param(State const &st, std::vector<double> const &y)
    {
        Param p = st.initial;
        for (std::size_t k = 0; k < st.active.size(); ++k)
        {
            auto const &f = fields()[st.active[k]];
            p.*f.member = f.lower + (f.upper - f.lower) * io_cmaes::mirror(y[k]);
        }
        return p;
    }

    // League groups: slot (0 = the current mean, 1..A = anchors, A + 1 = hall of fame) x own pace x ratio.
    struct GroupKey
    {
        int slot;
        int own;
        int ratio;
    };

    int group_count(State const &st)
    {
        return static_cast<int>((st.anchors.size() + 2) * st.scenario.own.size() * st.scenario.ratios.size());
    }

    GroupKey decode(State const &st, int g)
    {
        int const ratios = static_cast<int>(st.scenario.ratios.size());
        int const own = static_cast<int>(st.scenario.own.size());
        return {g / ratios / own, g / ratios % own, g % ratios};
    }

    // Groups come from a shuffled deck, so every group is played equally often over time.
    int deal(State &st)
    {
        if (st.dealt >= st.deck.size())
        {
            st.deck.resize(static_cast<std::size_t>(group_count(st)));
            std::iota(st.deck.begin(), st.deck.end(), 0);
            std::shuffle(st.deck.begin(), st.deck.end(), st.rng);
            st.dealt = 0;
        }
        return st.deck[st.dealt++];
    }

    Pace opponent_pace(Scenario const &s, int own, int ratio)
    {
        return {s.own[static_cast<std::size_t>(own)].pps * s.ratios[static_cast<std::size_t>(ratio)], s.opponent_cadence};
    }

    // Validation panel of round `round`: anchors x own paces x ratios x seeds, both seats. Policy 0 is the subject and
    // 1.. the anchors; each (anchor, pace, seed) is one cluster. Every round draws fresh seeds.
    struct Panel
    {
        std::vector<Game> games;
        std::vector<int> cluster;
        std::vector<int> seat;
        std::vector<int> ratio;
    };

    Panel make_panel(State const &st, int round)
    {
        int const clusters = static_cast<int>(st.anchors.size() * st.scenario.own.size() * st.scenario.ratios.size()) * st.scenario.validation_seeds;
        Panel panel;
        int cluster = 0;
        for (int a = 0; a < static_cast<int>(st.anchors.size()); ++a)
        {
            for (int o = 0; o < static_cast<int>(st.scenario.own.size()); ++o)
            {
                for (int r = 0; r < static_cast<int>(st.scenario.ratios.size()); ++r)
                {
                    Pace const own = st.scenario.own[static_cast<std::size_t>(o)];
                    Pace const opp = opponent_pace(st.scenario, o, r);
                    for (int v = 0; v < st.scenario.validation_seeds; ++v, ++cluster)
                    {
                        std::int64_t const seed = io_league::panel_seed(io_league::SeedStream::Validation, round * clusters + cluster);
                        for (int seat = 0; seat < 2; ++seat)
                        {
                            panel.games.push_back(seat == 0 ? Game{0, 1 + a, own, opp, seed} : Game{1 + a, 0, opp, own, seed});
                            panel.cluster.push_back(cluster);
                            panel.seat.push_back(seat);
                            panel.ratio.push_back(r);
                        }
                    }
                }
            }
        }
        return panel;
    }

    // The mean and the champion play the same fresh panel, so their difference is paired and unbiased: a champion's
    // score from the panel that selected it would be inflated by that selection. played[s * n + i] is panel game i of
    // subject s (0 = mean, 1 = champion).
    std::optional<std::array<std::vector<double>, 2>> validate(Param const &mean, Param const &champion, State const &st, Panel const &panel,
                                                               io_sim::MatchConfig const &base, int workers, std::function<bool()> const &stop,
                                                               std::vector<io_league::Played> &played)
    {
        std::vector<Param> policies{mean, champion};
        policies.insert(policies.end(), st.anchors.begin(), st.anchors.end());
        std::vector<Game> games;
        for (int subject = 0; subject < 2; ++subject)
        {
            for (Game g : panel.games)
            {
                g.first = g.first == 0 ? subject : g.first + 1;
                g.second = g.second == 0 ? subject : g.second + 1;
                games.push_back(g);
            }
        }
        if (io_league::play(policies, games, {}, base, workers, stop, played) != games.size())
            return std::nullopt;
        std::array<std::vector<double>, 2> scores;
        for (std::size_t s = 0; s < 2; ++s)
        {
            for (std::size_t i = 0; i < panel.games.size(); ++i)
                scores[s].push_back(io_league::seat_score(played[s * panel.games.size() + i].result, panel.seat[i]));
        }
        return scores;
    }

    std::string ratio_breakdown(State const &st, Panel const &panel, std::vector<double> const &scores)
    {
        std::string text;
        for (int r = 0; r < static_cast<int>(st.scenario.ratios.size()); ++r)
        {
            double sum = 0;
            int n = 0;
            for (std::size_t i = 0; i < scores.size(); ++i)
            {
                if (panel.ratio[i] == r)
                {
                    sum += scores[i];
                    ++n;
                }
            }
            char cell[48];
            std::snprintf(cell, sizeof cell, "%sx%s %.3f", text.empty() ? "" : " ", io_league::format_number(st.scenario.ratios[static_cast<std::size_t>(r)]).c_str(),
                          n > 0 ? sum / n : 0.0);
            text += cell;
        }
        return text;
    }

    void write_param(std::ostream &out, Param const &p)
    {
        for (auto const &f : fields())
            out << ' ' << p.*f.member;
    }

    bool read_param(std::istream &in, Param &p)
    {
        for (auto const &f : fields())
        {
            if (!(in >> p.*f.member))
                return false;
        }
        return true;
    }

    std::string checkpoint_text(State const &st, io_cmaes::SepCmaes const &es)
    {
        std::ostringstream out;
        out << std::setprecision(17);
        out << kCheckpointMagic << '\n';
        out << "scenario " << format_scenario(st.scenario) << '\n';
        out << "fields " << fields().size();
        for (auto const &f : fields())
            out << ' ' << f.name;
        out << "\ninitial";
        write_param(out, st.initial);
        out << "\nanchors " << st.anchors.size() << '\n';
        for (auto const &a : st.anchors)
        {
            out << "anchor";
            write_param(out, a);
            out << '\n';
        }
        out << "hof " << st.hof.size() << '\n';
        for (auto const &h : st.hof)
        {
            out << "entry " << h.generation;
            write_param(out, h.param);
            out << '\n';
        }
        out << "champion " << st.champion_generation;
        write_param(out, st.champion);
        out << "\nvalidations " << st.validations;
        out << "\ngroups " << st.groups << '\n';
        out << "reliability " << st.reliability << ' ' << st.reliability_count << ' ' << st.grown_at << '\n';
        out << "deck " << st.deck.size() << ' ' << st.dealt;
        for (int g : st.deck)
            out << ' ' << g;
        out << "\ngames " << st.games << '\n';
        out << "game_seconds " << st.game_seconds << '\n';
        out << "wall_seconds " << st.wall_seconds << '\n';
        out << "rng " << st.rng << '\n';
        es.save(out);
        return out.str();
    }

    std::optional<io_cmaes::SepCmaes> read_checkpoint(std::filesystem::path const &path, State &st)
    {
        std::ifstream in(path);
        std::string word;
        auto expect = [&](std::string_view tag)
        { return (in >> word) && word == tag; };
        if (!expect(kCheckpointMagic) || !expect("scenario"))
            return std::nullopt;
        std::string line;
        std::getline(in, line);
        auto scenario = parse_scenario(line);
        if (!scenario)
            return std::nullopt;
        st.scenario = *scenario;
        auto active = active_fields(st.scenario.freeze);
        std::size_t count = 0;
        if (!active || !expect("fields") || !(in >> count) || count != fields().size())
            return std::nullopt;
        st.active = *active;
        for (auto const &f : fields())
        {
            if (!(in >> word) || word != f.name)
                return std::nullopt;
        }
        if (!expect("initial") || !read_param(in, st.initial) || !expect("anchors") || !(in >> count) || count != st.scenario.anchors.size())
            return std::nullopt;
        st.anchors.assign(count, Param{});
        for (auto &a : st.anchors)
        {
            if (!expect("anchor") || !read_param(in, a))
                return std::nullopt;
        }
        if (!expect("hof") || !(in >> count) || count < 1)
            return std::nullopt;
        st.hof.assign(count, HofEntry{});
        for (auto &h : st.hof)
        {
            if (!expect("entry") || !(in >> h.generation) || !read_param(in, h.param))
                return std::nullopt;
        }
        if (!expect("champion") || !(in >> st.champion_generation) || !read_param(in, st.champion) || !expect("validations") || !(in >> st.validations))
            return std::nullopt;
        if (!expect("groups") || !(in >> st.groups) || !expect("reliability") || !(in >> st.reliability >> st.reliability_count >> st.grown_at) ||
            !expect("deck") || !(in >> count >> st.dealt))
            return std::nullopt;
        st.deck.assign(count, 0);
        for (int &g : st.deck)
        {
            if (!(in >> g))
                return std::nullopt;
        }
        if (!expect("games") || !(in >> st.games) || !expect("game_seconds") || !(in >> st.game_seconds) || !expect("wall_seconds") ||
            !(in >> st.wall_seconds) || !expect("rng") || !(in >> st.rng))
            return std::nullopt;
        auto es = io_cmaes::SepCmaes::load(in);
        if (!es || es->dimension() != st.active.size() || es->lambda() != st.scenario.lambda)
            return std::nullopt;
        return es;
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

    std::string cpp_initializer(Param const &p, std::string const &origin)
    {
        std::ostringstream out;
        out << std::setprecision(17);
        out << "// io_cmaes champion: " << origin << "\n";
        out << "ai_zzz::IO::Param param;\n";
        for (auto const &f : fields())
            out << "param." << f.name << " = " << p.*f.member << ";\n";
        return out.str();
    }

    std::string champion_origin(int generation)
    {
        return generation < 0 ? std::string("initial parameters") : "distribution mean after generation " + std::to_string(generation);
    }

    bool write_champion(std::filesystem::path const &out_dir, State const &st)
    {
        return write_atomically(out_dir / "best.params", ai_zzz::IO::format_params(st.champion)) &&
               write_atomically(out_dir / "best_params.inc", cpp_initializer(st.champion, champion_origin(st.champion_generation)));
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

    std::string format(char const *fmt, auto... args)
    {
        int const size = std::snprintf(nullptr, 0, fmt, args...);
        std::string text(static_cast<std::size_t>(size), '\0');
        std::snprintf(text.data(), text.size() + 1, fmt, args...);
        return text;
    }

    double seconds_since(Clock::time_point start)
    {
        return std::chrono::duration<double>(Clock::now() - start).count();
    }
}

int main(int argc, char const *argv[])
{
    Options options;
    Scenario scenario;
    std::vector<std::string> new_run_flags;
    if (!parse_options(argc, argv, options, scenario, new_run_flags))
        return 2;
    if (options.resume && !new_run_flags.empty())
    {
        std::cerr << "--resume keeps the checkpoint's scenario; drop " << new_run_flags.front() << "\n";
        return 2;
    }
    std::filesystem::path const out_dir = options.out;
    std::filesystem::create_directories(out_dir / "hof");
    auto const checkpoint_path = out_dir / "checkpoint.txt";

    State st;
    std::optional<io_cmaes::SepCmaes> es;
    if (options.resume)
    {
        es = read_checkpoint(checkpoint_path, st);
        if (!es)
        {
            std::cerr << "cannot resume from " << checkpoint_path << " (missing, corrupt, or different param fields)\n";
            return 2;
        }
    }
    else
    {
        st.scenario = scenario;
        auto active = active_fields(scenario.freeze);
        if (!active || active->empty())
        {
            std::cerr << "--freeze names an unknown field or every field\n";
            return 2;
        }
        st.active = *active;
        if (!options.init.empty() && !ai_zzz::IO::load_params(options.init, st.initial))
        {
            std::cerr << "cannot read param file " << options.init << "\n";
            return 2;
        }
        for (auto const &name : scenario.anchors)
        {
            auto p = io_league::load_policy(name);
            if (!p)
            {
                std::cerr << "cannot read anchor " << name << "\n";
                return 2;
            }
            st.anchors.push_back(*p);
        }
        st.hof.push_back({-1, st.initial});
        if (!options.hof.empty())
        {
            std::vector<std::filesystem::path> files;
            std::error_code ec;
            for (auto const &entry : std::filesystem::directory_iterator(options.hof, ec))
            {
                if (entry.path().extension() == ".params")
                    files.push_back(entry.path());
            }
            std::sort(files.begin(), files.end());
            for (auto const &file : files)
            {
                Param p;
                if (!ai_zzz::IO::load_params(file.string(), p))
                {
                    std::cerr << "cannot read param file " << file << "\n";
                    return 2;
                }
                st.hof.push_back({-1, p});
            }
            if (ec || files.empty())
            {
                std::cerr << "no .params files in " << options.hof << "\n";
                return 2;
            }
        }
        st.rng.seed(scenario.seed);
        st.groups = scenario.groups;
        if (st.scenario.lambda == 0)
            st.scenario.lambda = std::max(io_cmaes::SepCmaes::default_lambda(st.active.size()), kGamesPerWorker * options.workers / (2 * st.groups));
        es.emplace(to_genotype(st, st.initial), st.scenario.sigma, st.scenario.lambda);
        st.champion = st.initial;
    }

    io_sim::MatchConfig base;
    base.next_count = st.scenario.next_count;
    base.budget = {st.scenario.nodes, st.scenario.think_ms};
    base.max_frames = static_cast<int>(st.scenario.max_minutes * 60 * io_sim::kFramesPerSecond);
    for (int o = 0; o < static_cast<int>(st.scenario.own.size()); ++o)
    {
        for (int r = 0; r < static_cast<int>(st.scenario.ratios.size()); ++r)
        {
            io_sim::MatchConfig check = base;
            check.pace = {st.scenario.own[static_cast<std::size_t>(o)], opponent_pace(st.scenario, o, r)};
            if (!io_sim::fits_pending_capacity(check))
            {
                std::cerr << "--own, --ratios and --max-minutes allow more garbage packets than io_sim::kPendingCapacity\n";
                return 2;
            }
        }
    }
    std::size_t const panel_games = make_panel(st, 0).games.size();

    Log log(out_dir / "io_cmaes.log");
    std::signal(SIGINT, on_signal);
    std::signal(SIGTERM, on_signal);
    auto const csv_path = out_dir / "games.csv";
    std::error_code size_error;
    bool const csv_fresh = std::filesystem::file_size(csv_path, size_error) == 0 || size_error;
    std::ofstream csv(csv_path, std::ios::app);
    if (csv_fresh)
        io_league::write_csv_header(csv);

    std::string frozen;
    for (auto const &name : st.scenario.freeze)
        frozen += " " + name;
    log.line(format("io_cmaes %s: generation %d, lambda %d, mu %d, %zu dims (frozen:%s), sigma %.4f, groups %d (max %d), %d games per generation, "
                    "validation 2 x %zu games every %d generations, %zu hall-of-fame entries, workers %d, hours %g | %s | out %s",
                    options.resume ? "resume" : "start", es->generation(), es->lambda(), es->mu(), st.active.size(), frozen.empty() ? " none" : frozen.c_str(),
                    es->sigma(), st.groups, st.scenario.max_groups, es->lambda() * st.groups * 2, panel_games, st.scenario.validate_every,
                    st.hof.size(), options.workers, options.hours, format_scenario(st.scenario).c_str(), options.out.c_str()));

    auto const start = Clock::now();
    bool const has_deadline = options.hours > 0;
    auto const deadline = start + std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(options.hours * 3600));
    std::function<bool()> const stop = [&]
    { return g_interrupted.load() || (has_deadline && Clock::now() > deadline); };

    auto write_validation_rows = [&](Panel const &panel, int generation, std::vector<io_league::Played> const &played)
    {
        for (std::size_t s = 0; s < 2; ++s)
        {
            for (std::size_t i = 0; i < panel.games.size(); ++i)
            {
                int const anchor = panel.games[i].first == 0 ? panel.games[i].second : panel.games[i].first;
                io_league::write_csv_row(csv, "valid", generation, s == 0 ? "mean" : "champion", st.scenario.anchors[static_cast<std::size_t>(anchor - 1)],
                                         panel.games[i], panel.seat[i], played[s * panel.games.size() + i]);
            }
        }
        csv.flush();
    };

    std::vector<io_league::Played> played;
    double last_validation_seconds = 0;
    if (!options.resume && (!write_champion(out_dir, st) || !write_atomically(checkpoint_path, checkpoint_text(st, *es))))
        log.line("CHECKPOINT WRITE FAILED");

    std::vector<double> group_seconds(static_cast<std::size_t>(group_count(st)), -1);
    double last_generation_seconds = 0;
    while (!g_interrupted.load() && (options.generations == 0 || es->generation() < options.generations))
    {
        int const generation = es->generation();
        bool const validate_now = (generation + 1) % st.scenario.validate_every == 0;
        auto const gen_start = Clock::now();
        if (has_deadline && gen_start + std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(
                                            last_generation_seconds + (validate_now ? last_validation_seconds : 0))) >
                                deadline)
            break;

        // Candidates, the mean, the anchors and this generation's hall-of-fame picks.
        std::vector<std::vector<double>> genotypes = es->ask(st.rng);
        int const lambda = es->lambda();
        std::vector<Param> policies;
        for (auto const &y : genotypes)
            policies.push_back(to_param(st, y));
        int const mean_index = static_cast<int>(policies.size());
        policies.push_back(to_param(st, es->mean()));
        int const anchor_base = static_cast<int>(policies.size());
        policies.insert(policies.end(), st.anchors.begin(), st.anchors.end());

        struct Dealt
        {
            int group;
            int opponent;
            std::string name;
            int kind; // 0 = self, 1..A = anchor, A + 1 = hall of fame
            int ratio;
            Pace own;
            Pace opp;
            std::int64_t seed;
        };
        std::vector<Dealt> dealt;
        int const anchors = static_cast<int>(st.anchors.size());
        for (int k = 0; k < st.groups; ++k)
        {
            int const g = deal(st);
            GroupKey const key = decode(st, g);
            Dealt d{g, 0, "", key.slot, key.ratio, st.scenario.own[static_cast<std::size_t>(key.own)], opponent_pace(st.scenario, key.own, key.ratio), 0};
            if (key.slot == 0)
            {
                d.opponent = mean_index;
                d.name = "self";
            }
            else if (key.slot <= anchors)
            {
                d.opponent = anchor_base + key.slot - 1;
                d.name = st.scenario.anchors[static_cast<std::size_t>(key.slot - 1)];
            }
            else
            {
                std::size_t const h = std::uniform_int_distribution<std::size_t>(0, st.hof.size() - 1)(st.rng);
                d.opponent = static_cast<int>(policies.size());
                d.name = "hof" + std::to_string(h);
                policies.push_back(st.hof[h].param);
            }
            d.seed = io_league::training_seed(st.rng);
            dealt.push_back(d);
        }

        struct Meta
        {
            int candidate;
            int dealt;
            int seat;
        };
        std::vector<Game> games;
        std::vector<Meta> meta;
        for (int c = 0; c < lambda; ++c)
        {
            for (int k = 0; k < st.groups; ++k)
            {
                Dealt const &d = dealt[static_cast<std::size_t>(k)];
                for (int seat = 0; seat < 2; ++seat)
                {
                    games.push_back(seat == 0 ? Game{c, d.opponent, d.own, d.opp, d.seed} : Game{d.opponent, c, d.opp, d.own, d.seed});
                    meta.push_back({c, k, seat});
                }
            }
        }
        // Longest expected games first, from the wall time of earlier generations' groups; unknown groups lead.
        std::vector<std::size_t> order(games.size());
        std::iota(order.begin(), order.end(), std::size_t{0});
        auto expected = [&](std::size_t i)
        {
            double const s = group_seconds[static_cast<std::size_t>(dealt[static_cast<std::size_t>(meta[i].dealt)].group)];
            return s < 0 ? std::numeric_limits<double>::infinity() : s;
        };
        std::stable_sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b)
                         { return expected(a) > expected(b); });

        std::size_t const done = io_league::play(policies, games, order, base, options.workers, stop, played);
        if (done != games.size())
        {
            log.line(format("generation %d stopped after %zu/%zu games (%s); checkpoint keeps generation %d", generation, done, games.size(),
                            g_interrupted.load() ? "interrupted" : "time budget", generation - 1));
            break;
        }

        // Fitness, split-half reliability over disjoint group halves, and the population's score per opponent kind and ratio.
        std::vector<double> fitness(static_cast<std::size_t>(lambda), 0.0);
        std::array<std::vector<double>, 2> half{std::vector<double>(static_cast<std::size_t>(lambda), 0.0), std::vector<double>(static_cast<std::size_t>(lambda), 0.0)};
        std::vector<double> kind_sum(static_cast<std::size_t>(anchors + 2), 0.0), ratio_sum(st.scenario.ratios.size(), 0.0);
        std::vector<int> kind_n(kind_sum.size(), 0), ratio_n(ratio_sum.size(), 0);
        std::vector<double> group_wall(dealt.size(), 0.0);
        double game_seconds = 0;
        int time_limit = 0;
        for (std::size_t i = 0; i < games.size(); ++i)
        {
            Meta const &m = meta[i];
            Dealt const &d = dealt[static_cast<std::size_t>(m.dealt)];
            double const s = io_league::seat_score(played[i].result, m.seat);
            fitness[static_cast<std::size_t>(m.candidate)] += s;
            half[static_cast<std::size_t>(m.dealt % 2)][static_cast<std::size_t>(m.candidate)] += s;
            kind_sum[static_cast<std::size_t>(d.kind)] += s;
            ++kind_n[static_cast<std::size_t>(d.kind)];
            ratio_sum[static_cast<std::size_t>(d.ratio)] += s;
            ++ratio_n[static_cast<std::size_t>(d.ratio)];
            group_wall[static_cast<std::size_t>(m.dealt)] += played[i].wall_seconds;
            game_seconds += static_cast<double>(played[i].result.frames) / io_sim::kFramesPerSecond;
            time_limit += played[i].result.loser < 0;
        }
        for (double &f : fitness)
            f /= 2.0 * st.groups;
        double const r = io_league::spearman(half[0], half[1]);
        double const reliability = r > 0 ? 2 * r / (1 + r) : 0;
        for (std::size_t k = 0; k < dealt.size(); ++k)
        {
            double &s = group_seconds[static_cast<std::size_t>(dealt[k].group)];
            double const v = group_wall[k] / (2.0 * lambda);
            s = s < 0 ? v : 0.7 * s + 0.3 * v;
        }

        std::vector<std::size_t> ranking(static_cast<std::size_t>(lambda));
        std::iota(ranking.begin(), ranking.end(), std::size_t{0});
        std::stable_sort(ranking.begin(), ranking.end(), [&](std::size_t a, std::size_t b)
                         { return fitness[a] > fitness[b]; });
        double const sigma = es->sigma();
        auto const [min_c, max_c] = std::ranges::minmax(es->variances());
        es->tell(genotypes, ranking);

        st.reliability = st.reliability_count == 0 ? reliability : (1 - kReliabilityDecay) * st.reliability + kReliabilityDecay * reliability;
        ++st.reliability_count;
        std::string growth;
        if (st.reliability_count >= kReliabilityWindow && generation - st.grown_at >= kReliabilityWindow && st.reliability < kReliabilityFloor &&
            st.groups * 2 <= st.scenario.max_groups)
        {
            st.groups *= 2;
            st.grown_at = generation;
            st.reliability_count = 0;
            growth = format(" | reliability below %.2f: groups -> %d", kReliabilityFloor, st.groups);
        }

        for (std::size_t i = 0; i < games.size(); ++i)
        {
            Meta const &m = meta[i];
            io_league::write_csv_row(csv, "train", generation, "c" + std::to_string(m.candidate), dealt[static_cast<std::size_t>(m.dealt)].name, games[i], m.seat,
                                     played[i]);
        }
        csv.flush();

        double const gen_seconds = seconds_since(gen_start);
        st.games += games.size();
        st.game_seconds += game_seconds;

        std::string versus;
        for (int kind = 0; kind < anchors + 2; ++kind)
        {
            if (kind_n[static_cast<std::size_t>(kind)] == 0)
                continue;
            std::string const name = kind == 0 ? "self" : kind <= anchors ? st.scenario.anchors[static_cast<std::size_t>(kind - 1)]
                                                                          : "hof";
            versus += format(" %s %.3f", name.c_str(), kind_sum[static_cast<std::size_t>(kind)] / kind_n[static_cast<std::size_t>(kind)]);
        }
        std::string by_ratio;
        for (std::size_t k = 0; k < ratio_sum.size(); ++k)
        {
            if (ratio_n[k] > 0)
                by_ratio += format(" x%s %.3f", io_league::format_number(st.scenario.ratios[k]).c_str(), ratio_sum[k] / ratio_n[k]);
        }
        double const n_games = static_cast<double>(games.size());
        log.line(format("gen %d | sigma %.4f, axis %.4f-%.4f | best %.4f, avg %.4f | reliability %.2f, avg %.2f | vs%s |%s | %zu games in %.1f min, "
                        "%.0f games/h, avg game %.1f s, time-limit %.1f%%%s",
                        generation, sigma, sigma * std::sqrt(min_c), sigma * std::sqrt(max_c), fitness[ranking.front()],
                        std::accumulate(fitness.begin(), fitness.end(), 0.0) / lambda, reliability, st.reliability, versus.c_str(), by_ratio.c_str(), games.size(),
                        gen_seconds / 60, n_games * 3600 / gen_seconds, game_seconds / n_games, 100.0 * time_limit / n_games, growth.c_str()));

        if (validate_now)
        {
            auto const validation_start = Clock::now();
            Param const snapshot = to_param(st, es->mean());
            Panel const panel = make_panel(st, st.validations);
            auto scores = validate(snapshot, st.champion, st, panel, base, options.workers, stop, played);
            if (scores)
            {
                ++st.validations;
                last_validation_seconds = seconds_since(validation_start);
                write_validation_rows(panel, generation, played);
                auto const &[mine, theirs] = *scores;
                auto const d = io_league::paired_difference(mine, theirs, panel.cluster);
                double const z = d.se > 0 ? d.mean / d.se : (d.mean > 0 ? std::numeric_limits<double>::infinity() : 0.0);
                bool const promote = d.mean > 0 && z >= st.scenario.promote_z;
                auto const e = io_league::clustered_mean(mine, panel.cluster);
                auto const c = io_league::clustered_mean(theirs, panel.cluster);
                std::string const previous = champion_origin(st.champion_generation);
                std::string outcome = "kept";
                if (promote)
                {
                    st.champion = snapshot;
                    st.champion_generation = generation;
                    st.hof.push_back({generation, snapshot});
                    char name[32];
                    std::snprintf(name, sizeof name, "gen_%05d.params", generation);
                    bool const saved = write_champion(out_dir, st) && ai_zzz::IO::save_params((out_dir / "hof" / name).string(), snapshot);
                    outcome = saved ? "promoted" : "promoted | CHAMPION WRITE FAILED";
                }
                log.line(format("validation %d after gen %d | mean %.4f (%s) vs champion (%s) %.4f (%s) | delta %+.4f ± %.4f, z %.2f | %s | %zu games in %.1f min",
                                st.validations, generation, e.mean, ratio_breakdown(st, panel, mine).c_str(), previous.c_str(), c.mean,
                                ratio_breakdown(st, panel, theirs).c_str(), d.mean, d.se, z, outcome.c_str(), 2 * panel.games.size(),
                                last_validation_seconds / 60));
            }
            else
                log.line(format("validation after gen %d stopped (%s)", generation, g_interrupted.load() ? "interrupted" : "time budget"));
        }

        last_generation_seconds = gen_seconds;
        st.wall_seconds += seconds_since(gen_start);
        bool const saved = write_atomically(checkpoint_path, checkpoint_text(st, *es)) &&
                           write_atomically(out_dir / "mean.params", ai_zzz::IO::format_params(to_param(st, es->mean())));
        log.line(format("total %.2f h, %llu games, champion: %s%s", st.wall_seconds / 3600, static_cast<unsigned long long>(st.games),
                        champion_origin(st.champion_generation).c_str(), saved ? "" : " | CHECKPOINT WRITE FAILED"));
    }
    log.line(format("io_cmaes stopped at generation %d; champion: %s; outputs in %s", es->generation(), champion_origin(st.champion_generation).c_str(),
                    options.out.c_str()));
    return 0;
}
