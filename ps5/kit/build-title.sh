#!/usr/bin/env bash
# Wind Waker HD for the PlayStation 5 - the builder kit: your dump into the title.
#
#   build-title.sh IMAGE [OUT]      OUT/PPSA99306/, the folder for /data/homebrew on the console
#   build-title.sh --tools          only build the kit's two small host tools (the container does)
#
# IMAGE is your own dump of the game: a disc image (.wux or .wud, with its disc
# key IMAGE's .key and the Wii U common key common.key beside it), a Cemu
# archive (.wua), or a folder already extracted (code/, content/, meta/).
# OUT defaults to ./out.
#
# What it does, all on this machine:
#   1. extracts the game's files                        OUT/PPSA99306/game/
#   2. translates the game's program to C               OUT/.work/gen/
#   3. compiles that C for the console                  OUT/.work/obj/   (the long step)
#   4. links it with the kit's objects and RADV, and makes the title's eboot.bin
#
# The kit has no game code and no game data in it. What this makes does: the
# title folder is yours alone, like the dump it came from.
#
# It runs on Linux with LLVM 22 (clang, lld, llvm-ar), Python 3, a C++ compiler
# and the zstd and zlib headers; make-title.sh runs it in a container that has them.
#
#   JOBS        compilers at once (default: one a processor; each can take 1-2 GB)
#   PS5_CLANG   the clang to use (default: clang-22, or /usr/lib/llvm-22/bin/clang)
#
# SPDX-License-Identifier: MIT
set -euo pipefail
kit=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
sdk="$kit/sdk" vulkan="$kit/vulkan" wwhd="$kit/vendor/wwhd"
say() { printf '==> %s\n' "$*"; }
fail() { echo "build-title.sh: $*" >&2; exit 2; }

# ---- the kit's host tools: the dump extractor and the ELF -> fake SELF tool
build_tools() {
    local host_cxx=${CXX:-c++} native="$vulkan/tooling/native"
    if [[ ! -x $kit/bin/wwhd-extract ]]; then
        mkdir -p "$kit/bin"
        (cd "$wwhd/tools/wudextract" &&
            "$host_cxx" -std=c++20 -O2 -I. crypto.cpp wwhd_extract.cpp zarchive.cpp -lzstd -o "$kit/bin/wwhd-extract")
    fi
    if [[ ! -x $vulkan/build/host/ps5-native-tool ]]; then
        mkdir -p "$vulkan/build/host"
        "$host_cxx" -std=c++20 -O2 "$native/native_app_builder.cpp" "$native/self_container.cpp" \
            "$native/elf_object.cpp" "$native/sce_module_writer.cpp" -lz -o "$vulkan/build/host/ps5-native-tool"
    fi
}
if [[ ${1:-} == --tools ]]; then
    build_tools
    exit 0
fi

image=${1:-}
[[ -n $image ]] || { sed -n '2,24p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//' >&2; exit 2; }
[[ -e $image ]] || fail "$image does not exist"
out=${2:-$PWD/out}
mkdir -p "$out"
out=$(cd -- "$out" && pwd)
work="$out/.work"
jobs=${JOBS:-$(nproc)}
title_id=$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["titleId"])' "$kit/ps5/sce_sys/param.json")
app="$out/$title_id"
mkdir -p "$work/bin" "$work/obj" "$app"

# ---- LLVM 22: what the kit's objects were built and tested with
real_clang=${PS5_CLANG:-}
if [[ -z $real_clang ]]; then
    for candidate in /usr/lib/llvm-22/bin/clang clang-22; do
        if command -v "$candidate" > /dev/null; then real_clang=$(command -v "$candidate"); break; fi
    done
fi
[[ -n $real_clang ]] || fail "clang 22 was not found (set PS5_CLANG, or use make-title.sh)"
real_clang=$(readlink -f "$real_clang")
llvm_bin=$(dirname "$real_clang")
for tool in ld.lld llvm-ar llvm-nm llvm-config; do
    [[ -x $llvm_bin/$tool ]] || fail "$llvm_bin/$tool is missing (LLVM's lld and tools, beside clang)"
done
export LLVM_CONFIG="$llvm_bin/llvm-config"
# The link asks the compiler where its builtins for the console's processor are; a Linux on
# arm64 has none for x86-64, so the kit carries them and answers for it.
cat > "$work/bin/clang" <<WRAPPER
#!/bin/sh
if [ "\$#" -eq 1 ] && [ "\$1" = "--print-resource-dir" ]; then echo "$kit/clang-rt"; exit 0; fi
exec "$real_clang" "\$@"
WRAPPER
chmod +x "$work/bin/clang"
export PS5_CLANG="$work/bin/clang"

