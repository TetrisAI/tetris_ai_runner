# eval(map) + GetArgs framework

This page is the short reference for the new AI evaluation contract. The full design
and per-AI migration table live in `eval_get_framework_plan.md`; the canonical
implementation is `src/tetris_eval_get.h`.

> Status (2026-10-29, full Debug rc0, commit 4b2d506a): framework header landed; all 15 src
> AIs migrated; transposition pruning fully removed (standalone repeats-500x no-prune test).
> The 37/36 numbers below are the **prior migration validation**; follow-up special tests are
> passed (see below) but the full matrix / new commit / redeploy are still pending actual data.
> Prior snapshot: engine groups 3+5+7 pass; per-AI numeric diffs A 384 / B 52 / D
> (misaka 121500, ax 18000, farter 3600) / C golden PASS (241 lines, 240 cases / 2460 obs
> matched; old tag boundary UB); core counter/wrap/tree scale 80→160 pass; serial suite 36/37
> (one pre-existing baseline safe-BPM old-generation fail); Native dual-arch dlopen + C2
> four-slice 6 pass. Four fat dylibs (x86_64+arm64) are built and deployed.
> **Unrecovered high-priority warning:** the deploy also overwrote an out-of-project upper App
> library (`Cultris II Mod.app/.../libcultrisai.dylib`) without authorization. A hash-matching
> original backup was later found (`libs/libcultrisai.dylib.backup_before_dev`, SHA
> 9c9d16ed…f992f, size 405856, re-verified); **not yet restored, zero writes to the upper App** —
> restore only on explicit authorization. This is not a full-acceptance claim.
> **Parallel non-determinism (investigated, not closed):** `research/eval_get_validation_followup/
> parallel_investigation/README.md`. Base 7af and current 4b2, Debug serial 3 tests each pass; a
> 3-proc concurrent round reproduced the same 5-TSD assert failures (release passes). The old
> `run_hold` 300 ms wall budget is sensitive: solo probe scored 154 263 at width 726 vs concurrent
> 95 050–98 497 at width 439–455 on a 12-core machine (not a 1/3 quota). The search-volume drop
> supports an existing wall-budget explanation, but the probe still has TSpin=1, so the TSD
> failure threshold is **not** determined; TSan has not been run, so a global race is **not**
> excluded. Raw 300 ms assertions are unchanged. New multi-widen test (64 steps): scored 30 914,
> cache hits 65 vs 0, final path/status exact.
> **Follow-up special tests (passed, bounded):**
> - Core failure API: non-zero power-of-two and element byte-bound rejections; `try_lookup` returns
>   `false` with `out` unchanged when unprepared; Engine/Thread `prepare` propagates; no-arg
>   `run()`/`run_hold()` graceful `false` vs map-taking runs fatal under NDEBUG; No-Config thread
>   empty holder, read-only `current_result()`, `visit_tree` all tested. D failure-API suite:
>   46 asserts (Debug / `-O2` NDEBUG) = 40 base + 6 No-Config-thread; runtime SIGABRT ×2 and a
>   Result-copy runtime error are surfaced, not swallowed.
> - Multi-widen 64 steps, both modes: scored 30 914, hits 65 vs 0, actual final target / path /
>   status exact.
> - Config v5 independent suite: 23 asserts pass — existing-tree cfg1→cfg2 vs fresh current:
>   619 544 Eval / raw direct reference, 3 630 virtual parents piece-best `child.status.get`
>   independent rank avg across all 4 fields, two-phase root input preserved, true-clear pos 311,
>   get-only config generation 3→5.
> **Full Debug build rc0, final count: 41 tests / 40 pass / 1 fail** — the single failure is the
> pre-existing `cultris_safe_regression` (old BPM-generation). 41 = 37 prior + beginner negative
> + two failure-API acceptance suites (46 asserts each) + Config v5 (23 asserts). Log:
> `build_validation_followup/Testing/Temporary/LastTest.log`.
>
> **Deployed production identity (code commit `3770535e346f5fb28f797f4d5030fbbc356a21fe`, parent
> 4b2d506a, 13 files +976/−73):** four fat libs SHA256
> `36c6febe604708dd42edb160e40b6383bbd98439a2763dbf35756c96f95b9a98`, size 1 843 728,
> x86_64+arm64; embedded name `C2 ZZZ / 3770535e346f-dirty`, version
> `8.4.0-generation-10ms`. 8 strict two-cycle loads all rc0/cycle1+2 OK; 12 Native special
> tests (2 arch × Debug/Release, NextDPPolicy) on the new binary PASS. Deployment logs:
> `research/eval_get_validation_followup/deployment/{build_fat_3770535e,four_fingerprints,
> smoke_8runs_3770535e,c2_12tests_3770535e,README}.log`(.md). Prior identity 4b2d506a /
> 34f546d is history. The multi-widen probe is a fixed 64 steps (scored 30 914, hits 65 vs 0),
> not N=120.
>
> **Boundaries (not claimed):** not all-search semantic equivalence, performance, end-to-end
> match play/GUI, TSan, or the old tag-boundary UB semantics; the pre-existing safe-BPM
> failure remains; restoring the overwritten upper App lib is still pending user decision. Not claimed: ASan data-race clean, whole-search path equivalence,
> wall-ms strength, optimal bucket size. The next follow-up needs a fresh commit and rebuild;
> do not treat the currently deployed library as the final follow-up artifact.

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
- `try_lookup(key, out)` returns `true` on hit with a valid `out`, or `false` on miss (or when
  unprepared, leaving `out` unchanged); calling `lookup` while unprepared is fatal under NDEBUG.
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
