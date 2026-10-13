/*
 * Wind Waker HD for the PlayStation 5 - the frame's uploads copied by a helper thread.
 *
 * The render thread turns the game's draws into Vulkan, and for every draw it copies what the draw
 * reads out of guest memory into the upload arena: vertex ranges, index ranges and uniform blocks,
 * about 11 MiB a frame on the first island. The copying is a fifth of the thread's time, and
 * the thread is what keeps the open island at 30 frames a second.
 *
 * So the render thread only reserves the arena bytes and hands the copy here, to a thread of its
 * own on a core the busy two do not use (threads_ps5.cpp), and goes on with the draw. What it
 * must not do is let anyone read those bytes before they are written: the GPU reads them when the
 * command buffer is submitted (gfx/vulkan/backend.cpp waits here first), and the game may write the
 * guest memory they come from once the render thread reports that it has passed a fence
 * (gx2/gx2_core.cpp waits here before it reports). Both wait with ps5_copy_drain(), which is quick:
 * the copies of a frame take the helper about a millisecond.
 *
 * The job ring is single-producer (the render thread), single-consumer (the helper). A full ring
 * makes the render thread copy the job itself. WWHD_PS5_ASYNC_COPY=0 in /app0/env.txt turns the
 * helper off, and every copy is made where it was before.
 *
 * SPDX-License-Identifier: MIT
 */
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <thread>

#include <pthread.h>

#include <immintrin.h>

extern "C" int32_t scePthreadSetaffinity(pthread_t thread, uint64_t mask);

namespace {
struct Job {
    void* dst;
    const void* src;
    size_t size;
    size_t zero;  // bytes to clear after the copy (the slice's padding)
};
constexpr size_t kRing = 4096;  // jobs; a frame has about 7,000 on the first island, drained twice
Job g_ring[kRing];
std::atomic<uint64_t> g_produced{0};  // jobs published by the render thread
std::atomic<uint64_t> g_consumed{0};  // jobs finished by the helper
std::atomic<bool> g_quit{false};
std::mutex g_mu;
std::condition_variable g_cv;
std::atomic<bool> g_sleeping{false};
uint64_t g_jobs = 0, g_bytes = 0, g_overflow = 0, g_drains = 0, g_drain_spins = 0;

void run(const Job& j) {
    if (j.src && j.size) std::memcpy(j.dst, j.src, j.size);
    if (j.zero) std::memset(static_cast<uint8_t*>(j.dst) + j.size, 0, j.zero);
}

void helper() {
    // CPU 6 and 7 are one core, free of the game's main thread (CPU 2) and the render thread (CPU 4)
    static const bool pin = [] { const char* e = getenv("WWHD_PS5_PIN"); return !e || strcmp(e, "0") != 0; }();
    if (pin) scePthreadSetaffinity(pthread_self(), 1ull << 6);
    uint64_t done = 0;
    for (;;) {
        uint64_t produced = g_produced.load(std::memory_order_acquire);
        if (produced == done) {
            // nothing queued: spin a little (the next job is usually microseconds away), then sleep
            for (int spin = 0; spin < 2000 && produced == done; spin++) {
                _mm_pause();
                produced = g_produced.load(std::memory_order_acquire);
            }
            if (produced == done) {
                std::unique_lock<std::mutex> lk(g_mu);
                g_sleeping.store(true, std::memory_order_release);
                g_cv.wait(lk, [&] { return g_quit.load() || g_produced.load(std::memory_order_acquire) != done; });
                g_sleeping.store(false, std::memory_order_release);
                if (g_quit.load()) return;
                continue;
            }
        }
        while (done != produced) {
            run(g_ring[done % kRing]);
            ++done;
            g_consumed.store(done, std::memory_order_release);
        }
    }
}

bool enabled() {
    static const bool on = [] {
        const char* e = getenv("WWHD_PS5_ASYNC_COPY");
        const bool on = !e || strcmp(e, "0") != 0;
        if (on) {
            std::thread(helper).detach();
            fprintf(stderr, "[copier] the frame's uploads are copied by a helper thread on CPU 6 (WWHD_PS5_ASYNC_COPY=0 turns it off)\n");
        }
        return on;
    }();
    return on;
}
}  // namespace

extern "C" bool ps5_copy_async_enabled() { return enabled(); }

// Copy size bytes from src to dst, then clear zero bytes after them. dst is upload memory nobody
// reads before the next ps5_copy_drain(); src stays valid until then too (guest memory, or a
// buffer the caller keeps). Only the render thread calls this.
extern "C" void ps5_copy_async(void* dst, const void* src, size_t size, size_t zero) {
    if (!enabled()) {
        run({dst, src, size, zero});
        return;
    }
    const uint64_t produced = g_produced.load(std::memory_order_relaxed);
    if (produced - g_consumed.load(std::memory_order_acquire) >= kRing) {
        ++g_overflow;
        run({dst, src, size, zero});
        return;
    }
    g_ring[produced % kRing] = {dst, src, size, zero};
    g_produced.store(produced + 1, std::memory_order_release);
    ++g_jobs;
    g_bytes += size;
    if (g_sleeping.load(std::memory_order_acquire)) {
        std::lock_guard<std::mutex> lk(g_mu);
        g_cv.notify_one();
    }
}

// Wait until every copy handed over so far is done. Called before a submission and before the
// render thread reports a fence to the game.
extern "C" void ps5_copy_drain() {
    if (!enabled()) return;
    const uint64_t produced = g_produced.load(std::memory_order_relaxed);
    if (g_consumed.load(std::memory_order_acquire) >= produced) return;
    ++g_drains;
    if (g_sleeping.load(std::memory_order_acquire)) {
        std::lock_guard<std::mutex> lk(g_mu);
        g_cv.notify_one();
    }
    while (g_consumed.load(std::memory_order_acquire) < produced) {
        _mm_pause();
        ++g_drain_spins;
    }
}

// Counters for the renderer's statistics line (reset after reading).
extern "C" void ps5_copy_stats(uint64_t* jobs, uint64_t* bytes, uint64_t* overflow, uint64_t* drains, uint64_t* spins) {
    *jobs = g_jobs; *bytes = g_bytes; *overflow = g_overflow; *drains = g_drains; *spins = g_drain_spins;
    g_jobs = g_bytes = g_overflow = g_drains = g_drain_spins = 0;
}
