// Regression: an explicit Config change must invalidate the whole searched tree, not just bump the
// eval-cache generation. A generation bump only refreshes nodes the next search re-visits; nodes built
// under the old Config that the beam does not re-reach keep a stale eval_slot copy (and derived raw
// Status / iterate result) that still feeds root child selection. This test runs the real production C2
// weights (rule_c2 / cautious Search, 10x28, iterate[4,7]) on a reused tree after cfgA->cfgB (p[21]
// 46016.9 -> 1000) and requires:
//   1. reused tree after cfgB selects the exact same move, bit-for-bit, as a fresh cfgB engine;
//   2. every reachable non-root node carries an eval_slot.eval_gen equal to the current generation;
//   3. the ROOT shell owns an eval_slot but under the CURRENT lazy-invalidation contract a stale root
//      slot is left UNEVALUATED on purpose: update_root() marks it eval_gen=0 (and on the
//      free-list-recapture path drops the foreign-map copy to eval_gen=0) instead of re-looking the
//      cache or force-evaluating the raw board — forcing would hit partial-AI empty-board UB and
//      double-count provider evals. The real consumer (GetArgs args::Eval operator()) resolves by
//      full map + current Config on demand. Asserted here: root map == the board just run, root slot
//      eval_gen == 0 (deliberately unevaluated, NOT refreshed to current generation, NOT read back as
//      a valid Result), and the reused tree still selects the exact same move as a fresh cfgB engine
//      (assert 1) — i.e. lazy invalidation does not break selection. The old-valid-map-misuse negative
//      control (recaptured slot with stale map) is covered by rootwriter's formal CMake regressions,
//      not re-asserted here.
// On the pre-fix lazy-bump behaviour this fails: the reused tree picks the cfgA-frozen child and tens of
// thousands of nodes keep eval_gen == previous. NOTE: the separate same-Config free-list-recapture hole
// (recaptured node keeps current-gen slot with a stale map) is out of scope here; this test only covers
// explicit Config-change coverage and does not probe that bug.
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "tetris_core.h"
#include "tetris_map.h"
#include "rule_c2.h"
#include "search_cautious.h"
#include "ai_zzz.h"

using E = m_tetris2::TetrisEngine<rule_c2::TetrisRule, ai_zzz::C2, cautious::Search>;
using Spec = rule_c2::TetrisRule::rule_spec;
using MapT = E::map_t;

static int failures = 0;
static void expect(bool ok, const char *msg)
{
    if (!ok)
    {
        ++failures;
        std::printf("FAIL: %s\n", msg);
    }
}

static ai_zzz::C2::Config make_cfg(double p21)
{
    ai_zzz::C2::Config c{};
    double w[24] = {
        2.87224, 0.372169, 0.102604, 0.723501, 3.08721, 0.802789,
        -0.786174, 107.713, -0.540719, 109.116, -3.84305, 116.58,
        -1.00066, 49.7899, -1.23986, 391.808, -4.30493, 91.0623,
        -1.60608, 67.7934, 2.36365, p21, 34.2515, 0.285739,
    };
    for (int i = 0; i < 24; ++i) c.p[i] = w[i];
    c.p_rate = 1.0;
    c.iterate_low = 4;
    c.iterate_high = 7;
    // Match the host production installed Config: planning_bpm=60.0; safe/mode/danger/soft_drop all 0.
    c.planning_bpm = 60.0;
    c.safe = 0;
    c.mode = 0;
    c.danger = 0;
    c.soft_drop = 0;
    return c;
}

static MapT well_board()
{
    MapT m;
    for (int y = 0; y <= 3; ++y)
        for (int x = 0; x <= 8; ++x) m.set(x, y);
    return m;
}

static void reset_root(E &e)
{
    auto *r = e.status();
    r->combo = 3;
    r->combo_limit = 10.0;
    r->attack = 0;
    r->map = 0;
    r->value = 0;
}

static std::uint64_t bits(double v)
{
    std::uint64_t u;
    std::memcpy(&u, &v, 8);
    return u;
}

