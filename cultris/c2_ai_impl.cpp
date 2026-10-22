// cultris/c2_ai_impl.cpp
//
// ============================================================================
// Cultris II AI dylib on the master-branch classic engine.
//
// Design: **continuous fixed-10ms search + path[0] every c2ai_step**. Java calls c2ai_step
// once per fixed tick asking "given the current live board + active piece,
// what single op should I press right now?". A background worker owns the
// engine; every c2ai_step publishes the latest input and wakes the worker.
// It then tries to read the most recently published target without waiting,
// rebuilds a path from the current live coordinates, and returns path[0].
//
// There is NO cached plan/path across ticks: every frame Java gets a fresh
// answer derived from a full re-search on the current board+piece state,
// so DAS/soft-drop transient states cannot desync the AI (the AI just
// "re-decides" on the fly). Soft drops appear naturally when path[0] is
// 'd' or 'D'; recovery from any transient state is automatic.
//
// make_path character encoding (see src/search_cautious.cpp):
//   'l' -> MOVE_LEFT       'L' -> DAS_LEFT
//   'r' -> MOVE_RIGHT      'R' -> DAS_RIGHT
//   'c' -> ROT_CW          'z' -> ROT_CCW      'x' -> ROT_180
//   'd' -> SOFT_DROP       'D' -> SOFT_DROP_LOCK (slide to floor, no lock)
// If path is empty (cur node == target's filtered index) -> HARD_DROP.
//
// Coordinate note: search returns the engine anchor x. For the O piece we
// feed ex = java_x - 1 so op_O1's cells at cols 1,2 align with Java's O
// cells at cols 0,1. path[0] is invariant under that shift so we don't need
// to undo anything for op mapping.
// ============================================================================

#include "c2_ai_api.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "tetris_core.h"
#include "rule_c2.h"
#include "ai_zzz.h"
#include "search_cautious.h"

namespace {

bool env_flag_enabled(const char* name) noexcept {
    const char* value = std::getenv(name);
    return value != nullptr && std::strcmp(value, "1") == 0;
}

// INFO and DEBUG are independent, explicit opt-in levels.
bool info_log_enabled() noexcept {
    static const bool enabled = env_flag_enabled("CULTRISAI_INFO");
    return enabled;
}

bool debug_log_enabled() noexcept {
    static const bool enabled = env_flag_enabled("CULTRISAI_DEBUG");
    return enabled;
}

using CultrisEngine = m_tetris2::TetrisEngine<
    rule_c2::TetrisRule,
    ai_zzz::C2,
    cautious::Search>;
using CultrisMap = CultrisEngine::map_t;
using CultrisLandPoint = CultrisEngine::LandPoint;
using CultrisHelpers = m_tetris2::Helpers<rule_c2::TetrisRule::rule_spec>;

constexpr int    kBoardW       = 10;
constexpr int    kEngineH      = 28;
constexpr int    kColMask      = (1 << kBoardW) - 1;   // 0x3FF
constexpr std::time_t kSearchBudgetMs = 10;
// c2ai_step may wait for the current 10ms slice on Java's dedicated worker;
// the game frame thread remains non-blocking.

struct Handle {
    // The engine and its node graph are pinned for the Handle lifetime.
    // engine_mutex serializes all mutable engine/search graph access.
    CultrisEngine engine;
    std::mutex    engine_mutex;
    std::mutex    engine_turn_mutex;
    std::condition_variable engine_turn_cv;
    bool consumer_waiting = false;

    bool engine_prepared    = false;
    int  engine_prepared_w  = 0;
    int  engine_prepared_h  = 0;
    bool config_installed   = false;
    std::atomic<int> ai_mode{0};

    // Main-thread-only piece identity, retained for diagnostics.
    std::int32_t last_kind = -2;
    std::int64_t last_piece_serial = -1;

    // ----- Input pipeline (main -> worker) -----
    std::thread              ai_thread;
    std::atomic<bool>        stop_flag{false};

    std::mutex               input_mutex;
    std::condition_variable  input_cv;
    C2StepInput              latest_input{};
    std::uint64_t            input_gen   = 0;
    // Changes only for a new active-piece identity/reset, never for gravity.
    std::uint64_t            piece_generation = 0;
    bool                     have_input  = false;

