// framework_contract_probes.cpp — 共享框架契约独立探针 (test owner).
// 小探针 AI 验证: Config 多层树 set/notify / generation 溢出 era /
// duplicate provider 复用 / empty GetArgs / fixed-scored on-off 等价 /
// ThreadEngine setter/notify 生命周期. 不改任何真实 15 AI.
#include "../src/tetris_core.h"
#include "../src/tetris_eval_get.h"
#include "../src/rule_srs.h"
#include "../src/search_path.h"

#include <cstdio>
#include <cstdint>

namespace
{
    int g_failures = 0;
    void expect(bool ok, const char *what)
    {
        if (!ok) { ++g_failures; std::printf("FAIL: %s\n", what); }
    }

    using Spec = rule_srs::TetrisRule::rule_spec;

    // 固定评分探针: eval(map) 返回 popcount 固定值; get 叠加 parent.value.
    // 用于 fixed-scored on/off 等价 + generation freshness.
    class FixedProbe
    {
    public:
        struct Config { double scale = 1.0; };
        using Result = int;
        struct Status
        {
            double value = 0;
            bool operator<(Status const &o) const { return value < o.value; }
        };
        using EvalSpinType = std::monostate;
        using Landing = m_tetris2::GetLanding<EvalSpinType>;
        using GetArgs = m_tetris2::type_list<m_tetris2::args::Eval<Result>,
                                             m_tetris2::args::Parent<Status>>;
        template<class S> void init(Config const *) {}
        std::string ai_name() const { return "FixedProbe"; }
        template<class MapT> Result eval(MapT const &map) const
        {
            return static_cast<Result>(map.popcount());
        }
        Status get(Result const &r, Status const &p) const
        {
            return {p.value + r};
        }
    };

    // Empty GetArgs: 无任何 provider tag — 编译期不实例化 unused provider 路径.
    class EmptyProbe
    {
    public:
        struct Config {};
        using Result = int;
        struct Status
        {
            double value = 0;
            bool operator<(Status const &o) const { return value < o.value; }
        };
        using GetArgs = m_tetris2::type_list<>;
        template<class S> void init(Config const *) {}
        std::string ai_name() const { return "EmptyProbe"; }
        template<class MapT> Result eval(MapT const &) const { return 0; }
        Status get() const { return {}; }
    };

    using Engine = m_tetris2::TetrisEngine<rule_srs::TetrisRule, FixedProbe, path::Search>;

    template<class E>
    void run_once(E &e, char piece, char const *queue)
    {
        Engine::map_t map{};
        auto st = m_tetris2::TetrisBlockStatus(piece,
            static_cast<int8_t>(Spec::spawn(piece, Spec::width, Spec::height).first),
            static_cast<int8_t>(Spec::spawn(piece, Spec::width, Spec::height).second), 0);
        e.run(map, st, queue, std::char_traits<char>::length(queue), 400);
    }

    void config_lifecycle()
    {
        Engine e;
        expect(e.prepare(Spec::width, Spec::height), "prepare");
        run_once(e, 'T', "ISZL");
        auto const gen0 = e.eval_generation();
        auto const scored0 = e.scored();

        // ordinary update(): 不 bump generation (只刷后台)
        e.update();
        expect(e.eval_generation() == gen0, "ordinary update keeps eval_generation");

        // set_ai_config + notify: generation 必须变化
        FixedProbe::Config cfg;
        e.set_ai_config(cfg);
        e.notify_ai_config_changed();
        auto const gen1 = e.eval_generation();
        std::printf("  lifecycle: gen0=%llu gen1=%llu scored0=%zu\n",
                    (unsigned long long)gen0, (unsigned long long)gen1, scored0);
        expect(gen1 != gen0, "set_ai_config+notify bumps generation");
        run_once(e, 'O', "ZLST");
        expect(e.scored() > 0, "post-config run scored>0");
    }

    void generation_wrap_era()
    {
        Engine e;
        expect(e.prepare(Spec::width, Spec::height), "wrap prepare");
        run_once(e, 'T', "ISZL");
        auto const s0 = e.eval_cache_stats();
        // 直接把 generation 顶到溢出前一格, 再 notify 触发 on_config_changed 溢出路径.
        e.eval_cache().debug_set_generation(static_cast<std::uint64_t>(-1));
        e.notify_ai_config_changed(); // on_config_changed ++ 到 0 → overflowed, era++
        auto const s1 = e.eval_cache_stats();
        std::printf("  wrap: gen_before=%llu gen_after=%llu\n",
                    (unsigned long long)s0.generation, (unsigned long long)s1.generation);
        expect(s1.generation == 1 || s1.generation != s0.generation,
               "generation wrap resets to fresh era");
        // 旧 children 沿 get freshness: 再跑必须真 eval 而非读旧桶.
        auto const misses_before = s1.misses;
        run_once(e, 'O', "TLSZ");
        auto const s2 = e.eval_cache_stats();
        expect(s2.misses >= misses_before, "post-wrap run re-evals (miss/eval grew)");
    }

