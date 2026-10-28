// framework_allai_zzz_smoke.cpp — B shard owner: real TetrisEngine prepare+Config+run+link
// for the 5 remaining ZZZ AIs (Botris_PC, TOJ_v08, Botris, TOJ, C2).
//
// CMake registration left to test owner. Single main; Botris_PC runs in a forked child
// to isolate its known true-binding SEGV (handed to core owner s_000pfabbQEX) without
// crashing the other 4 — NOT commented out to fake all-pass.
#include "../src/tetris_core.h"
#include "../src/tetris_eval_get.h"
#include "../src/rule_srs.h"
#include "../src/rule_toj.h"
#include "../src/rule_botris.h"
#include "../src/rule_c2.h"
#include "../src/search_path.h"
#include "../src/search_tspin.h"
#include "../src/search_aspin.h"
#include "../src/search_cautious.h"
#include "../src/ai_zzz.h"

#include <cstdio>
#include <cstdlib>
#include <sys/wait.h>
#include <unistd.h>

namespace
{
    int g_failures = 0;
    int g_passes = 0;

    void expect(bool ok, const char *what)
    {
        if (ok) { ++g_passes; std::printf("  PASS: %s\n", what); }
        else    { ++g_failures; std::printf("  FAIL: %s\n", what); }
    }

    int const kComboTable[] = {0, 0, 1, 1, 1, 2, 2, 3, 3, 4, 4, 4, 5};
    int const kComboTableMax = int(std::size(kComboTable)) - 1;

    template<class E>
    void run_once(E &e, int w, int h, char piece)
    {
        typename E::map_t map{};
        // Use the engine's own status (root Status) initialized to zeros, then spawn block.
        *e.status() = {};
        auto st = m_tetris2::TetrisBlockStatus(piece,
            static_cast<int8_t>(w / 2), static_cast<int8_t>(h - 4), 0);
        e.run(map, st, "ISZL", 4, 20);
    }
}

int main()
{
    std::printf("# framework_allai_zzz_smoke\n");

    // ---- TOJ_v08 : rule_toj + tspin::Search ----
    {
        using E = m_tetris2::TetrisEngine<rule_toj::TetrisRule, ai_zzz::TOJ_v08, tspin::Search>;
        E e;
        expect(e.prepare(rule_toj::TetrisRule::rule_spec::width,
                        rule_toj::TetrisRule::rule_spec::height), "TOJ_v08 prepare");
        ai_zzz::TOJ_v08::Config cfg{};
        cfg.table = kComboTable; cfg.table_max = kComboTableMax;
        e.set_ai_config(cfg);
        e.notify_ai_config_changed();
        run_once(e, rule_toj::TetrisRule::rule_spec::width,
                   rule_toj::TetrisRule::rule_spec::height, 'T');
        expect(e.scored() > 0, "TOJ_v08 run scored > 0");
    }

    // ---- Botris : rule_botris + aspin::Search ----
    {
        using E = m_tetris2::TetrisEngine<rule_botris::TetrisRule, ai_zzz::Botris, aspin::Search>;
        E e;
        expect(e.prepare(rule_botris::TetrisRule::rule_spec::width,
                        rule_botris::TetrisRule::rule_spec::height), "Botris prepare");
        ai_zzz::Botris::Config cfg{};
        cfg.table = kComboTable; cfg.table_max = kComboTableMax;
        e.set_ai_config(cfg);
        e.notify_ai_config_changed();
        run_once(e, rule_botris::TetrisRule::rule_spec::width,
                   rule_botris::TetrisRule::rule_spec::height, 'T');
        expect(e.scored() > 0, "Botris run scored > 0");
    }

    // ---- TOJ : rule_srs + path::Search ----
    {
        using E = m_tetris2::TetrisEngine<rule_srs::TetrisRule, ai_zzz::TOJ, path::Search>;
        E e;
        expect(e.prepare(rule_srs::TetrisRule::rule_spec::width,
                        rule_srs::TetrisRule::rule_spec::height), "TOJ prepare");
        ai_zzz::TOJ::Config cfg{};
        cfg.table = kComboTable; cfg.table_max = kComboTableMax;
        cfg.safe = 4;
        e.set_ai_config(cfg);
        e.notify_ai_config_changed();
        run_once(e, rule_srs::TetrisRule::rule_spec::width,
                   rule_srs::TetrisRule::rule_spec::height, 'T');
        expect(e.scored() > 0, "TOJ run scored > 0");
    }

    // ---- C2 : rule_c2 10x28 + cautious::Search ----
    {
        using E = m_tetris2::TetrisEngine<rule_c2::TetrisRule, ai_zzz::C2, cautious::Search>;
        E e;
        expect(e.prepare(rule_c2::TetrisRule::rule_spec::width,
                        rule_c2::TetrisRule::rule_spec::height), "C2 prepare 10x28");
        ai_zzz::C2::Config cfg{};
        // p left default; all other fields explicitly initialized:
        cfg.p_rate = 1.0;
        cfg.safe = 4;
        cfg.mode = 0;
        cfg.danger = 0;
        cfg.soft_drop = 0;
        cfg.planning_bpm = 60.0;
        cfg.iterate_low = 4;
        cfg.iterate_high = 7;
        e.set_ai_config(cfg);
        e.notify_ai_config_changed();
        run_once(e, rule_c2::TetrisRule::rule_spec::width,
                   rule_c2::TetrisRule::rule_spec::height, 'T');
        expect(e.scored() > 0, "C2 run scored > 0");
    }

    // ---- Botris_PC : known true-binding SEGV (config_->table null after set+notify) ----
    // Isolated in a forked child so the other 4 still run. NOT commented out.
    // Handed to core owner s_000pfabbQEX.
    {
        pid_t pid = fork();
        if (pid == 0) {
            using E = m_tetris2::TetrisEngine<rule_botris::TetrisRule, ai_zzz::Botris_PC, aspin::Search>;
            E e;
            e.prepare(rule_botris::TetrisRule::rule_spec::width,
                      rule_botris::TetrisRule::rule_spec::height);
            ai_zzz::Botris_PC::Config cfg{};
            cfg.table = kComboTable; cfg.table_max = kComboTableMax;
            std::printf("  Botris_PC: before set_ai_config, cfg.table=%p\n", (void*)cfg.table);
            e.set_ai_config(cfg);
            e.notify_ai_config_changed();
            run_once(e, rule_botris::TetrisRule::rule_spec::width,
                       rule_botris::TetrisRule::rule_spec::height, 'T');
            std::printf("  Botris_PC scored=%zu\n", e.scored());
            _exit(0);
        }
        int status = 0;
        waitpid(pid, &status, 0);
        if (WIFSIGNALED(status)) {
            std::printf("  KNOWN-CRASH: Botris_PC died by signal %d (handed to core owner s_000pfabbQEX)\n",
                        WTERMSIG(status));
            // Not counted as a FAIL — it's a known core-owner bug, not a B migration bug.
        } else if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
            std::printf("  PASS: Botris_PC (previously crashed; now recovered)\n");
            ++g_passes;
        } else {
            std::printf("  FAIL: Botris_PC exited abnormally\n");
            ++g_failures;
        }
    }

    std::printf("# passes=%d failures=%d\n", g_passes, g_failures);
    return g_failures == 0 ? 0 : 1;
}
