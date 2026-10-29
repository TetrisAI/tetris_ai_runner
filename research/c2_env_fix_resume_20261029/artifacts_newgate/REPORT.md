# C2 Native Strict Trace Gate — FINAL Report

Scope: old 7af full-AI + old Engine provider vs candidate-production full-AI + new Engine provider,
fixed identical legal-placement expansion, per-node input + Result migration semantics + raw Status
field bit-exact. Experimental overlay only; **production src/CMake/docs untouched, no commit/deploy/push/branch**.

## Frozen source & overlay hashes (final)

| artifact | sha256 |
|---|---|
| production `src/tetris_core.h` (FINAL root-stable, lazy invalidator) | `aa50db569e69ec719b2b09df9c99e0cb23fc93e79359171451096cb2e2c9b9dd` |
| `overlay/final6/tetris_core.h` (aa50db56 production + observer) | `9e32a3f41c3a4fe9f7661989f0f6c0ad725274bc29488b3dcdb55016d824635e` |
| stage history: `overlay/final5/tetris_core.h` (6edc + observer) | `8bb1f8331d3ffa3ac538e79b570a92624bdfb4f4173822333161353f583ee664` |
| stage history: `overlay/final/tetris_core.h` (5f1f + observer) | `83e3b876822c43e7e243db81ce4777f5f867a345659e544ca71404b40dd106ad` |
| stage history: production cores 6edc5bfb… / 5f1fbe85… | intermediate stages, NOT current verification |
| `test_c2_trace_driver.cpp` | `877aa63daff2deab7e2de5342368d6dc81708ca1eeb651ff972ff664c1860ebf` |
| `test_c2_nonub_negctl.cpp` | `2056b922a92136a6b733fb9de68ad32d40f4be8e4b9f9246535dd55df732c146` |
| `compare.py` (v3 strict gate, this round) | `75933de6463d6812fb16b170ab7854fef676a4d58defaf2c9a878290435ea288` |
| `gate_selftest.py` (v3 gate self-test harness) | `752a68a040ed6610c49e0f4e4d0a47611424e8a7e602ae973664a74a98f6a700` |

History: `compare.py` v1 `1db8a6e8…` → v2 `195a6eaf…` → v3 `75933de6…` (this round).

The production diff vs the pre-fix tree is only EnableEnv + Config change (discard_search_tree /
on_config_changed); no AI arithmetic / weights change. The overlay instrumentation delta is ONLY:
global `extern FILE *g_c2_trace; extern int g_c2_trace_safe;` + a read-only G/MA/MB/XD dump block in
`Core::get` (pure reads; no cache/Config/state mutation). No production computation changed.

## Result

| run | rc | matched | unmatched_old | unmatched_new | field mismatch |
|---|---|---|---|---|---|
| final6 (current production core `aa50db56…`, freshly compiled+run) vs old(7af) | **0** | **165056** | 0 | 0 | **NONE** |
| out_final5 (stage `6edc5bfb…`) vs old(7af) | **0** | **165056** | 0 | 0 | NONE (stage history) |
| out_final4 (stage `5f1fbe85…`) vs old(7af) | **0** | **165056** | 0 | 0 | NONE (stage history, NOT used as current verification) |
| broken(pre-fix) vs old | **3** (v2 gate: per-case record-count mismatch) | n/a — gate stops at structure | n/a | n/a | v1 gate previously reported YES nn/nl on vp=0 (UB boundary exhibit, freeze_gate REPORT) |

Note: `cf3f058` in the previous draft was the interim preDiscard overlay hash, NOT production. The
production core under current test is `aa50db569e69ec719b2b09df9c99e0cb23fc93e79359171451096cb2e2c9b9dd`
(verified `shasum -a 256 src/tetris_core.h` at gate time). Root lifecycle evolution across stages:
5f1f → 6edc (unconditional refresh_root_eval) → aa50db56 (lazy invalidator: stale root holder reset
to `{}` result + gen=0 + era=0, consumer resolves by full map+Config on demand; never force-eval the
raw board, never double-count provider evals). No scoring formula / weights / Eval-provider behavior
change, confirmed by the 165056-record bit-exact re-run below.

