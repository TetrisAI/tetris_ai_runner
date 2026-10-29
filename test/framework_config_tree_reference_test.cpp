// Config generation invalidation, raw and virtual aggregation reference checks.
#include "../src/tetris_core.h"
#include "../src/tetris_eval_get.h"
#include "../src/rule_srs.h"
#include "../src/search_path.h"
#include "../src/ai_zzz.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <set>
#include <vector>

namespace
{
    int g_failures = 0, g_passes = 0;
    void expect(bool ok, const char *what)
    {
        if (ok) { ++g_passes; std::printf("  PASS: %s\n", what); }
        else    { ++g_failures; std::printf("  FAIL: %s\n", what); }
    }

    struct ConfigTreeAI
    {
        struct Config { double weight = 1.0; int mode = 0; double clear_bonus = 0.0; };
        struct Result { double value; int cells; };
        struct Status {
            double accum; double value; int count; bool root_marker;
            bool operator<(Status const &o) const { return value < o.value; }
        };
        using Landing = m_tetris2::GetLanding<std::monostate>;
        using GetArgs = m_tetris2::type_list<
            m_tetris2::args::Eval<Result>, m_tetris2::args::Clear,
            m_tetris2::args::Parent<Status>, m_tetris2::args::Landing<Landing>,
            m_tetris2::args::NextNode, m_tetris2::args::NextLength>;

        Config const *config_ = nullptr;
        int get_count = 0, iterate_count = 0;

        template<class Spec> void init(Config const *cfg) { config_ = cfg; get_count = 0; iterate_count = 0; }
        template<class MapT> Result eval(MapT const &map) const
        { int pop = map.popcount(); return {double(pop) * config_->weight, pop}; }
        Status get(Result const &er, int clear, Status const &parent,
                   Landing const &, char, size_t next_len) const
        {
            ++const_cast<ConfigTreeAI*>(this)->get_count;
            Status s;
            s.accum = parent.accum + er.value + double(clear) * config_->clear_bonus;
            s.value = er.value;
            s.count = int(next_len);
            s.root_marker = false;
            return s;
        }
        Status iterate(Status const **status, size_t length) const
        {
            ++const_cast<ConfigTreeAI*>(this)->iterate_count;
            if (length == 0) return {0.0, 0.0, 0, false};
            std::vector<double> vals;
            for (size_t i = 0; i < length; ++i)
                if (status[i] != nullptr) vals.push_back(status[i]->value);
            if (vals.empty()) return {0.0, 0.0, 0, false};
            std::sort(vals.begin(), vals.end());
            size_t lo = vals.size() > 5 ? (vals.size() - 5) / 2 : 0;
            size_t hi = vals.size() > 5 ? lo + 5 : vals.size();
            double sum = 0;
            for (size_t i = lo; i < hi; ++i) sum += vals[i];
            Status s; s.accum = sum; s.value = sum / double(hi - lo); s.count = int(length); s.root_marker = false;
            return s;
        }
        // Independent rank-avg math (does NOT touch ai.counters — no counter pollution).
        static Status rank_avg(Status const *const *status, size_t length)
        {
            if (length == 0) return {0.0, 0.0, 0, false};
            std::vector<double> vals;
            for (size_t i = 0; i < length; ++i)
                if (status[i] != nullptr) vals.push_back(status[i]->value);
            if (vals.empty()) return {0.0, 0.0, 0, false};
            std::sort(vals.begin(), vals.end());
            size_t lo = vals.size() > 5 ? (vals.size() - 5) / 2 : 0;
            size_t hi = vals.size() > 5 ? lo + 5 : vals.size();
            double sum = 0;
            for (size_t i = lo; i < hi; ++i) sum += vals[i];
            Status s; s.accum = sum; s.value = sum / double(hi - lo); s.count = int(length); s.root_marker = false;
            return s;
        }
    };

    using Spec = rule_srs::TetrisRule::rule_spec;
    using Engine = m_tetris2::TetrisEngine<rule_srs::TetrisRule, ConfigTreeAI, path::Search>;
    using MapT = Engine::map_t;

