// Compile-only sanity check for `examples/dummy_strategy.h`.
//
// Instantiates Searcher<DummyStrategy, NoSpinHook, rule_srs::rule_spec> and
// invokes its public API (init / search / make_path) so that the linker
// is forced to materialise every template entry point the strategy
// contract advertises. The translation unit deliberately does not run
// the searcher in a meaningful way — it only has to compile and link.
//
// Adding new fields, mixins, or contract methods to the public movegen
// surface should be checked here first; if this file stops building, the
// public docs and the strategy contract have drifted apart.

#include "dummy_strategy.h"

#include "movegen_searcher.h"
#include "rule_srs.h"
#include "tetris_core.h"

#include <vector>

namespace
{
    using Backend = m_tetris2::movegen::Searcher<m_tetris2::DummyStrategy,
                                                m_tetris2::NoSpinHook,
                                                rule_srs::TetrisRule::rule_spec>;

    void touch_api()
    {
        Backend backend{};
        m_tetris2::TetrisContext const *dummy_ctx = nullptr;
        Backend::Config const *cfg = nullptr;
        backend.init(dummy_ctx, cfg);

        m_tetris2::TetrisMap map(10, 40);
        m_tetris2::TetrisNode const *node = nullptr;
        auto spawn = node == nullptr ? m_tetris2::bb::BBState{0, 0, 0, 0}
            : m_tetris2::bb::Helpers<Backend::rule_spec>::state_from_status(
                node->status.t,
                static_cast<std::uint8_t>(node->status.r),
                node->status.x,
                node->status.y);
        std::vector<Backend::LandPoint> const *cache = backend.search(map, node, 0);
        (void)cache;

        Backend::LandPoint lp{};
        auto board = [&]()
        {
            using map_t = m_tetris2::Map<Backend::rule_spec::width, Backend::rule_spec::height>;
            map_t out{};
            using row_t = typename map_t::row_t;
            constexpr row_t row_mask = map_t::row_full;
            int h = map.height < map_t::height ? map.height : map_t::height;
            for (int y = 0; y < h; ++y)
            {
                std::uint32_t occ = (~map.row[y]) & static_cast<std::uint32_t>(row_mask);
                out.set_row(y, static_cast<row_t>(occ));
            }
            return out;
        }();
        std::vector<char> path = backend.make_path(spawn, lp, board);
        (void)path;
    }
} // namespace

extern "C" int dummy_strategy_compile_check_entry(int argc, char **argv)
{
    (void)argv;
    if (argc < 0)
        touch_api();
    return 0;
}

int main(int argc, char **argv)
{
    return dummy_strategy_compile_check_entry(argc, argv);
}
