#!/usr/bin/env python3
# Wind Waker HD for the PlayStation 5 - where a sampled thread spent its time.
# SPDX-License-Identifier: MIT
"""Turns samples.txt (ps5/src/sampler_ps5.cpp) into a table of functions.

  symbolise-samples.py SAMPLES ELF [--top N] [--symbolizer llvm-symbolizer]

Two tables: "self" counts the function the thread was in, "with callers" counts
every function on the sampled part of its stack once a sample (so a function is
charged for what it calls).
"""
import argparse
import collections
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument("samples")
parser.add_argument("elf")
parser.add_argument("--top", type=int, default=40)
parser.add_argument("--symbolizer", default="llvm-symbolizer")
args = parser.parse_args()

# "at NAME ADDRESS": where a function the ELF names was at run time (the title is loaded at a
# different address than it is linked for)
slide = 0
rows = []
for line in open(args.samples):
    words = line.split()
    if not words:
        continue
    if words[0] == "at":
        listing = subprocess.run([args.symbolizer.replace("symbolizer", "nm"), "--defined-only", args.elf],
                                 capture_output=True, text=True, check=True).stdout
        linked = next(int(l.split()[0], 16) for l in listing.splitlines() if l.split()[-1] == words[1])
        slide = int(words[2], 16) - linked
        continue
    rows.append([int(x, 16) - slide if int(x, 16) else 0 for x in words])
rows = [r for r in rows if r and r[0]]
addresses = sorted({a for r in rows for a in r if a})
out = subprocess.run([args.symbolizer, "--obj=" + args.elf, "--functions=short", "--no-inlines", "--output-style=GNU"],
                     input="\n".join(hex(a) for a in addresses), capture_output=True, text=True, check=True).stdout
names = {}
lines = out.splitlines()
for i, a in enumerate(addresses):
    names[a] = lines[2 * i] if 2 * i < len(lines) else "?"
self_count = collections.Counter(names[r[0]] for r in rows)
inclusive = collections.Counter()
for r in rows:
    for name in {names[a] for a in r if a}:
        inclusive[name] += 1
total = len(rows)
print(f"{total} samples")
for title, table in (("self", self_count), ("with callers", inclusive)):
    print(f"\n{title}:")
    for name, n in table.most_common(args.top):
        print(f"  {100.0 * n / total:5.1f}%  {name[:110]}")
