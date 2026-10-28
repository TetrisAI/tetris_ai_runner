#!/usr/bin/env bash
# a_diff_repro.sh — A组(qq::Attack/Dig/TOJ_PC/Botris_PC)旧新数值对拍的可复现驱动.
#
# 归属: A 组 owner (s_000pfnS10ab). 不改 CMake / 既有 test.
#
# 用法 (在任意目录均可, 路径全部相对本脚本):
#   test/a_diff_repro.sh --regenerate   # 从冻结旧基线(snapshot)重算 golden
#   test/a_diff_repro.sh                # 编译新侧(live src, 需 A patch 已被 B 合并)并比对 golden
#
# 依赖:
#   - clang++ 取自 /opt/homebrew/opt/llvm/bin/clang++ (Apple clang 15 在重 TU 上 crash, 非源码问题).
#   - 冻结旧基线: research/baseline_freeze_7af0537a_20261028/snapshot/ (只读, 含 ax 既有 guard).
#   - 新侧 live src: src/ai_zzz.{h,cpp} 已应用 A 组迁移 patch (由 B owner 合并).
#
# 产物: 仅在 build_a_repro/ 下, golden 落点 test/framework_ai_a_diff.golden.
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO="$(cd "$HERE/.." && pwd)"
TEST="$HERE"
SNAP="$REPO/research/baseline_freeze_7af0537a_20261028/snapshot"
BUILD="$REPO/build_a_repro"
CXX="${CXX:-/opt/homebrew/opt/llvm/bin/clang++}"
mkdir -p "$BUILD"

[ -d "$SNAP" ] || { echo "missing frozen snapshot: $SNAP" >&2; exit 2; }
command -v "$CXX" >/dev/null 2>&1 || { echo "clang++ not found: $CXX" >&2; exit 2; }

echo "== repo=$REPO"
echo "== snapshot=$SNAP"
echo "== cxx=$CXX"

# ---- 旧侧: 直接编译冻结 snapshot 的真实旧 ai_zzz, 不重写任何公式 ----
build_old() {
  "$CXX" -std=c++20 -g -O0 \
    -I"$SNAP/src" -I"$SNAP/cultris" -I"$TEST" \
    "$TEST/_a_old_golden_main.cpp" "$SNAP/src/ai_zzz.cpp" "$SNAP/src/integer_utils.cpp" \
    -o "$BUILD/old_side"
}

# ---- 新侧: 编译 live src 的迁移后 ai_zzz (A patch 已被 B 合并) ----
build_new() {
  "$CXX" -std=c++20 -g -O0 \
    -I"$SNAP/src" -I"$REPO/src" -I"$TEST" \
    "$TEST/framework_ai_a_diff.cpp" "$REPO/src/ai_zzz.cpp" "$SNAP/src/integer_utils.cpp" \
    -o "$BUILD/new_side"
}

if [[ "${1:-}" == "--regenerate" ]]; then
  build_old
  ( cd "$TEST" && "$BUILD/old_side" > framework_ai_a_diff.golden )
  echo "== regenerated golden: $TEST/framework_ai_a_diff.golden ($(wc -l < "$TEST/framework_ai_a_diff.golden") lines)"
  exit 0
fi

build_old
build_new
( cd "$TEST" && "$BUILD/old_side" > "$BUILD/old.txt" )
( cd "$TEST" && "$BUILD/new_side" )
echo "== diff old(new-built baseline) vs golden:"
diff -u "$TEST/framework_ai_a_diff.golden" "$BUILD/old.txt" && echo "== golden reproduces bitwise; new side matched (exit 0 above)"
