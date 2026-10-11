#!/usr/bin/env bash
# Wind Waker HD for the PlayStation 5 - the builder kit, run in a container.
#
#   make-title.sh IMAGE [OUT]       OUT/PPSA99306/ from your dump (OUT defaults to ./out)
#
# IMAGE is your own dump: game.wux or game.wud (with game.key and common.key in
# the same folder), a Cemu .wua archive, or an extracted folder. Needs Docker
# (or Podman: CONTAINER=podman). The first run builds the container's image,
# which downloads a compiler; every run then works offline and reads the dump
# read-only. See README.md.
#
#   JOBS        compilers at once (default: one a processor; lower it if the build is killed
#               for memory: each can take 1-2 GB)
#
# SPDX-License-Identifier: MIT
set -euo pipefail
kit=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
image=${1:?usage: make-title.sh IMAGE [OUT]}
[[ -e $image ]] || { echo "make-title.sh: $image does not exist" >&2; exit 2; }
out=${2:-$PWD/out}
mkdir -p "$out"
out=$(cd -- "$out" && pwd)
container=${CONTAINER:-docker}
command -v "$container" > /dev/null || { echo "make-title.sh: $container was not found: install Docker Desktop (or Podman)" >&2; exit 2; }
tag=wwhd-ps5-kit:$(<"$kit/VERSION")

"$container" image inspect "$tag" > /dev/null 2>&1 || "$container" build -t "$tag" "$kit"
if [[ -d $image ]]; then
    dump=$(cd -- "$image" && pwd) inside=/dump
else
    dump=$(cd -- "$(dirname "$image")" && pwd) inside=/dump/$(basename "$image")
fi
exec "$container" run --rm ${JOBS:+-e JOBS="$JOBS"} -v "$dump":/dump:ro -v "$out":/out "$tag" "$inside" /out