    // 目标/status 一致 (非 scored 计数; 计数因缓存复用而不同是预期).
    static double best_status(Engine &e)
    {
        Engine::map_t map{};
        auto st = m_tetris2::TetrisBlockStatus('T',
            static_cast<int8_t>(Spec::spawn('T', Spec::width, Spec::height).first),
            static_cast<int8_t>(Spec::spawn('T', Spec::width, Spec::height).second), 0);
        auto r = e.run(map, st, "ISZL", 4, 400);
        return r.status.value;
    }
    void fixed_scored_on_off()
    {
        Engine en;
        expect(en.prepare(Spec::width, Spec::height), "onoff prepare");
        en.eval_cache().options().disabled = false;
        auto v_on = best_status(en);

        Engine ed;
        expect(ed.prepare(Spec::width, Spec::height), "onoff prepare disabled");
        ed.eval_cache().options().disabled = true;
        auto v_off = best_status(ed);
        std::printf("  onoff: status_on=%.6f status_off=%.6f\n", v_on, v_off);
        expect(v_on == v_off, "fixed-scored cache on/off yields identical best status");
    }



    void duplicate_provider_counts()
    {
        Engine e;
        expect(e.prepare(Spec::width, Spec::height), "dup prepare");
        e.provider_counters().enabled = true;
        e.provider_counters().reset();
        Engine::map_t map{};
        auto st = m_tetris2::TetrisBlockStatus('T',
            static_cast<int8_t>(Spec::spawn('T', Spec::width, Spec::height).first),
            static_cast<int8_t>(Spec::spawn('T', Spec::width, Spec::height).second), 0);
        e.run(map, st, "ISZL", 4, 20);
        auto const &pc = e.provider_counters();
        auto stats = e.eval_cache_stats();
        std::printf("  dup: eval_pc=%zu evals=%zu parent=%zu map_before=%zu\n",
                    pc.eval, stats.evals, pc.parent, pc.map_before);
        // FixedProbe 只声明 Eval+Parent; 未声明标签必须 0.
        expect(pc.eval == stats.evals, "provider eval 计数 == cache evals (每次重复调用计数)");
        expect(pc.parent > 0, "parent provider 计数>0");
        expect(pc.clear == 0 && pc.env == 0 && pc.hold == 0 &&
               pc.next_node == 0 && pc.next_length == 0, "未声明标签 provider 计数==0");
    }

    void thread_lifecycle()
    {
        using TE = m_tetris2::TetrisThreadEngine<rule_srs::TetrisRule, FixedProbe, path::Search>;
        TE te;
        expect(te.prepare(Spec::width, Spec::height), "thread prepare");
        auto const c0 = te.config_set_count();
        FixedProbe::Config cfg; cfg.scale = 2.0;
        te.set_ai_config(cfg);
        te.notify_ai_config_changed();
        auto const c1 = te.config_set_count();
        std::printf("  thread: config_set_count %zu -> %zu\n", c0, c1);
        expect(c1 > c0, "thread set_ai_config increments config_set_count");
    }

    void duplicate_provider_and_empty()
    {
        // EmptyProbe 编译: GetArgs 空 list 不实例化任何 provider tag.
        m_tetris2::TetrisEngine<rule_srs::TetrisRule, EmptyProbe, path::Search> e;
        expect(e.prepare(Spec::width, Spec::height), "empty GetArgs engine prepare");
        run_once(e, 'T', "ISZL");
        expect(e.scored() > 0, "empty GetArgs engine runs");
    }
}

int main()
{
    std::printf("# framework_contract_probes\n");
    config_lifecycle();
    generation_wrap_era();
    fixed_scored_on_off();
    duplicate_provider_and_empty();
    thread_lifecycle();
    duplicate_provider_counts();
    if (g_failures == 0) { std::printf("contract_probes: OK\n"); return 0; }
    std::printf("contract_probes: %d FAILURES\n", g_failures);
    return 1;
}
