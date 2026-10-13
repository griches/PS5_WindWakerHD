/*
 * Wind Waker HD for the PlayStation 5 - what runs before the game's main.
 *
 * The recompilation's main is its own (vendor/wwhd/runtime/src/main.cpp), so
 * the console's start-up is a constructor: standard error into klog and the
 * shell's splash dismissed, and where a title may keep its files.
 *
 * A title sees its own folder (/app0, which is /data/homebrew/<TITLE_ID>) and
 * nothing else of /data:
 *
 *   /app0/game/         the game's files, from your own dump (code, content, meta)
 *   /app0/userdata/     settings, the shader cache, logs, and save/
 *   /app0/env.txt       optional: NAME=value lines, the WWHD_ options of upstream's README
 *
 * SPDX-License-Identifier: MIT
 */
#include <SDL3/SDL_stdinc.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "platform.h"

/* Both copies of the environment: the console's libc keeps what setenv sets
 * apart from the `environ` SDL copies, and the runtime reads the first. */
static void
set(const char *name, const char *value, int overwrite)
{
   setenv(name, value, overwrite);
   SDL_SetEnvironmentVariable(SDL_GetEnvironment(), name, value, overwrite != 0);
}

__attribute__((constructor)) static void
ps5_startup(void)
{
   platform_init(PS5_TITLE_NAME);
   mkdir("/app0/userdata", 0777);
   mkdir("/app0/userdata/save", 0777);
   /* PS5 DevLoop's grab takes the engine's screenshots from here, and removes them */
   mkdir("/app0/userdata/screenshots", 0777);
   chmod("/app0/userdata/screenshots", 0777);
   set("XDG_DATA_HOME", "/app0/userdata", 1);
   set("XDG_CONFIG_HOME", "/app0/userdata", 1);
   set("HOME", "/app0/userdata", 1);
   /* the runtime's "portable" folder: settings, controls, save states, caches, crash logs */
   set("WWHD_USER_DIR", "/app0/userdata", 1);
   /* one output: the GamePad's picture is drawn into the TV's, never a window of its own */
   set("WWHD_NO_GAMEPAD", "1", 1);
   /* the runtime writes its logs and captures relative to the working directory */
   (void)chdir("/app0/userdata"); /* refused for a title: the runtime names its folders in full */

   /* the renderer's continued-draw fast path (upstream's vk-fastpath branch, in the patch): on, as
    * the first island needs it for 60 fps; env.txt's WWHD_VK_FASTPATH=0 turns it off */
   set("WWHD_VK_FASTPATH", "1", 0);

   FILE *file = fopen("/app0/env.txt", "r");
   if (file) {
      char line[512];
      while (fgets(line, sizeof line, file)) {
         line[strcspn(line, "\r\n")] = 0;
         char *equals = strchr(line, '=');
         if (line[0] == '#' || !equals || equals == line)
            continue;
         *equals = 0;
         set(line, equals + 1, 1);
         say("env.txt: %s=%s", line, equals + 1);
      }
      fclose(file);
   }
}
