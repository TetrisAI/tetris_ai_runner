# Commit 7f - Drop legacy pointer-returning drop_bb

## Background
After Commits 7c/7d/7e migrated every BFS callsite to `drop_bb_state`, the
pointer-returning `drop_bb(BBState const&, ...)` overload in
`MoveGenSearch::Algorithm` had no remaining callers.

## Change
- Removed the orphaned `drop_bb(BBState const&, ...)` method from
  `src/movegen_search.h`.

## Validation
- `oracle_diff` passes for all 1g/20g scenarios (`# all diffs ok`).
