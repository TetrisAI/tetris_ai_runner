# eval(map)+GetArgs migration — final evidence index

Status: **ROOT lazy fix deployed — code `4a77bdea2137b72b8086ace5de6b00f3247ef3f6`, core source `aa50`, fat `c8a1c4921ba3c66b92ef81e6784c0dde9c662e98f478f6dd023d4edf3936931e`. Independent readonly final check PASS. Docs commit completed (no push); does not change code provenance.**
NOT green all-pass, NOT universal 15-AI search equivalence; see boundaries below.

## Provenance

- Git commits (branch `dev`, no push): `7af0537a` (old baseline) → `4b2d506a` (migration)
  → `3770535e` (cache hardening) → `037125c4` (docs) → `e0731c6ccbb2` (virtual-env/Config fix)
  → `33dc85ad7437` (docs-only) → `4a77bdea2137b72b8086ace5de6b00f3247ef3f6`
  (`fix(core): invalidate recycled root eval holders lazily`, 5-file scope, deploy commit).
- Source fingerprints (not commits; build identity hash):
  `5f1fbe8512b73b2dd0b5cecc47066c37f750cfb5f41662cfc9891fc5ed20915a` (intermediate root eval-slot)
  → `6edc5bfb61d3f64095a7a870d22d53913cdcd58eb02b10a4637547b6ba5bf522` (intermediate root map/epoch)
  → `aa50db569e69ec719b2b09df9c99e0cb23fc93e79359171451096cb2e2c9b9dd` (ROOT lazy final source, stable).
- Deployed fat SHA256: `c8a1c4921ba3c66b92ef81e6784c0dde9c662e98f478f6dd023d4edf3936931e`
  (1,860,720 B; arm64+x86_64; four authorized white paths same hash;
  identity `C2 ZZZ / 4a77bdea2137`; ARM before/after App Resources two-round 2-cycle load rc0).
- DP binary (latest source rebuild): `1e5434fc09bea25bbe83d01b51e30257e1ff337e7328ce69bab7fadd22104868`
  (12-fresh 46/0 rc0).

## Key numbers

- Full ctest: 43 registered / 42 PASS / 1 FAIL rc8. Only FAIL is known-safe
  `tetris_cultris_safe_regression` ("BPM invalidates old results but preserves root combo latch",
  generation assertion not cut). Three newly added regression targets all clear.
- Strict C2 trace gate (fresh overlay aa50, out_final 6): 9-case compare v3 hard gate —
  165056 nodes matched, 0 unmatched, per-side unique 164,240, per-side dup 816
  (both-sides sum 1,632). 21 positive/negative self-test cases pass (exit codes conform).
- Formal: old `5f1f` negative control rc1 4 FAIL (cases not deleted); new `aa50` formal 11/11 rc0.
- 3 new root force-Eval regression targets source-fixed: contract 1791==1791; all AIs no empty-board abort.
- ConfigTree 24/24 (valid 619544 + invalid root shell 1): root diagnostic contract,
  invalid default NOT used as AI score.
- Independent readonly final check PASS: four libs c8a1… identical; code 4a77 exact 5-file scope;
  Java 3 diagnostics / upstream 34f5 protected hashes unchanged.

## ROOT lazy contract

After aa50, an unevaluated root node holds empty holder `{}` and does not pre-fill any AI score;
consumers that actually request `args::Eval` resolve it on demand against the current map+Config;
root nodes that are never consumed are not forced to evaluate to fill a clean holder. The intermediate `6edc` force-empty-Eval
regression is fixed in aa50. Diagnostic path for root outputs only a root shell (gen0 is unevaluated metadata, not illegal input)
and does not use the shell default as an AI score.

## Relative evidence paths (from repo root)

- Strict gate report + explicit reproduce commands: `research/c2_env_fix_resume_20261029/artifacts_newgate/REPORT.md`
  (compare.py v3 hard gate; 21 positive/negative self-test cases; exact compile/run invocations in REPORT.md / compare.py).
  Version-preserved files: `compare.py`, `gate_selftest.py`, `REPORT.md`.
- Config fix report: `research/config_resume_20261029/REPORT.md`
- Root eval fix report: `research/root_eval_fix_20261029/REPORT.md`
- Root map guard (same-Config free-list) fix report: `research/root_map_guard_fix_20261029/REPORT.md`
- New regression test sources: `test/test_c2_config_invalidation.cpp`,
  `test/framework_config_tree_reference_test.cpp` (CMake registered; formal old `5f1f`
  negative control rc1 4 FAIL, new `aa50` formal 11/11 rc0 including real consumer resolve).
- Build/ctest/DP final (aa50/4a77, small files in-repo): `research/build_env_resume_20261029/final/`
  (`ctest_debug_aa50.log`, `main_debug_build_aa50_r5.tail.log`,
  `fresh_c2_nextdp_4a77_run.log`, `slice4a77_*.log`, `x86_dyld_blocked_final.txt`;
  405 MB raw `main_debug_build_aa50_r5.log` not in-repo, tail only).
- Independent audit: `research/independent_audit_resume/AUDIT_MATRIX.md`
- Non-prod fixtures: `research/nonprod_resume_20261029/REPORT.md`
- Regression record: `research/eval_get_virtual_env_correction/`
- Vetoed-acceptance record: `research/eval_get_validation_followup/`

## Boundaries (do not overclaim)

- Full suite 1 known fail (safe_regression BPM generation assertion, intentionally not cut).
- x86_64 dynamic loading still blocked by the dyld arm-only injected lib; real no-bypass,
  static exports pass, dynamic unverified — not dual-arch dynamic all-green.
- GUI complete match, TSan, and wall-ms perf not tested.
- The 165056-node match is limited fixture/node coverage, not full-search/perf/GUI equivalence.
- Transition fat `8144cd85` (e0731c6ccbb2) and intermediate sources `5f1fbe85` / `6edc5bfb` are history.
  - Upper real App lib `CultrisII.def/Cultris II Mod.app` remains `34f546d…` (not restored, no write).
- Old production fat `36c6febe` and candidate `44854a21` are unused history.
