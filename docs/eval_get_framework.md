# eval(map) + GetArgs framework

This page is the short reference for the new AI evaluation contract. The full design
and per-AI migration table live in `eval_get_framework_plan.md`; the canonical
implementation is `src/tetris_eval_get.h`.

> **Status (2026-10-29, HEAD 037125c4 — prior semantic acceptance vetoed by the user):**
> all 15 src AIs are migrated to eval(map)+GetArgs; transposition pruning is removed.
> Commit chain on `dev`, not pushed: 7af0537a (old baseline) → 4b2d506a (migration) →
> 3770535e (cache hardening) → 037125c4 (docs/comment-only). Working tree tracked edits:
> this doc, the long-protected src/ai_ax.h guard/polarity two-hunk (pre-existing), and
> in-progress src/tetris_core.h / src/tetris_eval_get.h EnableEnv fixes (not deployed).
>
> **Fact vs conclusion.** The following are valid, real historical run records (they
> happened; their hashes are correct): serial 40/41 (one pre-existing safe-BPM fail, not
> cut); four fat libs 36c6febe (size 1843728, identity 3770535e-dirty); eight two-cycle
> loads; twelve fresh C2 tests; failure API Debug/NDEBUG 46 assertions each; 64-widen
> scored 30914/30914 hits 65/0; ConfigTreeAI 23 assertions / 619544 Eval-raw / 3630 virtual
> aggregates (synthetic fixture, not all-production-AI whole-tree proof). **What the user
> vetoed is the upgrade of these records into a "migration semantically equivalent /
> complete" conclusion**, which overreached because of the regression below.
>
> **Open regression:** `GetArgsInvoker(args::Env)` ignores `EnableEnv` and always returns
> `env<true>`; the O(1) tags `NextView/NextLength/NextNode/Hold/IsHold` read real values
> with no virtual gating. Old 7af `run_virtual` used `env<false>` = `{nullptr,0,' ',' ',false}`.
> On virtual nodes C2 `length_ratio` is 64 where the old empty-env value gives 0.5, and
> TOJ_v08/Botris/TOJ/IO/misaka read real parent hold/queue. As a conditional historical
> example (root combo=3, combo_limit=10, `?T?` queue), one logged virtual child attack moved
> from ~-4712.9 to ~-1.65M; the exact factor is node-dependent, not a universal constant.
> Production fat libs still contain this bug; a scratch fixed build ran 5/5 but is not deployed.
>
> **Status: deployment complete for the requested fix scope (label e0731c6ccbb2); NOT all-green / NOT
> universal 15-search equivalence.** Commit e0731c6ccbb2bbc19751f8c7f08499a20d69895f
> (fix(core): preserve virtual env and invalidate config search state, identity C2 ZZZ/e0731c6ccbb2
> no dirty); new fat 8144cd85…fd431 (1,860,720 B, arm64+x86_64 four authorized paths same hash)
> deployed, arm64 dlopen rc0. Three issues (virtual-env gating, Config invalidation, root eval-slot
> residual) fixed; strict 9-case 165056 all match; DP e073 rebuild 12-fresh 46/0 rc0 (true combo 5 /
> 500ms / this-step attack not clipped). Boundaries: full suite 1 known fail (safe_regression BPM
> generation assert not削); x86_64 dynamic still blocked by arm-only injection, unverified; GUI
> complete-match / TSan / perf untested; semantic GO is limited fixture/node coverage. Upper real
> App lib 34f546d… untouched; Java/JAR/ax protected hashes original. Old 36c6febe and candidate
> 44854a21 are unused history. Subsequent: docs-only commit, no push, no Java 3 diagnostics.
>
> **This round:** candidate fat built but not deployed; no commit, no push. Build stage
> (LLVM22, 4-slice rc0): candidate fat SHA256 44854a21031e3fad1cd397f53ce4247a54b2abceee4fc895d72c3420c0d632ed,
> size 1860560, embedded identity 037125c480ac-dirty — carries the in-progress EnableEnv fix
> but is NOT written to the four authorized paths; the final commit rebuild will change
> identity/hash. Debug serial 41/40/1 rc8; the pre-existing safe-BPM generation assertion
> (120→121) still fails and is not cut. arm64 dbg/rel/fat create/step/destroy two-cycle smoke
> rc0; x86_64 dynamic load is blocked by the dyld arm-only injected lib (static exports pass,
> dynamic unverified) — do not claim new dual-arch dynamic all-green. C2 is not closed: old-6
> recompile is stage evidence only; build/smoke must not be rewritten as C2 closure.
> GUI complete match / TSan / wall-ms perf-strength not run. Not claimed: all-search semantic
> equivalence, ASan data-race clean, whole-search path equivalence, optimal cache bucket size.
> Parallel observations have a limited wall-budget explanation, not global race exclusion.
> Earlier upper old App overwrite remains unrecovered (currently 34f546d); matching
> original bytes exist in libs/libcultrisai.dylib.backup_before_dev; restoration has not
> been authorized or performed. No further writes to that extra path.
> Evidence: research/eval_get_validation_followup/ and research/eval_get_virtual_env_correction/;
> this-round build stage: research/build_env_resume_20261029/REPORT.md (+ ctest_debug.log);
> independent audit: research/independent_audit_resume/AUDIT_MATRIX.md (10/15 production AIs
> floatbit-matched on 7 boards; tag/enemy b4 shared topout invalid; 4 Env consumers have no
> iterate so get<false> is engine-unreachable; direct_get is AI direct-call, not provider proof);
> nonprod fixtures: research/nonprod_resume_20261029/REPORT.md (sections 29/30 matched; ax
> empty-board old UB single-column); config lifecycle: research/config_resume_20261029/REPORT.md
> (fresh convergence only, hit=0, no full-map hit/iterate/frontier claim). Still open: misaka Core
> provider and the formal old/new full C2 engine trace. final HTML in parent project directory.

