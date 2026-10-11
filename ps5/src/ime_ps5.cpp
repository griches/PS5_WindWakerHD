/*
 * Wind Waker HD for the PlayStation 5 - text entry through the console's own keyboard.
 *
 * When the game asks for text (the Wii U's software keyboard: the name at the
 * start of a new game), the console's keyboard dialog (libSceImeDialog) takes
 * it, in place of the runtime's on-screen keyboard. The dialog runs beside the
 * game, which keeps drawing under it; a thread waits for it to finish and
 * hands the answer back. If the dialog cannot start, ps5_ime_prompt returns
 * false and the runtime's own keyboard shows, as on every other platform.
 *
 * Only exported functions of the system software are called.
 *
 * SPDX-License-Identifier: MIT
 */
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <string>
#include <thread>
#include <vector>

extern "C" {
int sceSysmoduleLoadModule(uint32_t id);
int sceUserServiceGetInitialUser(int32_t* user);
int sceImeDialogInit(const void* param, const void* extended);
int sceImeDialogGetStatus(void);
int sceImeDialogGetResult(void* result);
int sceImeDialogTerm(void);
int sceKernelUsleep(uint32_t microseconds);
}

namespace {
constexpr uint32_t kModuleImeDialog = 0x0096;
enum { kTypeDefault = 0, kTypeBasicLatin = 1, kTypeNumber = 4 };
enum { kStatusNone = 0, kStatusRunning = 1, kStatusFinished = 2 };
enum { kEndOk = 0, kEndUserCanceled = 1, kEndAborted = 2 };

struct DialogParam {
    int32_t user;
    int32_t type;
    uint64_t supported_languages;
    int32_t enter_label;
    int32_t input_method;
    void* filter;
    uint32_t option;
    uint32_t max_text_length;
    char16_t* input_text_buffer;
    float x, y;  // of a 1920x1080 screen
    int32_t horizontal_alignment, vertical_alignment;  // 0 left / top, 1 centre, 2 right / bottom
    const char16_t* placeholder;
    const char16_t* title;
    int8_t reserved[16];
};
static_assert(sizeof(DialogParam) == 96, "the dialog's parameters are 96 bytes");
struct DialogResult {
    int32_t end_status;
    int32_t reserved[12];
};

std::atomic<bool> g_busy{false};
void say(const char* text, int value) { fprintf(stderr, "[keyboard] %s (%#x)\n", text, (unsigned)value); }
}

bool ps5_ime_prompt(const std::u16string& initial, const std::u16string& hint, int max_len, int mode,
                    std::function<void(bool, std::u16string)> done) {
    if (max_len <= 0 || g_busy.exchange(true)) return false;
    static const int loaded = sceSysmoduleLoadModule(kModuleImeDialog);
    // what the dialog reads stays alive until it has finished: owned by the waiting thread
    struct Job {
        std::vector<char16_t> text;
        std::u16string title;
        DialogParam param{};
        std::function<void(bool, std::u16string)> done;
    };
    auto* job = new Job;
    job->text.assign((size_t)max_len + 1, 0);
    memcpy(job->text.data(), initial.data(), std::min(initial.size(), (size_t)max_len) * sizeof(char16_t));
    job->title = hint.empty() ? u"Enter text" : hint;
    job->done = std::move(done);
    int32_t user = 0;
    sceUserServiceGetInitialUser(&user);
    DialogParam& p = job->param;
    p.user = user;
    p.type = mode == 1 ? kTypeNumber : kTypeBasicLatin;  // the game's name font is Latin
    p.max_text_length = (uint32_t)max_len;
    p.input_text_buffer = job->text.data();
    p.x = 960, p.y = 540;
    p.horizontal_alignment = p.vertical_alignment = 1;
    p.title = job->title.c_str();
    const int started = sceImeDialogInit(&p, nullptr);
    if (started != 0) {
        say("the console's keyboard did not open; the on-screen one shows", started);
        if (loaded != 0) say("its module did not load", loaded);
        delete job;
        g_busy = false;
        return false;
    }
    std::thread([job] {
        while (sceImeDialogGetStatus() == kStatusRunning) sceKernelUsleep(50000);
        DialogResult result{};
        const int got = sceImeDialogGetResult(&result);
        sceImeDialogTerm();
        const bool ok = got == 0 && result.end_status == kEndOk;
        std::u16string text(job->text.data());
        auto done = std::move(job->done);
        delete job;
        g_busy = false;
        fprintf(stderr, "[keyboard] closed: %s, %zu characters\n", ok ? "OK" : "cancelled", text.size());
        done(ok, ok ? std::move(text) : std::u16string());
    }).detach();
    return true;
}
