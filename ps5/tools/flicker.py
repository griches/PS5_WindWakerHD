#!/usr/bin/env python3
# flicker.py CLIP...: per second, the ratio d2/d1 around Link (lower centre of the picture):
# d1 = mean difference of consecutive frames, d2 = of frames two apart. Smooth motion gives 1.3-2;
# a 30 Hz flicker (every other frame off) makes consecutive frames differ more than frames two
# apart, and the ratio falls towards and below 1.
import subprocess, sys, numpy as np
W, H = 320, 250
def load(path):
    raw = subprocess.run(["ffmpeg", "-v", "error", "-i", path, "-vf", f"crop=640:500:640:520,scale={W}:{H},format=gray", "-f", "rawvideo", "-"],
                         capture_output=True).stdout
    return np.frombuffer(raw, dtype=np.uint8).reshape(-1, H, W).astype(np.int16)
rows = {}
for path in sys.argv[1:]:
    f = load(path)
    d1 = np.abs(f[1:] - f[:-1]).mean(axis=(1, 2)); d2 = np.abs(f[2:] - f[:-2]).mean(axis=(1, 2))
    out = []
    for s in range(len(f) // 60):
        a = d1[s*60:(s+1)*60].mean(); b = d2[s*60:(s+1)*60 - 1].mean()
        out.append((a, b / a if a > 0.05 else float("nan")))
    rows[path] = out
names = [p.split("run-")[-1].replace(".webm", "") for p in sys.argv[1:]]
print("sec  " + "   ".join(f"{n:>14s}" for n in names) + "    (motion d1, ratio d2/d1)")
for s in range(min(len(r) for r in rows.values())):
    print(f"{s:3d}  " + "   ".join(f"{rows[p][s][0]:6.2f} {rows[p][s][1]:6.2f}" for p in sys.argv[1:]))