build_tools

# ---- 1. the game's files
if [[ -d $image ]]; then
    game=$(cd -- "$image" && pwd)
    [[ -f $game/code/cking.rpx ]] || fail "$image is not an extracted game (no code/cking.rpx)"
else
    game="$app/game"
    if [[ ! -f $game/code/cking.rpx ]]; then
        say "extracting $(basename "$image")"
        case $image in
        # (the game's title in the archive: the USA disc's, or the European one's)
        *.wua) "$kit/bin/wwhd-extract" --title 0005000010143500 extract "$image" "$game" ||
            "$kit/bin/wwhd-extract" --title 0005000010143600 extract "$image" "$game" ;;
        *) "$kit/bin/wwhd-extract" extract "$image" "$game" ;;
        esac
    fi
fi

# ---- 2. the game's program, translated to C (again only when the program or the kit changed)
gen="$work/gen"
stamp="$(sha256sum "$game/code/cking.rpx" | cut -d' ' -f1) $(<"$kit/VERSION")"
if [[ ! -f $gen/table.c || $(cat "$gen/.stamp" 2>/dev/null) != "$stamp" ]]; then
    say "translating the game's program to C"
    rm -rf -- "$gen" "$work/obj"
    mkdir -p "$gen" "$work/obj"
    python3 "$wwhd/tools/recomp/recomp.py" "$game/code/cking.rpx" "$gen"
    echo "$stamp" > "$gen/.stamp"
fi

# ---- 3. compile it: the flags of the build the kit was made from
sources=("$gen"/code_*.c "$gen/table.c" "$gen/imports.c")
say "compiling ${#sources[@]} files, $jobs at a time (this is the long step)"
compile() {
    local source=$1 object="$2/$(basename "$1").o"
    [[ -f $object && $object -nt $source ]] && return 0
    sh "$kit/ps5/tools/cc-init-array.sh" "$sdk/bin/prospero-clang" --sysroot="$sdk/target" \
        -I"$wwhd/runtime/include" -I"$(dirname "$source")" -DNDEBUG -std=gnu11 -fPIC -g1 \
        -ffunction-sections -fdata-sections -fno-omit-frame-pointer -march=znver2 \
        -O3 -ffp-contract=off -fno-strict-aliasing -w -o "$object.tmp" -c "$source"
    mv "$object.tmp" "$object"
    echo "    $(basename "$source")"
}
export -f compile
export kit sdk wwhd
printf '%s\0' "${sources[@]}" | xargs -0 -P "$jobs" -I{} bash -c 'compile "$1" "$2"' _ {} "$work/obj"
objects=()
for source in "${sources[@]}"; do
    objects+=("$work/obj/$(basename "$source").o")
    [[ -f ${objects[-1]} ]] || fail "$(basename "$source") did not compile"
done
rm -f "$work/libgamecode.a"
"$llvm_bin/llvm-ar" qc "$work/libgamecode.a" "${objects[@]}"
"$llvm_bin/llvm-ranlib" "$work/libgamecode.a" 2>/dev/null || "$llvm_bin/llvm-ar" s "$work/libgamecode.a"

# ---- 4. the link the kit's build used, with your game code where it had its own
say "linking"
inputs=()
while IFS= read -r line; do
    case $line in
    @GAMECODE@) inputs+=("$work/libgamecode.a") ;;
    -*) inputs+=("$line") ;;
    *) inputs+=("$kit/lib/$line") ;;
    esac
done < "$kit/lib/link.txt"
PS5_DIST_DIR="$out" bash "$kit/ps5/tools/link-title.sh" "$work" "$vulkan" "$sdk" "${inputs[@]}"
cp "$kit/LICENSING.md" "$app/licenses/"

# a container runs as root: hand the folder to whoever owns OUT
if [[ $(id -u) -eq 0 ]]; then
    owner=$(stat -c '%u:%g' "$out")
    [[ $owner == 0:0 ]] || chown -R "$owner" "$out" 2>/dev/null || true
fi
say "$app"
if [[ $game != "$app/game" ]]; then
    echo "    the game's files stayed where they were: copy $game to $title_id/game on the console as well"
fi
echo "    copy the folder $title_id to /data/homebrew/ on the console (FTP). It is your game: do not share it."