## Coverage — one consistent scope (9-case out_final5.txt, total G = 165056)

vp: `vp=0 virtual=163866`, `vp=1 real=1190` (sum = 165056).
clr: `clr=0:163484, clr=1:974, clr=2:386, clr=3:106, clr=4:106` (sum = 165056).
Computed v3 metrics (printed by the gate itself, not hand-counted):
- **per-side duplicate-occurrence extras: old=816, new=816** (the legacy "1632" figure was the
  both-sides sum; per side it is 816).
- **unique chain keys: 164240 per side** (= 165056 records − 816 extra occurrences; old==new).
- **route_distinct = 78345** (distinct full ROUT strings, gate-computed); distinct ROUT first token =
  4695. NOTE: the legacy draft's "ROUT distinct = 39234" reproduces under neither definition — flagged
  as an unreproduced ad-hoc figure; the gate number above is the authoritative one.

Per case (oldG/finalG equal):
virt_preview_T_combo3=28538 · virt_only=5670 · preview_only=170 · neither=34 ·
clear1=25651 · clear4=25461 · clear3=25477 · clear2=25517 · virt_preview_T_combo0=28538.

## Negative controls

1. **Engine-broken UB boundary exhibit** (kept, not used for causality): broken run reads
   `parent->next` on virtual TreeNodes the virtual run never wires → nl ≈ 5.28e13 (UB).
   FIRST_MISMATCH `field=nn`, OLD `nn=' ' nl=0` vs broken `nn='T' nl=<huge>`. This is the safety
   boundary violation; post-UB ROUT does NOT prove scoring causality.

2. **Non-UB deterministic negative control** (controlled direct `Core::get<false>`): parent.next
   explicitly wired to `ctx.next.cbegin()` (bounded distance=3). Reuses bridge fixtures noclear_T(clear=0)
   + clear1_I(clear=1). old==new bit-exact; broken differs on env-dependent atk/value.
   ```
   diff nonub_old.txt nonub_final.txt   -> rc=0   (old == candidate, 5 fields 0 diff)
   diff nonub_old.txt nonub_broken.txt -> rc=1   (broken atk/value differ; map/combo/cl identical)
   ```

## Reproduce

Each binary lists its exact TUs / include / macro explicitly — `overlay/*/` contains ~80 cpp
files (demo.cpp, vs.cpp, botris.cpp, cmd_tris.cpp, tree_test.cpp … each with its own `main()`),
so a `overlay/final/*.cpp` glob would pull multiple `main()` symbols and is NOT a reproducible
command. The trace old/final binaries below already exist in `bin/` (built 13:20 / 14:21);
this round did NOT re-run them (each regenerates ~381 MB) — the gate ran against the saved
out_old3.txt / out_final4.txt produced by them. The nonUB binary WAS recompiled with the
explicit TU list this round and its output is byte-identical to the saved log.

