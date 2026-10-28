#include "../cultris/c2_ai_impl.cpp"

#include <cmath>
#include <cstdio>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>

namespace {
int failures = 0;

void expect(bool ok, const char* message) {
    if (!ok) {
        ++failures;
        std::printf("FAIL: %s\n", message);
    }
}

ai_zzz::C2::Config scoring_config(int safe, double bpm = 60.0) {
    ai_zzz::C2::Config cfg{};
    cfg.p_rate = 1;
    cfg.safe = safe;
    cfg.mode = 0;
    cfg.soft_drop = 1;
    cfg.planning_bpm = bpm;
    return cfg;
}

ai_zzz::C2::Status transition(std::size_t combo, double remaining,
                              std::size_t lines, double bpm = 60.0,
                              double new_hole = 0.0) {
    auto cfg = scoring_config(0, bpm);
    // These fixtures test physical timing, not low-potential continuation.
    // Isolate that policy via its production danger bypass; policy has its own test.
    cfg.danger = 1;
    ai_zzz::C2 ai;
    ai.init<rule_c2::TetrisRule::rule_spec>(&cfg);
    ai_zzz::C2::Result eval{};
    eval.clear = lines;
    eval.fill = 0.30;
    eval.hole = 0.02;
    eval.new_hole = new_hole;
    ai_zzz::C2::Status status{};
    status.combo = combo;
    status.combo_limit = remaining;
    m_tetris2::AIEnv env{};
    env.node = ' ';
    return ai.get(eval, 0, status, env);
}

ai_zzz::C2::Status score_fixture(int safe, double fill, double hole,
                                 double attack, double new_hole,
                                 std::size_t combo) {
    auto cfg = scoring_config(safe, 600.0);
    ai_zzz::C2 ai;
    ai.init<rule_c2::TetrisRule::rule_spec>(&cfg);
    ai_zzz::C2::Result eval{};
    eval.attack = attack;
    eval.clear = 0;
    eval.fill = fill;
    eval.hole = hole;
    eval.new_hole = new_hole;
    ai_zzz::C2::Status status{};
    status.combo = combo;
    status.combo_limit = 20.0;
    m_tetris2::AIEnv env{};
    env.node = ' ';
    return ai.get(eval, 0, status, env);
}

void check_reserved_abi_and_config_versions() {
    expect(sizeof(C2StepInput) == 304, "C2StepInput size stays ABI compatible");
    expect(offsetof(C2StepInput, reserved) == 288, "reserved offset stays ABI compatible");
    C2StepInput in{};
    in.reserved[C2_INPUT_SOFT_DROP] = 1;
    in.reserved[C2_INPUT_SAFE] = 2;
    in.reserved[C2_INPUT_PLANNING_BPM] = 123;
    in.reserved[C2_INPUT_COMBO_REMAINING_MS] = 4567;
    expect(input_soft_drop_enabled(in), "reserved[0] carries soft_drop");
    expect(input_safe(in) == 2, "reserved[1] carries safe");
    expect(input_planning_bpm(in) == 123, "reserved[2] carries planning BPM");
    expect(input_combo_remaining_ms(in) == 4567, "reserved[3] carries remaining ms");
    in.reserved[C2_INPUT_SOFT_DROP] = 0;
    expect(input_safe(in) == 2 && input_planning_bpm(in) == 123
               && input_combo_remaining_ms(in) == 4567,
           "all four ABI slots are independent");
    in.reserved[C2_INPUT_PLANNING_BPM] = 0;
    expect(input_planning_bpm(in) == kDefaultPlanningBpm,
           "zero legacy BPM uses safe default");
    in.reserved[C2_INPUT_PLANNING_BPM] = 241;
    expect(input_planning_bpm(in) == kMaxPlanningBpm,
           "planning BPM clamps to the F9-supported maximum");
    in.reserved[C2_INPUT_SAFE] = -20;
    expect(input_safe(in) == 0, "safe clamps low");
    in.reserved[C2_INPUT_COMBO_REMAINING_MS] = -1;
    expect(input_combo_remaining_ms(in) == 0, "remaining clamps low");

    CultrisEngine engine;
    expect(engine.prepare(kBoardW, kEngineH), "C2 engine prepares");
    install_c2_config_once(engine);
    C2StepInput frame{};
    frame.combo_count = 4;
    frame.reserved[C2_INPUT_SAFE] = 1;
    frame.reserved[C2_INPUT_PLANNING_BPM] = 120;
    frame.reserved[C2_INPUT_COMBO_REMAINING_MS] = 900;
    auto version = engine.search_version();
    bool changed = update_c2_config_per_frame(engine, frame);
    expect(changed && engine.ai_config()->safe == 1
               && engine.ai_config()->planning_bpm == 120.0,
           "safe/BPM install into config");
    if (changed) engine.update();
    auto configured_version = engine.search_version();
    expect(configured_version != version, "config change calls engine.update/version");
    frame.combo_count = 9;
    frame.reserved[C2_INPUT_COMBO_REMAINING_MS] = 1;
    changed = update_c2_config_per_frame(engine, frame);
    if (changed) engine.update();
    expect(!changed && engine.search_version() == configured_version,
           "combo/remaining payload alone does not invalidate search");
}

static_assert(ai_zzz::c2_detail::kMaxCombo == 16,
              "240 BPM legal combo reachability bound changed");

void check_combo_precompute_and_bound() {
    constexpr double scale = 1.95;
    constexpr double bias = 2.0;
    constexpr double divisor = 1.6;
    constexpr double exponent = 1.6;
    constexpr double offset = 1.0;
    constexpr double penalty = 0.1;
    constexpr double tolerance = 8.0 * std::numeric_limits<double>::epsilon();

    for (std::size_t combo = 0; combo <= ai_zzz::c2_detail::kMaxCombo; ++combo) {
        for (std::size_t clear = 0; clear <= ai_zzz::c2_detail::kMaxClear; ++clear) {
            const double expected = clear == 0 ? -0.39
                : scale * (static_cast<double>(clear) + bias) / divisor
                    / (std::pow(static_cast<double>(combo), exponent) + offset)
                    - static_cast<double>(combo) * penalty;
            const double actual = ai_zzz::c2_detail::kComboTable[combo].increment[clear];
            expect(std::abs(actual - expected) <= tolerance * std::max(1.0, std::abs(expected)),
                   "combo increment table matches original double formula");
        }
    }
    expect(ai_zzz::c2_detail::clamp_combo(0) == 0
               && ai_zzz::c2_detail::clamp_combo(16) == 16
               && ai_zzz::c2_detail::clamp_combo(17) == 16
               && ai_zzz::c2_detail::clamp_combo(1000000) == 16,
           "combo table index clamps at the proven bound");

    constexpr long double dt = 60.0L / 240.0L;
    long double remaining = 1.95L * 6.0L / 1.6L;
    std::size_t combo = 1;
    while (remaining >= dt) {
        const long double increment = 1.95L * 6.0L / 1.6L
            / (std::pow(static_cast<long double>(combo), 1.6L) + 1.0L)
            - static_cast<long double>(combo) * 0.1L;
        remaining = remaining - dt + increment;
        ++combo;
        if (remaining < 0.0L) break;
    }
    expect(combo == ai_zzz::c2_detail::kMaxCombo + 1 && remaining < 0.0L,
           "optimal 240 BPM recurrence proves combo 16 is last reachable state");

    const auto equal = transition(1, 0.25, 4, 240.0);
    const auto below = transition(1, std::nextafter(0.25, 0.0), 4, 240.0);
    expect(equal.combo == 2 && below.combo == 1,
           "240 BPM strict timeout boundary keeps equality active");
}

void check_combo_reference_model() {
    const auto expired = transition(1, 0.999, 4);
    const auto equal = transition(1, 1.000, 4);
    expect(expired.combo == 1, "remaining<dt expires before lock");
    expect(equal.combo == 2, "remaining==dt preserves strict Java boundary");

    const auto ms_low = transition(1, 0.999, 4);
    const auto ms_equal = transition(1, 1.000, 4);
    expect(ms_low.combo == 1 && ms_equal.combo == 2,
           "millisecond boundary straddles strict timeout");

    const auto zero = transition(3, 2.0, 0);
    expect(zero.combo == 3 && std::abs(zero.combo_limit - 0.61) < 1e-12,
           "zero-line active combo applies dt and -0.39");

    const double active_inc = 1.95 * (2.0 + 2.0) / 1.6
        / (std::pow(3.0, 1.6) + 1.0) - 3.0 * 0.1;
    const auto active = transition(3, 2.0, 2);
    expect(active.combo == 4
               && std::abs(active.combo_limit - (1.0 + active_inc)) < 1e-12,
           "active combo clear matches Java formula");

    const auto fresh = transition(0, 0.0, 1);
    expect(fresh.combo == 1 && std::abs(fresh.combo_limit - 3.65625) < 1e-12,
           "no-combo clear resets deadline and starts combo=1");

    const auto high = transition(100, 2.0, 1);
    expect(high.combo == 0 && high.combo_limit == 0.0,
           "high combo negative increment normalizes expired child safely");

    auto first = transition(0, 0.0, 1, 120.0);
    auto second = transition(first.combo, first.combo_limit, 0, 120.0);
    auto third = transition(second.combo, second.combo_limit, 2, 120.0);
    expect(first.combo == 1 && second.combo == 1 && third.combo == 2,
           "continuous multi-piece propagation advances only on simulated locks");

    ai_zzz::C2::Status parent{};
    parent.combo = 7;
    parent.combo_limit = 1.25;
    const auto child = transition(parent.combo, parent.combo_limit, 1, 120.0);
    expect(parent.combo == 7 && parent.combo_limit == 1.25
               && (child.combo != parent.combo || child.combo_limit != parent.combo_limit),
           "only parent-to-child simulated merge advances combo_limit");

    const auto huge = transition(1000000, 1000.0, 1, 1000.0);
    const auto bounded = transition(ai_zzz::c2_detail::kMaxCombo, 1000.0, 1, 1000.0);
    expect(std::isfinite(huge.value), "high-combo scoring table index is bounded");
    expect(huge.combo == ai_zzz::c2_detail::kMaxCombo
               && huge.combo_limit == bounded.combo_limit
               && huge.value == bounded.value,
           "over-bound combo clamps timing and scoring to the same entry");
}

C2StepInput base_step_input() {
    C2StepInput in{};
    in.api_version = C2_AI_API_VERSION;
    in.cur_kind = C2_KIND_T;
    in.cur_x = 3;
    in.cur_y = 8;
    in.next_kind = C2_KIND_I;
    in.piece_serial = 10;
    in.board_gen = 20;
    in.combo_count = 4;
    in.reserved[C2_INPUT_PLANNING_BPM] = 120;
    in.reserved[C2_INPUT_COMBO_REMAINING_MS] = 900;
    return in;
}

void check_root_latching_and_generation() {
    const std::int64_t raw = c2ai_create();
    expect(raw != 0, "native handle creates");
    if (raw == 0) return;
    auto* h = reinterpret_cast<Handle*>(raw);
    C2StepOutput out{};
    C2StepInput in = base_step_input();
    c2ai_step(raw, &in, &out);
    std::uint64_t generation = 0;
    {
        std::lock_guard<std::mutex> lock(h->input_mutex);
        generation = h->piece_generation;
        expect(h->latest_input.combo_count == 4
                   && input_combo_remaining_ms(h->latest_input) == 900,
               "first real piece latches combo root");
    }

    in.combo_count = 3;
    in.reserved[C2_INPUT_COMBO_REMAINING_MS] = 800;
    c2ai_step(raw, &in, &out);
    {
        std::lock_guard<std::mutex> lock(h->input_mutex);
        expect(h->piece_generation == generation
                   && h->latest_input.combo_count == 4
                   && input_combo_remaining_ms(h->latest_input) == 900,
               "same piece frame decay neither relatches nor updates generation");
    }

    in.reserved[C2_INPUT_PLANNING_BPM] = 121;
    c2ai_step(raw, &in, &out);
    {
        std::lock_guard<std::mutex> lock(h->input_mutex);
        expect(h->piece_generation > generation
                   && h->latest_input.combo_count == 4
                   && input_combo_remaining_ms(h->latest_input) == 900,
               "BPM invalidates old results but preserves root combo latch");
        generation = h->piece_generation;
    }

    in.board_gen = 21;
    in.combo_count = 2;
    in.reserved[C2_INPUT_COMBO_REMAINING_MS] = 700;
    c2ai_step(raw, &in, &out);
    {
        std::lock_guard<std::mutex> lock(h->input_mutex);
        expect(h->piece_generation > generation && h->latest_input.combo_count == 2
                   && input_combo_remaining_ms(h->latest_input) == 700,
               "board generation change resynchronizes root combo");
        generation = h->piece_generation;
    }

    in.piece_serial = 11;
    in.combo_count = 5;
    in.reserved[C2_INPUT_COMBO_REMAINING_MS] = 650;
    c2ai_step(raw, &in, &out);
    {
        std::lock_guard<std::mutex> lock(h->input_mutex);
        expect(h->piece_generation > generation && h->latest_input.combo_count == 5
                   && input_combo_remaining_ms(h->latest_input) == 650,
               "new piece resynchronizes root combo");
    }

    in.piece_serial = 12;
    in.combo_count = 1000000;
    c2ai_step(raw, &in, &out);
    {
        std::lock_guard<std::mutex> lock(h->input_mutex);
        expect(h->latest_input.combo_count
                   == static_cast<std::int32_t>(ai_zzz::c2_detail::kMaxCombo),
               "out-of-domain Java root combo clamps before latching");
    }
    c2ai_destroy(raw);
}

void check_hole_scoring_fixtures() {
    struct Fixture { const char* name; double fill; double hole; };
    const Fixture fixtures[] = {{"low-stack", 0.22, 0.01}, {"high-stack", 0.78, 0.12}};
    constexpr double new_hole = 0.1;
    constexpr double advantage = 4000.0;
    for (int safe = 0; safe <= 3; ++safe) {
        for (const auto& fixture : fixtures) {
            const auto a = score_fixture(safe, fixture.fill, fixture.hole, 0, 0, 0);
            const auto b = score_fixture(safe, fixture.fill, fixture.hole,
                                         advantage, new_hole, 0);
            const double upstack = std::max(0.0, 1.0 - fixture.hole * 3.3)
                * std::max(0.0, 1.0 - (fixture.fill < 0.4 ? 0.0 : fixture.fill - 0.4) * 4.0);
            const double expected_margin = 60000.0 * new_hole - advantage * upstack;
            expect(a.value > b.value, fixture.name);
            expect(std::abs((a.value - b.value) - expected_margin) < 1e-9,
                   "combo==0 keeps new_hole=60000");
        }
    }
    auto combo_a = score_fixture(2, 0.30, 0.02, 123, 0.0, 4);
    auto combo_b = score_fixture(2, 0.30, 0.02, 123, 0.7, 4);
    expect(std::abs(combo_a.value - combo_b.value) < 1e-9,
           "combo>0 ignores new_hole");
}

void check_source_contracts() {
    std::ifstream input(CULTRIS_SAFE_SOURCE_DIR "/src/ai_zzz.h");
    std::ostringstream text;
    text << input.rdbuf();
    const std::string source = text.str();
    expect(source.find("double BoardDeadZone = map_in_danger_(map);") != std::string::npos,
           "DeadZone function remains map_in_danger_(map)");
    expect(source.find("BoardDeadZone += 70;") != std::string::npos,
           "DeadZone top-row increment remains 70");
    expect(source.find("(BoardDeadZone + config_->p[20]) * config_->p[21]") != std::string::npos,
           "DeadZone parameter slots remain p[20]/p[21]");
    expect(source.find("transposition_key() const") == std::string::npos,
           "C2 transposition remains disabled");

    const auto assert_get_has_no_pow = [](const char* path, const char* signature) {
        std::ifstream stream(path);
        std::ostringstream buffer;
        buffer << stream.rdbuf();
        const std::string unit = buffer.str();
        const std::size_t begin = unit.find(signature);
        const std::size_t end = unit.find("C2::Status C2::iterate", begin);
        expect(begin != std::string::npos && end != std::string::npos
                   && unit.substr(begin, end - begin).find("std::pow") == std::string::npos,
               "C2::get hot path contains no runtime pow");
    };
    assert_get_has_no_pow(CULTRIS_SAFE_SOURCE_DIR "/src/ai_zzz.cpp",
                          "C2::Status C2::get(Result const &eval_result");
    assert_get_has_no_pow(CULTRIS_SAFE_SOURCE_DIR "/oracle/ai_zzz.cpp",
                          "C2::Status C2::get(m_tetris::TetrisNode const *node");
}
} // namespace

int main() {
    check_reserved_abi_and_config_versions();
    check_combo_precompute_and_bound();
    check_combo_reference_model();
    check_root_latching_and_generation();
    check_hole_scoring_fixtures();
    check_source_contracts();
    std::printf("%s cultris_safe_regression_test (%d failures)\n",
                failures == 0 ? "PASS" : "FAIL", failures);
    return failures == 0 ? 0 : 1;
}
