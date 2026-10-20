// io_cmaes_test: sep-CMA-ES core — convergence with optima on the bounds, exact resume from a saved state.

#include "../src/io_cmaes.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <numeric>
#include <random>
#include <sstream>
#include <vector>

namespace
{
    // Optima on both bounds and inside; axis weights span 1e6, so convergence needs the diagonal covariance to adapt.
    std::vector<double> const kTarget{0, 1, 0.3, 0.7, 0, 0.5, 1, 0.2};

    double ellipsoid(std::vector<double> const &y)
    {
        double f = 0;
        for (std::size_t j = 0; j < y.size(); ++j)
        {
            double d = io_cmaes::mirror(y[j]) - kTarget[j];
            f += std::pow(1e6, static_cast<double>(j) / static_cast<double>(y.size() - 1)) * d * d;
        }
        return f;
    }

    void step(io_cmaes::SepCmaes &es, std::mt19937_64 &rng)
    {
        auto x = es.ask(rng);
        std::vector<double> f;
        for (auto const &xi : x)
            f.push_back(ellipsoid(xi));
        std::vector<std::size_t> order(x.size());
        std::iota(order.begin(), order.end(), std::size_t{0});
        std::stable_sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b)
                         { return f[a] < f[b]; });
        es.tell(x, order);
    }

    io_cmaes::SepCmaes centred(int lambda)
    {
        return io_cmaes::SepCmaes(std::vector<double>(kTarget.size(), 0.5), 0.3, lambda);
    }

    void converges_to_optima_on_bounds()
    {
        auto es = centred(io_cmaes::SepCmaes::default_lambda(kTarget.size()));
        std::mt19937_64 rng(1);
        while (es.generation() < 3000 && ellipsoid(es.mean()) > 1e-14)
            step(es, rng);
        std::printf("ellipsoid: generation %d, f %.3g, sigma %.3g\n", es.generation(), ellipsoid(es.mean()), es.sigma());
        assert(ellipsoid(es.mean()) <= 1e-14);
        for (std::size_t j = 0; j < kTarget.size(); ++j)
            assert(std::abs(io_cmaes::mirror(es.mean()[j]) - kTarget[j]) < 1e-6);
    }

    void resume_continues_exactly()
    {
        auto straight = centred(6);
        std::mt19937_64 straight_rng(7);
        for (int g = 0; g < 60; ++g)
            step(straight, straight_rng);

        auto first = centred(6);
        std::mt19937_64 rng(7);
        for (int g = 0; g < 30; ++g)
            step(first, rng);
        std::stringstream saved;
        first.save(saved);
        auto resumed = io_cmaes::SepCmaes::load(saved);
        assert(resumed && resumed->generation() == 30);
        for (int g = 30; g < 60; ++g)
            step(*resumed, rng);

        assert(resumed->lambda() == straight.lambda() && resumed->generation() == straight.generation());
        assert(resumed->sigma() == straight.sigma() && resumed->mean() == straight.mean() && resumed->variances() == straight.variances());
    }
}

int main()
{
    converges_to_optima_on_bounds();
    resume_continues_exactly();
    std::puts("io_cmaes_test: ok");
    return 0;
}
