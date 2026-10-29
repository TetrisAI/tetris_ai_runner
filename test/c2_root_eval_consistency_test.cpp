// c2_root_eval_consistency_test.cpp — root eval_slot lazy-holder lifecycle.
// The root Status is written by the host and root is never scored via args::Eval, so the engine must
// NOT force ai.eval on the raw/empty root board. Instead:
//   - a recycled free_list node (same ptr, foreign map) promoted to root is marked UNEVALUATED
//     (eval_gen=0); its stale foreign-map result must not be advertised as valid;
//   - a sameMap early-return whose holder generation/era is stale is likewise invalidated;
//   - the real consumer (GetArgs args::Eval) resolves on demand by full map + current Config.
// Returns non-zero on any stale/foreign result left advertised as valid.
#include "tetris_core.h"
#include "tetris_map.h"
#include "rule_c2.h"
#include "search_cautious.h"
#include "rule_srs.h"
#include "search_path.h"
#include "ai_zzz.h"

#include <cstdio>
#include <cstdint>
#include <cstring>

namespace
{
    int g_fail = 0;
    void check(bool ok, const char *what)
    {
        if (ok) std::printf("  PASS: %s\n", what);
        else { ++g_fail; std::printf("  FAIL: %s\n", what); }
    }
    std::uint64_t hexd(double v) { std::uint64_t u; std::memcpy(&u, &v, 8); return u; }

    using E = m_tetris2::TetrisEngine<rule_c2::TetrisRule, ai_zzz::C2, cautious::Search>;
    using TreeNode = E::TreeNode;
    using Context = TreeNode::Context;
    using MapT = E::map_t;
    using Spec = rule_c2::TetrisRule::rule_spec;

    ai_zzz::C2::Config make_cfg()
    {
        ai_zzz::C2::Config c{};
        double w[24] = {2.87224,0.372169,0.102604,0.723501,3.08721,0.802789,
            -0.786174,107.713,-0.540719,109.116,-3.84305,116.58,-1.00066,49.7899,
            -1.23986,391.808,-4.30493,91.0623,-1.60608,67.7934,2.36365,46016.9,34.2515,0.285739};
        for (int i = 0; i < 24; ++i) c.p[i] = w[i];
        c.p_rate = 1.0; c.iterate_low = 4; c.iterate_high = 7;
        return c;
    }

    // Replicate the consumer (args::Eval) resolution: on stale/unevaluated, resolve by full map.
    void consumer_resolve(Context *ctx, TreeNode *n)
    {
        if (n->eval_slot.eval_gen != ctx->eval_cache.generation() ||
            n->eval_slot.eval_era != ctx->eval_era)
        {
            n->eval_slot.result = ctx->eval_cache.lookup(*ctx->ai, n->map);
            n->eval_slot.eval_gen = ctx->eval_cache.generation();
            n->eval_slot.eval_era = ctx->eval_era;
        }
    }

    struct NoEvalAI
    {
        struct Config { double weight = 1.0; };
        struct Result { double value; int cells; };
        struct Status {
            double accum; double value; int count; bool root_marker;
            bool operator<(Status const &o) const { return value < o.value; }
        };
        using Landing = m_tetris2::GetLanding<std::monostate>;
        using GetArgs = m_tetris2::type_list<
            m_tetris2::args::Clear, m_tetris2::args::Parent<Status>,
            m_tetris2::args::Landing<Landing>, m_tetris2::args::NextNode,
            m_tetris2::args::NextLength>;
        Config const *config_ = nullptr;
        int eval_calls = 0;
        template<class Spec> void init(Config const *c) { config_ = c; }
        template<class MapT> Result eval(MapT const &map) const {
            ++const_cast<NoEvalAI *>(this)->eval_calls;
            return {double(map.popcount()), map.popcount()};
        }
        Status get(int, Status const &parent, Landing const &, char, size_t next_len) const {
            Status s; s.accum = parent.accum; s.value = 0; s.count = int(next_len); s.root_marker = false; return s;
        }
        Status iterate(Status const **, size_t) const { return {0.0, 0.0, 0, false}; }
    };
}