    // ----- Result pipeline (worker -> main) -----
    std::mutex               result_mutex;
    std::condition_variable  result_cv;  // notification is diagnostic-only
    std::uint64_t            result_gen  = 0;
    std::uint64_t            result_piece_generation = 0;
    std::int64_t             result_board_gen = -1;
    std::int64_t             plan_serial = -1;
    bool                     last_ok     = false;
    CultrisLandPoint          last_target{};
    std::int32_t             last_target_x = -1;
    std::int32_t             last_target_y_native = -1;
    std::int32_t             last_target_y_java = -1;
    std::int32_t             last_target_r = -1;

    // A SOFT_DROP_LOCK must be observed at its planned y before a subsequent
    // HARD_DROP. Other plan reuse intentionally ignores per-frame y changes.
    bool                     last_sdl_valid = false;
    std::int64_t             last_sdl_serial = -1;
    std::int32_t             last_sdl_target_y_java = -1;
};

void clear_result_locked(Handle& h) {
    h.result_gen = 0;
    h.result_piece_generation = 0;
    h.result_board_gen = -1;
    h.plan_serial = -1;
    h.last_ok = false;
    h.last_target = CultrisLandPoint{};
    h.last_target_x = -1;
    h.last_target_y_native = -1;
    h.last_target_y_java = -1;
    h.last_target_r = -1;
}

void reset_plan_state(Handle& h) {
    // Match worker publication lock order so an in-flight old search cannot
    // republish after reset.
    std::unique_lock<std::mutex> ilk(h.input_mutex);
    std::unique_lock<std::mutex> rlk(h.result_mutex);
    h.last_kind = -2;
    h.last_piece_serial = -1;
    h.last_sdl_valid = false;
    h.last_sdl_serial = -1;
    h.last_sdl_target_y_java = -1;
    ++h.piece_generation;
    h.have_input = false;
    clear_result_locked(h);
}

// Cultris kind -> C2 rule piece letter.
char kind_to_piece_char(std::int32_t kind) noexcept {
    switch (kind) {
        case C2_KIND_I: return 'I';
        case C2_KIND_T: return 'T';
        case C2_KIND_S: return 'S';
        case C2_KIND_Z: return 'Z';
        case C2_KIND_L: return 'L';
        case C2_KIND_J: return 'J';
        case C2_KIND_O: return 'O';
        default:        return '\0';
    }
}

void install_c2_config_once(CultrisEngine& engine) {
    auto* cfg = engine.ai_config();
    cfg->p = {
        2.87224, 0.372169, 0.102604, 0.723501, 3.08721, 0.802789,
        -0.786174, 107.713, -0.540719, 109.116, -3.84305, 116.58,
        -1.00066, 49.7899, -1.23986, 391.808, -4.30493, 91.0623,
        -1.60608, 67.7934, 2.36365, 46016.9, 34.2515, 0.285739,
    };
    cfg->p_rate    = 1;
    cfg->safe      = 4;
    cfg->mode      = 0;
    cfg->soft_drop = 0;

    engine.search_config()->fast_move_down = true;

    auto* st = engine.status();
    st->combo       = 0;
    st->combo_limit = 20;
    st->value       = 0;
}

void update_c2_config_per_frame(CultrisEngine& engine, const C2StepInput& in) {
    auto* cfg = engine.ai_config();
    cfg->danger = (in.incoming_garbage >= 4) ? 1 : 0;
    (void)in.pressure_blend;
    (void)in.attack_curve_scale;
    auto* st = engine.status();
    st->combo = in.combo_count;
}

bool fill_map_from_input(CultrisMap& map, const C2StepInput& in) {
    for (int y = 0; y < kEngineH; ++y) {
        map.set_row(y, static_cast<CultrisMap::row_t>(in.row_bits[y] & kColMask));
    }
    return true;
}

// Resolve Java coordinates to the engine master-coordinate status used by the
// compile-time move generator.  kEngineH remains 28 because Java reports y in
// the 28-row Cultris playfield; the C2 search board itself is 21 rows high.
bool make_current_status(const C2StepInput& in, CultrisMap& map,
                         m_tetris2::TetrisBlockStatus* out_status,
                         char* out_piece) {
    const char cur_c = kind_to_piece_char(in.cur_kind);
    if (cur_c == 0 || !fill_map_from_input(map, in)) return false;

    const int rot = std::clamp(static_cast<int>(in.cur_r), 0, 3);
    const int ex = (cur_c == 'O') ? (in.cur_x - 1) : in.cur_x;
    const int java_matrix_top_offset = (cur_c == 'I') ? 0 : 1;
    const int start_y = std::clamp(
        kEngineH - 1 - static_cast<int>(in.cur_y) + java_matrix_top_offset,
        0, kEngineH - 1);

    std::array<CultrisMap, CultrisHelpers::kMaxR> usable{};
    CultrisHelpers::build_usable_for_piece(cur_c, map, usable);
    if (!CultrisHelpers::check_T(cur_c, ex, start_y,
                                 static_cast<std::uint8_t>(rot), usable)) {
        return false;
    }
    *out_status = m_tetris2::TetrisBlockStatus(
        cur_c, static_cast<std::int8_t>(ex), static_cast<std::int8_t>(start_y),
        static_cast<std::uint8_t>(rot));
    if (out_piece) *out_piece = cur_c;
    return true;
}

// Runs on the worker thread. Performs a full search on every input and
// publishes only the stable target-node pointer plus diagnostic coordinates.
bool plan_real(Handle& h, const C2StepInput& in,
               CultrisLandPoint* out_target,
               std::int32_t* out_tx, std::int32_t* out_ty_native,
               std::int32_t* out_ty_java, std::int32_t* out_tr) {
    *out_target = CultrisLandPoint{};
    if (out_tx) *out_tx = -1;
    if (out_ty_native) *out_ty_native = -1;
    if (out_ty_java) *out_ty_java = -1;
    if (out_tr) *out_tr = -1;

    const char cur_c = kind_to_piece_char(in.cur_kind);
    if (cur_c == 0) return false;

    if (!h.engine_prepared
        || h.engine_prepared_w != kBoardW
        || h.engine_prepared_h != kEngineH) {
        if (!h.engine.prepare(kBoardW, kEngineH)) return false;
        h.engine.memory_limit(1ull << 30);
        h.engine_prepared = true;
        h.engine_prepared_w = kBoardW;
        h.engine_prepared_h = kEngineH;
    }
    if (!h.config_installed) {
        install_c2_config_once(h.engine);
        h.config_installed = true;
    }
    update_c2_config_per_frame(h.engine, in);
    h.engine.ai_config()->mode = h.ai_mode.load(std::memory_order_acquire);

    CultrisMap map{};
    m_tetris2::TetrisBlockStatus status{};
    char resolved_piece = 0;
    if (!make_current_status(in, map, &status, &resolved_piece)) return false;

    std::string next;
    if (in.next_kind >= 0) {
        const char nc = kind_to_piece_char(in.next_kind);
        if (nc != 0) { next.push_back('?'); next.push_back(nc); next.push_back('?'); }
        else next.push_back('?');
    } else {
        next.push_back('?');
    }

    auto result = h.engine.run(map, status, next.data(), next.size(), kSearchBudgetMs);
    if (!CultrisEngine::TreeNode::lp_is_valid(result.target)) return false;

    const std::vector<char> validation_path = h.engine.make_path(status, result.target, map);
    (void)validation_path;
    const auto master = CultrisHelpers::master_xy_from_state(result.target.state);
    if (out_tx) *out_tx = static_cast<std::int32_t>(master.first);
    if (out_ty_native) *out_ty_native = static_cast<std::int32_t>(master.second);
    if (out_ty_java) *out_ty_java = static_cast<std::int32_t>((resolved_piece == 'I')
        ? (kEngineH - 1 - master.second)
        : (kEngineH - master.second));
    if (out_tr) *out_tr = static_cast<std::int32_t>(result.target.state.r);
    *out_target = result.target;
    return true;
}

// Background worker: after the first input, continuously search in fixed 10ms
// slices. Each slice snapshots the newest input; publication is rejected when
// piece identity or board generation changed while engine.run was in flight.
void ai_worker(Handle* h) {
    while (!h->stop_flag.load(std::memory_order_acquire)) {
        C2StepInput in_copy{};
        std::uint64_t my_gen = 0;
        std::uint64_t my_piece_generation = 0;
        {
            std::unique_lock<std::mutex> lk(h->input_mutex);
            h->input_cv.wait(lk, [h] {
                return h->have_input || h->stop_flag.load(std::memory_order_acquire);
            });
            if (h->stop_flag.load(std::memory_order_acquire)) break;
            in_copy = h->latest_input;
            my_gen = h->input_gen;
            my_piece_generation = h->piece_generation;
        }

        CultrisLandPoint target{};
        std::int32_t tx = -1, ty_native = -1, ty_java = -1, tr = -1;
        bool ok = false;
        {
            // If c2ai_step is waiting to consume a path, do not start another
            // slice until it has had exclusive engine access. The Java frame
            // thread remains non-blocking; JNI runs on NativeAiWorker.
            std::unique_lock<std::mutex> turn_lk(h->engine_turn_mutex);
            h->engine_turn_cv.wait(turn_lk, [h] {
                return !h->consumer_waiting
                    || h->stop_flag.load(std::memory_order_acquire);
            });
            if (h->stop_flag.load(std::memory_order_acquire)) break;
        }
        {
            // engine.run itself is not interruptible. Because each slice is 10ms,
            // a newer generation is picked up as soon as this bounded slice ends.
            std::lock_guard<std::mutex> elk(h->engine_mutex);
            ok = (in_copy.cur_kind >= 0)
                && plan_real(*h, in_copy, &target, &tx, &ty_native, &ty_java, &tr);
        }

        {
            std::unique_lock<std::mutex> ilk(h->input_mutex);
            if (h->piece_generation != my_piece_generation
                || !h->have_input
                || h->latest_input.piece_serial != in_copy.piece_serial
                || h->latest_input.board_gen != in_copy.board_gen) {
                if (info_log_enabled()) {
                    std::fprintf(stderr,
                                 "[C2AI] discard_stale_result serial=%lld board_gen=%lld input_gen=%llu piece_gen=%llu current_piece_gen=%llu\n",
                                 (long long)in_copy.piece_serial,
                                 (long long)in_copy.board_gen,
                                 (unsigned long long)my_gen,
                                 (unsigned long long)my_piece_generation,
                                 (unsigned long long)h->piece_generation);
                }
                continue;
            }
            std::unique_lock<std::mutex> rlk(h->result_mutex);
            const bool generation_changed = h->result_gen != my_gen
                || h->result_piece_generation != my_piece_generation
                || h->result_board_gen != in_copy.board_gen;
            h->last_ok = ok;
            h->last_target = ok ? target : CultrisLandPoint{};
            h->last_target_x = tx;
            h->last_target_y_native = ty_native;
            h->last_target_y_java = ty_java;
            h->last_target_r = tr;
            h->result_gen = my_gen;
            h->result_piece_generation = my_piece_generation;
            h->result_board_gen = in_copy.board_gen;
            h->plan_serial = in_copy.piece_serial;
            if (generation_changed && debug_log_enabled()) {
                std::fprintf(stderr,
                             "[C2AI] new_target ok=%d serial=%lld board_gen=%lld input_gen=%llu piece_gen=%llu kind=%d target(x=%d y=%d r=%d)\n",
                             ok ? 1 : 0, (long long)in_copy.piece_serial,
                             (long long)in_copy.board_gen,
                             (unsigned long long)my_gen,
                             (unsigned long long)my_piece_generation,
                             (int)in_copy.cur_kind, (int)tx, (int)ty_java, (int)tr);
            }
        }
        h->result_cv.notify_all();
        // Give a waiting c2ai_step a scheduling point between fixed slices;
        // the next search iteration still begins immediately afterwards.
        std::this_thread::yield();
    }
}

C2Op map_char_to_op(char c) noexcept {
    switch (c) {
        case 'l': return C2OP_MOVE_LEFT;
        case 'r': return C2OP_MOVE_RIGHT;
        case 'L': return C2OP_DAS_LEFT;
        case 'R': return C2OP_DAS_RIGHT;
        case 'c': return C2OP_ROT_CW;
        case 'z': return C2OP_ROT_CCW;
        case 'x': return C2OP_ROT_180;
        case 'd': return C2OP_SOFT_DROP;
        case 'D': return C2OP_SOFT_DROP_LOCK;  // slide to floor without locking; subsequent ops continue from floor
        case 'V': case 'v': return C2OP_HARD_DROP;
        default:  return C2OP_NONE;
    }
}

void invalidate_result_if_matches(Handle& h,
                                  CultrisLandPoint const& target,
                                  std::int64_t plan_serial,
                                  std::uint64_t plan_piece_generation,
                                  std::uint64_t result_gen) {
    std::lock_guard<std::mutex> rlk(h.result_mutex);
    if (h.last_target == target
        && h.plan_serial == plan_serial
        && h.result_piece_generation == plan_piece_generation
        && h.result_gen == result_gen) {
        clear_result_locked(h);
    }
}

bool hard_drop_gate(Handle& h, const C2StepInput& in,
                    std::int64_t plan_serial,
                    std::uint64_t plan_piece_generation,
                    std::int32_t target_x_java,
                    std::int32_t target_r) {
    if (in.cur_x != target_x_java || in.cur_r != target_r) return false;
    std::unique_lock<std::mutex> ilk(h.input_mutex);
    std::unique_lock<std::mutex> rlk(h.result_mutex);
    // input_gen/result_gen advance for ordinary per-frame replanning and are
    // deliberately not lifecycle epochs. Piece generation protects serial
    // identity; board_gen independently invalidates results after board change.
    return h.have_input
        && h.latest_input.piece_serial == in.piece_serial
        && h.latest_input.board_gen == in.board_gen
        && h.result_board_gen == in.board_gen
        && h.piece_generation == plan_piece_generation
        && plan_serial == in.piece_serial;
}

const char* op_name(C2Op op) noexcept {
    switch (op) {
        case C2OP_NONE:           return "NONE";
        case C2OP_ROT_CW:         return "ROT_CW";
        case C2OP_ROT_CCW:        return "ROT_CCW";
        case C2OP_ROT_180:        return "ROT_180";
        case C2OP_MOVE_LEFT:      return "MOVE_LEFT";
        case C2OP_MOVE_RIGHT:     return "MOVE_RIGHT";
        case C2OP_DAS_LEFT:       return "DAS_LEFT";
        case C2OP_DAS_RIGHT:      return "DAS_RIGHT";
        case C2OP_SOFT_DROP:      return "SOFT_DROP";
        case C2OP_HARD_DROP:      return "HARD_DROP";
        case C2OP_SOFT_DROP_LOCK: return "SOFT_DROP_LOCK";
        default:                  return "UNKNOWN";
    }
}

} // namespace

