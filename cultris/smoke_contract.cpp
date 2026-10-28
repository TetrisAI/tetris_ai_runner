// smoke_contract.cpp — runtime contract smoke for libcultrisai under the current
// c2_ai_api.h. Loads a dylib by path (argv[1]), resolves the full export surface,
// validates identity, and drives two independent API-step cycles (set_mode /
// reset_plan / set_iterate_range / set_next_mode exercised in between).
//
// Deliberately does NOT assert retired identity substrings (e.g. older builds
// carried "combo-table-16" in c2ai_name; current builds return
// "C2 ZZZ / <revision>"). Exit code 0 == contract smoke passed.
//
// Build standalone (no repo CMake wiring yet; test owner coordinates final wiring):
//   clang++ -std=c++20 -O1 -I cultris cultris/smoke_contract.cpp -o smoke_contract
//   ./smoke_contract path/to/libcultrisai.dylib
#include "c2_ai_api.h"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <dlfcn.h>
#include <thread>

using Create = int64_t (*)();
using Destroy = void (*)(int64_t);
using Step = void (*)(int64_t, const C2StepInput*, C2StepOutput*);
using SetMode = void (*)(int64_t, int32_t);
using SetIter = void (*)(int64_t, int32_t, int32_t);
using SetNext = void (*)(int64_t, int32_t, int32_t);
using Reset = void (*)(int64_t);
using NameFn = const char* (*)();

static int fail(const char* msg) {
    std::printf("SMOKE CONTRACT FAIL: %s\n", msg);
    return 1;
}

static C2StepOutput run_cycle(int64_t h, Step step, const C2StepInput& in) {
    C2StepOutput out{};
    for (int i = 0; i < 500 && out.status != C2_STATUS_OK; ++i) {
        step(h, &in, &out);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return out;
}

int main(int argc, char** argv) {
    if (argc < 2) return fail("usage: smoke_contract <libcultrisai.dylib>");
    void* lib = dlopen(argv[1], RTLD_NOW);
    if (!lib) return fail(dlerror());
    Create create = (Create)dlsym(lib, "c2ai_create");
    Destroy destroy = (Destroy)dlsym(lib, "c2ai_destroy");
    Step step = (Step)dlsym(lib, "c2ai_step");
    SetMode set_mode = (SetMode)dlsym(lib, "c2ai_set_mode");
    Reset reset_plan = (Reset)dlsym(lib, "c2ai_reset_plan");
    SetIter set_iter = (SetIter)dlsym(lib, "c2ai_set_iterate_range");
    SetNext set_next = (SetNext)dlsym(lib, "c2ai_set_next_mode");
    NameFn name = (NameFn)dlsym(lib, "c2ai_name");
    NameFn version = (NameFn)dlsym(lib, "c2ai_version");
    if (!create || !destroy || !step || !set_mode || !reset_plan || !set_iter ||
        !set_next || !name || !version)
        return fail("missing export");
    const char* n = name();
    const char* v = version();
    if (!n || !*n || !std::strstr(n, "C2 ZZZ")) return fail("name lacks 'C2 ZZZ'");
    for (const unsigned char* q = (const unsigned char*)n; *q; ++q)
        if (*q > 127) return fail("name non-ASCII");
    if (!v || !*v) return fail("version empty");

    int64_t h = create();
    if (h == 0) return fail("create returned 0");
    set_mode(h, 0);
    set_mode(h, 1);
    reset_plan(h);
    set_iter(h, 4, 7);
    set_iter(h, 1, 1);  // clamp path must not crash
    set_next(h, 1, 1);

    C2StepInput in{};
    in.api_version = C2_AI_API_VERSION;
    in.cur_kind = C2_KIND_T;
    in.cur_x = 3;
    in.cur_y = 8;
    in.next_kind = C2_KIND_I;
    in.attack_curve_scale = 1.95f;
    in.reserved[0] = 1;     // soft drop
    in.reserved[1] = 1;     // safe rows
    in.reserved[2] = 60;    // planning bpm
    in.reserved[3] = 1500;  // combo remaining ms
    in.piece_serial = 1;
    in.board_gen = 1;
    C2StepOutput out = run_cycle(h, step, in);
    std::printf("cycle1 status=%d op=%d name=%s version=%s\n", out.status, out.next_op, n, v);
    if (out.status != C2_STATUS_OK) return fail("cycle1 not OK");

    // New piece + new generation: policy / DP / next handling must not crash.
    in.piece_serial = 2;
    in.board_gen = 2;
    in.cur_kind = C2_KIND_O;
    out = run_cycle(h, step, in);
    std::printf("cycle2 status=%d op=%d\n", out.status, out.next_op);
    if (out.status != C2_STATUS_OK) return fail("cycle2 not OK");

    destroy(h);
    dlclose(lib);
    return 0;
}
