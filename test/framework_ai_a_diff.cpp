// framework_ai_a_diff.cpp — AI shard A (qq::Attack / Dig / TOJ_PC / Botris_PC) 独立旧新数值对拍.
//
// 归属: A 组 owner (s_000pfnS10ab). 本文件是本 shard 唯一允许新增的 test;
//       CMake 注册由 test owner 完成 (本 owner 不改 CMake / 既有 tests).
//
// 目的: 在新契约 eval(map) + GetArgs get 下, 对 A 组 4 个 AI 的 Result/Status
//       全字段做确定性逐位对拍。基准 = 冻结旧源 baseline_freeze_7af0537a_20261028
//       (旧 4 参 eval/get)。比对口径 = 全字段逐行 (%.17g), 不是源码 diff, 也不是
//       新公式对新公式。golden 由同目录 framework_ai_a_diff.golden 给出, 且可由
//       同目录 a_diff_repro.sh 从旧基线重新生成。
//
// 覆盖矩阵 (A 组): clear 0..4; spin TSpin(None/Mini/TSpin) 与 ASpin(None/ASpin,
//   含 tspin_for_clear 转换); Attack mode 0..3 (含 case3 mode!=0 break 穿透/default);
//   depth 0..2; 6 种盘形 (最小/平底/洞/高栈/well/近顶); land_height 含==map_height
//   触发 DeadZone=500000; col 两端与中列; parent 累积 (under/recv/combo/b2b/like/pc).
//
// 注意: 这是单元级数值对拍 (直接实例化 AI 模板跑 eval(map)/get), 不是 full-engine
//   端到端 smoke; full-engine 接线与冒烟由 test/core owner 在 core 落定后验证。

#include "../src/tetris_core.h"
#include "../src/tetris_eval_get.h"
#include "../src/ai_zzz.h"
#include "../src/rule_srs.h"
#include "../src/tetris_node.h"
#include "../src/tetris_shape.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "_a_cases.h"

using Spec = rule_srs::TetrisRule::rule_spec;
using MapT = m_tetris2::TetrisMap<Spec::width, Spec::height>;
using TSpin = search_tspin::Search::TSpinType;
using ASpin = search_aspin::Search::ASpinType;

static MapT board_from(const aicases::Board &b)
{
    MapT m{};
    for (int y = 0; y < Spec::height; ++y)
        m.set_row(y, static_cast<MapT::row_t>(MapT::row_full ^ b.rows[y]));
    return m;
}

static constexpr int kPieceH = (int)m_tetris2::shape::piece_h<Spec, 'T', (uint8_t)0>;

template <class SpinT>
static m_tetris2::GetLanding<SpinT> make_landing(int col, int land_height, SpinT spin)
{
    m_tetris2::GetLanding<SpinT> L;
    L.xb = static_cast<int8_t>(col);
    L.h = static_cast<int8_t>(kPieceH);
    L.yb = static_cast<int8_t>(land_height - kPieceH);
    if constexpr (!std::is_same_v<SpinT, std::monostate>)
        L.spin = spin;
    return L;
}

