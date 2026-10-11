#!/usr/bin/env bash
# Wind Waker HD for the PlayStation 5 - the builder kit: everything of a build but the game.
#
#   ps5/tools/package-kit.sh [VERSION]      dist/kit/wwhd-ps5-kit-<VERSION>/ and its .zip
#
# A built title cannot be shared: eboot.bin holds the game's code. The kit is the
# rest of one, as it stands after tools/build.sh: the port's own objects (the
# runtime, the renderer, SDL3, glslang), RADV, the SDK's headers, libraries and
# link scripts, the recompiler, and kit/build-title.sh, which on somebody else's
# machine translates their dump, compiles it and links the same title with the
# same recipe (tools/link-title.sh). What goes in is listed here, file by file;
# the game's code (libgamecode.a, the generated C) and game/ never do, and the
# script checks that before it finishes.
#
# The link line is the build's own (ninja's), so the kit links what was tested.
#
# SPDX-License-Identifier: MIT
set -euo pipefail

ps5=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
root=$(dirname "$ps5")
vulkan=$(cd -- "${PS5_VULKAN_DIR:-$root/../PS5_Vulkan}" && pwd)
sdk=$(bash "$ps5/tools/setup-sdk.sh")
build=${PS5_BUILD_DIR:-$root/build/game}
ninja=$(command -v ninja || echo "$HOME/.local/bin/ninja")
export PS5_CLANG=${PS5_CLANG:-$(command -v clang || true)}
version=${1:-$(python3 -c 'import json,sys; v=json.load(open(sys.argv[1]))["contentVersion"].split("."); print("%d.%d.%d" % tuple(int(x) for x in v))' "$ps5/sce_sys/param.json")}
name=wwhd-ps5-kit-$version
kit="$root/dist/kit/$name"

# the kit is the build as the sources stand now: bring it up to date first
bash "$ps5/tools/build.sh"

rm -rf -- "$kit"
mkdir -p "$kit/lib" "$kit/ps5/tools"

# ---- the title's link inputs, in the build's order, without the game's code
# (ninja's command: ... link-title.sh BUILD VULKAN SDK INPUT... && :)
mapfile -t inputs < <("$ninja" -C "$build" -t commands wwhd | tail -1 |
    sed -e 's/.*link-title\.sh //' -e 's/ && :$//' | tr ' ' '\n' | sed -n '4,$p' | sed '/^$/d')