## Contract

An AI in `src/ai_*.h/.cpp` provides:

- `Result` — any type (`double`, a struct, or an empty type).
- `template<class Spec> void init(Config const*)`.
- `ai_name()`.
- `template<class MapT> Result eval(MapT const& map) const` — a **pure post-map
  function** of the bound `Config`. It must not read the landing piece, clear, spin,
  parent status, source map, depth, hold/next, or write external state. It may read
  the bound `Config const*`.
- `using GetArgs = m_tetris2::type_list<...>` — the single source of truth for what
  `get` consumes. The engine extracts only listed providers.
- one `get(...)` whose parameters match the `GetArgs` tags in order.

`Status` needs `operator<`. Optional: `Config`, `ratio()`, `iterate()`,
`EvalSpinType`. There is **no** `Status::transposition_key()` — same-depth
transposition pruning is removed because `get` consumes the whole placement sequence.

## Provider tags (`m_tetris2::args`)

| Tag | Delivers |
|---|---|
| `Eval<E>` | node `Result const&` (via the eval cache; the only cache entry point) |
| `Parent<S>` | parent's raw `Status const&` |
| `Depth` | `size_t` (parent level; root depth = 0) |
| `Clear` | `int` lines cleared by this attach |
| `Landing<L>` | `GetLanding<SpinT> const&` (typed landing geometry + spin) |
| `MapBefore` / `MapAfter` | parent / child `map const&` (non-template tags; the `get` parameter itself may be `template<class MapT>`) |
| `NextNode` / `NextLength` / `NextView` | next piece type / queue length O(1) / expanded queue view |
| `Hold` / `IsHold` | held piece / whether it came from hold |
| `Env` | full `AIEnv` (compat convenience; prefer the fine-grained tags) |

A tag listed in `GetArgs` is computed exactly once for a call; an unlisted provider
is never instantiated. Duplicate semantic tags are extracted once but passed at their
declared positions.

## GetLanding<SpinT>

Runtime POD precomputed at child creation. `h = shape::piece_h` (the N×N box height,
**not** the bbox height and **not** the occupied span); `land_height() = row() + h` (a
simple box offset — it does **not** saturate by line count); `y_plus1() = node.y + 1`.
`kMaxLines = 8` is only the size of the clear-`lines[]` view (enforced by a `static_assert`
on `piece_h`), unrelated to `land_height()`. `SpinT` defaults to `std::monostate` for AIs
that do not spin.

## EvalCache<MapT, Result>

Per-`Context` direct-mapped table:

- `prepare(bucket_count)` requires a **non-zero power-of-two** capacity and returns `bool`:
  0 / non-power-of-two / size overflow / entry-bounds misuse all return `false`; legal capacity
  is fixed as given (no normalization). Index is `hash & (capacity-1)`.
- `try_lookup(ai, map, out)` returns `true` on hit or miss after immediate eval, with valid `out`;
  it returns `false` only when unprepared, leaving `out` unchanged; calling `lookup` while unprepared is fatal under NDEBUG.
  `Engine::prepare` / `Thread::prepare` propagate the `false`; `run()` / `run_hold()` are
  guarded by the same `PauseBackground` as the current thread. `current_result()` is a read-only
  snapshot of the current best — it does not widen or score the queue. A normal already-bound
  `Config` is not re-initialized.
- A hit requires `written_gen == generation` **and** `hash` match **and** full raw
  `map` equality — hash alone is never the correctness proof.
- A miss runs `ai.eval(map)` immediately and overwrites the single bucket entry; no
  chaining, probing, LRU, eviction, or background cleanup. An empty board is a normal
  key (never conflated with an empty slot).
- A Config change calls `on_config_changed()` which `++generation`; on overflow it
  clears `written_gen` and signals a full-tree invalidation. Ordinary `update()`
  (root/status re-scoring) does **not** clear the cache.
- `get` is still called once per node; `scored()` still counts every `get`. A cache
  hit shares the `Result` but never skips `get` or collapses path-dependent status.

## Migration rules of thumb

- Split only what is purely post-map into `eval`; keep landing/clear/spin/accumulator
  work in `get`.
- Mixed floating expressions (e.g. Attack combining landing and board-dead-zone in
  one formula) must keep their original association order: `eval` emits the needed
  post-map intermediate(s), `get` recombines them at the original expression site.
- `Dig` keeps `Result = double` and an identity `get` (`GetArgs = type_list<Eval<double>>`).
- PC AIs (`TOJ_PC`, `Botris_PC`) `eval` only the board `Result{value, roof}`; clear and
  spin are replayed in `get` — `TOJ` via `tspin_for_clear(spin, clear)`, `Botris` via
  the ASpin value directly (do not unify the two).
