# Licences

The repository as a whole is offered under the GNU General Public License,
version 3 ([`LICENSE`](LICENSE)). Its files come from projects under several
licences, each compatible with that; every source file names its own in its
header, and may also be used under that licence alone.

| Path | Licence | Text |
| --- | --- | --- |
| `ps5/tools/png-to-title-dds.py` (encoder from PS5 RetroArch, by Mihawk) | GPL-3.0-or-later | [`licenses/GPL-3.0.txt`](licenses/GPL-3.0.txt) |
| `ps5/src/platform.c`, `ps5/src/platform.h` and the tools taken from the PS5 Vulkan Template (by Mihawk) | MIT | [`licenses/MIT.txt`](licenses/MIT.txt) |
| The rest of `ps5/src/`, `ps5/tools/`, `ps5/cmake/` and `ps5/art/` (written for this port) | MIT | [`licenses/MIT.txt`](licenses/MIT.txt) |
| `ps5/sdl/` (SDL3 drivers for the console) and `ps5/patches/sdl.patch` | Zlib, as SDL | [`licenses/Zlib.txt`](licenses/Zlib.txt) |
| `ps5/patches/wwhd.patch` | MPL-2.0, as ZeldaWWHDRecomp | in `vendor/wwhd/LICENSE` |

## What is built from elsewhere

Nothing below is stored in this repository; the build takes each from its own
project, under that project's licence.

| Part | Project | Licence |
| --- | --- | --- |
| The recompiler, the runtime and the renderer | [ZeldaWWHDRecomp](https://github.com/ZeldaWWHDRecomp/ZeldaWWHDRecomp), with pieces of [Cemu](https://github.com/cemu-project/Cemu) | MPL-2.0 |
| Windowing, input and sound | [SDL3](https://github.com/libsdl-org/SDL) | Zlib |
| The shader compiler | [glslang](https://github.com/KhronosGroup/glslang) | BSD-3-Clause and others, permissive |
| The Vulkan driver | RADV from [PS5_Mesa](https://github.com/mihawk-99/PS5_Mesa), linked by [PS5_Vulkan](https://github.com/mihawk-99/PS5_Vulkan) | MIT (Mesa); GPL-3.0 (PS5_Vulkan's tools) |
| The platform layer and SDK | [PS5_PayloadSDK](https://github.com/mihawk-99/PS5_PayloadSDK), a fork of ps5-payload-dev/sdk | GPL-3.0 |
| Smaller libraries | LZ4, Dear ImGui, fmt, stb, xxHash, zlib | each its own, all permissive |

## A built title is not for distribution

A built `eboot.bin` holds the game's own code, translated to C from your dump and
compiled. Whatever the licences above allow for the rest, that part is not yours
or mine to share: a built title, the `game/` folder and the generated code stay
on the machine that made them.

## What is not covered

- **The game.** The Legend of Zelda: The Wind Waker HD, its disc image and
  everything on it belong to Nintendo. None of it is in this repository, and no
  licence above covers it.
- **Artwork.** `ps5/art/icon0-source.png` and `ps5/art/pic-source.png` (a sailing
  boat and islands), and the `ps5/sce_sys/icon0.png`, `pic0.dds` and `pic1.dds`
  made from them, are the port's own: made for it by Gary Riches with an image
  generator. Nothing in them is Nintendo's, and no licence above is claimed for
  them beyond their use with this port.
- **Trademarks.** "The Legend of Zelda", "The Wind Waker", "Wii U" and
  "Nintendo" are trademarks of Nintendo. "PlayStation", "PS5" and "DualSense" are
  trademarks of Sony Interactive Entertainment. They are used here only to say
  what this software works with. This project is not affiliated with, endorsed
  or sponsored by either.
