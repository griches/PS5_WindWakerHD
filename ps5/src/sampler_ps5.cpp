/*
 * Wind Waker HD for the PlayStation 5 - a sampling profiler for one of the title's threads.
 *
 * The console has no profiler a title can use, and guessing where a thread's
 * time goes is how wrong fixes get written. With
 *
 *     WWHD_PS5_SAMPLE=<thread name>:<seconds to wait>:<samples>
 *
 * in /app0/env.txt (for example "GX2 render:60:8000"), the named thread is
 * interrupted about a thousand times a second once the wait is over; each time,
 * where it was (its instruction pointer and up to four return addresses, by
 * frame pointer) is recorded. The samples go to /app0/userdata/samples.txt, one
 * line each, for ps5/tools/symbolise-samples.py and the linked ELF
 * (build/last-title.elf). Without the variable nothing here runs.
 *
 * SPDX-License-Identifier: MIT
 */
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>

#include <pthread.h>
#include <signal.h>
#include <unistd.h>

#include <ps5platform/context.h>

extern "C" void ps5_sampler_thread(const char* name);

namespace {
constexpr int kDepth = 5;
struct Sample { uintptr_t pc[kDepth]; };
Sample* g_samples;
int g_wanted;
std::atomic<int> g_taken{0};
std::string g_name;
pthread_t g_thread;
std::atomic<bool> g_found{false};

void on_sample(int, siginfo_t*, void* context) {
    const int i = g_taken.fetch_add(1, std::memory_order_relaxed);
    if (i >= g_wanted) return;
    const auto* uc = static_cast<const ucontext_t*>(context);
    Sample& s = g_samples[i];
    s.pc[0] = (uintptr_t)uc->uc_mcontext.mc_rip;
    uintptr_t fp = (uintptr_t)uc->uc_mcontext.mc_rbp;
    const uintptr_t sp = (uintptr_t)uc->uc_mcontext.mc_rsp;
    for (int level = 1; level < kDepth; level++) {
        // a frame pointer is above the stack pointer, not far, and aligned: anything else is a
        // function that keeps none (the walk stops rather than read a wild address)
        if (fp < sp || fp - sp > (64u << 20) || (fp & 7)) { s.pc[level] = 0; fp = 0; continue; }
        s.pc[level] = ((const uintptr_t*)fp)[1];
        const uintptr_t next = ((const uintptr_t*)fp)[0];
        fp = next > fp ? next : 0;
    }
}

void run(int wait_seconds) {
    sleep((unsigned)wait_seconds);
    if (!g_found.load()) {
        fprintf(stderr, "[sampler] no thread named \"%s\" started\n", g_name.c_str());
        return;
    }
    fprintf(stderr, "[sampler] sampling \"%s\": %d samples\n", g_name.c_str(), g_wanted);
    while (g_taken.load(std::memory_order_relaxed) < g_wanted) {
        pthread_kill(g_thread, SIGUSR2);
        usleep(1000);
    }
    usleep(50000);
    FILE* out = fopen("/app0/userdata/samples.txt", "w");
    if (!out) { fprintf(stderr, "[sampler] cannot write samples.txt\n"); return; }
    // where the title was loaded: the address of a function the ELF names
    fprintf(out, "at ps5_sampler_thread %lx\n", (unsigned long)(uintptr_t)&ps5_sampler_thread);
    for (int i = 0; i < g_wanted; i++) {
        for (int level = 0; level < kDepth; level++) fprintf(out, "%s%lx", level ? " " : "", (unsigned long)g_samples[i].pc[level]);
        fputc('\n', out);
    }
    fclose(out);
    fprintf(stderr, "[sampler] wrote /app0/userdata/samples.txt (%d samples of \"%s\")\n", g_wanted, g_name.c_str());
}
}

// Every thread of the runtime names itself as it starts (platform/host.h).
extern "C" void ps5_sampler_thread(const char* name) {
    static const bool wanted = [] {
        const char* spec = getenv("WWHD_PS5_SAMPLE");
        if (!spec || !*spec) return false;
        std::string s = spec;
        int wait_seconds = 60;
        g_wanted = 8000;
        if (size_t a = s.find(':'); a != std::string::npos) {
            wait_seconds = atoi(s.c_str() + a + 1);
            if (size_t b = s.find(':', a + 1); b != std::string::npos) g_wanted = atoi(s.c_str() + b + 1);
            s.resize(a);
        }
        if (g_wanted <= 0 || g_wanted > 200000) g_wanted = 8000;
        g_name = s;
        g_samples = (Sample*)calloc((size_t)g_wanted, sizeof(Sample));
        struct sigaction sa{};
        sa.sa_sigaction = on_sample;
        sa.sa_flags = SA_SIGINFO | SA_RESTART;
        sigemptyset(&sa.sa_mask);
        sigaction(SIGUSR2, &sa, nullptr);
        std::thread(run, wait_seconds).detach();
        return true;
    }();
    if (wanted && !g_found.load() && g_name == name) {
        g_thread = pthread_self();
        g_found = true;
    }
}