    void run_steps(Engine &e, MapT const &map, m_tetris2::TetrisBlockStatus const &st,
                   char const *next, size_t next_len, int n_steps)
    { for (int i = 0; i < n_steps; ++i) e.run(map, st, next, next_len, 0); }

    MapT build_board()
    { MapT m{}; for (int y = 0; y < 4; ++y) for (int x = 0; x < 10; ++x) if ((x + y) % 3 != 0) m.set(x, y); return m; }

    // Board with bottom-row gap cols 3-6 so I-piece clears a line (exercises clear_bonus).
    MapT build_board_with_clear()
    {
        MapT m{};
        for (int y = 1; y < 4; ++y)
            for (int x = 0; x < 10; ++x) m.set(x, y);
        for (int x = 0; x < 10; ++x) if (x < 3 || x > 6) m.set(x, 0);
        return m;
    }
}

int main()
{
    std::printf("# framework_config_tree_reference_test (v4)\n");
    ConfigTreeAI::Config cfg1; cfg1.weight = 1.0; cfg1.clear_bonus = 0.01;
    ConfigTreeAI::Config cfg2; cfg2.weight = 2.5; cfg2.clear_bonus = 0.07;
    ConfigTreeAI::Status root_input{10.0, 5.0, 1, true};

    // ---- Phase 1: e1 cfg1 ----
    Engine e1;
    expect(e1.prepare(Spec::width, Spec::height), "e1 prepare cfg1");
    e1.set_ai_config(cfg1); e1.notify_ai_config_changed();
    MapT map = build_board();
    *e1.status() = root_input;
    auto st = m_tetris2::TetrisBlockStatus('T', 3, 20, 0);
    run_steps(e1, map, st, "O?T?I?", 6, 32);
    auto r1 = e1.current_result();
    expect(r1.target.state.t != 0, "e1 cfg1 has target");
    std::printf("  e1 cfg1: target=%c(%d,%d,r=%d) get=%d iterate=%d\n",
                r1.target.state.t, r1.target.state.xb, r1.target.state.yb, r1.target.state.r,
                e1.ai()->get_count, e1.ai()->iterate_count);
    expect(e1.ai()->iterate_count > 0, "e1 cfg1 iterate called >0");

    // Root 4-field assertion.
    bool root_ok = false;
    e1.visit_tree([&](auto const &n, auto const *p) {
        if (p == nullptr) {
            auto const &rs = n.status.get_raw();
            root_ok = (rs.accum == root_input.accum && rs.value == root_input.value &&
                       rs.count == root_input.count && rs.root_marker == root_input.root_marker);
        }
    });
    expect(root_ok, "root node raw 4 fields match input");

    // ---- Phase 2: e1 set cfg2 + notify, re-run ----
    e1.set_ai_config(cfg2); e1.notify_ai_config_changed();
    *e1.status() = root_input;
    e1.ai()->get_count = 0; e1.ai()->iterate_count = 0;
    run_steps(e1, map, st, "O?T?I?", 6, 32);
    auto r1b = e1.current_result();
    std::printf("  e1 cfg2: target=%c(%d,%d,r=%d) get=%d iterate=%d\n",
                r1b.target.state.t, r1b.target.state.xb, r1b.target.state.yb, r1b.target.state.r,
                e1.ai()->get_count, e1.ai()->iterate_count);
    expect(e1.ai()->iterate_count > 0, "e1 cfg2 iterate called >0");

    // Root 4-field check on cfg2 re-run (not just cfg1).
    bool root2_ok = false;
    e1.visit_tree([&](auto const &n, auto const *p) {
        if (p == nullptr) {
            auto const &rs = n.status.get_raw();
            root2_ok = (rs.accum == root_input.accum && rs.value == root_input.value &&
                        rs.count == root_input.count && rs.root_marker == root_input.root_marker);
        }
    });
    expect(root2_ok, "root node raw 4 fields match input (cfg2 re-run)");

    // ---- Phase 3: e2 fresh cfg2 ----
    Engine e2;
    expect(e2.prepare(Spec::width, Spec::height), "e2 prepare cfg2");
    e2.set_ai_config(cfg2); e2.notify_ai_config_changed();
    *e2.status() = root_input;
    run_steps(e2, map, st, "O?T?I?", 6, 32);
    auto r2 = e2.current_result();
    std::printf("  e2 cfg2: target=%c(%d,%d,r=%d) get=%d iterate=%d\n",
                r2.target.state.t, r2.target.state.xb, r2.target.state.yb, r2.target.state.r,
                e2.ai()->get_count, e2.ai()->iterate_count);

    // ---- Phase 4: target + makepath + Status bit-exact ----
    bool target_ok = (r1b.target.state.t == r2.target.state.t) &&
                     (r1b.target.state.xb == r2.target.state.xb) &&
                     (r1b.target.state.yb == r2.target.state.yb) &&
                     (r1b.target.state.r == r2.target.state.r);
    expect(target_ok, "target (t,xb,yb,r) bit-exact");
    auto p1 = e1.make_path(st, r1b.target, map);
    auto p2 = e2.make_path(st, r2.target, map);
    expect(!p1.empty(), "makepath non-empty");
    expect(p1 == p2, "makepath identical");
    expect(r1b.status.accum == r2.status.accum, "Status.accum bit-exact");
    expect(r1b.status.value == r2.status.value, "Status.value bit-exact");
    expect(r1b.status.count == r2.status.count, "Status.count bit-exact");
    expect(r1b.status.root_marker == r2.status.root_marker, "Status.root_marker bit-exact");

    // ---- Phase 5: per-node eval — every evaluated holder must match; root shell is unevaluated ----
    ConfigTreeAI const &ai = *e1.ai();
    int eval_ok = 0, eval_tot = 0, unevaluated = 0, root_unevaluated = -1;
    e1.visit_tree([&](auto const &n, auto const *par) {
        if (n.identity.state.t != 0) {
            ++eval_tot;
            int pop = n.map.popcount();
            bool evaluated = (n.eval_slot.eval_gen != 0);   // gen0 = unevaluated root holder
            if (par == nullptr) root_unevaluated = evaluated ? 0 : 1;
            if (!evaluated) { ++unevaluated; return; }
            if (n.eval_slot.result.value == double(pop) * cfg2.weight && n.eval_slot.result.cells == pop)
                ++eval_ok;
        }
    });
    expect(eval_ok == eval_tot - unevaluated, "ALL evaluated nodes eval.value==popcount*2.5, cells==popcount");
    expect(eval_tot > 0, "eval_tot > 0 (non-vacuous)");
    expect(root_unevaluated == 1, "root shell is an unevaluated holder (gen0; no consumer resolves it)");
    std::printf("  eval: %d/%d evaluated, unevaluated holders=%d\n", eval_ok, eval_tot - unevaluated, unevaluated);

    // ---- Phase 6: per-node raw status (parent.raw + current eval/clear_bonus) ----
    int raw_ok = 0, raw_tot = 0;
    bool raw_all = true;
    e1.visit_tree([&](auto const &n, auto const *par) {
        if (n.identity.state.t != 0 && par != nullptr) {
            ++raw_tot;
            auto er = ai.eval(n.map);
            int clear = 0;
            if constexpr (requires { n.clear; }) clear = n.clear;
            double exp_accum = par->status.get_raw().accum + er.value + double(clear) * cfg2.clear_bonus;
            auto const &act = n.status.get_raw();
            if (act.value == er.value && act.accum == exp_accum) ++raw_ok;
            else raw_all = false;
        }
    });
    expect(raw_all, "ALL nodes raw.value==eval, raw.accum==parent.raw.accum+eval+clear*bonus");
    expect(raw_tot > 0, "raw_tot > 0 (non-vacuous)");
    std::printf("  raw (value+accum): %d/%d\n", raw_ok, raw_tot);

    // ---- Phase 7: virtual parent iterate reference (distinct-piece detection) ----
    // Normal expansion has <=2 distinct piece types (active+hold). Virtual has up to 7.
    int virt_count = 0, virt_ok = 0, virt_tot = 0;
    bool virt_all = true;
    e1.visit_tree([&](auto const &n, auto const *) {
        if (n.children == nullptr) return;
        std::set<char> pieces;
        for (auto *c = n.children; c; c = c->children_next)
            pieces.insert(c->identity.state.t);
        if (pieces.size() >= 3) { // virtual parent
            ++virt_count;
            // Replicate run_virtual: best per piece type using child.status.get() (NOT raw).
            ConfigTreeAI::Status const *cache[7] = {};
            for (auto *c = n.children; c; c = c->children_next) {
                char t = c->identity.state.t;
                int idx = 0;
                for (char p : "OISZJLT") { if (p == t) break; ++idx; }
                if (idx < 7) {
                    if (cache[idx] == nullptr || cache[idx]->value < c->status.get().value)
                        cache[idx] = &c->status.get();
                }
            }
            auto expected = ConfigTreeAI::rank_avg(cache, 7); // independent math, no counter poll
            auto const &actual = n.status.get();
            ++virt_tot;
            if (actual.value == expected.value && actual.accum == expected.accum &&
                actual.count == expected.count && actual.root_marker == expected.root_marker)
                ++virt_ok;
            else virt_all = false;
        }
    });
    expect(virt_count > 0, "virtual parent nodes detected (>=3 distinct piece types)");
    expect(virt_all, "virtual parent aggregate == independent rank_avg(piece-best child.status.get()) bit-exact");
    std::printf("  virtual parents: %d, iterate ref: %d/%d\n", virt_count, virt_ok, virt_tot);

    // ---- Phase 8: get-only Config change on SAME engine (clear_bonus only, weight same) ----
    {
        Engine e3;
        e3.prepare(Spec::width, Spec::height);
        ConfigTreeAI::Config cfg3a; cfg3a.weight = 2.5; cfg3a.clear_bonus = 0.07;
        e3.set_ai_config(cfg3a); e3.notify_ai_config_changed();
        *e3.status() = root_input;
        MapT mc = build_board_with_clear();
        run_steps(e3, mc, st, "O?T?I?", 6, 16);
        // Count nodes with clear > 0 (fixture actually exercises line clears).
        int clear_positive = 0;
        e3.visit_tree([&](auto const &n, auto const *) {
            if constexpr (requires { n.clear; }) if (n.clear > 0) ++clear_positive;
        });
        expect(clear_positive > 0, "clear-positive nodes exist on line-clear fixture");
        size_t gen_before = e3.eval_generation();
        int get_before = e3.ai()->get_count;
        auto r3a = e3.current_result();
        // Change ONLY clear_bonus (weight unchanged).
        ConfigTreeAI::Config cfg3b; cfg3b.weight = 2.5; cfg3b.clear_bonus = 0.50;
        e3.set_ai_config(cfg3b); e3.notify_ai_config_changed();
        size_t gen_after = e3.eval_generation();
        expect(gen_after != gen_before, "eval generation bumped on clear_bonus-only Config change");
        *e3.status() = root_input;
        run_steps(e3, mc, st, "O?T?I?", 6, 16);
        auto r3b = e3.current_result();
        expect(r3b.status.value == r3a.status.value, "get-only change: value unchanged (weight same)");
        std::printf("  e3 cfg3a: accum=%.17g value=%.17g clear_pos=%d | cfg3b: accum=%.17g value=%.17g | gen %zu->%zu | get %d->%d\n",
                    r3a.status.accum, r3a.status.value, clear_positive, r3b.status.accum, r3b.status.value,
                    gen_before, gen_after, get_before, e3.ai()->get_count);
    }

    std::printf("# passes=%d failures=%d\n", g_passes, g_failures);
    return g_failures == 0 ? 0 : 1;
}
