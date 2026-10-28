// framework_failure_api_acceptance_test.cpp — 新失败 API（EvalCache/Engine/Thread）独立真值验收.
//
// 不做标题对拍，直接构造当前 API 的合法/非法/未准备/越界/运行期 guard 路径并断言：
//   a) 未 prepare 的 raw cache try_lookup → false，out sentinel 不变，requests==0；
//   b) prepare(合法) → miss true 且 out==ai.eval 真值；再查 hit true，stats 正确；
//   c) prepare(0 / 5 / SIZE_MAX / 超界最高幂) 拒绝，prepared==false，不分配天量；失败后 try_lookup 仍 false；
//   d) Engine options.bucket_count=5 → prepare false → !prepared，no-arg run/run_hold 优雅 false，
//      current_result invalid；改合法容量 prepare true + 真 run；再 wrong-dims prepare false → !prepared 不复用旧 result；
//   e) Thread prepare good + 真 run 触 worker；wrong-dims prepare false → current_result invalid；作用域 dtor join 不 abort；
//   f/g) NDEBUG 运行期 guard death：未 prepare 调 raw cache.lookup / Engine.run(map) 在子进程 SIGABRT/非0，
//        stderr 含 guard 字串（fork/wait/pipe 收集，Unix）；
//   h) Result 复制抛 std::runtime_error（非 bad_alloc）在 prepare 分配时不被 catch(...) 吞，外部 try/catch 捕到。
// 同文件在 Debug 与 -O2 -DNDEBUG 下分别编译运行（assert 被禁但运行期 guard 仍生效）。

#include "../src/tetris_core.h"
#include "../src/tetris_map.h"
#include "../src/tetris_eval_get.h"
#include "../src/rule_srs.h"
#include "../src/search_path.h"
#include "../src/ai_ax.h"
#include "../src/ai_misaka.h"

#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <new>

#if defined(__unix__) || defined(__APPLE__)
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#endif

namespace
{
    int g_pass = 0, g_fail = 0;
    void check(bool cond, const char *name)
    {
        if (cond) { ++g_pass; std::printf("PASS: %s\n", name); }
        else      { ++g_fail; std::printf("FAIL: %s\n", name); }
    }

    using Map = m_tetris2::TetrisMap<10, 40>;
    using AIE = m_tetris2::EvalCache<Map, ai_ax::AI::Result>;

    // h) 复制期可抛的 Result（非 bad_alloc）。
    struct ThrowingResult
    {
        double v = 0.0;
        static bool throw_on_copy;
        ThrowingResult() = default;
        ThrowingResult(ThrowingResult const &o) { if (throw_on_copy) throw std::runtime_error("ThrowingResult copy"); v = o.v; }
        ThrowingResult &operator=(ThrowingResult const &o) { if (throw_on_copy) throw std::runtime_error("ThrowingResult copy"); v = o.v; return *this; }
    };
    bool ThrowingResult::throw_on_copy = false;

#if defined(__unix__) || defined(__APPLE__)
    // 在子进程跑 fn，回收退出码与 stderr；返回 (exit_signal_or_nonzero, stderr 含 needle)。
    struct Death
    {
        int wstatus;
        std::string stderr_text;
    };
    template<class Fn>
    Death run_death(Fn &&fn)
    {
        int pipefd[2];
        if (pipe(pipefd) != 0) { return {-1, "pipe-failed"}; }
        pid_t pid = fork();
        if (pid < 0) return {-2, "fork-failed"};
        if (pid == 0)
        {
            dup2(pipefd[1], STDERR_FILENO);
            close(pipefd[0]); close(pipefd[1]);
            fn();
            _exit(0); // 不应到达
        }
        close(pipefd[1]);
        char buf[512];
        std::string out;
        ssize_t n;
        while ((n = read(pipefd[0], buf, sizeof(buf))) > 0) out.append(buf, n);
        close(pipefd[0]);
        int st = 0;
        waitpid(pid, &st, 0);
        return {st, out};
    }
#endif
}

