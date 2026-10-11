#!/usr/bin/env bash
# Wind Waker HD for the PlayStation 5 - build the game into dist/<TITLE_ID>/.
#
#   ps5/tools/build.sh [ninja arguments]
#
# Needs the game's code translated to C first (tools/prepare-game.sh, from your
# own dump): nothing of the game is in this repository. Applies the console's
# patch to vendor/wwhd (tools/setup-source.sh), configures it with the payload
# SDK's CMake toolchain (once) and ps5/cmake/ps5-game.cmake, which gives it SDL3
# and Vulkan for the console and the title's link (tools/link-title.sh).
#
#   PS5_BUILD_DIR    the build tree (default build/game; the Linux VM's own disk
#                    is several times faster than the shared folder)
#   PS5_GEN_DIR      the translated game code (default $PS5_BUILD_DIR/../gen)
#   PS5_VULKAN_DIR   the PS5_Vulkan checkout (default ../PS5_Vulkan)
#
# SPDX-License-Identifier: MIT
set -euo pipefail

ps5=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
root=$(dirname "$ps5")
vulkan=$(cd -- "${PS5_VULKAN_DIR:-$root/../PS5_Vulkan}" && pwd)
bash "$ps5/tools/setup-source.sh"
sdl=$(bash "$ps5/tools/setup-sdl.sh")
sdk=$(bash "$ps5/tools/setup-sdk.sh")
build=${PS5_BUILD_DIR:-$root/build/game}
gen=${PS5_GEN_DIR:-$(dirname "$build")/gen}
ninja=$(command -v ninja || echo "$HOME/.local/bin/ninja")
export PS5_CLANG=${PS5_CLANG:-$(command -v clang || true)}
[[ -f $gen/table.c ]] || { echo "build.sh: no translated game code in $gen: run ps5/tools/prepare-game.sh first" >&2; exit 2; }

if [[ ! -f $build/build.ninja ]]; then
    mkdir -p "$build"
    cmake -S "$root/vendor/wwhd" -B "$build" -G Ninja -DCMAKE_MAKE_PROGRAM="$ninja" \
        -DCMAKE_TOOLCHAIN_FILE="$sdk/toolchain/prospero.cmake" \
        -DCMAKE_PROJECT_wwhd_recomp_INCLUDE="$ps5/cmake/ps5-game.cmake" \
        -DPS5_PORT_DIR="$root" -DPS5_VULKAN_DIR="$vulkan" -DSDL_DIR="$sdl" -DGEN_DIR="$gen" \
        -DCMAKE_BUILD_TYPE=Release -DCMAKE_VERBOSE_MAKEFILE=OFF > "$build/configure.log" 2>&1 \
        || { tail -60 "$build/configure.log" >&2; exit 1; }
fi
"$ninja" -C "$build" "${@:-wwhd}"