// ---- C API implementations -----------------------------------------------

extern "C" std::int64_t c2ai_create(void) {
    auto* h = new (std::nothrow) Handle();
    if (h == nullptr) return 0;
    try {
        h->ai_thread = std::thread(ai_worker, h);
    } catch (...) {
        delete h;
        return 0;
    }
    return reinterpret_cast<std::int64_t>(h);
}

extern "C" void c2ai_destroy(std::int64_t handle) {
    if (handle == 0) return;
    auto* h = reinterpret_cast<Handle*>(handle);
    {
        std::lock_guard<std::mutex> lk(h->input_mutex);
        h->stop_flag.store(true, std::memory_order_release);
    }
    h->input_cv.notify_all();
    h->engine_turn_cv.notify_all();
    if (h->ai_thread.joinable()) h->ai_thread.join();
    delete h;
}

extern "C" void c2ai_set_mode(std::int64_t handle, std::int32_t mode) {
    if (handle == 0) return;
    auto* h = reinterpret_cast<Handle*>(handle);
    const int normalized_mode = mode == 1 ? 1 : 0;
    if (h->ai_mode.exchange(normalized_mode, std::memory_order_acq_rel)
        != normalized_mode) {
        reset_plan_state(*h);
    }
}

extern "C" void c2ai_reset_plan(std::int64_t handle) {
    if (handle == 0) return;
    auto* h = reinterpret_cast<Handle*>(handle);
    reset_plan_state(*h);
}

