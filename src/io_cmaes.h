#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <istream>
#include <optional>
#include <ostream>
#include <random>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace io_cmaes
{
    // Triangle wave onto [0, 1]: an unbounded search point maps into the box without rejection, penalty or clamping bias.
    inline double mirror(double y)
    {
        double t = std::fmod(std::abs(y), 2.0);
        return t <= 1 ? t : 2 - t;
    }

    // sep-CMA-ES (Ros & Hansen 2008): CMA-ES with a diagonal covariance, whose covariance learning rates are (n + 2) / 3
    // times those of full CMA-ES. Rank-based: tell() only needs the order of the candidates.
    class SepCmaes
    {
    public:
        SepCmaes(std::vector<double> mean, double sigma, int lambda)
            : mean_(std::move(mean)), sigma_(sigma), lambda_(lambda), c_(mean_.size(), 1.0), ps_(mean_.size(), 0.0), pc_(mean_.size(), 0.0)
        {
            configure();
        }

        static int default_lambda(std::size_t n)
        {
            return 4 + static_cast<int>(std::floor(3 * std::log(static_cast<double>(n))));
        }

        std::size_t dimension() const
        {
            return mean_.size();
        }
        int lambda() const
        {
            return lambda_;
        }
        int mu() const
        {
            return static_cast<int>(weights_.size());
        }
        int generation() const
        {
            return generation_;
        }
        double sigma() const
        {
            return sigma_;
        }
        std::vector<double> const &mean() const
        {
            return mean_;
        }
        std::vector<double> const &variances() const
        {
            return c_;
        }

        std::vector<std::vector<double>> ask(std::mt19937_64 &rng) const
        {
            std::normal_distribution<double> gauss(0, 1);
            std::vector<std::vector<double>> x(static_cast<std::size_t>(lambda_), mean_);
            for (auto &xi : x)
            {
                for (std::size_t j = 0; j < xi.size(); ++j)
                    xi[j] += sigma_ * std::sqrt(c_[j]) * gauss(rng);
            }
            return x;
        }

        // `best_first`: indices into `candidates` (this generation's ask()), best first; the best mu() are recombined.
        void tell(std::vector<std::vector<double>> const &candidates, std::span<std::size_t const> best_first)
        {
            std::size_t const n = dimension();
            std::vector<double> y_w(n, 0.0);
            std::vector<double> y2_w(n, 0.0);
            for (std::size_t i = 0; i < weights_.size(); ++i)
            {
                auto const &x = candidates[best_first[i]];
                for (std::size_t j = 0; j < n; ++j)
                {
                    double y = (x[j] - mean_[j]) / sigma_;
                    y_w[j] += weights_[i] * y;
                    y2_w[j] += weights_[i] * y * y;
                }
            }
            ++generation_;
            double ps_norm2 = 0;
            double const ps_gain = std::sqrt(cs_ * (2 - cs_) * mueff_);
            for (std::size_t j = 0; j < n; ++j)
            {
                mean_[j] += sigma_ * y_w[j];
                ps_[j] = (1 - cs_) * ps_[j] + ps_gain * y_w[j] / std::sqrt(c_[j]);
                ps_norm2 += ps_[j] * ps_[j];
            }
            double const ps_norm = std::sqrt(ps_norm2);
            bool const hsig = ps_norm / std::sqrt(1 - std::pow(1 - cs_, 2.0 * generation_)) < (1.4 + 2 / (static_cast<double>(n) + 1)) * chi_n_;
            double const pc_gain = hsig ? std::sqrt(cc_ * (2 - cc_) * mueff_) : 0;
            double const c1_refund = hsig ? 0 : c1_ * cc_ * (2 - cc_);
            for (std::size_t j = 0; j < n; ++j)
            {
                pc_[j] = (1 - cc_) * pc_[j] + pc_gain * y_w[j];
                c_[j] = (1 - c1_ - cmu_ + c1_refund) * c_[j] + c1_ * pc_[j] * pc_[j] + cmu_ * y2_w[j];
            }
            sigma_ *= std::exp(cs_ / damps_ * (ps_norm / chi_n_ - 1));
        }

        void save(std::ostream &out) const
        {
            auto const precision = out.precision(17);
            out << "dimension " << dimension() << '\n';
            out << "lambda " << lambda_ << '\n';
            out << "generation " << generation_ << '\n';
            out << "sigma " << sigma_ << '\n';
            write_vector(out, "mean", mean_);
            write_vector(out, "c", c_);
            write_vector(out, "ps", ps_);
            write_vector(out, "pc", pc_);
            out.precision(precision);
        }

        static std::optional<SepCmaes> load(std::istream &in)
        {
            std::size_t n = 0;
            int lambda = 0;
            int generation = 0;
            double sigma = 0;
            if (!expect(in, "dimension", n) || !expect(in, "lambda", lambda) || !expect(in, "generation", generation) || !expect(in, "sigma", sigma) ||
                n < 1 || lambda < 2 || generation < 0 || !(sigma > 0))
                return std::nullopt;
            SepCmaes s(std::vector<double>(n), sigma, lambda);
            s.generation_ = generation;
            if (!read_vector(in, "mean", s.mean_) || !read_vector(in, "c", s.c_) || !read_vector(in, "ps", s.ps_) || !read_vector(in, "pc", s.pc_))
                return std::nullopt;
            return s;
        }

    private:
        void configure()
        {
            double const n = static_cast<double>(dimension());
            weights_.resize(static_cast<std::size_t>(lambda_ / 2));
            double sum = 0;
            for (std::size_t i = 0; i < weights_.size(); ++i)
            {
                weights_[i] = std::log((lambda_ + 1) / 2.0) - std::log(static_cast<double>(i + 1));
                sum += weights_[i];
            }
            double sum2 = 0;
            for (double &w : weights_)
            {
                w /= sum;
                sum2 += w * w;
            }
            mueff_ = 1 / sum2;
            cs_ = (mueff_ + 2) / (n + mueff_ + 5);
            damps_ = 1 + 2 * std::max(0.0, std::sqrt((mueff_ - 1) / (n + 1)) - 1) + cs_;
            cc_ = (4 + mueff_ / n) / (n + 4 + 2 * mueff_ / n);
            double const sep = (n + 2) / 3;
            c1_ = std::min(1.0, sep * 2 / ((n + 1.3) * (n + 1.3) + mueff_));
            cmu_ = std::min(1 - c1_, sep * 2 * (mueff_ - 2 + 1 / mueff_) / ((n + 2) * (n + 2) + mueff_));
            chi_n_ = std::sqrt(n) * (1 - 1 / (4 * n) + 1 / (21 * n * n));
        }

        static void write_vector(std::ostream &out, std::string_view tag, std::vector<double> const &v)
        {
            out << tag;
            for (double d : v)
                out << ' ' << d;
            out << '\n';
        }

        static bool read_vector(std::istream &in, std::string_view tag, std::vector<double> &v)
        {
            std::string word;
            if (!(in >> word) || word != tag)
                return false;
            for (double &d : v)
            {
                if (!(in >> d))
                    return false;
            }
            return true;
        }

        static bool expect(std::istream &in, std::string_view tag, auto &value)
        {
            std::string word;
            return (in >> word) && word == tag && (in >> value);
        }

        std::vector<double> mean_;
        double sigma_;
        int lambda_;
        int generation_ = 0;
        std::vector<double> c_, ps_, pc_;
        std::vector<double> weights_;
        double mueff_ = 0, cs_ = 0, damps_ = 0, cc_ = 0, c1_ = 0, cmu_ = 0, chi_n_ = 0;
    };
}