int main()
{
    auto board = well_board();
    auto sp = Spec::spawn('I', Spec::width, Spec::height);
    auto st = m_tetris2::TetrisBlockStatus('I', (int8_t)sp.first, (int8_t)sp.second, 0);

    // Reused: cfgA run, then explicit set_ai_config(cfgB), then run on the same engine.
    E reused;
    reused.eval_cache().options().bucket_count = 1;
    reused.prepare(Spec::width, Spec::height);
    reused.provider_counters().enabled = true;   // count real/virtual gets to catch an empty-tree false pass
    reused.set_ai_config(make_cfg(46016.9));
    reset_root(reused);
    reused.run(board, st, "?T?", 3, 0);
    reused.set_ai_config(make_cfg(1000.0));
    reset_root(reused);
    auto rb = reused.run(board, st, "?T?", 3, 0);

    // Fresh: a brand-new engine that only ever saw cfgB.
    E fresh;
    fresh.eval_cache().options().bucket_count = 1;
    fresh.prepare(Spec::width, Spec::height);
    fresh.set_ai_config(make_cfg(1000.0));
    reset_root(fresh);
    auto fb = fresh.run(board, st, "?T?", 3, 0);

    expect(rb.target.state.t == fb.target.state.t && rb.target.state.xb == fb.target.state.xb &&
               rb.target.state.yb == fb.target.state.yb && rb.target.state.r == fb.target.state.r,
           "reused-cfgB move equals fresh-cfgB move (target)");
    expect(bits(rb.status.value) == bits(fb.status.value), "reused-cfgB value bits equal fresh-cfgB");
    expect(bits(rb.status.attack) == bits(fb.status.attack), "reused-cfgB attack bits equal fresh-cfgB");
    expect(bits(rb.status.map) == bits(fb.status.map), "reused-cfgB map bits equal fresh-cfgB");
    expect(rb.status.combo == fb.status.combo, "reused-cfgB combo equals fresh-cfgB");
    expect(bits(rb.status.combo_limit) == bits(fb.status.combo_limit),
           "reused-cfgB combo_limit bits equal fresh-cfgB");

    // Every reachable non-root node must carry a current-generation eval: no old-Config copy lingers in
    // the tree to later feed iterate / beam ordering. The root shell (parent == nullptr) is visited too:
    // under the lazy contract its slot is intentionally unevaluated (checked separately below).
    std::size_t stale = 0, reachable = 0;
    std::uint64_t cur = reused.eval_generation();
    E::TreeNode const *root_n = nullptr;
    reused.visit_tree([&](auto const &node, auto const *parent) {
        if (parent == nullptr) { root_n = &node; return; } // root shell: its eval_slot copy is asserted below
        ++reachable;
        if (node.eval_slot.eval_gen != cur) ++stale;
    });
    expect(stale == 0, "no reachable non-root node keeps a stale eval_slot generation after cfg change");

    // Root shell: under the lazy-invalidation contract a stale root slot is deliberately left
    // UNEVALUATED (eval_gen=0) — update_root() must NOT re-look the cache nor force-eval the raw
    // board. What we can positively assert: the root map is still the board just run, and the slot
    // is flagged unevaluated (never a stale-Config Result being read back as valid). The consumer
    // resolves on demand; the user-visible truth that selection is still correct is assert 1.
    expect(root_n != nullptr, "root shell present in visited reused tree");
    if (root_n != nullptr)
    {
        expect(root_n->map == board, "root eval_slot map is the current board just run");
        expect(root_n->eval_slot.eval_gen == 0,
               "root stale eval_slot deliberately left unevaluated (eval_gen==0) after cfgB; no force-eval");
        expect(root_n->eval_slot.eval_era == 0,
               "root invalidated holder resets eval_era==0 alongside gen==0 (whole-holder invalid)");
        expect(bits(root_n->eval_slot.result.attack) == bits(0.0) &&
                   bits(root_n->eval_slot.result.map) == bits(0.0) &&
                   bits(root_n->eval_slot.result.fill) == bits(0.0) &&
                   bits(root_n->eval_slot.result.hole_count) == bits(0.0) &&
                   root_n->eval_slot.result.cells == 0,
               "root invalidated holder result=={} default placeholders; no stale foreign-map Result readable");
    }
    // Consumer-side resolution contract: the search already resolved a reachable non-root node's
    // eval_slot under the NEW Config. Re-run the real consumer path (eval_cache.lookup on the same
    // map via the live ai) and require the resolved 5 fields to equal the slot the search trusted
    // bit-for-bit — i.e. consumer resolution is map-driven and self-consistent. (The root shell stays
    // deliberately unevaluated above; nothing demands its Result during root child selection.)
    E::TreeNode const *sample = nullptr;
    reused.visit_tree([&](auto const &node, auto const *parent) {
        if (parent != nullptr && sample == nullptr) sample = &node;
    });
    expect(sample != nullptr, "reachable non-root sample node present for consumer-lookup check");
    if (sample != nullptr)
    {
        auto res = reused.eval_cache().lookup(*reused.ai(), sample->map);
        expect(bits(sample->eval_slot.result.attack) == bits(res.attack),
               "consumer lookup(attack) bits == slot cached under NEW Config");
        expect(bits(sample->eval_slot.result.map) == bits(res.map),
               "consumer lookup(map) bits == slot cached under NEW Config");
        expect(bits(sample->eval_slot.result.fill) == bits(res.fill),
               "consumer lookup(fill) bits == slot cached under NEW Config");
        expect(bits(sample->eval_slot.result.hole_count) == bits(res.hole_count),
               "consumer lookup(hole_count) bits == slot cached under NEW Config");
        expect(sample->eval_slot.result.cells == res.cells,
               "consumer lookup(cells) == slot cached under NEW Config");
    }
    // Guard against an empty-tree / zero-get false pass: the run must actually expand nodes and take
    // both a real get<true> and a virtual get<false>, otherwise stale==0 and move-equality are vacuous.
    expect(reachable > 0, "tree actually expanded reachable non-root nodes (reachable>0)");
    expect(reused.provider_counters().real_get > 0, "real get<true> calls > 0");
    expect(reused.provider_counters().virtual_get > 0, "virtual get<false> calls > 0");

    std::printf("%s c2_config_invalidation (reachable=%zu stale=%zu failures=%d)\n",
                failures ? "FAIL" : "PASS", reachable, stale, failures);
    return failures != 0;
}