int main()
{
    std::printf("# c2_root_eval_consistency_test\n");
    ai_zzz::C2::Config cfg = make_cfg();
    ai_zzz::C2 ai; ai.init<Spec>(&cfg);

    std::deque<TreeNode> storage;
    Context ctx(&storage);
    ctx.ai = &ai;
    ctx.eval_cache.prepare(1024);
    const std::uint64_t gen = ctx.eval_cache.generation();
    const std::uint64_t era = ctx.eval_era;

    MapT mapA; for (int y = 0; y <= 3; ++y) for (int x = 0; x <= 8; ++x) mapA.set(x, y); // pop 36
    MapT mapB; for (int y = 0; y <= 2; ++y) for (int x = 0; x <= 7; ++x) mapB.set(x, y); // pop 24

    // ---- (A) recycled free_list node (same ptr, foreign map) promoted to root ----
    TreeNode *old_root = ctx.alloc(nullptr);
    old_root->map = mapA;
    old_root->status.set(ai_zzz::C2::Status{0.0, 0.0, 3, 10.0, 0.0});
    TreeNode *nodeA = ctx.alloc(old_root);
    nodeA->map = mapA;
    nodeA->eval_slot.result = ai.eval(mapA);
    nodeA->eval_slot.eval_gen = gen;
    nodeA->eval_slot.eval_era = era;

    nodeA->parent = nullptr;
    ctx.dealloc(nodeA);
    ctx.reclaim(100);

    TreeNode *new_root = old_root->update_root(&ctx, mapB);
    check((void *)new_root == (void *)nodeA, "recycled free_list nodeA becomes new_root (same ptr)");
    // The foreign-map holder must NOT be advertised as valid (old gen/era match is not proof):
    // it is reset to the unevaluated default (gen0 + result cleared), not left with stale mapA bits.
    check(new_root->eval_slot.eval_gen == 0 && new_root->eval_slot.result.cells == 0 &&
              hexd(new_root->eval_slot.result.map) == hexd(0.0),
          "recycled foreign-map root holder is reset to unevaluated (gen0, result cleared)");
    // On-demand consumer resolution must produce exactly eval(mapB), not eval(mapA).
    consumer_resolve(&ctx, new_root);
    auto expectB = ai.eval(mapB);
    check(new_root->eval_slot.eval_gen == gen && new_root->eval_slot.eval_era == era,
          "after consumer resolve root holder gen/era == current");
    check(new_root->eval_slot.result.cells == expectB.cells &&
              hexd(new_root->eval_slot.result.attack) == hexd(expectB.attack) &&
              hexd(new_root->eval_slot.result.map) == hexd(expectB.map) &&
              hexd(new_root->eval_slot.result.fill) == hexd(expectB.fill) &&
              new_root->eval_slot.result.hole_count == expectB.hole_count,
          "consumer-resolved root slot bits == ai.eval(mapB) (not stale eval(mapA))");

    // ---- (B) sameMap early-return with stale eval_gen ----
    {
        TreeNode *r = ctx.alloc(nullptr);
        r->map = mapB;
        r->status.set(ai_zzz::C2::Status{1.0, 2.0, 4, 11.0, 0.0});
        auto host_before = r->status.get_raw();
        r->eval_slot.result = ai.eval(mapB);
        r->eval_slot.eval_gen = gen + 100;   // stale future generation
        r->eval_slot.eval_era = era;
        TreeNode *ret = r->update_root(&ctx, mapB);
        check(ret == r, "early-return returns the same root node");
        check(r->eval_slot.eval_gen == 0, "stale eval_gen holder invalidated (gen0) on early return");
        auto host_after = r->status.get_raw();
        check(std::memcmp(&host_before, &host_after, sizeof(host_before)) == 0,
              "root host Status untouched by root eval invalidation");
        consumer_resolve(&ctx, r);
        check(r->eval_slot.eval_gen == gen && r->eval_slot.result.cells == ai.eval(mapB).cells,
              "early-return stale holder re-resolves correctly on consumer demand");
    }

    // ---- (C) sameMap early-return with stale eval_era ----
    {
        TreeNode *r = ctx.alloc(nullptr);
        r->map = mapB;
        r->status.set(ai_zzz::C2::Status{3.0, 4.0, 5, 12.0, 0.0});
        auto host_before = r->status.get_raw();
        r->eval_slot.result = ai.eval(mapB);
        r->eval_slot.eval_gen = gen;
        r->eval_slot.eval_era = era + 1;     // stale future era
        r->update_root(&ctx, mapB);
        check(r->eval_slot.eval_gen == 0, "stale eval_era holder invalidated (gen0) on early return");
        auto host_after = r->status.get_raw();
        check(std::memcmp(&host_before, &host_after, sizeof(host_before)) == 0,
              "root host Status untouched by root eval invalidation (era case)");
    }

    // ---- (D) no-Eval-provider AI: root establishment must produce zero eval() calls ----
    {
        using SE = m_tetris2::TetrisEngine<rule_srs::TetrisRule, NoEvalAI, path::Search>;
        using STreeNode = SE::TreeNode;
        using SContext = STreeNode::Context;
        using SMap = SE::map_t;
        static_assert(!m_tetris2::GetTraits<NoEvalAI>::kWantEval, "fixture: NoEvalAI must not request Eval");

        NoEvalAI::Config scfg;
        NoEvalAI s_ai; s_ai.init<rule_srs::TetrisRule::rule_spec>(&scfg);
        std::deque<STreeNode> st;
        SContext sctx(&st);
        sctx.ai = &s_ai;
        sctx.eval_cache.prepare(16);
        STreeNode *root = sctx.alloc(nullptr);
        SMap m{}; for (int x = 0; x <= 5; ++x) m.set(x, 0);
        root->map = m;
        root->update_root(&sctx, m);
        root->update_root(&sctx, m);
        check(s_ai.eval_calls == 0, "no-Eval AI: root establishment produces no eval() call");
    }

    std::printf("# c2_root_eval_consistency: failures=%d\n", g_fail);
    return g_fail == 0 ? 0 : 1;
}