int main()
{
    std::printf("# framework_failure_api_acceptance_test\n");

    // ---------- a) 未 prepare：try_lookup false / out 不变 / requests==0 ----------
    {
        AIE cache;
        ai_ax::AI ai;
        Map map{};
        ai_ax::AI::Result out{-123.0, 999.0};
        bool got = cache.try_lookup(ai, map, out);
        check(got == false, "a1 try_lookup unprepared returns false");
        check(out.land_point == -123.0 && out.map == 999.0, "a2 out sentinel untouched when unprepared");
        check(cache.stats().requests == 0, "a3 requests==0 when unprepared");
        check(cache.prepared() == false, "a4 prepared()==false initially");
    }

    // ---------- b) prepare 合法：miss 真值 → hit，stats 正确 ----------
    {
        AIE cache;
        ai_ax::AI ai;
        Map map{};
        // 给 map 一点高度，使 eval 非平凡。
        map.set(0, 0); map.set(1, 0); map.set(2, 0); map.set(3, 0); map.set(4, 0);
        check(cache.prepare(1024), "b1 prepare(1024) true");
        ai_ax::AI::Result out{};
        bool miss = cache.try_lookup(ai, map, out);
        auto expect = ai.eval(map);
        check(miss == true, "b2 try_lookup miss returns true");
        check(out.land_point == expect.land_point && out.map == expect.map, "b3 miss out == ai.eval(map) real");
        ai_ax::AI::Result out2{};
        bool hit = cache.try_lookup(ai, map, out2);
        check(hit == true, "b4 second lookup returns true (hit)");
        check(out2.land_point == expect.land_point && out2.map == expect.map, "b5 hit out matches cached");
        auto st = cache.stats();
        check(st.requests == 2 && st.hits == 1 && st.misses == 1 && st.evals == 1, "b6 stats requests/hit/miss/eval=2/1/1/1");
    }

    // ---------- c) 非法容量拒绝；不分配天量；失败后 try_lookup 仍 false ----------
    {
        AIE cache;
        check(cache.prepare(0) == false, "c1 prepare(0) rejected");
        check(cache.prepared() == false, "c2 after prepare(0) unprepared");
        check(cache.prepare(5) == false, "c3 prepare(5) non-pow2 rejected");
        check(cache.prepared() == false, "c4 after prepare(5) unprepared");
        check(cache.prepare((std::size_t)-1) == false, "c5 prepare(SIZE_MAX) rejected (no huge alloc)");
        // 高于 max_buckets 的最高幂（max_buckets 本身已是 bytes 预算上界）。
        std::size_t hi = AIE::max_buckets();
        // hi 已是允许上界；给 hi 的下一个 2 幂（必越界）。
        std::size_t next_pow2 = hi < ((std::size_t)-1 / 2) ? (hi << 1) : hi;
        if (next_pow2 != hi)
            check(cache.prepare(next_pow2) == false, "c6 prepare(next-pow2 above max_buckets) rejected");
        else
            check(true, "c6 skipped (already at ceiling)");
        check(cache.prepared() == false, "c7 after all failed prepare still unprepared");
        ai_ax::AI ai; Map map{}; ai_ax::AI::Result out{-5.0, 7.0};
        bool got = cache.try_lookup(ai, map, out);
        check(got == false && out.land_point == -5.0, "c8 failed-prepare try_lookup false/out untouched");
    }

    // ---------- d) Engine：bucket_count=5 拒绝 prepare → 优雅 false / invalid ----------
    {
        using Engine = m_tetris2::TetrisEngine<rule_srs::TetrisRule, ai_ax::AI, path::Search>;
        Engine eng;
        eng.eval_cache().options().bucket_count = 5; // 非 2 幂
        bool pr = eng.prepare(10, 40);
        check(pr == false, "d1 Engine prepare with bucket_count=5 returns false");
        check(eng.prepared() == false, "d2 Engine prepared()==false");
        check(eng.run() == false, "d3 no-arg run() graceful false when unprepared");
        check(eng.run_hold() == false, "d4 no-arg run_hold() graceful false when unprepared");
        check(eng.current_result().target == m_tetris2::TetrisLandPoint{}, "d5 current_result invalid when unprepared");

        // 改合法容量，prepare 成功并真 run。
        eng.eval_cache().options().bucket_count = 1024;
        check(eng.prepare(10, 40) == true, "d6 re-prepare with bucket_count=1024 true");
        using Spec = rule_srs::TetrisRule::rule_spec;
        typename Engine::map_t map{};
        auto blk = m_tetris2::TetrisBlockStatus('T',
            (int8_t)Spec::spawn('T', 10, 40).first, (int8_t)Spec::spawn('T', 10, 40).second, 0);
        auto r = eng.run(map, blk, "ISZL", 4, 400);
        check(r.target != m_tetris2::TetrisLandPoint{} || eng.scored() > 0, "d7 actual run produced work after valid prepare");

        // wrong-dims prepare 失败 → 不复用旧 result。
        check(eng.prepare(99, 99) == false, "d8 prepare(99,99) invalid dims returns false");
        check(eng.prepared() == false, "d9 prepared()==false after wrong-dims prepare");
        check(eng.current_result().target == m_tetris2::TetrisLandPoint{}, "d10 current_result invalid, old result not reused");
    }

    // ---------- e) Thread 有 Config AI（misaka）：prepare+run+wrong-dims+dtor ----------
    {
        using MisakaSpec22 = m_tetris2::RuleSpec<10, 22, 4,
            rule_srs::detail::op_O1,
            rule_srs::detail::op_I1, rule_srs::detail::op_I2, rule_srs::detail::op_I3, rule_srs::detail::op_I4,
            rule_srs::detail::op_S1, rule_srs::detail::op_S2, rule_srs::detail::op_S3, rule_srs::detail::op_S4,
            rule_srs::detail::op_Z1, rule_srs::detail::op_Z2, rule_srs::detail::op_Z3, rule_srs::detail::op_Z4,
            rule_srs::detail::op_L1, rule_srs::detail::op_L2, rule_srs::detail::op_L3, rule_srs::detail::op_L4,
            rule_srs::detail::op_J1, rule_srs::detail::op_J2, rule_srs::detail::op_J3, rule_srs::detail::op_J4,
            rule_srs::detail::op_T1, rule_srs::detail::op_T2, rule_srs::detail::op_T3, rule_srs::detail::op_T4>;
        struct MisakaRule { using rule_spec = MisakaSpec22; };
        using ThreadEngine = m_tetris2::TetrisThreadEngine<MisakaRule, ai_misaka::misaka, tspin::Search>;
        auto *eng = new ThreadEngine();
        static int table[8] = {0, 0, 1, 2, 4, 8, 16, 32};
        ai_misaka::misaka::Config mcfg{};
        mcfg.miny_factor = 100; mcfg.hole = 80; mcfg.open_hole = 40; mcfg.v_transitions = 20; mcfg.tspin3 = 60;
        mcfg.clear_efficient = 50; mcfg.upcomeAtt = 25; mcfg.h_factor = 15; mcfg.hole_dis_factor2 = 30;
        mcfg.hole_dis = 10; mcfg.hole_dis_factor = 12; mcfg.tspin = 45; mcfg.hold_t = 70; mcfg.hold_i = 60;
        mcfg.clear_useless_factor = 35; mcfg.dif_factor = 8; mcfg.strategy_4w = 1;
        mcfg.table = table; mcfg.table_max = 8;
        eng->set_ai_config(mcfg);
        check(eng->prepare(10, 22) == true, "e1 Thread prepare(10,22) true");
        using Spec = MisakaSpec22;
        ThreadEngine::map_t map{};
        auto blk = m_tetris2::TetrisBlockStatus('T',
            (int8_t)Spec::spawn('T', 10, 22).first, (int8_t)Spec::spawn('T', 10, 22).second, 0);
        auto r = eng->run(map, blk, "ISZL", 4, 400);
        check(r.target != m_tetris2::TetrisLandPoint{} || eng->current_result().target != m_tetris2::TetrisLandPoint{},
              "e2 Thread run triggered worker / result available");
        check(eng->prepare(99, 99) == false, "e3 Thread prepare wrong dims false");
        check(eng->current_result().target == m_tetris2::TetrisLandPoint{}, "e4 Thread current_result invalid after wrong prepare");
        delete eng; // 作用域 dtor：join worker，不 abort
        check(true, "e5 Thread dtor joined worker without abort");
    }

    // ---------- e2) Thread 无 Config AI（ai_ax）生命周期：prepare/notify/run/wrong-dims/dtor ----------
    {
        using ThreadEngine = m_tetris2::TetrisThreadEngine<rule_srs::TetrisRule, ai_ax::AI, path::Search>;
        using Spec = rule_srs::TetrisRule::rule_spec;
        auto *eng = new ThreadEngine();
        check(eng->prepare(10, 40) == true, "e6 Thread<ai_ax> prepare(10,40) true (no-Config AI)");
        eng->notify_ai_config_changed(); // 无 Config 表无 shim，仍 bump eval generation 不崩
        check(true, "e7 Thread<ai_ax> notify_ai_config_changed() no-crash (no Config shim)");
        ThreadEngine::map_t map{};
        for (int x = 0; x < 10; ++x) map.set(x, 0); // 非空 root：过 ai_ax guard(roof>0)/polarity 路径
        auto blk = m_tetris2::TetrisBlockStatus('T',
            (int8_t)Spec::spawn('T', 10, 40).first, (int8_t)Spec::spawn('T', 10, 40).second, 0);
        auto r = eng->run(map, blk, "ISZL", 4, 400);
        check(r.target != m_tetris2::TetrisLandPoint{}, "e8 Thread<ai_ax> actual run returned valid target");
        check(eng->prepare(99, 99) == false, "e9 Thread<ai_ax> wrong-dims prepare false after run");
        check(eng->current_result().target == m_tetris2::TetrisLandPoint{}, "e10 Thread<ai_ax> current_result invalid after wrong prepare");
        delete eng;
        check(true, "e11 Thread<ai_ax> dtor joined worker safely");
    }

    // ---------- f/g) NDEBUG 运行期 guard death（Unix 子进程） ----------
#if defined(__unix__) || defined(__APPLE__)
    {
        auto d1 = run_death([] {
            AIE cache; ai_ax::AI ai; Map map{};
            cache.lookup(ai, map); // 未 prepare → abort
        });
        bool aborted = WIFSIGNALED(d1.wstatus) && WTERMSIG(d1.wstatus) != 0;
        check(aborted, "f1 raw cache.lookup() before prepare aborts child");
        check(d1.stderr_text.find("lookup before prepare") != std::string::npos, "f2 death stderr contains guard string");
    }
    {
        auto d2 = run_death([] {
            using Engine = m_tetris2::TetrisEngine<rule_srs::TetrisRule, ai_ax::AI, path::Search>;
            Engine eng; // 未 prepare
            Engine::map_t map{};
            eng.run(map, m_tetris2::TetrisBlockStatus('T', 3, 21, 0), "ISZL", 4, 400);
        });
        bool aborted = WIFSIGNALED(d2.wstatus) && WTERMSIG(d2.wstatus) != 0;
        check(aborted, "g1 Engine.run(map,...) before prepare aborts child");
        check(d2.stderr_text.find("run() before successful prepare") != std::string::npos, "g2 death stderr contains guard string");
    }
#else
    check(true, "f death test skipped (non-Unix)");
    check(true, "g death test skipped (non-Unix)");
#endif

    // ---------- h) Result 复制抛 runtime_error 不被吞 ----------
    {
        using ThrowCache = m_tetris2::EvalCache<Map, ThrowingResult>;
        ThrowCache cache;
        ThrowingResult::throw_on_copy = true;
        bool caught_runtime = false;
        bool caught_other = false;
        try
        {
            cache.prepare(1024); // buckets_.assign 复制 Entry → Result 抛 runtime_error
        }
        catch (std::runtime_error const &) { caught_runtime = true; }
        catch (...) { caught_other = true; }
        ThrowingResult::throw_on_copy = false;
        check(caught_runtime, "h1 Result copy runtime_error propagates out of prepare (not swallowed)");
        check(caught_other == false, "h2 unexpected catch-all did not swallow as unknown");
        check(cache.prepared() == false, "h3 prepare left unprepared after copy-throw");
    }

    std::printf("framework_failure_api_acceptance_test: pass=%d fail=%d\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