```
R=research/c2_env_fix_resume_20261029; L=/opt/homebrew/opt/llvm/bin/clang++; B=$R/artifacts_newgate/bin
# trace FINAL6 (current aa50db56 production + observer, FRESH run this round): driver + exactly 2 TUs
$L -std=c++20 -O0 -g -I $R/overlay/final6 $R/test_c2_trace_driver.cpp $R/overlay/final6/integer_utils.cpp $R/overlay/final6/ai_zzz.cpp -o $B/c2_trace_final6
$B/c2_trace_final6 > $R/artifacts_newgate/out_final6.txt    # freshly generated this round (~381 MB)
# trace old: same driver, OLD_SNAPSHOT macro, old overlay TUs
$L -std=c++20 -O0 -g -DOLD_SNAPSHOT -I $R/overlay/old $R/test_c2_trace_driver.cpp $R/overlay/old/integer_utils.cpp $R/overlay/old/ai_zzz.cpp -o $B/c2_trace_old
# gate (current evidence): out_old3.txt vs FRESH out_final5.txt
python3 $R/artifacts_newgate/compare.py $R/artifacts_newgate/out_old3.txt $R/artifacts_newgate/out_final6.txt
#   -> rc=0, GATE PASS, matched=165056 unmatched 0/0 dup extras 816/816 unique_chain_keys 164240 route_distinct 78345
# (stage history only) out_final5/out_final4 were earlier core stages, NOT reused as verification:
python3 $R/artifacts_newgate/compare.py $R/artifacts_newgate/out_old3.txt $R/artifacts_newgate/out_final4.txt   # rc=0 (stage)
# nonUB negctl rebuilt on final5 this round (explicit TUs, NO *.cpp glob):
$L -std=c++20 -O0 -g -I $R/overlay/final5 $R/test_c2_nonub_negctl.cpp $R/overlay/final5/integer_utils.cpp $R/overlay/final5/ai_zzz.cpp -o $B/c2_nonub_final5
$L -std=c++20 -O0 -g -DOLD_SNAPSHOT -I $R/overlay/old $R/test_c2_nonub_negctl.cpp $R/overlay/old/integer_utils.cpp $R/overlay/old/ai_zzz.cpp -o $B/c2_nonub_old5
$B/c2_nonub_old5 >/dev/null 2>$R/artifacts_newgate/nonub_old5.err      # rc=0
$B/c2_nonub_final5 >/dev/null 2>$R/artifacts_newgate/nonub_final5.err  # rc=0
cmp $R/artifacts_newgate/nonub_old5.err $R/artifacts_newgate/nonub_final5.err   # rc=0 stderr byte-identical
```

## test_c2_config_invalidation.cpp — real negative control (preDiscard overlay/new)

- test hash (this round, root-assert strengthened): `79b9fbfbbf5792e0bf90545e6719f7e802309cf61d8195e981c3823d792d6094`
  (previous: `52bcb9042d85e031c6fdbdcb447e407da719c7a943c549aa2a1762ffa2695c72`; +45/-5 lines: outdated
  "root shell never takes an eval" comment corrected to the update_root-refreshed eval_slot copy
  semantics, plus root eval_gen==current-gen / root.map==board / root eval_era==fresh / full Result
  bits equality assertions; all original non-root/target/Status/provider-count assertions retained).
- preDiscard header `overlay/new/tetris_core.h` hash: `bcee3dbcbb3a3cbca00c0a58272958f1567e16289110ec216f4a3df23f6e6bde`.
- On the FIXED production core (current `5f1fbe85…` family; rebuild-time dirty hash `6edc5bfb…`):
  PASS reachable=24813 stale=0 failures=0, direct rc=0, ctest `tetris_c2_config_invalidation` Passed.
- On the preDiscard (noDiscard) overlay/new: **FAIL rc=1, reachable=46953 stale=22554 failures=7**
  (move/value/attack/map/combo/combo_limit bits + stale gen≠cur all fail). Matches original c2_stale_probe
  (reachable=46954/stale=22555) within the visit_tree root-count off-by-one.
- Build agent's "意外 PASS" was a mixed-include (did not actually compile the preDiscard overlay);
  the test fixture itself reliably catches Config stale.

Reproduce:
```
R=research/c2_env_fix_resume_20261029; L=/opt/homebrew/opt/llvm/bin/clang++
$L -std=c++20 -O0 -g -I $R/overlay/new test/test_c2_config_invalidation.cpp $R/artifacts_newgate/shim.cpp \
   $R/overlay/new/integer_utils.cpp $R/overlay/new/ai_zzz.cpp -o $R/artifacts_newgate/bin/cfg_inval_prediscard
$R/artifacts_newgate/bin/cfg_inval_prediscard   # rc=1, reachable=46953 stale=22554 failures=7
```

## Gate v3 (this round) — strict frozen-format comparator

The old `compare.py` had four gate holes, all closed:

