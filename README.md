# Wind Waker HD for PlayStation 5

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
| Frame rate | 60 fps in menus, the prologue and the pause menu (59.9 measured). On the open first island the game keeps full speed (29.6 to 30.0 steps a second) and shows 30 frames a second: a full pass takes the render thread about 20 ms, and two do not fit in a step | A long session; later, heavier areas |
| 30 fps | The game's own rate, chosen in Settings: 30.0 frames and 30.0 game steps a second on the island's pier | |
| Settings | Opened with the touchpad button and driven from the pad: tabs, frame rate, internal resolution, save states; choices kept across a restart | Every option in it |
| Text entry | The console's own keyboard for the name of a new Quest Log, and the name it gave back in the game | |
| Button prompts | The DualSense's symbols in the HUD and the pause menu: Cross, Circle, Square, Triangle, R1, R2 | L1 and L2; prompts inside message text (still the Wii U's); French and Spanish |
| Input | Menus, dialogue, walking and the camera from the sticks, the pause menu; "Wii U GamePad" chosen on the selection screen and the game still played, menus on the TV | Each button in play; rumble; motion; a second controller |
| Saving | The game's own save from the pause menu, found again after a restart; a save state written, listed and loaded | |
| Sound | The output opens (48 kHz stereo) and the game's sound threads run | What it sounds like: nobody has listened yet |
| Exit | The title closes through the shell when a test run asks it to | Closing it from the home screen (the usual way: the game has no quit of its own) |

Known limits:

- **60 fps is frame interpolation, and the open island does not reach it.** The
  game runs 30 steps a second and each is drawn twice. On the open island a pass
  is about 5,150 draws and about 20 ms of the render thread, so two do not fit
  in a thirtieth of a second: there the in-between frame is left out, the game
  keeps its speed and the scene shows 30 frames a second. Keeping the step's
  shadow maps for the in-between frame (2,739 of those draws) did reach 60, but
  the in-between frame's shadow parameters do not match the kept maps: whole
  surfaces went dark for a frame whenever the camera moved. It is off;
  `WWHD_PS5_HOLD_SHADOWS=1` in `env.txt` turns it on for experiments.
- **Nothing past the first island has been measured.** The game cannot run ahead of the render thread to make room
  (tried: the render thread read a surface the game had reused), so a heavier
  scene needs cheaper passes: the render thread spends about 3 microseconds a
  draw (`ps5/src/sampler_ps5.cpp` has the profile).
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
  late and the game ran slow. The console's rule looks at the step instead:
  three late steps in four stop the in-between frames, and a probe (every one
  to four seconds) brings them back (`runtime/src/interp.cpp`, in the patch).
- **Fewer, lazier submissions.** A submission is a system call on the render
  thread, and the game's GX2Flush and GX2DrawDone made two of the five a frame:
  their work goes with the next submission instead.

## Keeping up with upstream

`vendor/wwhd` is a submodule at an upstream revision; nothing of it is copied or
forked here. The console's changes are one patch, `ps5/patches/wwhd.patch`, each
hunk behind `__PROSPERO__`. The Status table above is for the revision it was
tested at (`5c39d48`, v0.2.12).

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
