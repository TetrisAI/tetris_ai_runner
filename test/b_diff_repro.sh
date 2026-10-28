#!/usr/bin/env bash
# b_diff_repro.sh — B组(TOJ_v08/Botris/TOJ/C2)旧新数值对拍可复现驱动.
# 用法:
#   test/b_diff_repro.sh --regenerate   # 从冻结旧基线(snapshot)重算 golden
#   test/b_diff_repro.sh                # 编译新侧(live src)并比对 golden
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO="$(cd "$HERE/.." && pwd)"
TEST="$HERE"
SNAP="$REPO/research/baseline_freeze_7af0537a_20261028/snapshot"
BUILD="$REPO/build_b_repro"
CXX="${CXX:-/opt/homebrew/opt/llvm/bin/clang++}"
mkdir -p "$BUILD"
[ -d "$SNAP" ] || { echo "missing snapshot: $SNAP" >&2; exit 2; }

build_old() {
  "$CXX" -std=c++20 -g -O0 -I"$SNAP/src" -I"$SNAP/cultris" -I"$TEST" \
    "$TEST/_b_old_golden_main.cpp" "$SNAP/src/ai_zzz.cpp" "$SNAP/src/integer_utils.cpp" \
    -o "$BUILD/old_side"
}
build_new() {
  "$CXX" -std=c++20 -g -O0 -I"$SNAP/src" -I"$REPO/src" -I"$TEST" \
    "$TEST/framework_ai_b_diff.cpp" "$REPO/src/ai_zzz.cpp" "$SNAP/src/integer_utils.cpp" \
    -o "$BUILD/new_side"
}

if [[ "${1:-}" == "--regenerate" ]]; then
  build_old
  ( cd "$TEST" && "$BUILD/old_side" > framework_ai_b_diff.golden )
  echo "== regenerated golden ($(wc -l < "$TEST/framework_ai_b_diff.golden") lines)"
  exit 0
fi

build_old
build_new
( cd "$TEST" && "$BUILD/old_side" > "$BUILD/old.txt" )
( cd "$TEST" && "$BUILD/new_side" > "$BUILD/new.txt" )
echo "== diff golden(old-built) vs old_side:"
diff -u "$TEST/framework_ai_b_diff.golden" "$BUILD/old.txt" && echo "== golden reproduces bitwise"
echo "== diff old_side vs new_side (full field):"
if diff -u "$BUILD/old.txt" "$BUILD/new.txt"; then
  echo "== B MATCH: old vs new bitwise identical"
else
  echo "== B MISMATCH (see diff above)"; exit 1
fi
