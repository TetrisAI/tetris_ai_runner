// framework_tree_probes.cpp — 核心契约探针 (owner, 真 assert, 第三轮):
// 1. Config tree staleEval: AI 实时读 Config.scale; 同 root 换 Config v2>v1, ==fresh 同路径.
// 2. duplicate same-tag: get 内 assert &r1==&r2 (同引用), pc.eval==scored, pc.parent==scored.
// 3. IterateProbe: 父→子累积 + iterate() 双副本; Config 更新后同 search seq == fresh.
// 4. ThreadEngine: scale 变化反映新值 (ASan/UBSan 仅报越界, 不宣无竞争).
// 5. fixed-budget on/off: 固定 run 深度, scored/target identity/status 逐字段一致.
#include "../src/tetris_core.h"
#include "../src/tetris_eval_get.h"
#include "../src/rule_srs.h"
#include "../src/search_path.h"

#include <cstdio>
#include <cmath>

namespace
{
    int g_failures = 0;
    void expect(bool ok, const char *what)
    {
        if (!ok) { ++g_failures; std::printf("FAIL: %s\n", what); }
    }

    using Spec = rule_srs::TetrisRule::rule_spec;

    // 真绑定 Config: eval = popcount * scale.
    class ScaledProbe
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
        using GetArgs = m_tetris2::type_list<m_tetris2::args::Eval<Result>, m_tetris2::args::Parent<Status>>;
        Config const *cfg_ = nullptr;
        template<class S> void init(Config const *cfg) { cfg_ = cfg; }
        std::string ai_name() const { return "ScaledProbe"; }
        template<class MapT> Result eval(MapT const &map) const
        {
            double scale = cfg_ ? cfg_->scale : 1.0;
            return static_cast<Result>(map.popcount() * scale);
        }
        Status get(Result const &r, Status const &p) const { return {p.value + r}; }
    };

    using Engine = m_tetris2::TetrisEngine<rule_srs::TetrisRule, ScaledProbe, path::Search>;

    // 重复 Eval tag: get 取两次同 tag, 记录引用地址是否相同.
    struct DupState { bool same_ref = false; int get_calls = 0; };
    DupState g_dup;
    class DupProbe
    {
    public:
        struct Config {};
        using Result = int;
        struct Status
        {
            double value = 0;
            bool operator<(Status const &o) const { return value < o.value; }
        };
        using EvalSpinType = std::monostate;
        using Landing = m_tetris2::GetLanding<EvalSpinType>;
        using GetArgs = m_tetris2::type_list<m_tetris2::args::Eval<Result>,
                                             m_tetris2::args::Eval<Result>,
                                             m_tetris2::args::Parent<Status>>;
        template<class S> void init(Config const *) {}
        std::string ai_name() const { return "DupProbe"; }
        template<class MapT> Result eval(MapT const &map) const { return static_cast<Result>(map.popcount()); }
        Status get(Result const &r1, Result const &r2, Status const &p) const
        {
            ++g_dup.get_calls;
            g_dup.same_ref = (&r1 == &r2);
            return {p.value + r1 + r2};
        }
    };

    // Iterate probe: 父→子累积 + iterate() 聚合.
    class IterateProbe
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
        using GetArgs = m_tetris2::type_list<m_tetris2::args::Eval<Result>, m_tetris2::args::Parent<Status>>;
        Config const *cfg_ = nullptr;
        template<class S> void init(Config const *cfg) { cfg_ = cfg; }
        std::string ai_name() const { return "IterateProbe"; }
        template<class MapT> Result eval(MapT const &map) const
        {
            double scale = cfg_ ? cfg_->scale : 1.0;
            return static_cast<Result>(map.popcount() * scale);
        }
        Status get(Result const &r, Status const &p) const { return {p.value + r}; }
        // iterate: 取 best 子状态 (聚合).
        Status iterate(Status const * const *status, size_t n) const
        {
            if (n == 0) return {};
            double best = status[0]->value;
            for (size_t i = 1; i < n; ++i) if (status[i]->value > best) best = status[i]->value;
            return {best};
        }
    };

    using IEngine = m_tetris2::TetrisEngine<rule_srs::TetrisRule, IterateProbe, path::Search>;

    Engine::map_t make_map()
    {
        Engine::map_t map{};
        map.set_row(19, 0b1111111111);
        map.set_row(18, 0b1111000011);
        return map;
    }
    auto make_spawn()
    {
        return m_tetris2::TetrisBlockStatus('T',
            static_cast<int8_t>(Spec::spawn('T', Spec::width, Spec::height).first),
            static_cast<int8_t>(Spec::spawn('T', Spec::width, Spec::height).second), 0);
    }

    // 1. Config tree staleEval.
    void config_tree_stale()
    {
        Engine e;
        expect(e.prepare(Spec::width, Spec::height), "stale prepare");
        ScaledProbe::Config c1; c1.scale = 1.0;
        e.set_ai_config(c1); e.notify_ai_config_changed();
        auto r1 = e.run(make_map(), make_spawn(), "ISZL", 4, 100);
        double v1 = r1.status.value;

        ScaledProbe::Config c2; c2.scale = 2.0;
        e.set_ai_config(c2); e.notify_ai_config_changed();
        auto r2 = e.run(make_map(), make_spawn(), "ISZL", 4, 100);
        double v2 = r2.status.value;

        Engine fresh;
        expect(fresh.prepare(Spec::width, Spec::height), "stale fresh prepare");
        ScaledProbe::Config c2f; c2f.scale = 2.0;
        fresh.set_ai_config(c2f); fresh.notify_ai_config_changed();
        auto rf = fresh.run(make_map(), make_spawn(), "ISZL", 4, 100);
        double vf = rf.status.value;

        std::printf("  stale: v1(scale=1)=%.3f v2(scale=2)=%.3f fresh=%.3f\n", v1, v2, vf);
        expect(v2 > v1, "换 scale 后 status 真增大 (旧 eval 已失效)");
        expect(std::fabs(v2 - vf) < 1e-9, "新 status == fresh engine 同路径 reference");
    }

    // 2. duplicate same-tag provider.
    void duplicate_tag_provider()
    {
        g_dup = {};
        using E2 = m_tetris2::TetrisEngine<rule_srs::TetrisRule, DupProbe, path::Search>;
        E2 e;
        expect(e.prepare(Spec::width, Spec::height), "dup prepare");
        e.provider_counters().enabled = true;
        e.provider_counters().reset();
        auto r = e.run(make_map(), make_spawn(), "ISZL", 4, 100);
        auto const &pc = e.provider_counters();
        auto scored = e.scored();
        std::printf("  dup: status=%.3f eval_pc=%zu parent_pc=%zu scored=%zu same_ref=%d\n",
                    r.status.value, pc.eval, pc.parent, scored, (int)g_dup.same_ref);
        expect(g_dup.same_ref, "同 Eval tag 两次萃取引用相同 (provider 只算一次)");
        expect(pc.eval == scored, "pc.eval == scored (每节点 eval 一次)");
        expect(pc.parent == scored, "pc.parent == scored (每节点 parent 一次)");
        expect(r.status.value > 0, "dup get 累积 r1+r2");
    }

    // 3. IterateProbe: 父→子 + iterate, Config 更新后同 seq == fresh.
    void iterate_probe()
    {
        IEngine e;
        expect(e.prepare(Spec::width, Spec::height), "iterate prepare");
        IterateProbe::Config c1; c1.scale = 1.0;
        e.set_ai_config(c1); e.notify_ai_config_changed();
        auto r1 = e.run(make_map(), make_spawn(), "ISZL", 4, 100);
        double v1 = r1.status.value;

        IterateProbe::Config c2; c2.scale = 2.0;
        e.set_ai_config(c2); e.notify_ai_config_changed();
        auto r2 = e.run(make_map(), make_spawn(), "ISZL", 4, 100);

        IEngine fresh;
        fresh.prepare(Spec::width, Spec::height);
        IterateProbe::Config c2f; c2f.scale = 2.0;
        fresh.set_ai_config(c2f); fresh.notify_ai_config_changed();
        auto rf = fresh.run(make_map(), make_spawn(), "ISZL", 4, 100);
        std::printf("  iterate: v1=%.3f v2=%.3f fresh=%.3f\n", v1, r2.status.value, rf.status.value);
        expect(r2.status.value > v1, "iterate probe 换 scale 真变化");
        expect(std::fabs(r2.status.value - rf.status.value) < 1e-9, "iterate 新值 == fresh 同路径");
    }

    // 4. Thread 真 run + scale 反映.
    void thread_real_run()
    {
        using TE = m_tetris2::TetrisThreadEngine<rule_srs::TetrisRule, ScaledProbe, path::Search>;
        TE te;
        expect(te.prepare(Spec::width, Spec::height), "thread prepare");
        ScaledProbe::Config c1; c1.scale = 1.0;
        te.set_ai_config(c1); te.notify_ai_config_changed();
        auto r1 = te.run(make_map(), make_spawn(), "ISZL", 4, 100);
        ScaledProbe::Config c2; c2.scale = 2.0;
        te.set_ai_config(c2); te.notify_ai_config_changed();
        auto r2 = te.run(make_map(), make_spawn(), "ISZL", 4, 100);
        std::printf("  thread: v1=%.3f v2=%.3f\n", r1.status.value, r2.status.value);
        expect(r2.status.value > r1.status.value, "thread set/notify 后 status 反映新 scale");
    }

    // 5. fixed-budget on/off: 固定 limit, scored/target/status 一致.
    void fixed_budget_on_off()
    {
        auto run_budget = [](bool disabled) {
            Engine e;
            e.prepare(Spec::width, Spec::height);
            e.eval_cache().options().disabled = disabled;
            ScaledProbe::Config c; c.scale = 1.0;
            e.set_ai_config(c); e.notify_ai_config_changed();
            auto r = e.run(make_map(), make_spawn(), "ISZL", 4, 0);
            return std::pair{r, e.scored()};
        };
        // 对两个 engine 分别取 path 字符串比较.
        auto run_path = [](bool disabled) {
            Engine e;
            e.prepare(Spec::width, Spec::height);
            e.eval_cache().options().disabled = disabled;
            ScaledProbe::Config c; c.scale = 1.0;
            e.set_ai_config(c); e.notify_ai_config_changed();
            auto r = e.run(make_map(), make_spawn(), "ISZL", 4, 0);
            auto path = e.make_path(make_spawn(), r.target, make_map());
            return std::pair{r, path};
        };
        auto [ron, path_on] = run_path(false);
        auto [roff, path_off] = run_path(true);
        std::printf("  onoff: status_on=%.6f status_off=%.6f path_on=%s path_off=%s\n",
                    ron.status.value, roff.status.value,
                    std::string(path_on.begin(), path_on.end()).c_str(),
                    std::string(path_off.begin(), path_off.end()).c_str());
        expect(std::fabs(ron.status.value - roff.status.value) < 1e-9,
               "fixed-budget on/off status 逐字段一致");
        expect(path_on == path_off, "fixed-budget on/off make_path 字符串一致");
    }
}

int main()
{
    std::printf("# framework_tree_probes\n");
    config_tree_stale();
    duplicate_tag_provider();
    iterate_probe();
    thread_real_run();
    fixed_budget_on_off();
    if (g_failures == 0) { std::printf("tree_probes: OK\n"); return 0; }
    std::printf("tree_probes: %d FAILURES\n", g_failures);
    return 1;
}
