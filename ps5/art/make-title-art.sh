#!/usr/bin/env bash
# Wind Waker HD for the PlayStation 5 - the title's art, from the pictures in this folder.
#
#   ps5/art/make-title-art.sh
#
#   icon0-source.png  ->  ps5/sce_sys/icon0.png   the home-screen tile, 512x512
#   pic-source.png    ->  ps5/sce_sys/pic0.dds    the background behind the title, 3840x2160
#                     ->  ps5/sce_sys/pic1.dds    the splash while it starts
#
# Both pictures are the port's own (a sailing boat and islands, made for it by
# Gary Riches with an image generator): nothing in them is from the game.
# Needs Python with Pillow and numpy.
#
# SPDX-License-Identifier: MIT
set -euo pipefail
art=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
ps5=$(dirname "$art")
python3 - "$art/icon0-source.png" "$ps5/sce_sys/icon0.png" <<'PY'
import sys
from PIL import Image
Image.open(sys.argv[1]).convert("RGB").resize((512, 512), Image.LANCZOS).save(sys.argv[2])
PY
python3 "$ps5/tools/png-to-title-dds.py" "$art/pic-source.png" "$ps5/sce_sys/pic0.dds"
cp "$ps5/sce_sys/pic0.dds" "$ps5/sce_sys/pic1.dds"
ls -l "$ps5/sce_sys"
