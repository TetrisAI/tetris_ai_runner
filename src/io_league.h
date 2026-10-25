#pragma once

// io_league: what io_cmaes and io_eval share. Named policies, paces, disjoint seed streams, a parallel match runner
// whose results do not depend on the thread count, paired statistics, and the per-game CSV.

#include "io_sim.h"

#include <cstdint>
#include <functional>
#include <optional>
#include <ostream>
#include <random>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace io_league
{
    using Param = ai_zzz::IO::Param;

    // "default" (the in-code IO defaults), "spiker" (b2b = surge = 0) and "staller" (4x b2b and surge).
    std::optional<Param> builtin_policy(std::string_view name);
    // A builtin name or an IO param file.
    std::optional<Param> load_policy(std::string const &spec);
    bool same_param(Param const &a, Param const &b);

    // "2.5" = constant cadence, "2.5b" = burst cadence; formatting round-trips exactly.
    std::optional<io_sim::Pace> parse_pace(std::string_view text);
    std::string format_pace(io_sim::Pace const &pace);
    std::string format_number(double value);
    std::vector<std::string> split(std::string_view text, char separator);
    // Comma-separated lists; nullopt if any item is invalid or the list is empty.
    std::optional<std::vector<io_sim::Pace>> parse_paces(std::string_view text);
    std::optional<std::vector<double>> parse_numbers(std::string_view text);

    // Disjoint Park–Miller seed ranges, so validation and test games never repeat a training game. Panel seeds are
    // hashed because neighbouring Park–Miller seeds start with nearly equal draws.
    enum class SeedStream : std::uint8_t
    {
        Validation, // [2^30, 2^30 + 2^29)
        Test        // [2^30 + 2^29, 2^31 - 1)
    };
    std::int64_t training_seed(std::mt19937_64 &rng); // [1, 2^30)
    std::int64_t panel_seed(SeedStream stream, int index);

    struct Game
    {
        int first = 0; // policy index in seat 0
        int second = 0;
        io_sim::Pace first_pace;
        io_sim::Pace second_pace;
        std::int64_t seed = 1;
    };

    struct Played
    {
        io_sim::MatchResult result;
        double wall_seconds = 0;
        bool done = false;
    };

    // Plays `games` in `order` (empty: as listed) on `workers` threads; results[i] belongs to games[i] whatever the
    // thread count, so a run repeats exactly on any machine. Threads stop taking games once `stop()` returns true.
    // Returns the number of games played.
    std::size_t play(std::span<Param const> policies, std::span<Game const> games, std::span<std::size_t const> order,
                     io_sim::MatchConfig const &base, int workers, std::function<bool()> const &stop, std::vector<Played> &results);

    inline double seat_score(io_sim::MatchResult const &result, int seat)
    {
        return seat == 0 ? result.score : 1 - result.score;
    }

    // Mean and standard error across clusters of the per-cluster mean of x[i] (or of a[i] - b[i]).
    struct Estimate
    {
        double mean = 0;
        double se = 0;
        int clusters = 0;
    };
    Estimate clustered_mean(std::span<double const> x, std::span<int const> cluster);
    Estimate paired_difference(std::span<double const> a, std::span<double const> b, std::span<int const> cluster);

    // Spearman rank correlation (average ranks for ties); 0 when either side is constant.
    double spearman(std::span<double const> a, std::span<double const> b);

    void write_csv_header(std::ostream &out);
    // One row from the point of view of the policy in `seat`.
    void write_csv_row(std::ostream &out, std::string_view phase, int generation, std::string_view subject, std::string_view opponent,
                       Game const &game, int seat, Played const &played);
}
