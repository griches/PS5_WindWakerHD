# Wind Waker HD for the PlayStation 5: builder kit

This kit turns **your own copy** of The Legend of Zelda: The Wind Waker HD
(Wii U) into a game you can run on a PS5 that has homebrew enabled.

You run one command. Your computer does the rest in about 15 to 60 minutes.

> **There is no game in this kit.** No code, no data, nothing of Nintendo's.
> You supply the game, from your own disc. What the kit makes from it is
> yours alone: **never share the folder it produces, or its `eboot.bin`.**

---

## Before you start

You need three things.

**1. A PS5 that runs homebrew**, with:

- etaHEN (or an equivalent)
- ShadowMountPlus, which shows folders in `/data/homebrew` as games
- an FTP server running on the console

**2. Your dump of the game.** Any one of these:

| You have | What must be in the folder |
| --- | --- |
| A disc image | `game.wux` (or `game.wud`), `game.key` and `common.key`, all together |
| A Cemu archive | `game.wua` |
| An extracted game | a folder containing `code/`, `content/` and `meta/` |

The disc key must have the same name as the image (`game.wux` goes with
`game.key`). The USA disc is the one this kit was tested with. The European
disc should work but has not been tried.

**3. A computer** (Windows, macOS or Linux) with:

- [Docker Desktop](https://www.docker.com/products/docker-desktop/), installed and running
- about 6 GB of free disk space
- 8 GB of memory or more

---

## Step 1: Unzip the kit

Unzip `wwhd-ps5-kit-<version>.zip` anywhere. You get a folder with this
`README.md`, `make-title.sh` and the rest of the kit in it.

## Step 2: Start Docker

Open Docker Desktop and wait until it says it is running.

## Step 3: Build the game

Open a terminal **in the kit's folder** and run the command for your system,
with the path to your own dump.

**macOS or Linux**

```sh
./make-title.sh /path/to/game.wux
```

**Windows (PowerShell)**

```powershell
docker build -t wwhd-ps5-kit .
docker run --rm -v C:\path\to\dump-folder:/dump:ro -v ${PWD}\out:/out wwhd-ps5-kit /dump/game.wux /out
```

Replace `C:\path\to\dump-folder` with the folder that holds your dump, and
`game.wux` with your file's name. (If you use WSL, the macOS and Linux
command works there instead.)

### What you will see

| Stage | What it does | How long |
| --- | --- | --- |
| First run only | Builds the container; downloads a compiler (about 1 GB) | a few minutes |
| `extracting` | Reads the game's files out of your dump | 1 to 2 minutes |
| `translating` | Turns the game's program into C | about a minute |
| `compiling 80 files` | Compiles it for the PS5. **This is the long one.** | 10 to 60 minutes |
| `linking` | Joins it to the kit's parts and makes `eboot.bin` | under a minute |

It is finished when it prints the path of a folder ending in `PPSA99306`.

Your dump is only ever read, never changed, and after the first run the build
works with no internet connection.

## Step 4: Copy it to the PS5

The result is the folder `out/PPSA99306` inside the kit's folder (about 1.8 GB,
most of it the game's own files).

With an FTP program (FileZilla, Cyberduck, WinSCP), connect to your PS5 and
copy that **whole folder** into `/data/homebrew/`, so that you end up with:

```text
/data/homebrew/PPSA99306/eboot.bin
/data/homebrew/PPSA99306/sce_sys/
/data/homebrew/PPSA99306/sce_module/
/data/homebrew/PPSA99306/licenses/
/data/homebrew/PPSA99306/game/
```

## Step 5: Play

"Wind Waker HD" appears on the PS5's home screen. Start it like any game.

| DualSense | In the game |
| --- | --- |
| Circle | A (confirm, action) |
| Cross | B (back, sword) |
| Triangle | X |
| Square | Y |
| L1, R1, L2, R2 | L, R, ZL, ZR |
| Options | + (the game's menu) |
| Touchpad button | **Settings**: frame rate, resolution, aspect ratio, save states, controls |
| Sticks, D-pad, L3, R3 | as on a Wii U |

The buttons shown on screen sit where they are on the controller. If you would
rather have Cross confirm, change it in Settings > Controls > Face buttons.

Your saves and settings are kept on the console in
`/data/homebrew/PPSA99306/userdata/`.

---

## If something goes wrong

**"docker was not found"**
Docker Desktop is not installed, or not running. Start it and try again.

**The build stops part way with "Killed", or Docker closes**
It ran out of memory. Either give Docker more (Docker Desktop > Settings >
Resources), or run fewer compilers at once:

```sh
JOBS=4 ./make-title.sh /path/to/game.wux
```

On Windows, add `-e JOBS=4` after `docker run --rm`.

**It was interrupted**
Run the same command again. It carries on from where it stopped.

**It cannot read the disc image**
Check that `game.key` and `common.key` are in the same folder as the image,
and that the disc key has the same name as the image.

**The game does not appear on the PS5**
Check the folder is exactly `/data/homebrew/PPSA99306/` with `eboot.bin`
directly inside it (not `PPSA99306/PPSA99306/`), and that ShadowMountPlus is
running.

## Updating to a newer kit

Unzip the new kit, build again, and copy the new `out/PPSA99306` over the old
one on the console. Your saves and settings are left alone. You can skip
copying the `game` folder again: it does not change.

## Without Docker (Linux only)

If you already have LLVM 22 (clang, lld, llvm tools), Python 3, a C++ compiler
and the zstd and zlib development headers:

```sh
./build-title.sh /path/to/game.wux
```

---

## What is in the kit

| Path | What |
| --- | --- |
| `make-title.sh`, `Dockerfile` | Runs the build in a container. |
| `build-title.sh` | The build itself: extract, translate, compile, link. |
| `lib/` | The port's own compiled parts: the runtime, the renderer, SDL3, glslang. |
| `vulkan/` | RADV (the Vulkan driver), its link recipe and the tool that makes `eboot.bin`. |
| `sdk/` | The payload SDK's headers, libraries and link scripts. |
| `vendor/wwhd/` | ZeldaWWHDRecomp's recompiler and the headers its output includes. |
| `ps5/` | The title's link script, icon and pictures. |
| `SOURCES.txt`, `LICENSING.md`, `licenses/` | Where each part's source is, at which revision, and its licence. |

The project, with what works, what does not, and how the port is made:
<https://github.com/griches/PS5_WindWakerHD>

This project is not affiliated with, endorsed or sponsored by Nintendo or Sony
Interactive Entertainment.
