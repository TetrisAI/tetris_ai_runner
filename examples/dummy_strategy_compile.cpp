// Sanity check for `examples/dummy_strategy.h`.
//
// Instantiates Searcher<DummyStrategy, NoSpinHook, rule_srs::rule_spec> and
// invokes its whole public API (init / search / search_eval / make_path) so
// that every template entry point the strategy contract advertises is
// materialised. main() runs it once on an empty board and returns non-zero
// if the entry points disagree (search vs. search_eval landing count, or
// make_path not returning "D").
//
// Adding new fields, mixins, or contract methods to the public movegen
// surface should be checked here first; if this file stops building, the
// public docs and the strategy contract have drifted apart.
//
// The target is EXCLUDE_FROM_ALL; build it explicitly with
//   cmake --build build --target dummy_strategy_compile_check

#include "dummy_strategy.h"

#include "movegen_searcher.h"
#include "rule_srs.h"
#include "tetris_core.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace
{
    using Backend = m_tetris2::movegen::Searcher<m_tetris2::DummyStrategy,
                                                m_tetris2::NoSpinHook,
                                                rule_srs::TetrisRule::rule_spec>;
    using Spec = Backend::rule_spec;
    using Map = m_tetris2::TetrisMap<Spec::width, Spec::height>;

    int touch_api()
    {
        Backend backend{};
        Backend::Config const *cfg = nullptr;
        backend.init(cfg);

        Map map{};

        // 直接构造 TetrisBlockStatus，不依赖 TetrisNode 指针。
        auto [sx, sy] = Spec::spawn('S', Spec::width, Spec::height);
        m_tetris2::TetrisBlockStatus status{'S',
                                            static_cast<std::int8_t>(sx),
                                            static_cast<std::int8_t>(sy),
                                            0};
        m_tetris2::State spawn = m_tetris2::Helpers<Spec>::state_from_status(
            status.t, static_cast<std::uint8_t>(status.r), status.x, status.y);

        // search: Searcher 把 TetrisBlockStatus 转成 State 后转发给 Strategy::search.
        std::vector<Backend::LandPoint> const *cache = backend.search(map, status, 0);
        if (cache == nullptr || cache->empty())
            return 1;

        // search_eval: 每个落点以编译期 <T, R> 回调.
        std::size_t eval_count = 0;
        auto on_land = [&]<char T, std::uint8_t R>(Backend::LandPoint const &lp)
        {
            if (lp.state.t == static_cast<std::uint8_t>(T) && lp.state.r == R)
                ++eval_count;
        };
        backend.search_eval(map, spawn, 0, on_land);
        if (eval_count != cache->size())
            return 2;

        std::vector<char> path = backend.make_path(spawn, cache->front(), map);
        if (path.size() != 1 || path.front() != 'D')
            return 3;
        return 0;
    }
} // namespace

int main()
{
    return touch_api();
}
