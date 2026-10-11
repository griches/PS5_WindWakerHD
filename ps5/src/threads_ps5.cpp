/*
 * Wind Waker HD for the PlayStation 5 - where the two busy threads run.
 *
 * A frame is made by two threads: the game's main thread, which runs the
 * recompiled code and issues the frame's graphics calls, and the render thread,
 * which turns them into Vulkan. The console's CPU has two hardware threads a
 * core, and the system places a title's threads on any of its thirteen
 * (CPUs 0 to 12): when those two share a core, or share theirs with the audio
 * and worker threads, each runs slower for it.
 *
 * So each of the two gets a core of its own (CPUs 2k and 2k+1 are one core),
 * and every other thread of the runtime keeps off those two cores.
 * WWHD_PS5_PIN=0 in /app0/env.txt leaves every thread where the system puts it.
 *
 * Every thread of the runtime names itself as it starts (platform/host.h).
 *
 * SPDX-License-Identifier: MIT
 */
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <pthread.h>

extern "C" int32_t scePthreadSetaffinity(pthread_t thread, uint64_t mask);

namespace {
constexpr uint64_t kTitleCpus = 0x1fff;       // what a title's threads may run on
constexpr uint64_t kMainCore = 0x3ull << 2;   // CPUs 2 and 3
constexpr uint64_t kRenderCore = 0x3ull << 4; // CPUs 4 and 5
}

extern "C" void ps5_thread_place(const char* name) {
    static const bool on = [] { const char* e = getenv("WWHD_PS5_PIN"); return !e || strcmp(e, "0") != 0; }();
    if (!on) return;
    static std::atomic<bool> main_placed{false};
    uint64_t mask = kTitleCpus & ~(kMainCore | kRenderCore);
    const char* what = nullptr;
    if (!strcmp(name, "GX2 render")) {
        mask = 1ull << 4;
        what = "the render thread on CPU 4";
    } else if (!strcmp(name, "guest") && !main_placed.exchange(true)) {  // the game's main thread: the first, unnamed one
        mask = 1ull << 2;
        what = "the game's main thread on CPU 2";
    }
    const int32_t result = scePthreadSetaffinity(pthread_self(), mask);
    if (what) fprintf(stderr, "[threads] %s, the others off cores 1 and 2 (%#x)\n", what, (unsigned)result);
}
