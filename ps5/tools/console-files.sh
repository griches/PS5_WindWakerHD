#!/usr/bin/env bash
# Wind Waker HD for the PlayStation 5 - tools/console-files.py (so that vm.sh can run it).
# SPDX-License-Identifier: MIT
exec python3 "$(dirname -- "${BASH_SOURCE[0]}")/console-files.py" "$@"
