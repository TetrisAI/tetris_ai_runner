# eval(map) + GetArgs framework

This page is the short reference for the new AI evaluation contract. The full design
and per-AI migration table live in `eval_get_framework_plan.md`; the canonical
implementation is `src/tetris_eval_get.h`.

> Status (2026-10-29, full Debug rc0): framework header landed; all 15 src AIs migrated;
> transposition pruning fully removed (standalone repeats-500x no-prune test). Engine groups
> 3+5+7 pass; per-AI numeric diffs A 384 / B 52 / D (misaka 121500, ax 18000, farter 3600)
> / C golden PASS (241 lines, 240 cases / 2460 obs matched; old tag boundary UB); core
> counter/wrap/tree scale 80→160 pass; serial suite 36/37 (one pre-existing baseline safe-BPM
> old-generation fail); Native dual-arch dlopen + C2 four-slice 6 pass. Commit/deploy pending.
> Not claimed: ASan data-race clean, whole-search path equivalence, wall-ms strength, optimal
> bucket size. See `eval_get_validation.html`.

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
| `MapBefore<M>` / `MapAfter<M>` | parent / child `map const&` |
| `NextNode` / `NextLength` / `NextView` | next piece type / queue length O(1) / expanded queue view |
| `Hold` / `IsHold` | held piece / whether it came from hold |
| `Env` | full `AIEnv` (compat convenience; prefer the fine-grained tags) |

A tag listed in `GetArgs` is computed exactly once for a call; an unlisted provider
is never instantiated. Duplicate semantic tags are extracted once but passed at their
declared positions.

## GetLanding<SpinT>

Runtime POD precomputed at child creation. `h = shape::piece_h` (the N×N box height,
**not** the bbox height); `land_height() = row() + h`; `y_plus1() = node.y + 1`;
`land_height()` saturates into a `kMaxLines = 8`-sized `lines[]` table (with a
`static_assert` on `piece_h`). `SpinT` defaults to `std::monostate` for AIs that do
not spin.

## EvalCache<MapT, Result>

Per-`Context` direct-mapped table:

- `prepare(bucket_count)` locks capacity (power of two, may be 1); index is
  `hash & (capacity-1)`.
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
