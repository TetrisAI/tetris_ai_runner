#!/usr/bin/env bash
set -euo pipefail
REPO="$(cd "$(dirname "$0")/.." && pwd)"
PROJECT_ROOT="$(cd "$REPO/.." && pwd)"
CXX=/opt/homebrew/opt/llvm/bin/clang++
[[ -x "$CXX" ]] || CXX=clang++
SOURCE_REVISION="source-package"
if git -C "$REPO" rev-parse --is-inside-work-tree >/dev/null 2>&1; then
  SOURCE_REVISION="$(git -C "$REPO" rev-parse --short=12 HEAD)"
  if ! git -C "$REPO" diff --quiet --ignore-submodules -- ||
     ! git -C "$REPO" diff --cached --quiet --ignore-submodules --; then
    SOURCE_REVISION="${SOURCE_REVISION}-dirty"
  fi
fi
printf 'Native source identity: %s\n' "$SOURCE_REVISION"
for arch in arm64 x86_64; do
  build="$REPO/build_cultris_$arch"
  rm -f "$build/CMakeCache.txt"
  cmake -S "$REPO/cultris" -B "$build" -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES="$arch" -DCMAKE_CXX_COMPILER="$CXX" -DC2AI_SOURCE_REVISION="$SOURCE_REVISION"
  cmake --build "$build" --target cultrisai --config Release -- -j1
done
mkdir -p "$PROJECT_ROOT/libs"
lipo -create "$REPO/build_cultris_arm64/libcultrisai.dylib" "$REPO/build_cultris_x86_64/libcultrisai.dylib" -output "$PROJECT_ROOT/libs/libcultrisai.dylib"
lipo -info "$PROJECT_ROOT/libs/libcultrisai.dylib"
