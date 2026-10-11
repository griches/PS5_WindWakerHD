/*
 * Wind Waker HD for the PlayStation 5 - the guest's 4 GiB address space.
 *
 * The recompiled code reads guest address ea at PPC_MEM_BASE + ea. Elsewhere
 * that is one anonymous mapping the system backs on first touch. A title's
 * anonymous mappings are charged to its small flexible budget, so here the
 * range is reserved, and the regions the game and the runtime use (runtime.h)
 * are direct memory mapped into it. Anything else stays a reservation with no
 * access: the null page, and every stray pointer. A fault in such a hole is
 * committed on the spot (ps5_guest_fault, from the write-tracking handler),
 * logged once, so a region this list missed costs a line in klog, not a crash.
 *
 * SPDX-License-Identifier: MIT
 */
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include <ps5platform/shm.h>

#include "ppc.h"
#include "runtime.h"

namespace {
struct Region { uint32_t start, end; const char* what; };
const Region kRegions[] = {
    {0x02000000, 0x06000000, "code"},
    {mem::kMem2Start, mem::kMem2End, "MEM2"},
    {mem::kRuntimeStart, mem::kRuntimeEnd, "runtime"},
    {mem::kFgBucket, mem::kFgBucket + mem::kFgBucketSize, "foreground bucket"},
    {mem::kMem1, mem::kMem1 + mem::kMem1Size, "MEM1"},
};
constexpr uint64_t kSpace = 0x100000000ull, kUnit = 0x10000;
std::atomic<int> g_stray{0};
}

extern "C" bool ps5_guest_memory_init(void) {
    int r = ps5_vrange_reserve_at(PPC_MEM_BASE, kSpace);
    if (r != 0) {
        fprintf(stderr, "[mem] cannot reserve the guest address space at %p (%#x)\n", (void*)PPC_MEM_BASE, r);
        return false;
    }
    for (const Region& region : kRegions) {
        const size_t bytes = region.end - region.start;
        ps5_shm shm{};
        void* view = nullptr;
        r = ps5_shm_create(bytes, &shm);
        if (r == 0)
            r = ps5_shm_map(&shm, 0, bytes, PPC_MEM_BASE + region.start, PS5_SHM_READ | PS5_SHM_WRITE, PS5_SHM_FIXED, &view);
        if (r != 0 || view != PPC_MEM_BASE + region.start) {
            fprintf(stderr, "[mem] cannot map %s (%08X..%08X): %#x\n", region.what, region.start, region.end, r);
            return false;
        }
        memset(view, 0, bytes);  // direct memory comes as it was left
    }
    return true;
}

// A fault at a host address: true when it was a hole of the guest space (not the null page) that is
// now backed, and the access can run again.
extern "C" bool ps5_guest_fault(void* address) {
    const uintptr_t a = (uintptr_t)address, base = (uintptr_t)PPC_MEM_BASE;
    if (a < base + kUnit || a >= base + kSpace) return false;
    for (const Region& region : kRegions)
        if (a - base >= region.start && a - base < region.end) return false;  // mapped: not a hole
    if (g_stray.fetch_add(1) > 4096) return false;  // a runaway pointer: let it crash
    void* unit = (void*)(a & ~(uintptr_t)(kUnit - 1));
    if (ps5_vrange_commit(unit, kUnit, PS5_SHM_READ | PS5_SHM_WRITE) != 0) return false;
    static std::atomic<bool> said{false};
    if (!said.exchange(true)) {
        char line[96];
        int n = snprintf(line, sizeof line, "[mem] guest %08X is outside the mapped regions: committed on demand\n",
                         (unsigned)(a - base));
        if (fwrite(line, 1, (size_t)n, stderr)) {}
    }
    return true;
}

// ---------------------------------------------------------------- guest threads' host stacks
// The runtime gives each guest thread a 64 MiB host stack (deep guest call chains recurse on it).
// libkernel takes a stack of that size from the title's flexible budget, which sixteen of them
// exhaust: the seventeenth thread was never created, and the game waited for it for ever. These
// come from direct memory, with the unit below each left reserved as a guard.
#include <chrono>
#include <mutex>
#include <vector>

namespace {
struct Stack { void* base; size_t bytes; std::chrono::steady_clock::time_point freed; };
std::mutex g_stacks_m;
std::vector<Stack> g_free_stacks;
}

extern "C" void* ps5_thread_stack_take(size_t bytes) {
    {
        std::lock_guard<std::mutex> lock(g_stacks_m);
        const auto now = std::chrono::steady_clock::now();
        for (auto it = g_free_stacks.begin(); it != g_free_stacks.end(); ++it)
            // given back by the thread that still stood on it: reused once that thread is long gone
            if (it->bytes == bytes && now - it->freed > std::chrono::seconds(2)) {
                void* base = it->base;
                g_free_stacks.erase(it);
                return base;
            }
    }
    void* range = nullptr;
    if (ps5_vrange_reserve(bytes + kUnit, nullptr, kUnit, &range) != 0) return nullptr;
    ps5_shm shm{};
    void* view = nullptr;
    if (ps5_shm_create(bytes, &shm) != 0) return nullptr;
    if (ps5_shm_map(&shm, 0, bytes, (uint8_t*)range + kUnit, PS5_SHM_READ | PS5_SHM_WRITE, PS5_SHM_FIXED, &view) != 0)
        return nullptr;
    return view;
}

extern "C" void ps5_thread_stack_give(void* base, size_t bytes) {
    std::lock_guard<std::mutex> lock(g_stacks_m);
    g_free_stacks.push_back({base, bytes, std::chrono::steady_clock::now()});
}