strip="$sdk/bin/llvm-strip"
: > "$kit/lib/link.txt"
game_code=0
for input in "${inputs[@]}"; do
    case $input in
    -*) echo "$input" >> "$kit/lib/link.txt"; continue ;;
    libgamecode.a | */libgamecode.a) echo "@GAMECODE@" >> "$kit/lib/link.txt"; game_code=$((game_code + 1)); continue ;;
    esac
    file=$input
    [[ $file == /* ]] || file="$build/$input"
    [[ -f $file ]] || { echo "package-kit.sh: link input $input is missing" >&2; exit 2; }
    relative=${file#"$build"/}
    [[ $relative != /* ]] || { echo "package-kit.sh: link input $input is outside the build tree" >&2; exit 2; }
    if [[ ! -f $kit/lib/$relative ]]; then
        mkdir -p "$(dirname "$kit/lib/$relative")"
        # no debug information: it names this machine's folders, and is most of the size
        "$strip" --strip-debug -o "$kit/lib/$relative" "$file"
    fi
    echo "$relative" >> "$kit/lib/link.txt"
done
[[ $game_code -eq 1 ]] || { echo "package-kit.sh: expected the game's code once in the link line, found it $game_code times" >&2; exit 2; }

# ---- the port: the link, the title's own files, the recompiler and what the game's C includes
cp "$ps5/tools/link-title.sh" "$ps5/tools/cc-init-array.sh" "$kit/ps5/tools/"
cp -R "$ps5/sce_sys" "$kit/ps5/sce_sys"
mkdir -p "$kit/vendor/wwhd/tools" "$kit/vendor/wwhd/runtime"
cp "$root/vendor/wwhd/LICENSE" "$kit/vendor/wwhd/"
cp "$root/vendor/wwhd/tools/rpx.py" "$kit/vendor/wwhd/tools/"
cp -R "$root/vendor/wwhd/tools/recomp" "$root/vendor/wwhd/tools/wudextract" "$kit/vendor/wwhd/tools/"
cp -R "$root/vendor/wwhd/runtime/include" "$kit/vendor/wwhd/runtime/include"
find "$kit/vendor" -name __pycache__ -prune -exec rm -rf {} +
cp "$ps5/kit/build-title.sh" "$ps5/kit/make-title.sh" "$ps5/kit/Dockerfile" "$kit/"
cp "$ps5/kit/README.md" "$kit/README.md"
chmod +x "$kit/build-title.sh" "$kit/make-title.sh"

# ---- the payload SDK: compiler wrappers, link scripts, headers, libraries
mkdir -p "$kit/sdk/target"
cp -R "$sdk/bin" "$sdk/ldscripts" "$kit/sdk/"
cp -R "$sdk/target/include" "$sdk/target/lib" "$kit/sdk/target/"
cp "$sdk/.ps5-sdk-revision" "$kit/sdk/"

# ---- PS5_Vulkan: RADV, the link recipe, the CRT, the fake-SELF tool's source, libc.prx
radv="$vulkan/.deps/native/radv-release"
mkdir -p "$kit/vulkan/tools" "$kit/vulkan/tooling/psbc" "$kit/vulkan/vendor/ps5/sdk/stubs" \
    "$kit/vulkan/.deps/native/radv-release/lib" "$kit/vulkan/runtime"
cp "$vulkan/LICENSE" "$vulkan/NOTICE.md" "$kit/vulkan/"
cp "$vulkan/tools/radv-link.sh" "$kit/vulkan/tools/"
cp "$vulkan/tooling/prospero-clang18" "$kit/vulkan/tooling/"
cp "$vulkan/tooling/psbc/ps5-pie-unwind.ld" "$kit/vulkan/tooling/psbc/"
mkdir -p "$kit/vulkan/tooling/native"
cp "$vulkan"/tooling/native/*.cpp "$vulkan"/tooling/native/*.hpp "$vulkan"/tooling/native/*.map \
    "$vulkan"/tooling/native/*.ld "$kit/vulkan/tooling/native/"
cp "$vulkan/vendor/ps5/sdk/stubs/agc_canary_link_stub.c" "$vulkan/vendor/ps5/sdk/stubs/agc_driver_canary_link_stub.c" \
    "$kit/vulkan/vendor/ps5/sdk/stubs/"
cp "$vulkan/runtime/libc.prx" "$vulkan/runtime/libc.prx.sha256" "$vulkan/runtime/README.md" "$kit/vulkan/runtime/"
cp "$radv/PROVENANCE.txt" "$kit/vulkan/.deps/native/radv-release/"
"$strip" --strip-debug -o "$kit/vulkan/.deps/native/radv-release/lib/libvulkan_radeon.ps5.a" "$radv/lib/libvulkan_radeon.ps5.a"

# ---- Clang's builtins for the console's processor: a Linux on arm64 does not install x86-64's
builtins="$("$PS5_CLANG" --print-resource-dir)/lib/linux/libclang_rt.builtins-x86_64.a"
[[ -f $builtins ]] || { echo "package-kit.sh: missing $builtins" >&2; exit 2; }
mkdir -p "$kit/clang-rt/lib/linux"
cp "$builtins" "$kit/clang-rt/lib/linux/"

# ---- licences, and where each part's source is
mkdir -p "$kit/licenses"
cp "$root/LICENSE" "$kit/LICENSE"
cp "$root/LICENSING.md" "$kit/"
cp "$root"/licenses/* "$kit/licenses/"
sdl_revision=$(git -C "$root/.deps/SDL" describe --tags --always 2>/dev/null || echo unknown)
{
    echo "The builder kit $version of Wind Waker HD for the PlayStation 5 was built from:"
    echo
    echo "PS5_WindWakerHD  https://github.com/griches/PS5_WindWakerHD            $(git -C "$root" rev-parse HEAD)"
    echo "ZeldaWWHDRecomp  https://github.com/ZeldaWWHDRecomp/ZeldaWWHDRecomp    $(git -C "$root/vendor/wwhd" rev-parse HEAD) (+ ps5/patches/wwhd.patch)"
    echo "PS5_PayloadSDK   https://github.com/mihawk-99/PS5_PayloadSDK           $(<"$sdk/.ps5-sdk-revision")"
    echo "PS5_Vulkan       https://github.com/mihawk-99/PS5_Vulkan               $(git -C "$vulkan" rev-parse HEAD)"
    echo "PS5_Mesa (RADV)  https://github.com/mihawk-99/PS5_Mesa                 $(sed -n 's/^revision: //p' "$radv/PROVENANCE.txt")"
    echo "SDL3             https://github.com/libsdl-org/SDL                     $sdl_revision (+ ps5/patches/sdl.patch, ps5/sdl)"
    echo "Clang builtins   https://github.com/llvm/llvm-project                  $("$PS5_CLANG" --version | head -1)"
    echo
    echo "glslang, LZ4, Dear ImGui and the smaller libraries are at the revisions ZeldaWWHDRecomp pins."
    echo "LICENSING.md says which licence covers which part."
} > "$kit/SOURCES.txt"
echo "$version" > "$kit/VERSION"

# ---- nothing of the game: no generated C, no game code archive, no dump
leaks=$(find "$kit" \( -name 'code_[0-9][0-9][0-9].c*' -o -name 'libgamecode*' -o -name 'table.c' -o -name 'imports.c' -o -name 'funcs.h' \
    -o -name '*.rpx' -o -name '*.wux' -o -name '*.wud' -o -name '*.wua' -o -name '*.key' -o -name '*.szs' \) -print)
[[ -z $leaks ]] || { echo "package-kit.sh: these must not be in a kit:" >&2; echo "$leaks" >&2; exit 1; }
(cd "$root/dist/kit" && rm -f "$name.zip" && zip -qry "$name.zip" "$name")
printf '==> %s (%s unpacked, %s zipped)\n' "$root/dist/kit/$name.zip" \
    "$(du -sh "$kit" | cut -f1)" "$(du -sh "$root/dist/kit/$name.zip" | cut -f1)"
sha256sum "$root/dist/kit/$name.zip"
