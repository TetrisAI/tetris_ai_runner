// framework_ax_smoke.cpp — owner 负责的 ax 真实 engine prepare/run/link smoke.
// (其余 6 个 remaining-7 AI 由 D owner 在 framework_allai_remaining7_smoke.cpp.)
#include "../src/tetris_core.h"
#include "../src/tetris_eval_get.h"
#include "../src/rule_srs.h"
#include "../src/search_path.h"
#include "../src/ai_ax.h"

#include <cstdio>

namespace
{
    int g_failures = 0;
    void expect(bool ok, const char *what)
    {
        if (!ok) { ++g_failures; std::printf("FAIL: %s\n", what); }
    }
}

int main()
{
    std::printf("# framework_ax_smoke\n");
    using Rule = rule_srs::TetrisRule;
    using E = m_tetris2::TetrisEngine<Rule, ai_ax::AI, path::Search>;
    E e;
    using Spec = Rule::rule_spec;
    if (!e.prepare(Spec::width, Spec::height)) { expect(false, "ax prepare"); return 1; }
    E::map_t map{};
    auto sp = m_tetris2::TetrisBlockStatus('T',
        static_cast<int8_t>(Spec::spawn('T', Spec::width, Spec::height).first),
        static_cast<int8_t>(Spec::spawn('T', Spec::width, Spec::height).second), 0);
    e.run(map, sp, "ISZL", 4, 20);
    std::printf("  ax: scored=%zu\n", e.scored());
    expect(e.scored() > 0, "ax run scored");
    if (g_failures == 0) { std::printf("ax_smoke: OK\n"); return 0; }
    std::printf("ax_smoke: %d FAILURES\n", g_failures);
    return 1;
}
