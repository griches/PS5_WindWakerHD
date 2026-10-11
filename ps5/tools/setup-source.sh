#!/usr/bin/env bash
# Wind Waker HD for the PlayStation 5 - the recompilation's sources, with the console's changes.
#
#   ps5/tools/setup-source.sh    apply ps5/patches/wwhd.patch to vendor/wwhd
#
# vendor/wwhd is ZeldaWWHDRecomp at an upstream revision. What the console needs
# changed in it is kept here as a patch, not as a fork. A patch already applied
# is left alone, so this can run before every build.
#
# SPDX-License-Identifier: MIT
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)
wwhd="$root/vendor/wwhd"
patch="$root/ps5/patches/wwhd.patch"
[[ -f $wwhd/CMakeLists.txt ]] || git -C "$root" submodule update --init vendor/wwhd
[[ -s $patch ]] || exit 0
if git -C "$wwhd" apply --reverse --check "$patch" 2>/dev/null; then
    exit 0 # applied already
fi
git -C "$wwhd" apply --check "$patch" || {
    echo "setup-source.sh: wwhd.patch does not apply to $wwhd (a different upstream revision?)" >&2
    exit 2
}
git -C "$wwhd" apply --whitespace=nowarn "$patch"
echo "setup-source.sh: applied wwhd.patch" >&2