1. `zip(old_cases, new_cases)` silently truncated when case counts differed → extra/missing case passed.
2. unmatched chains on either side were counted but never gated → leftover records still `exit 0`.
3. records were grouped BY CHAIN NAME: `XD` was attached via `recs[chain][-1]`, so a later occurrence's
   XD overwrote an earlier duplicate occurrence's section; MA/MB lists were pooled across occurrences of
   the same chain. Incomplete records could then `None == None` silently match.
4. the `matched` Counter list itself was re-iterated, re-pairing full occurrence lists per duplicate.

v3 parses the stream in order: each `G` record owns the `XD` / 28×`MA` / 28×`MB` lines that follow it.
Format gates are FROZEN (no env override can weaken them):

- case name list must equal the exact 9 expected names, ordered (both sides renamed to the same wrong
  list still fails);
- every case block non-empty; per-case record count equal on both sides;
- exact emitted shape per record: 1 XD line with fixed 4 payload keys in order, MA exactly 28 rows
  `y0=..`..`y27=..` in order, MB the same, chain tags consistent;
- any-side unmatched chain occurrence fatal; each unique chain paired by ordinal exactly once;
- all 10 G fields + MA + MB + XD lists compared bit-wise.
Exit: 0 pass / 2 field mismatch / 3 structural gate violation.

### Self-test exit codes (actual, this round; 21/21 pass)

Fixtures are record-boundary slices of the real 9-case trace (3 records/case), no 381 MB copies:

```
python3 $R/artifacts_newgate/gate_selftest.py
  pos_small 9-case fixture old==new                    rc=0
  extra 10th case on new side                          rc=3
  missing clear3 case on new side                      rc=3
  empty new input / empty old input / empty both       rc=3 / rc=3 / rc=3
  extra G record on new side                          rc=3
  missing G record on old side                         rc=3
  same-total-count skewed chain multiset              rc=3   (unmatched branch fired)
  field diff (nn) case0 rec1                           rc=2
  record missing XD / MA / MB section                 rc=3 / rc=3 / rc=3
  record missing its G line (stray sections)          rc=3
  BOTH sides delete the same MA row (y5)             rc=3   (identical deletion cannot green)
  one case block has 0 G records                      rc=3
  BOTH sides renamed to the same wrong list           rc=3   (frozen-names gate)
  dup chain x2 same ordinal order                     rc=0
  dup chain x2 shuffled occurrences                   rc=2   (ordinal pairing catches it)
  REAL out_old3 vs out_final5 (fresh 6edc trace)      rc=0   matched=165056 unmatched 0/0;
                                                            per-side dup extras 816/816;
                                                            unique_chain_keys 164240; route_distinct 78345
  REAL out_broken vs out_old3                         rc=3   (per-case record-count gate)
```

## Artifact absolute paths

- Driver: `/Users/zhaoming.274/Documents/CultrisII.def/c2_apple_silicon/tetris_ai_runner/research/c2_env_fix_resume_20261029/test_c2_trace_driver.cpp`
- NonUB: `/Users/zhaoming.274/Documents/CultrisII.def/c2_apple_silicon/tetris_ai_runner/research/c2_env_fix_resume_20261029/test_c2_nonub_negctl.cpp`
- Compare: `/Users/zhaoming.274/Documents/CultrisII.def/c2_apple_silicon/tetris_ai_runner/research/c2_env_fix_resume_20261029/artifacts_newgate/compare.py`
- final trace: `/Users/zhaoming.274/Documents/CultrisII.def/c2_apple_silicon/tetris_ai_runner/research/c2_env_fix_resume_20261029/artifacts_newgate/out_final4.txt`
- old trace: `/Users/zhaoming.274/Documents/CultrisII.def/c2_apple_silicon/tetris_ai_runner/research/c2_env_fix_resume_20261029/artifacts_newgate/out_old3.txt`
- Freeze evidence: `/Users/zhaoming.274/Documents/CultrisII.def/c2_apple_silicon/tetris_ai_runner/research/c2_env_fix_resume_20261029/freeze_gate_20261029/`
