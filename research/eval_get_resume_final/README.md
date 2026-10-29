# eval(map)+GetArgs migration — final evidence index

Status: **deployment complete for the requested fix scope** (code label `e0731c6ccbb2`).
NOT all-green, NOT universal 15-AI search equivalence; see boundaries below.

## Provenance

- Code commit: `e0731c6ccbb2bbc19751f8c7f08499a20d69895f` —
  `fix(core): preserve virtual env and invalidate config search state`
- Frozen production source hash (strict gate): `cf3f058977e4b4d8338f3575bacd378a0c6ad95aeddff42308c148804e8fbda9`
- Deployed fat SHA256: `8144cd852b7273161f0dba9e7622cd2d2c3d94beccf74680569bc21aed9fd431`
  (1,860,720 B; arm64+x86_64; four authorized paths same hash; identity `C2 ZZZ / e0731c6ccbb2`)
- Chain: `7af0537a` (old baseline) → `4b2d506a` (migration) → `3770535e` (cache hardening) → `037125c4` (docs) → `e0731c6ccbb2` (fix).

## Key numbers

- Full ctest: 42 registered / 41 PASS / 1 FAIL. Only FAIL is known-safe
  `tetris_cultris_safe_regression` ("BPM invalidates old results but preserves root combo latch",
  generation assertion not cut).
- Strict C2 trace gate: 165056 nodes matched, 0 unmatched, no field mismatch
  (vp=0 virtual 163866 / vp=1 real 1190; clear 0–4 each non-zero).
- NextDP e073 rebuild: 12-fresh (3 DP × 4 V/P) 46/0 rc0; true combo 5 / 500ms /
  this-step attack not clipped, only returned.combo clipped.

## Relative evidence paths (from repo root)

- Strict gate report: `research/c2_env_fix_resume_20261029/artifacts_newgate/REPORT.md`
- Config fix report: `research/config_resume_20261029/REPORT.md`
- Build/ctest/DP final: `research/build_env_resume_20261029/final/`
  (`ctest_debug_final.log`, `main_debug_build_final.tail.log`,
  `fresh_c2_nextdp_e073_run.log`, `x86_dyld_blocked_final.txt`)
- Build stage summary: `research/build_env_resume_20261029/REPORT.md`
- Independent audit: `research/independent_audit_resume/AUDIT_MATRIX.md`
- Non-prod fixtures: `research/nonprod_resume_20261029/REPORT.md`
- Regression record: `research/eval_get_virtual_env_correction/`
- Vetoed-acceptance record: `research/eval_get_validation_followup/`

## Boundaries (do not overclaim)

- Full suite 1 known fail (safe_regression BPM generation assertion, intentionally not cut).
- x86_64 dynamic loading still blocked by the dyld arm-only injected lib; static exports pass,
  dynamic unverified — not dual-arch dynamic all-green.
- GUI complete match, TSan, and wall-ms perf not tested.
- The 165056-node match is limited fixture/node coverage, not full-search/perf/GUI equivalence.
- Upper real App lib `CultrisII.def/Cultris II Mod.app` remains `34f546d…` (not restored, no write).
- Old production fat `36c6febe` and candidate `44854a21` are unused history.
