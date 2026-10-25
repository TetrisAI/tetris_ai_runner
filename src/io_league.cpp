#include "io_league.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <numeric>
#include <thread>

namespace io_league
{
    std::optional<Param> builtin_policy(std::string_view name)
    {
        Param p;
        if (name == "default")
            return p;
        if (name == "spiker")
        {
            p.b2b = 0;
            p.surge = 0;
            return p;
        }
        if (name == "staller")
        {
            p.b2b *= 4;
            p.surge *= 4;
            return p;
        }
        return std::nullopt;
    }

    std::optional<Param> load_policy(std::string const &spec)
    {
        if (auto p = builtin_policy(spec))
            return p;
        Param p;
        if (!ai_zzz::IO::load_params(spec, p))
            return std::nullopt;
        return p;
    }

    bool same_param(Param const &a, Param const &b)
    {
        for (auto const &f : ai_zzz::IO::param_fields())
        {
            if (a.*f.member != b.*f.member)
                return false;
        }
        return true;
    }

    std::string format_number(double value)
    {
        char text[32];
        for (int precision = 1; precision <= 17; ++precision)
        {
            std::snprintf(text, sizeof text, "%.*g", precision, value);
            if (std::strtod(text, nullptr) == value)
                break;
        }
        return text;
    }

    std::optional<io_sim::Pace> parse_pace(std::string_view text)
    {
        io_sim::Pace pace;
        if (!text.empty() && text.back() == 'b')
        {
            pace.cadence = io_sim::Cadence::Burst;
            text.remove_suffix(1);
        }
        std::string const number(text);
        char *end = nullptr;
        pace.pps = std::strtod(number.c_str(), &end);
        if (number.empty() || end != number.c_str() + number.size() || !std::isfinite(pace.pps) || !(pace.pps > 0))
            return std::nullopt;
        return pace;
    }

    std::string format_pace(io_sim::Pace const &pace)
    {
        return format_number(pace.pps) + (pace.cadence == io_sim::Cadence::Burst ? "b" : "");
    }

    std::vector<std::string> split(std::string_view text, char separator)
    {
        std::vector<std::string> items;
        if (text.empty())
            return items;
        for (std::size_t start = 0;;)
        {
            std::size_t const end = text.find(separator, start);
            items.emplace_back(text.substr(start, end == std::string_view::npos ? std::string_view::npos : end - start));
            if (end == std::string_view::npos)
                return items;
            start = end + 1;
        }
    }

    std::optional<std::vector<io_sim::Pace>> parse_paces(std::string_view text)
    {
        std::vector<io_sim::Pace> paces;
        for (auto const &item : split(text, ','))
        {
            auto pace = parse_pace(item);
            if (!pace)
                return std::nullopt;
            paces.push_back(*pace);
        }
        if (paces.empty())
            return std::nullopt;
        return paces;
    }

    std::optional<std::vector<double>> parse_numbers(std::string_view text)
    {
        std::vector<double> numbers;
        for (auto const &item : split(text, ','))
        {
            char *end = nullptr;
            double const value = std::strtod(item.c_str(), &end);
            if (item.empty() || end != item.c_str() + item.size() || !std::isfinite(value) || !(value > 0))
                return std::nullopt;
            numbers.push_back(value);
        }
        if (numbers.empty())
            return std::nullopt;
        return numbers;
    }

    std::int64_t training_seed(std::mt19937_64 &rng)
    {
        return std::uniform_int_distribution<std::int64_t>(1, (std::int64_t{1} << 30) - 1)(rng);
    }

    std::int64_t panel_seed(SeedStream stream, int index)
    {
        std::uint64_t z = ((static_cast<std::uint64_t>(stream) << 32) | static_cast<std::uint32_t>(index)) + 0x9E3779B97F4A7C15ull;
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
        z ^= z >> 31;
        constexpr std::int64_t kValidation = std::int64_t{1} << 30;
        constexpr std::int64_t kTest = kValidation + (std::int64_t{1} << 29);
        constexpr std::int64_t kEnd = (std::int64_t{1} << 31) - 1;
        std::int64_t const lo = stream == SeedStream::Validation ? kValidation : kTest;
        std::int64_t const hi = stream == SeedStream::Validation ? kTest : kEnd;
        return lo + static_cast<std::int64_t>(z % static_cast<std::uint64_t>(hi - lo));
    }

