// framework_farter_engine_smoke_test.cpp — farter 真实 Engine 端到端 0-eval 证据.
//
// 归属: test owner. farter GetArgs = MapBefore, Landing, Parent, Depth (无 Eval/Clear/Env),
//   eval(MapT) 恒返回空 Result. 本测试用真实 TetrisEngine 跑确定性搜索, 验证:
//   1. eval_cache_stats().evals == 0 (整次搜索不调 eval);
//   2. get 真实跑过 (scored() > 0, Status 非退化);
//   3. provider_counters: eval==0, map_before/landing/parent/depth 均 > 0.
// enemy (shard C) 待迁移后另补同型 smoke.

#include "../src/tetris_core.h"
#include "../src/rule_srs.h"
#include "../src/search_path.h"
#include "../src/ai_farter.h"

#include <cstdio>

namespace
{
    int g_failures = 0;
    void expect(bool ok, char const *what)
    {
        if (!ok)
        {
            ++g_failures;
            std::printf("FAIL: %s\n", what);
        }
    }

    using Engine = m_tetris2::TetrisEngine<rule_srs::TetrisRule,
                                          ai_farteryhr::AI,
                                          path::Search>;
    using Spec = rule_srs::TetrisRule::rule_spec;

    void farter_runs_with_zero_eval()
    {
        Engine engine;
        expect(engine.prepare(Spec::width, Spec::height), "farter engine prepare");

        engine.provider_counters().enabled = true;
        engine.provider_counters().reset();

        typename Engine::map_t map{};
        auto spawn = m_tetris2::TetrisBlockStatus('T',
                                                  static_cast<int8_t>(Spec::spawn('T', Spec::width, Spec::height).first),
                                                  static_cast<int8_t>(Spec::spawn('T', Spec::width, Spec::height).second),
                                                  0);

        auto r0 = engine.run(map, spawn, "ISZL", 4, 400);
        (void)r0;
        // 再换一个 piece/队列再跑一次, 确保多节点多 get 路径都走过.
        auto spawn_o = m_tetris2::TetrisBlockStatus('O',
                                                   static_cast<int8_t>(Spec::spawn('O', Spec::width, Spec::height).first),
                                                   static_cast<int8_t>(Spec::spawn('O', Spec::width, Spec::height).second),
                                                   0);
        auto r1 = engine.run(map, spawn_o, "ZLST", 4, 400);
        (void)r1;

        auto stats = engine.eval_cache_stats();
        std::printf("  farter engine: scored=%zu evals=%zu requests=%zu misses=%zu\n",
                    engine.scored(), stats.evals, stats.requests, stats.misses);

        expect(engine.scored() > 0, "farter get 真实跑过 (scored>0)");
        expect(stats.evals == 0, "farter 整次搜索 evals==0 (GetArgs 无 Eval)");

        auto const &pc = engine.provider_counters();
        expect(pc.eval == 0, "provider_counters.eval==0");
        expect(pc.map_before > 0, "provider_counters.map_before>0");
        expect(pc.landing > 0, "provider_counters.landing>0");
        expect(pc.parent > 0, "provider_counters.parent>0");
        expect(pc.depth > 0, "provider_counters.depth>0");
        // farter 不声明这些标签, 计数必须为 0.
        expect(pc.clear == 0 && pc.env == 0 && pc.next_node == 0 &&
               pc.hold == 0 && pc.next_view == 0 && pc.next_length == 0,
               "farter 未声明标签 provider 计数==0");
    }
}

int main()
{
    std::printf("# framework_farter_engine_smoke: farter zero-eval real engine run\n");
    farter_runs_with_zero_eval();
    if (g_failures == 0)
    {
        std::printf("framework_farter_engine_smoke: OK\n");
        return 0;
    }
    std::printf("framework_farter_engine_smoke: %d FAILURES\n", g_failures);
    return 1;
}