extern "C" void c2ai_step(std::int64_t handle,
                          const C2StepInput* in,
                          C2StepOutput* out) {
    if (out == nullptr) return;
    std::memset(out, 0, sizeof(*out));

    if (handle == 0 || in == nullptr || in->api_version != C2_AI_API_VERSION) {
        out->next_op = C2OP_NONE;
        out->status = C2_STATUS_INVALID_INPUT;
        return;
    }
    auto* h = reinterpret_cast<Handle*>(handle);

    bool identity_changed = false;
    std::uint64_t current_piece_generation = 0;
    {
        std::unique_lock<std::mutex> ilk(h->input_mutex);
        identity_changed = in->piece_serial != h->last_piece_serial
            || in->cur_kind != h->last_kind;
        if (identity_changed) {
            std::unique_lock<std::mutex> rlk(h->result_mutex);
            h->last_piece_serial = in->piece_serial;
            h->last_kind = in->cur_kind;
            h->last_sdl_valid = false;
            h->last_sdl_serial = -1;
            h->last_sdl_target_y_java = -1;
            ++h->piece_generation;
            clear_result_locked(*h);
        }
        h->latest_input = *in;
        h->have_input = true;
        ++h->input_gen;
        current_piece_generation = h->piece_generation;
    }
    if (identity_changed && info_log_enabled()) {
        std::fprintf(stderr,
                     "[C2AI] new_piece_serial=%lld piece_gen=%llu kind=%d x=%d r=%d\n",
                     (long long)in->piece_serial,
                     (unsigned long long)current_piece_generation,
                     (int)in->cur_kind, (int)in->cur_x, (int)in->cur_r);
    }
    h->input_cv.notify_one();

    CultrisLandPoint target{};
    std::int32_t target_x = -1, target_y_native = -1;
    std::int32_t target_y_java = -1, target_r = -1;
    std::int64_t plan_serial = -1;
    std::uint64_t plan_piece_generation = 0, plan_result_gen = 0;
    {
        std::unique_lock<std::mutex> rlk(h->result_mutex, std::try_to_lock);
        if (!rlk.owns_lock()) {
            out->next_op = C2OP_NONE;
            out->status = C2_STATUS_SEARCHING;
            return;
        }
        if (!h->last_ok || h->last_target == nullptr) {
            out->next_op = C2OP_NONE;
            out->status = C2_STATUS_SEARCHING;
            return;
        }
        if (h->plan_serial != in->piece_serial
            || h->result_piece_generation != current_piece_generation
            || h->result_board_gen != in->board_gen) {
            out->next_op = C2OP_NONE;
            out->status = C2_STATUS_SEARCHING;
            return;
        }
        target = h->last_target;
        target_x = h->last_target_x;
        target_y_native = h->last_target_y_native;
        target_y_java = h->last_target_y_java;
        target_r = h->last_target_r;
        plan_serial = h->plan_serial;
        plan_piece_generation = h->result_piece_generation;
        plan_result_gen = h->result_gen;
    }

    {
        std::lock_guard<std::mutex> turn_lk(h->engine_turn_mutex);
        h->consumer_waiting = true;
    }
    // At most the current fixed 10ms engine.run slice can be ahead of us.
    // This executes on Java's NativeAiWorker, never on the frame thread.
    std::unique_lock<std::mutex> elk(h->engine_mutex);
    {
        std::lock_guard<std::mutex> turn_lk(h->engine_turn_mutex);
        h->consumer_waiting = false;
    }
    h->engine_turn_cv.notify_one();

    CultrisMap cur_map{};
    m_tetris2::TetrisBlockStatus cur_status{};
    char cur_piece = 0;
    if (!make_current_status(*in, cur_map, &cur_status, &cur_piece)
        || static_cast<char>(target.state.t) != cur_piece) {
        elk.unlock();
        invalidate_result_if_matches(*h, target, plan_serial,
                                     plan_piece_generation, plan_result_gen);
        out->next_op = C2OP_NONE;
        out->status = C2_STATUS_SEARCHING;
        return;
    }

    std::vector<char> path = h->engine.make_path(cur_status, target, cur_map);
    std::array<CultrisMap, CultrisHelpers::kMaxR> usable{};
    CultrisHelpers::build_usable_for_piece(cur_piece, cur_map, usable);
    const auto cur_state = CultrisHelpers::state_from_status(
        cur_piece, static_cast<std::uint8_t>(cur_status.r), cur_status.x, cur_status.y);
    const auto dropped = CultrisHelpers::drop(cur_state, usable);
    const bool at_filtered_target = dropped.has_value()
        && CultrisHelpers::cells_key_for_state(*dropped)
            == CultrisHelpers::cells_key_for_state(target.state);
    elk.unlock();

    const std::int32_t target_x_java_raw = (cur_piece == 'O')
        ? (target_x + 1) : target_x;
    (void)target_y_native;
    // C2 filtered nodes intentionally alias equivalent anchors/rotations (for
    // example I r0/r2). When the live drop already maps to the target slot,
    // expose the live Java x/r as the effective final target.
    const std::int32_t target_x_java = at_filtered_target
        ? in->cur_x : target_x_java_raw;
    const std::int32_t effective_target_r = at_filtered_target
        ? in->cur_r : target_r;
    C2Op op = path.empty() ? C2OP_HARD_DROP : map_char_to_op(path[0]);
    if (debug_log_enabled()) {
        std::fprintf(stderr,
                     "[plan_target] h=%lld piece_serial=%lld x=%d y=%d r=%d reached=%d input_gen=%llu piece_gen=%llu\n",
                     (long long)handle, (long long)plan_serial,
                     (int)target_x_java, (int)target_y_java,
                     (int)effective_target_r, at_filtered_target ? 1 : 0,
                     (unsigned long long)plan_result_gen,
                     (unsigned long long)plan_piece_generation);
    }
    const bool strict_y_required = h->last_sdl_valid
        && h->last_sdl_serial == in->piece_serial;
    const bool strict_y_ok = !strict_y_required
        || in->cur_y == h->last_sdl_target_y_java;

    if (op == C2OP_NONE
        || (op == C2OP_HARD_DROP
            && (!at_filtered_target
                || !strict_y_ok
                || !hard_drop_gate(*h, *in, plan_serial,
                                   plan_piece_generation,
                                   target_x_java, effective_target_r)))) {
        if (debug_log_enabled()) {
            std::fprintf(stderr,
                         "[c2step] reject_op serial=%lld plan_serial=%lld input_gen=%llu piece_gen=%llu op=%d strict_y=%d cur(x=%d y=%d r=%d) target(x=%d y=%d r=%d) path_len=%zu\n",
                         (long long)in->piece_serial, (long long)plan_serial,
                         (unsigned long long)plan_result_gen,
                         (unsigned long long)plan_piece_generation, (int)op,
                         strict_y_required ? 1 : 0,
                         (int)in->cur_x, (int)in->cur_y, (int)in->cur_r,
                         (int)target_x_java, (int)target_y_java,
                         (int)effective_target_r, path.size());
        }
        invalidate_result_if_matches(*h, target, plan_serial,
                                     plan_piece_generation, plan_result_gen);
        out->next_op = C2OP_NONE;
        out->status = C2_STATUS_SEARCHING;
        return;
    }

    if (op == C2OP_SOFT_DROP_LOCK) {
        h->last_sdl_valid = true;
        h->last_sdl_serial = in->piece_serial;
        h->last_sdl_target_y_java = target_y_java;
    } else {
        h->last_sdl_valid = false;
    }

    out->next_op = op;
    out->status = C2_STATUS_OK;
    if (op == C2OP_HARD_DROP && info_log_enabled()) {
        std::fprintf(stderr,
                     "[final_plan_target] h=%lld piece_serial=%lld x=%d y=%d r=%d input_gen=%llu piece_gen=%llu\n",
                     (long long)handle, (long long)plan_serial,
                     (int)target_x_java, (int)target_y_java,
                     (int)effective_target_r,
                     (unsigned long long)plan_result_gen,
                     (unsigned long long)plan_piece_generation);
    }
    if (debug_log_enabled()) {
        const char path_ch = path.empty() ? '-' : path[0];
        std::fprintf(stderr,
                     "[c2step] serial=%lld plan_serial=%lld input_gen=%llu piece_gen=%llu cur(x=%d y=%d r=%d kind=%d) -> path[0]=%c raw_op=%d target(x=%d y=%d r=%d) path_len=%zu\n",
                     (long long)in->piece_serial, (long long)plan_serial,
                     (unsigned long long)plan_result_gen,
                     (unsigned long long)plan_piece_generation,
                     (int)in->cur_x, (int)in->cur_y, (int)in->cur_r,
                     (int)in->cur_kind, path_ch, (int)op,
                     (int)target_x_java, (int)target_y_java,
                     (int)effective_target_r, path.size());
    }
}

extern "C" const char* c2ai_version(void) {
    static const char* kVersion = "8.4.0-generation-10ms";
    return kVersion;
}