    std::size_t play(std::span<Param const> policies, std::span<Game const> games, std::span<std::size_t const> order,
                     io_sim::MatchConfig const &base, int workers, std::function<bool()> const &stop, std::vector<Played> &results)
    {
        results.assign(games.size(), Played{});
        std::vector<std::size_t> listed;
        if (order.empty())
        {
            listed.resize(games.size());
            std::iota(listed.begin(), listed.end(), std::size_t{0});
            order = listed;
        }
        std::atomic<std::size_t> next{0};
        std::atomic<std::size_t> played{0};
        auto work = [&]
        {
            for (;;)
            {
                if (stop && stop())
                    return;
                std::size_t const k = next.fetch_add(1);
                if (k >= order.size())
                    return;
                std::size_t const i = order[k];
                Game const &game = games[i];
                io_sim::MatchConfig config = base;
                config.pace = {game.first_pace, game.second_pace};
                auto const start = std::chrono::steady_clock::now();
                io_sim::MatchResult result = io_sim::play_match(config, {policies[static_cast<std::size_t>(game.first)], policies[static_cast<std::size_t>(game.second)]}, game.seed);
                results[i] = {result, std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count(), true};
                played.fetch_add(1);
            }
        };
        std::vector<std::thread> threads;
        int const count = std::clamp(workers, 1, std::max(1, static_cast<int>(order.size())));
        for (int t = 0; t < count; ++t)
            threads.emplace_back(work);
        for (auto &t : threads)
            t.join();
        return played.load();
    }

    Estimate clustered_mean(std::span<double const> x, std::span<int const> cluster)
    {
        std::map<int, std::pair<double, int>> sums;
        for (std::size_t i = 0; i < x.size(); ++i)
        {
            auto &sum = sums[cluster[i]];
            sum.first += x[i];
            ++sum.second;
        }
        Estimate e;
        e.clusters = static_cast<int>(sums.size());
        if (sums.empty())
            return e;
        std::vector<double> means;
        for (auto const &entry : sums)
            means.push_back(entry.second.first / entry.second.second);
        double const n = static_cast<double>(means.size());
        e.mean = std::accumulate(means.begin(), means.end(), 0.0) / n;
        if (means.size() > 1)
        {
            double squares = 0;
            for (double m : means)
                squares += (m - e.mean) * (m - e.mean);
            e.se = std::sqrt(squares / (n - 1) / n);
        }
        return e;
    }

    Estimate paired_difference(std::span<double const> a, std::span<double const> b, std::span<int const> cluster)
    {
        std::vector<double> d(a.size());
        for (std::size_t i = 0; i < a.size(); ++i)
            d[i] = a[i] - b[i];
        return clustered_mean(d, cluster);
    }

    namespace
    {
        std::vector<double> ranks(std::span<double const> v)
        {
            std::vector<std::size_t> order(v.size());
            std::iota(order.begin(), order.end(), std::size_t{0});
            std::stable_sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b)
                             { return v[a] < v[b]; });
            std::vector<double> r(v.size());
            for (std::size_t i = 0; i < order.size();)
            {
                std::size_t j = i;
                while (j + 1 < order.size() && v[order[j + 1]] == v[order[i]])
                    ++j;
                for (std::size_t k = i; k <= j; ++k)
                    r[order[k]] = (static_cast<double>(i) + static_cast<double>(j)) / 2;
                i = j + 1;
            }
            return r;
        }
    }

    double spearman(std::span<double const> a, std::span<double const> b)
    {
        auto const ra = ranks(a);
        auto const rb = ranks(b);
        double const mean = (static_cast<double>(ra.size()) - 1) / 2;
        double cov = 0, va = 0, vb = 0;
        for (std::size_t i = 0; i < ra.size(); ++i)
        {
            cov += (ra[i] - mean) * (rb[i] - mean);
            va += (ra[i] - mean) * (ra[i] - mean);
            vb += (rb[i] - mean) * (rb[i] - mean);
        }
        return va > 0 && vb > 0 ? cov / std::sqrt(va * vb) : 0;
    }

    void write_csv_header(std::ostream &out)
    {
        out << "phase,generation,subject,opponent,own_pace,opp_pace,seed,seat,score,end,reason,frames,pieces,opp_pieces,sent,opp_sent,wall_ms\n";
    }

    void write_csv_row(std::ostream &out, std::string_view phase, int generation, std::string_view subject, std::string_view opponent,
                       Game const &game, int seat, Played const &played)
    {
        io_sim::MatchResult const &r = played.result;
        int const other = 1 - seat;
        char const *end = r.loser < 0 ? "time" : r.loser == seat ? "loss"
                                                                 : "win";
        char const *reason = r.reason == io_sim::EndReason::BlockOut       ? "blockout"
                             : r.reason == io_sim::EndReason::GarbageSmash ? "garbagesmash"
                                                                           : "timelimit";
        char score[32];
        std::snprintf(score, sizeof score, "%.6g", seat_score(r, seat));
        out << phase << ',' << generation << ',' << subject << ',' << opponent << ',' << format_pace(seat == 0 ? game.first_pace : game.second_pace)
            << ',' << format_pace(seat == 0 ? game.second_pace : game.first_pace) << ',' << game.seed << ',' << seat << ',' << score << ',' << end << ','
            << reason << ',' << r.frames << ',' << r.stats[static_cast<std::size_t>(seat)].pieces << ',' << r.stats[static_cast<std::size_t>(other)].pieces
            << ',' << r.stats[static_cast<std::size_t>(seat)].lines_sent << ',' << r.stats[static_cast<std::size_t>(other)].lines_sent << ','
            << std::llround(played.wall_seconds * 1000) << '\n';
    }
}
