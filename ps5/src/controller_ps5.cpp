/*
 * Wind Waker HD for the PlayStation 5 - the game always plays with a Pro Controller.
 *
 * The game's Controller Selection screen offers the Wii U GamePad. Played as a
 * GamePad the game puts its pause menu, inventory and maps on the GamePad's
 * screen, which the console does not have, and the port's pad (a Pro Controller
 * to the game, platform/input_sdl.cpp) sends it no GamePad buttons: choosing it
 * left the player at a screen that no longer answered.
 *
 * The game changes controller through one function of its controller manager
 * (02618094: this, mode), where mode is 0 for a Pro Controller, 1 or 2 for the
 * GamePad and 3 for both (the title and selection screens; docs/gyro.md has the
 * manager). The recompiler calls site_02618094 at its entry
 * (tools/recomp/hooks_ps5.txt): a request for the GamePad becomes one for the
 * Pro Controller, so the choice on that screen changes nothing. "Both" is left
 * alone. With the pad switched to a GamePad (WWHD_PRO_CONTROLLER=0, Controls)
 * the game is not touched.
 *
 * SPDX-License-Identifier: MIT
 */
#include "input.h"
#include "runtime.h"

extern "C" void site_02618094(Cpu* c) {
    if (!input::pro_controller() || (c->r[4] != 1 && c->r[4] != 2)) return;
    LOG("[input] the game asked for the GamePad (controller mode %u): it stays with the Pro Controller", c->r[4]);
    c->r[4] = 0;
}