int main()
{
    using namespace ai_zzz;

    qq::Attack::Config attack_cfg{0, 0};
    qq::Attack attack;
    attack.init<Spec>(&attack_cfg);

    Dig::Config dig_cfg;
    Dig dig;
    dig.init<Spec>(&dig_cfg);

    const int *ct = aicases::combo_table();
    TOJ_PC::Config toj_cfg{ct, aicases::combo_table_max};
    TOJ_PC toj;
    toj.init<Spec>(&toj_cfg);

    Botris_PC::Config bot_cfg{ct, aicases::combo_table_max};
    Botris_PC bot;
    bot.init<Spec>(&bot_cfg);

    std::vector<std::string> actual;
    char buf[512];

    for (int i = 0; i < aicases::kNumCases; ++i)
    {
        auto c = aicases::decode(i);
        attack_cfg.mode = c.attack_mode;
        MapT map = board_from(aicases::make_board(c.board_kind));

        TSpin tspin = (c.spin == 0) ? TSpin::None : (c.spin == 1 ? TSpin::TSpin : TSpin::TSpinMini);
        ASpin aspin = (c.spin == 0) ? ASpin::None : ASpin::ASpin;

        auto land_ms = make_landing<std::monostate>(c.col, c.land_height, std::monostate{});
        auto land_toj = make_landing<TSpin>(c.col, c.land_height, tspin);
        auto land_bot = make_landing<ASpin>(c.col, c.land_height, aspin);

        {
            auto er = attack.eval(map);
            double LH = land_ms.land_height();
            double Mid = std::fabs((land_ms.col() + 1) * 2 - (int)Spec::width);
            double land_point = (0. - LH * 16 + Mid * 0.2 + (double)c.clear * 6 - ((LH == (int)Spec::height) ? 500000. : 0.) - er.board_dz * 500000.);
            qq::Attack::Status parent{c.p_land_point, c.p_attack, c.p_rubbish, c.p_value};
            auto st = attack.get(er, land_ms, c.clear, parent, c.depth);
            std::snprintf(buf, sizeof buf,
                          "A case=%d eval.land_point=%.17g eval.map=%.17g eval.danger=%d get.land_point=%.17g get.attack=%.17g get.rubbish=%.17g get.value=%.17g",
                          i, land_point, er.map, er.danger, st.land_point, st.attack, st.rubbish, st.value);
            actual.emplace_back(buf);
        }
        {
            double er = dig.eval(map);
            double st = dig.get(er);
            std::snprintf(buf, sizeof buf, "D case=%d eval.value=%.17g get.value=%.17g", i, er, st);
            actual.emplace_back(buf);
        }
        {
            auto er = toj.eval(map);
            TOJ_PC::Status parent{c.p_under_attack, c.p_recv_attack, c.p_attack_int, c.p_like, c.p_combo, c.p_b2b, c.p_pc, c.p_value};
            auto st = toj.get(er, c.clear, land_toj, parent);
            int tspin_eq = (c.clear == 0) ? 0 : (tspin == TSpin::TSpinMini && c.clear != 1 ? 1 : (int)tspin);
            std::snprintf(buf, sizeof buf,
                          "T case=%d eval.value=%.17g eval.roof=%d eval.tspin=%d get.attack=%d get.like=%d get.combo=%d get.b2b=%d get.pc=%d get.recv=%d get.under=%d get.value=%.17g",
                          i, er.value, er.roof, tspin_eq, st.attack, st.like, st.combo, (int)st.b2b, (int)st.pc, st.recv_attack, st.under_attack, st.value);
            actual.emplace_back(buf);
        }
        {
            auto er = bot.eval(map);
            Botris_PC::Status parent{c.p_under_attack, c.p_recv_attack, c.p_attack_int, c.p_like, c.p_combo, c.p_b2b, c.p_pc, c.p_value};
            auto st = bot.get(er, c.clear, land_bot, parent);
            std::snprintf(buf, sizeof buf,
                          "B case=%d eval.value=%.17g eval.roof=%d eval.aspin=%d get.attack=%d get.like=%d get.combo=%d get.b2b=%d get.pc=%d get.recv=%d get.under=%d get.value=%.17g",
                          i, er.value, er.roof, (int)aspin, st.attack, st.like, st.combo, (int)st.b2b, (int)st.pc, st.recv_attack, st.under_attack, st.value);
            actual.emplace_back(buf);
        }
    }

    // 比对同目录 golden (相对本测试运行目录)。
    std::FILE *g = std::fopen("framework_ai_a_diff.golden", "r");
    if (!g) { std::fprintf(stderr, "golden not found (run from test/ dir): framework_ai_a_diff.golden\n"); return 2; }
    std::vector<std::string> golden;
    char line[512];
    while (std::fgets(line, sizeof line, g)) {
        std::string s = line;
        while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) s.pop_back();
        golden.push_back(s);
    }
    std::fclose(g);

    int bad = 0;
    if (golden.size() != actual.size())
        std::fprintf(stderr, "line count: golden=%zu actual=%zu\n", golden.size(), actual.size());
    size_t n = std::min(golden.size(), actual.size());
    for (size_t k = 0; k < n; ++k)
        if (golden[k] != actual[k]) {
            ++bad;
            if (bad <= 10) std::fprintf(stderr, "L%zu\n  want %s\n  got  %s\n", k, golden[k].c_str(), actual[k].c_str());
        }
    if (bad == 0 && golden.size() == actual.size()) {
        std::printf("framework_ai_a_diff: %zu/%zu lines bitwise-identical to golden\n", actual.size(), actual.size());
        return 0;
    }
    std::fprintf(stderr, "framework_ai_a_diff: %d mismatches\n", bad);
    return 1;
}
