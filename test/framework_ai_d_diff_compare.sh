#!/usr/bin/env bash
# framework_ai_d_diff_compare.sh — shard D (misaka/ai_ax/farteryhr) unit 级 direct eval/get
# 旧四参 API vs 新契约 eval(map)/GetArgs-get 对拍。自包含、相对路径、失配非零退出，
# 可直接被 ctest 调用（不需要进 CMake）。不硬编码任何 artifact 绝对路径。
#
# 用法:
#   test/framework_ai_d_diff_compare.sh                # Pair1: worktree-old vs new-worktree
#   test/framework_ai_d_diff_compare.sh --head-index  # Pair2: HEAD_pristine-old vs INDEX_candidate-new
#
# 退出码: 0=OLD/NEW 逐字节相等(仅首行 MODE 标签差异)；非0=失配或编译/运行失败。
#
# 编译 define / 源码依赖:
#   -DAGENT_OLD / -DAGENT_NEW : 选旧四参 eval/get 还是新契约 eval(map)/get。
#   -DPAIR2_SKIP_EMPTY_AX     : pair2(HEAD 无 guard) 跳过空盘 board0（occupied_row(-1) 越界）。
#   依赖头: tetris_core.h/tetris_map.h/tetris_node.h/tetris_shape.h/rule_srs.h/search_tspin.h
#           + ai_misaka.h/.cpp ai_ax.h/.cpp ai_farter.h/.cpp integer_utils.cpp。
#   10x22 几何: harness 内用 rule_srs::detail::op_* 自建 Spec22=RuleSpec<10,22,4,...>
#           （misaka static_assert 要求 10x22）；ai_ax/farter 用 rule_srs::detail::SrsRule(10x40)。
set -u

# ---- 相对路径：从脚本位置推导 repo root，不依赖调用时 cwd ----
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO="$(cd "$HERE/.." && pwd)"
SNAP="$REPO/research/baseline_freeze_7af0537a_20261028/snapshot"
PATCHES="$REPO/research/baseline_freeze_7af0537a_20261028/patches"
HARNESS="$HERE/framework_ai_d_diff.cpp"

CLANG="${CLANG:-/opt/homebrew/opt/llvm/bin/clang++}"
FLAGS="-std=c++20 -O0 -g -fno-omit-frame-pointer -w"

MODE=worktree
if [ "${1:-}" = "--head-index" ]; then MODE=head-index; fi

for d in "$SNAP/src" "$PATCHES" "$HARNESS"; do
  if [ ! -e "$d" ]; then echo "MISSING required path: $d" >&2; exit 2; fi
done
if [ ! -x "$CLANG" ]; then echo "MISSING clang: $CLANG (export CLANG=...)" >&2; exit 2; fi

BUILD="$(mktemp -d -t shard_d_diff.XXXXXX)"
trap 'rm -rf "$BUILD"' EXIT

OLD_SRCS="$SNAP/src/ai_misaka.cpp $SNAP/src/ai_ax.cpp $SNAP/src/ai_farter.cpp $SNAP/src/integer_utils.cpp"
NEW_SRCS="$REPO/src/ai_misaka.cpp $REPO/src/ai_ax.cpp $REPO/src/ai_farter.cpp $REPO/src/integer_utils.cpp"
EXTRA_DEF=""
OLD_INC="-I$SNAP/src"
NEW_INC="-I$REPO/src"

if [ "$MODE" = "head-index" ]; then
  EXTRA_DEF="-DPAIR2_SKIP_EMPTY_AX"
  mkdir -p "$BUILD/ovr_old" "$BUILD/ovr_new"
  cp "$PATCHES/ai_ax_h.HEAD_pristine.h"      "$BUILD/ovr_old/ai_ax.h"
  cp "$PATCHES/ai_ax_h.INDEX_candidate.migrated.h" "$BUILD/ovr_new/ai_ax.h"
  OLD_INC="-I$BUILD/ovr_old $OLD_INC"
  NEW_INC="-I$BUILD/ovr_new $NEW_INC"
fi

echo "[mode=$MODE] build OLD ..."
$CLANG $FLAGS -DAGENT_OLD $EXTRA_DEF $OLD_INC $HARNESS $OLD_SRCS -o "$BUILD/old" || { echo "OLD BUILD FAILED" >&2; exit 3; }
echo "[mode=$MODE] build NEW ..."
$CLANG $FLAGS -DAGENT_NEW $EXTRA_DEF $NEW_INC $HARNESS $NEW_SRCS -o "$BUILD/new" || { echo "NEW BUILD FAILED" >&2; exit 3; }

"$BUILD/old" > "$BUILD/old.txt" || { echo "OLD RUN FAILED" >&2; exit 4; }
"$BUILD/new" > "$BUILD/new.txt" || { echo "NEW RUN FAILED" >&2; exit 4; }

# 首行 MODE=OLD/NEW 是预期差异，剔除后逐字节比较。
tail -n +2 "$BUILD/old.txt" > "$BUILD/old.body"
tail -n +2 "$BUILD/new.txt" > "$BUILD/new.body"

OLD_N="$(wc -l < "$BUILD/old.body")"
NEW_N="$(wc -l < "$BUILD/new.body")"
echo "old data lines=$OLD_N  new data lines=$NEW_N"

if diff -q "$BUILD/old.body" "$BUILD/new.body" >/dev/null; then
  echo "PASS: OLD vs NEW unit eval/get output byte-identical (mode=$MODE)"
  exit 0
else
  echo "FAIL: OLD vs NEW output differs (mode=$MODE). first hunks:" >&2
  diff "$BUILD/old.body" "$BUILD/new.body" | head -40 >&2
  exit 1
fi
