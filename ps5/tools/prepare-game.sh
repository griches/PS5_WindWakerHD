#!/usr/bin/env bash
# Wind Waker HD for the PlayStation 5 - your dump into the two things a build needs.
#
#   ps5/tools/prepare-game.sh IMAGE      a disc image (.wux/.wud, with IMAGE's .key and common.key
#                                        beside it), a Cemu archive (.wua) or an extracted folder
#
#   game/    the game's files (code, content, meta), which go to the console beside the title
#   gen/     the game's code translated to C (PS5_GEN_DIR, default $PS5_BUILD_DIR/../gen)
#
# Both come from your own copy of the game and stay on your machine: .gitignore
# keeps them out of the repository.
#
# SPDX-License-Identifier: MIT
set -euo pipefail
ps5=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
root=$(dirname "$ps5")
wwhd="$root/vendor/wwhd"
build=${PS5_BUILD_DIR:-$root/build/game}
gen=${PS5_GEN_DIR:-$(dirname "$build")/gen}
image=${1:?usage: prepare-game.sh IMAGE|FOLDER}
host="$(dirname "$build")/host"
mkdir -p "$host"

if [[ -d $image ]]; then
    game=$image
else
    game="$root/game"
    if [[ ! -f $game/code/cking.rpx ]]; then
        extract="$host/wwhd-extract"
        [[ -x $extract ]] || (cd "$wwhd/tools/wudextract" &&
            c++ -std=c++20 -O2 -I. crypto.cpp wwhd_extract.cpp zarchive.cpp -lzstd -o "$extract")
        case $image in
        *.wua) "$extract" --title 0005000010143500 extract "$image" "$game" ;;
        *) "$extract" extract "$image" "$game" ;;
        esac
    fi
fi
bash "$ps5/tools/setup-source.sh"
python3 "$wwhd/tools/recomp/recomp.py" "$game/code/cking.rpx" "$gen"
