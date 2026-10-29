# Root eval lazy-holder fix — 2026-10-29

## Root cause
`discard_search_tree()` resets root; the next `update_root` recycles a free_list child node
(`alloc`) that still carries a foreign-map `eval_slot`. `reclaim()` clears children/node/hold/level
but **not** `eval_slot`. The root's Status is written by the host and root is never scored through
`args::Eval`, so the gen/era self-check path that refreshes child slots never fires on root.
Result: new root on mapB keeps the recycled child's eval(mapA) bits, with gen/era already == current
(same Config, same generation) — the original gen/era short-circuit could not see it.

## Contract (per Main) — lazy valid holder
- No consumer demand → never force `ai.eval` on the raw/empty root board (this was what aborts
  TOJ_v08/Attack on the empty SRS board and added 1 spurious cache eval in contract_probes).
- On recycle / map change / stale epoch: reset the whole eval holder to the **unevaluated default**
  (`eval_slot = {}` → result = R{}, eval_gen = 0, eval_era = 0). The stale foreign result must not
  be readable as a valid holder.
- The real consumer (`GetArgs args::Eval` in `GetArgsInvoker::operator()`) resolves on demand by
  full map + current Config (existing gen/era check → `eval_cache.lookup`).
- A cached holder whose map + epoch still match is kept unchanged.
- `kWantEval=false` AIs: member is monostate, zero eval calls.

## Source diff (src/tetris_core.h, update_root)
Replaced the unconditional `refresh_root_eval` (always `lookup(root->map)`) with
`invalidate_root_eval_if_stale`: only zeroes the holder when gen/era mismatch; the alloc-recycle
branch does `eval_slot = {}` directly. No AI formula/weight, no C2 combo/time/DP, no host Status
path, no float order touched.

## Hashes (stable)
- src/tetris_core.h = `aa50db569e69ec719b2b09df9c99e0cb23fc93e79359171451096cb2e2c9b9dd`
- src/tetris_eval_get.h = `bba95aa80397b32eacf6247724b45db5e52c07f70460fab01eb166d8a4f5a77e` (unchanged)
- src/ai_ax.h (protected) = `caf3117f25cb91206c21f2e4d88400619ed0fda0dd89ea2df4a115281e6e9bf8` (unchanged)
- test/c2_root_eval_consistency_test.cpp = `be22b85ac86c1b8c72138eca8786191a994e21ce074ccab2181c20ca6b2564b3`
- test/framework_config_tree_reference_test.cpp = `f28436f64fd7af169c909a4fcac48928c31b51e3e2a768bf777ef7769a3ca277`
- CMakeLists.txt = `ca45b976c88f7ad1929e4b3e89198cc8c3a153fd37d9cf6fc1de1525be7d0f58`

## Results (LLVM22 clang++, -std=c++20 -O0 -g -w)
New source:
- framework_contract_probes rc=0 — `dup: eval_pc=1791 evals=1791 parent=1791` (count restored)
- framework_allai_smoke rc=0 — Attack on empty SRS board no longer aborts
- framework_allai_zzz_smoke rc=0 — TOJ_v08 etc., passes=9 failures=0
- framework_config_tree_reference_test rc=0 — passes=24; `eval: 619544/619544 evaluated,
  unevaluated holders=1` (root shell explicitly asserted unevaluated; all valid holders full match;
  root raw 4 fields / target / makepath / Status bit-exact / raw 619544/619544 / virtual 3630 / gen
  bump all still PASS)
- c2_root_eval_consistency_test rc=0 — 11/11 PASS (recycle same-ptr foreign-map holder reset to
  unevaluated; consumer resolve yields exact ai.eval(mapB) bits; stale-gen / stale-era early-return
  invalidated; host Status untouched; no-Eval AI zero eval calls)
- test_c2_config_invalidation rc=0

Negative control (pre-fix snapshot tetris_core.h = 5f1fbe85, same regression test):
- rc=1, 4 FAILs: foreign-map holder not reset (gen==current, stale mapA), resolved bits != mapB,
  stale-gen not invalidated, stale-era not invalidated.

Logs: research/root_map_guard_fix_20261029/f_run_*.log, f_compile_*.log.
