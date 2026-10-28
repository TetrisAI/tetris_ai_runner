// framework_negative_test.cpp — Engine/Thread/Cache 负例 (owner, Debug).
#include "../src/tetris_core.h"
#include "../src/tetris_eval_get.h"
#include "../src/rule_srs.h"
#include "../src/search_path.h"
#include <cstdio>
#include <cstdint>
namespace {
    int g_failures = 0;
    void expect(bool ok, const char *what) { if(!ok){++g_failures; std::printf("FAIL: %s\n", what);} }
    using Spec = rule_srs::TetrisRule::rule_spec;
    class Probe {
    public:
        struct Config {};
        using Result = int;
        struct Status { double value=0; bool operator<(Status const&o)const{return value<o.value;} };
        using EvalSpinType = std::monostate;
        using Landing = m_tetris2::GetLanding<EvalSpinType>;
        using GetArgs = m_tetris2::type_list<m_tetris2::args::Eval<Result>, m_tetris2::args::Parent<Status>>;
        template<class S> void init(Config const*) {}
        std::string ai_name() const { return "Probe"; }
        template<class M> Result eval(M const &m) const { return (Result)m.popcount(); }
        Status get(Result const &r, Status const &p) const { return {p.value+r}; }
    };
    using Engine = m_tetris2::TetrisEngine<rule_srs::TetrisRule, Probe, path::Search>;
}
int main() {
    std::printf("# framework_negative_test\n");
    // (a) current_result 未 prepare invalid
    {
        Engine e;
        auto r = e.current_result();
        expect(r.target == Engine::LandPoint{} && r.status.value == 0.0, "(a) current_result 未 prepare invalid");
    }
    // (b) cache prepare 0/非2幂拒绝, prepared=false; 合法64通过
    {
        Engine e;
        expect(!e.eval_cache().prepare(0), "(b) cache.prepare(0) 拒绝");
        expect(!e.eval_cache().prepare(5), "(b) cache.prepare(5) 非2幂拒绝");
        expect(!e.eval_cache().prepared(), "(b) prepare 失败后 prepared=false");
        expect(e.eval_cache().prepare(64), "(b) cache.prepare(64) 合法");
        expect(e.eval_cache().prepared(), "(b) prepare(64) 后 prepared=true");
    }
    // (c) cache prepare SIZE_MAX / 最高 size_t 幂拒绝 (Entry bytes bound)
    {
        Engine e;
        expect(!e.eval_cache().prepare(SIZE_MAX), "(c) cache.prepare(SIZE_MAX) 拒绝");
        expect(!e.eval_cache().prepared(), "(c) SIZE_MAX 后 prepared=false");
    }
    // (d) Engine: 初始 prepared=false, no-arg run graceful false, current_result invalid
    {
        Engine e;
        expect(!e.prepared(), "(d) 初始 prepared=false");
        expect(!e.run(), "(d) 未 prepare no-arg run false (graceful)");
        auto r = e.current_result();
        expect(r.target == Engine::LandPoint{}, "(d) 未 prepare current_result invalid");
    }
    // (e) Engine: 合法 prepare true, wrong dims graceful false
    {
        Engine e;
        expect(e.prepare(Spec::width, Spec::height), "(e) prepare 合法 dims OK");
        expect(e.prepared(), "(e) prepare 后 prepared=true");
        expect(!e.prepare(0, 0), "(e) prepare(0,0) graceful false");
        expect(!e.prepared(), "(e) wrong dims 后 prepared=false");
    }
    // (f) Thread: valid prepare + run, wrong dims graceful, current_result invalid
    {
        using TE = m_tetris2::TetrisThreadEngine<rule_srs::TetrisRule, Probe, path::Search>;
        TE te;
        expect(te.prepare(Spec::width, Spec::height), "(f) Thread prepare 合法 OK");
        expect(!te.prepare(0, 0), "(f) Thread prepare(0,0) graceful false");
        auto r = te.current_result();
        expect(r.target == Engine::LandPoint{}, "(f) Thread current_result invalid");
    }
    std::printf("negative: %d failures\n", g_failures);
    return g_failures ? 1 : 0;
}
