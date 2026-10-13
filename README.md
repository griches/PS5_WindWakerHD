# Wind Waker HD for PlayStation 5

[![stars](https://img.shields.io/github/stars/griches/PS5_WindWakerHD?style=flat)](https://github.com/griches/PS5_WindWakerHD/stargazers)
[![release](https://img.shields.io/github/v/release/griches/PS5_WindWakerHD)](https://github.com/griches/PS5_WindWakerHD/releases/latest)
[![licence](https://img.shields.io/badge/licence-GPL--3.0-blue)](LICENSING.md)

A PS5 homebrew port of [ZeldaWWHDRecomp](https://github.com/ZeldaWWHDRecomp/ZeldaWWHDRecomp),
the static recompilation of the Wii U version of a classic sailing adventure. The
game's PowerPC code is translated to C ahead of time and compiled for the console;
its Wii U graphics calls (GX2) are drawn by the recompilation's own Vulkan renderer
on RADV, Mesa's Vulkan driver, from the [PS5 Vulkan](https://github.com/mihawk-99/PS5_Vulkan)
stack.

It needs a PS5 that already runs homebrew, and **your own dump of the game**, which
you prepare and build with on your own machine. No game code, game data, Nintendo
artwork, Sony software or keys are included here.

This is an unofficial port. It is not part of ZeldaWWHDRecomp and is not supported
by it: please do not take PS5 problems to them.

## Status

Tested on one console (firmware 13.60), on 2026-10-11, with the USA disc
(title 00050000-10143500, version 0). "Seen" means it ran there and was looked at,
frame by frame through [PS5 DevLoop](https://github.com/griches/PS5_DevLoop), or
read in the console's log; nothing else is claimed.

| Area | Seen on the console | Not yet verified |
| --- | --- | --- |
| Start-up | Boots to the title screen; a new Quest Log through the prologue and the first cutscene into play; a saved Quest Log continued after a restart | |
| Rendering | 3840x2160 output on a 59.94 Hz display; the title screen, menus, the prologue, the first island and its characters look right; the recompilation's renderer self-test passes | Anything past the first island; a comparison with a Wii U, frame for frame; 120 Hz |
| Frame rate | 60 fps (frame interpolation, "Keep game speed") on the first island, the game at full speed: on the beach and the pier, and in the view from the pier across the village with the pirate ship in the bay, the heaviest found (4,350 draws a pass), standing and with the camera turning: every in-between frame drawn, 59.7 to 60.1 frames and 29.9 to 30.1 game steps a second over 100-second runs, frame intervals 16.7 ms at the median and 18 ms at the 95th percentile | A long session; later, heavier areas |
| 30 fps | The game's own rate, chosen in Settings: 30.0 frames and 30.0 game steps a second on the island's pier | |
| Settings | Opened with the touchpad button and driven from the pad: tabs, frame rate, internal resolution, save states; choices kept across a restart | Every option in it |
| Text entry | The console's own keyboard for the name of a new Quest Log, and the name it gave back in the game | |
| Button prompts | The DualSense's symbols in the HUD and the pause menu: Cross, Circle, Square, Triangle, R1, R2 | L1 and L2; prompts inside message text (still the Wii U's); French and Spanish |
| Input | Menus, dialogue, walking and the camera from the sticks, the pause menu; "Wii U GamePad" chosen on the selection screen and the game still played, menus on the TV | Each button in play; rumble; motion; a second controller |
| Saving | The game's own save from the pause menu, found again after a restart; a save state written, listed and loaded | |
| Sound | The output opens (48 kHz stereo) and the game's sound threads run | What it sounds like: nobody has listened yet |
| Exit | The title closes through the shell when a test run asks it to | Closing it from the home screen (the usual way: the game has no quit of its own) |

Known limits:

- **60 fps is frame interpolation.** The game runs 30 steps a second and each
  is drawn twice. In the heaviest view found on the first island the two
  passes of a step average 13.7 ms of the render thread a frame, which leaves
  it idle 3 ms. The far shadow cascade of the in-between frame is the step's
  (below): what moves more than about 1,400 units from the camera casts a
  shadow one frame (16 ms) old there. A view whose passes no longer fit shows
  30 frames a second (the in-between frame is left out and the game keeps its
  speed). The game cannot run ahead of the render thread to make room (tried:
  the render thread read a surface the game had reused).
- **Nothing past the first island has been measured.**
- **Button symbols: the HUD and menus, in English.** The game prints a button's
  letter as text over a blank button, in the font of every other HUD label. The
  port prints those few texts as spare characters of that font and paints their
  cells: a cross, circle, square and triangle it draws, and R1, R2, L1, L2 put
  together from the font's own letters and digits (`ps5/src/glyphs_ps5.cpp`;
  `WWHD_PS5_GLYPHS=0` in `env.txt` keeps the Wii U's letters). The symbol is the
  DualSense button that plays that Wii U button, so it follows `Controls > Face
  buttons`. The font is known by its bytes (the USA game's); button pictures
  inside message text come from another font and still show the Wii U's.
- **You make it yourself, from your dump.** The executable contains the game's
  translated code, so no built title can be shared: see "Why there is no download".
  The builder kit does it for you.
- **No downloads from inside the title**: the mod catalogue and updates are off.
- **No code mods.** A title cannot load code at run time.
- **Save states need a Quest Log in play** (upstream's rule): a state asked for
  on the title screen loads once you have started or continued one.

## Why there is no download

Other ports here ship a title folder with no game in it, and you add a disc
image. This one cannot: a static recompilation turns the game's program into the
port's program, so `eboot.bin` *is* the game's code, compiled for another
processor. That file, the `game/` folder and the generated C all stay on the
machine that made them. `.gitignore` keeps them out of the repository, and no
release ever holds a built title.

What a release holds instead is the **builder kit**: everything of a build but
the game (the port's own compiled parts, RADV, the SDK's libraries, the
recompiler) and a script that does the last step on your computer, from your
dump, in a container. With it you need Docker and the dump, not the build
set-up below. Its own `README.md` (`ps5/kit/README.md` here) has the steps.

## Build

The build runs on Linux (x86-64 or arm64; on a Mac, in a Linux VM) with Mihawk's
stack beside this repository:

```text
somewhere/
  PS5_WindWakerHD/   this repository
  PS5_Vulkan/        https://github.com/mihawk-99/PS5_Vulkan   (with its RADV release archive built)
  PS5_Mesa/          https://github.com/mihawk-99/PS5_Mesa
  PS5_PayloadSDK/    https://github.com/mihawk-99/PS5_PayloadSDK
```

Set that stack up first, by its own instructions, until `PS5_Vulkan` has
`.deps/native/radv-release/lib/libvulkan_radeon.ps5.a` and
`build/host/ps5-native-tool`. It needs **LLVM 22**. Then, with your dump (a disc
image `game.wux` or `game.wud` with its disc key `game.key` and the Wii U common
key `common.key` beside it, a Cemu `.wua` archive, or an already extracted folder):

```sh
git clone --recurse-submodules https://github.com/griches/PS5_WindWakerHD.git
cd PS5_WindWakerHD
ps5/tools/prepare-game.sh /path/to/game.wux   # game/ (the files) and the code translated to C
ps5/tools/build.sh                            # dist/PPSA99306/
```

| Tool | What it does |
| --- | --- |
| `prepare-game.sh IMAGE` | Extracts your dump into `game/` and translates its code to C (upstream's `recomp.py`). |
| `build.sh` | Applies `ps5/patches/wwhd.patch` to `vendor/wwhd`, fetches SDL3 and the payload SDK at their pins, compiles and links with RADV, writes `dist/PPSA99306/`. |
| `deploy.sh` | Uploads `dist/PPSA99306/` over FTP (the console's address is `PS5_HOST` in `PS5_Vulkan/.env`). |
| `console-files.sh game` | Uploads `game/` beside the title, skipping what is already there. Also `put`, `get`, `rm`, `ls`. |
| `run.sh` | Launches the title through PS5_Vulkan's control payload and follows its log; symbolises a crash. |
| `package-kit.sh [VERSION]` | Builds, then collects the builder kit into `dist/kit/`: the build without the game's code, and `ps5/kit/`'s scripts. |
| `vm.sh TOOL` | From macOS, runs one of the above inside a Lima VM named `ps5`. |
| `symbolise-samples.py` | Turns the profile a thread wrote (`WWHD_PS5_SAMPLE=<thread>:<wait>:<count>` in `env.txt`) into a table of functions. |
| `ps5/art/make-title-art.sh` | Makes `sce_sys/icon0.png`, `pic0.dds` and `pic1.dds` from the pictures in `ps5/art/`. |

## Install

What the console needs: a PS5 that runs homebrew with etaHEN (or an equivalent),
ShadowMountPlus (which shows the folders in `/data/homebrew` as titles), and an
FTP server (port 2121 here).

```text
/data/homebrew/PPSA99306/eboot.bin          from your build
/data/homebrew/PPSA99306/sce_sys/           param.json, icon0.png, pic0.dds, pic1.dds
/data/homebrew/PPSA99306/sce_module/libc.prx
/data/homebrew/PPSA99306/licenses/
/data/homebrew/PPSA99306/game/              code/, content/, meta/ from your dump
```

`deploy.sh` and `console-files.sh game` put all of it there. Settings, the shader
cache, save states, screenshots and the game's saves are written to
`/data/homebrew/PPSA99306/userdata/`; a new build leaves them alone.

## Playing

| DualSense | In the game |
| --- | --- |
| Cross / Circle / Square / Triangle | B / A / Y / X: the Wii U's positions, so the prompts on screen sit where the buttons do (Circle confirms, Cross goes back) |
| L1, R1, L2, R2 | L, R, ZL, ZR |
| Options | + (the game's menu) |
| Touchpad button | **Settings**: frame rate, internal resolution, aspect ratio, save states, controls |
| Sticks, D-pad, L3, R3 | as on a Wii U |

On the game's Controller Selection screen either choice plays the same way, as a
Pro Controller. In Settings, the D-pad or left stick moves, Cross selects, Circle closes, and L1 / R1
change tab. `Controls > Face buttons` has the by-label choice (Cross as A) if you
prefer it; the prompts then show each symbol on the button it plays, away from its place on the pad. When the game asks for text, the console's keyboard opens.

`/data/homebrew/PPSA99306/env.txt`, if you make one, holds upstream's `WWHD_`
options, one `NAME=value` a line.

## How it works

```text
  the game's code, translated to C     build/gen (yours, never shared)
        |
  ZeldaWWHDRecomp's runtime ........... vendor/wwhd, with ps5/patches/wwhd.patch
   Cafe OS, threads, GX2, audio          |              |
        |                              SDL3        ps5/src: memory, keyboard
  its Vulkan renderer                ps5/sdl drivers
        |
  Vulkan: RADV + PS5 winsys, VK_KHR_display       PS5_Mesa, PS5_Vulkan
        |
  PS5 GPU
```

What the console needed, each found on it and explained where it is handled:

- **Guest memory below 1 TiB.** The recompiled code reads guest address `ea` at a
  fixed host base plus `ea`. Upstream's base is 32 TiB; the console grants a title
  nothing at or above 1 TiB, so the base is the one upstream uses for 39-bit
  kernels (`runtime/include/ppc.h`). The 4 GiB range is a reservation with direct
  memory mapped into the regions the game uses (`ps5/src/guestmem_ps5.cpp`): a
  title's anonymous mappings would come out of its small flexible budget.
- **Thread stacks from direct memory.** Each guest thread has a 64 MiB host stack.
  The system takes a stack that size from the same small budget; the seventeenth
  thread was silently not created and the game waited for it for ever.
- **One screen.** SDL3's video driver for the console hands the renderer RADV's
  entry point and a `VK_KHR_display` surface (`ps5/sdl/SDL_ps5video.c`); the
  GamePad's window is never made, and the game's Minus button, which only moves
  the picture to the GamePad, is not mapped.
- **The console's keyboard** in place of the runtime's on-screen one
  (`ps5/src/ime_ps5.cpp`, libSceImeDialog).
- **A Wii U Pro Controller, not a GamePad.** The pad acts as a Pro Controller, so
  the game shows its pause menu, inventory and maps on the TV; as a GamePad it
  puts them on the GamePad's screen. The game's Controller Selection screen
  still offers the GamePad; choosing it changes nothing (the game's request to
  switch is turned into one for the Pro Controller, `ps5/src/controller_ps5.cpp`),
  where before it left the player at a screen that no longer answered.
- **A clock that is not a system call.** `clock_gettime` costs about a
  microsecond here, and the runtime's always-on profiler reads it for every
  operation: a fifth of the render thread's busy time. The monotonic clocks are
  served from the CPU's timestamp counter, anchored to the kernel's clock once a
  second (`ps5/src/shims.c`).
- **Built for a Zen 2** (`-march=znver2`); the toolchain's default is the first x86-64.
- **A core each for the two busy threads**: the game's main thread and the
  render thread are kept on their own cores, and the runtime's other threads
  off them (`ps5/src/threads_ps5.cpp`).
- **In-between frames judged by their result.** Upstream's pacing times an
  in-between pass on the game's thread; here that measured one tick of the
  display while the render thread was still drawing, so the next step came
  late and the game ran slow. The console's rule looks at the step instead: a
  step more than 2 ms over its 33.3 ms counts as late (`WWHD_PS5_LATE_MS` in
  `env.txt` changes that; at 6 ms a view just over budget ran the game at 93 to
  97% speed with every frame drawn, and Gary preferred full speed at 30 fps for
  those moments), three late steps in four stop the in-between frames, and a
  probe every second brings them back as soon as the view fits again
  (`WWHD_PS5_PROBE_S` sets the longest wait between probes; `runtime/src/interp.cpp`,
  in the patch).
- **Fewer, lazier submissions.** A submission is a system call on the render
  thread, and the game's GX2Flush and GX2DrawDone made two of the five a frame:
  their work goes with the next submission instead.
- **The frame's copies on a helper thread.** For every draw the render thread
  copied what it reads out of guest memory into the upload arena: 11 MiB a
  frame of vertex and index ranges and uniform blocks. Now it only reserves the
  arena bytes and a thread on a free core copies them (`ps5/src/copier_ps5.cpp`);
  the render thread waits for it right before a GPU submission and before it
  tells the game it has passed a fence, which is when anyone could read them.
  `WWHD_PS5_ASYNC_COPY=0` turns it off.
- **A host-cached upload arena.** The driver's first host-visible memory type is
  uncached for the CPU, and every upload byte went through it, with a second,
  cached copy kept for the reuse checks. The arena now takes a host-cached type
  (the console's GPU reads it coherently) and the checks read the slices.
  `WWHD_PS5_ARENA_CACHED=0` keeps the old type.
- **Commands handed over in batches.** The game's thread issues about 46,000
  commands a frame, 40,000 of them register writes, and each took the queue's
  mutex and could wake the render thread with a system call. The game's main
  thread now keeps them until it has 4 KiB or one the render thread must act on
  at once; other threads hand over every command. `WWHD_PS5_BATCH_QUEUE=0`
  restores one hand-over per command.
- **A late frame flips when it is ready** (upstream's fix for its issue #125,
  in v0.2.13: `runtime/src/gx2/vsync_pacing.h`; first backported here, now
  upstream's own). The virtual vsync executed flips only on ticks of a 59.94 Hz
  grid, so a frame ready a millisecond after its tick waited a whole tick, the
  next step came late and the console's pacing rule dropped the in-between
  frames: 45 of 60 drawn, in bursts. With it, every in-between frame is drawn on
  the beach and the pier.
- **A continued-draw fast path** (upstream's unmerged `vk-fastpath` branch,
  commit 134617e, carried in the patch and on by default here; `WWHD_VK_FASTPATH=0`
  turns it off, `WWHD_VK_FASTPATH_VERIFY=8` compares every eighth continued draw
  with the full path and logs mismatches). Half the island's draws differ from
  the one before only in constants and buffer pointers; such a draw reuses the
  previous draw's shaders, render targets, pipeline and texture bindings
  (`runtime/src/gx2/state_groups.h` says which registers end a run). 47 to 63%
  of draws continued on the beach, a 100-second verify run found no mismatch,
  and a pass went from 15 to 13 ms standing and from 11.5 to 10.5 ms walking.
- **Shadow-map draws outside their cascade are left out.** The game draws
  every object of the scene into each of its three shadow cascades (757 draws
  a cascade in the view from the pier across the village), whatever the cascade
  covers. A draw whose positions all lie outside the cascade's footprint in the
  light's clip space cannot put anything into that map: 92% of the near
  cascade's draws and 93% of the middle one's in that view, 2% of the far
  one's. The test takes the box of the vertex buffer's positions through the
  draw's world and light matrices, only for draws laid out as the game's shadow
  pass was seen to lay them out (skinned models are never left out), not along
  the light's depth, and with a margin (`runtime/src/gfx/vulkan/draw.cpp`,
  `ps5_shadow_cull`). A frame with and without it shows the same shadows.
  `WWHD_PS5_SHADOW_CULL=0` turns it off.
- **In-between frames keep the step's far cascade.** The far cascade still
  takes 750 draws, and at 60 fps it was filled twice a step. On an in-between
  pass its draws and its clear are left out. The game applies its shadows in
  one full-screen draw (the sun shadow mask), whose uniform block holds for
  each cascade the matrix from view space to that cascade's map, bias * light
  * inverse view; it fits the cascades to the camera, so the in-between frame's
  own matrix belongs to a map that was not drawn (used with the kept map, whole
  surfaces went dark whenever the camera turned: the first attempt). The light
  matrix is taken from the shadow-map draws of the step's own frame, checked
  against that frame's mask, and the in-between frame's mask gets bias * that
  light * its own inverse view (`ps5_mask_seen`). A step whose check fails
  draws the map as before. Keeping all three cascades was tried and held 60 fps
  too, but Link's shadow, on the ground and on himself, flickered every frame
  as he ran: he stands half a step further on in the in-between frame than in
  the kept map (measured in recordings: consecutive frames differed more than
  frames two apart; with the near two drawn afresh they do not).
  `WWHD_PS5_HOLD_CASCADES` is the mask of cascades kept (default 4),
  `WWHD_PS5_HOLD_SHADOWS=0` turns it off, `WWHD_PS5_HOLD_SHADOWS_LOG=1` logs
  what both did; `WWHD_PS5_SHADOW_DUMP=<frame>` and
  `WWHD_PS5_SHADOW_DRAWS=<frame>` write the uniforms and draws they rely on.
  The view across the village went from 33 fps to 60, standing and turning.
- **Vertex data is copied once for the passes that draw it.** The game sends
  about 190 invalidations a frame, nearly all for textures and render targets;
  only those for attribute buffers now end the reuse of a copied vertex range
  (12% of repeated ranges were reused before, a third after). Direct-mapped
  front caches sit ahead of the hash maps of the vertex-buffer cache and the
  shader lookup.
- **Not taken: shadow distance.** Leaving out the farthest of the three shadow
  cascades (`WWHD_PS5_SHADOW_SKIP=4` in `env.txt`, a mask of slices; off by
  default) saves about a third of the shadow draws, 1.6 ms of a 15 ms pass in a
  view across the island, but loses the long shadows of distant cliffs, and a
  heavier view still fell to 30 fps with it: not worth the look, Gary decided.
  The renderer's reuse check of packed uniform blocks is off here (it rarely
  hit and cost 0.4 ms a pass; `WWHD_VK_REUSE_UNIFORM_SNAPSHOTS=1` turns it on).
- **The game's code is built with `-fwrapv`** (upstream PR #172): PowerPC
  integer arithmetic wraps, and the compiler now guarantees the same for the
  recompiled code instead of relying on it never overflowing.

## Keeping up with upstream

`vendor/wwhd` is a submodule at an upstream revision; nothing of it is copied or
forked here. The console's changes are one patch, `ps5/patches/wwhd.patch`, each
hunk behind `__PROSPERO__` except one backport (the continued-draw fast path
above, from upstream's unmerged `vk-fastpath` branch). The Status table above is
for the revision it was tested at (`ad8bcff`, upstream's `devel` of 2026-10-13,
120 commits after v0.2.13: it has the frame pacing fixes for upstream's issues
#125 and #164, the controller fix for #153 and `-fwrapv` for the game code).
After moving the submodule, run `ps5/tools/vm.sh prepare-game` again: the
runtime's hooks into the game's code change between revisions, and a stale
recompilation fails to link with undefined `f_..._orig` symbols.

## Credits

- [ZeldaWWHDRecomp](https://github.com/ZeldaWWHDRecomp/ZeldaWWHDRecomp) and its
  contributors: the recompiler, the runtime and the renderer. Its GPU pieces come
  from [Cemu](https://github.com/cemu-project/Cemu).
- [PS5_Vulkan](https://github.com/mihawk-99/PS5_Vulkan), [PS5_Mesa](https://github.com/mihawk-99/PS5_Mesa),
  [PS5_PayloadSDK](https://github.com/mihawk-99/PS5_PayloadSDK) and the
  [PS5 Vulkan Template](https://github.com/mihawk-99/PS5_VulkanTemplate) by Mihawk:
  the driver, the platform layer and the tools this is built with.
- [ps5-payload-dev/sdk](https://github.com/ps5-payload-dev/sdk) by John Törnblom.
- Mesa and RADV, SDL, glslang, LZ4, Dear ImGui.

## Licence

See [LICENSING.md](LICENSING.md). In short: the port's own files are MIT or Zlib,
the patch to the recompilation is MPL-2.0 as it is, one tool is GPL-3.0, and a
title as built links GPL-3.0 code. The game itself is Nintendo's and is not part
of this.
