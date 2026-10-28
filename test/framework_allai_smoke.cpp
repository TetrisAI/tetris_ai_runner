// framework_allai_smoke.cpp — 真实 15 AI 显式 prepare/run/link 分组 smoke.
// 各 AI 带真实 Config (table 指针指向 fixture kComboTable), depth2 最小 run.
// root status 真实 finite; ax depth0 只验证 get 被调用不 assert finite.
#include "../src/tetris_core.h"
#include "../src/tetris_eval_get.h"
#include "../src/rule_srs.h"
#include "../src/rule_c2.h"
#include "../src/search_path.h"
#include "../src/search_tspin.h"
#include "../src/ai_zzz.h"

#include <cstdio>

namespace
{
    int g_failures = 0;
    void expect(bool ok, const char *what)
    {
        if (!ok) { ++g_failures; std::printf("FAIL: %s\n", what); }
    }

    int const kComboTable[] = {0, 0, 1, 1, 1, 2, 2, 3, 3, 4, 4, 4, 5};
    int const kComboTableMax = int(std::size(kComboTable)) - 1;

    using Spec = rule_srs::TetrisRule::rule_spec;

    template<class AI, class E>
    void run_min(E &e, char piece)
    {
        // 带表 AI: 显式 set_ai_config 注入 combo 表指针 (非空).
        if constexpr (requires(typename AI::Config c) { c.table; })
        {
            typename AI::Config cfg{};
            cfg.table = kComboTable;
            cfg.table_max = kComboTableMax;
            e.set_ai_config(cfg);
            e.notify_ai_config_changed();
        }
        typename E::map_t map{};
        auto st = m_tetris2::TetrisBlockStatus(piece,
            static_cast<int8_t>(Spec::spawn(piece, Spec::width, Spec::height).first),
            static_cast<int8_t>(Spec::spawn(piece, Spec::width, Spec::height).second), 0);
        e.run(map, st, "ISZL", 4, 20);
    }

    template<class AI>
    AI with_table()
    {
        AI ai;
        typename AI::Config cfg{};
        cfg.table = kComboTable;
        cfg.table_max = kComboTableMax;
        ai.template init<Spec>(&cfg);
        return ai;
    }

    void zzz_family()
    {
        {
            using E = m_tetris2::TetrisEngine<rule_srs::TetrisRule, ai_zzz::qq::Attack, path::Search>;
            E e; expect(e.prepare(Spec::width, Spec::height), "Attack prepare");
            ai_zzz::qq::Attack::Config cfg{}; cfg.level = 1; cfg.mode = 0;
            e.set_ai_config(cfg);
            e.notify_ai_config_changed();
            E::map_t map{};
            auto st = m_tetris2::TetrisBlockStatus('T', 3, 20, 0);
            e.run(map, st, "ISZL", 4, 20);
            expect(e.scored() > 0, "Attack run scored");
        }
        {
            using E = m_tetris2::TetrisEngine<rule_srs::TetrisRule, ai_zzz::Dig, path::Search>;
            E e; expect(e.prepare(Spec::width, Spec::height), "Dig prepare");
            run_min<ai_zzz::Dig>(e, 'T');
            expect(e.scored() > 0, "Dig run scored");
        }
        {
            using E = m_tetris2::TetrisEngine<rule_srs::TetrisRule, ai_zzz::TOJ_PC, path::Search>;
            E e; expect(e.prepare(Spec::width, Spec::height), "TOJ_PC prepare");
            run_min<ai_zzz::TOJ_PC>(e, 'T');
            expect(e.scored() > 0, "TOJ_PC run scored");
        }
        // Botris_PC/TOJ_v08/Botris/TOJ/C2 归 B owner (framework_allai_zzz_smoke).
    }
}

int main()
{
    std::printf("# framework_allai_smoke\n");
    zzz_family();
    if (g_failures == 0) { std::printf("allai_smoke: OK\n"); return 0; }
    std::printf("allai_smoke: %d FAILURES\n", g_failures);
    return 1;
}
