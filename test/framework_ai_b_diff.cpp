// framework_ai_b_diff.cpp — AI shard B (TOJ_v08/Botris/TOJ/C2) 独立旧新数值对拍.
//
// 归属: AI shard B owner (ai_zzz.h/.cpp). 本文件是本 shard 唯一允许新增的 test;
//       CMake 注册由 test owner 完成.
//
// 在新契约 eval(map) + GetArgs get 下，对 B 组 4 个 AI 的 Result/Status 全字段做
// 逐位(%.17g)对拍。基准 = 冻结旧源 baseline (旧 4 参 eval/get)，由
// _b_old_golden_main.cpp 生成 framework_ai_b_diff.golden。不是源码 diff，也不是新对新。
#include "../src/tetris_core.h"
#include "../src/tetris_eval_get.h"
#include "../src/ai_zzz.h"
#include "../src/rule_srs.h"
#include "../src/rule_c2.h"
#include "../src/tetris_node.h"
#include "../src/tetris_shape.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "_b_cases.h"

using Spec = rule_srs::TetrisRule::rule_spec;
using MapT = m_tetris2::TetrisMap<Spec::width, Spec::height>;
using TSpin = search_tspin::Search::TSpinType;
using ASpin = search_aspin::Search::ASpinType;

static MapT board_from(const bicases::Board &b)
{
    MapT m{};
    for (int y = 0; y < Spec::height; ++y)
        m.set_row(y, static_cast<MapT::row_t>(MapT::row_full ^ b.rows[y]));
    return m;
}

using DetT = m_tetris2::TetrisNodeDetails<Spec, 'T', (uint8_t)0>;
using DetI = m_tetris2::TetrisNodeDetails<Spec, 'I', (uint8_t)0>;

// 与引擎 tetris_core.h child 创建一致：从真实 TetrisNodeDetails 取 kHeight/kWidth/kLines，
// yb = land_height - kHeight（= 旧 lp.row() = n.y - kOriginY），保证 SRS 4x4 padding 同口径。
template <class Details, class SpinT>
static m_tetris2::GetLanding<SpinT> make_landing(int col, int land_height, SpinT spin, char piece)
{
    using Base = m_tetris2::TetrisNodeBase<Details>;
    m_tetris2::GetLanding<SpinT> L{};
    L.piece = piece;
    L.xb = static_cast<int8_t>(col);
    L.yb = static_cast<int8_t>(land_height - Base::kHeight);
    L.w = static_cast<int8_t>(Base::kWidth);
    L.h = static_cast<int8_t>(Base::kHeight);
    for (int i = 0; i < Base::kHeight && i < (int)m_tetris2::GetLanding<SpinT>::kMaxLines; ++i)
        L.lines[i] = Base::kLines[i];
    if constexpr (!std::is_same_v<SpinT, std::monostate>)
        L.spin = spin;
    return L;
}

static m_tetris2::AIEnv make_env(const char *next, size_t len, char node, char hold)
{
    m_tetris2::AIEnv e{};
    e.next = next; e.length = len; e.node = node; e.hold = hold; e.is_hold = (hold != ' ');
    return e;
}

int main()
{
    using namespace ai_zzz;

    int const *ct = bicases::combo_table();
    TOJ_v08::Config v08_cfg{ct, bicases::combo_table_max()};
    TOJ_v08 v08; v08.init<Spec>(&v08_cfg);
    Botris::Config bot_cfg{ct, bicases::combo_table_max()};
    Botris bot; bot.init<Spec>(&bot_cfg);
    TOJ::Config toj_cfg{ct, bicases::combo_table_max()};
    toj_cfg.safe = 4;
    TOJ toj; toj.init<Spec>(&toj_cfg);
    C2::Config c2_cfg{};
    c2_cfg.mode = 0; c2_cfg.safe = 4; c2_cfg.planning_bpm = 60.0;
    C2 c2; c2.init<Spec>(&c2_cfg);

    static char nextbuf[16];
    int mismatches = 0;

    for (int i = 0; i < bicases::kNumCases(); ++i)
    {
        auto c = bicases::table()[i];
        MapT map = board_from(bicases::make_board(c.board_kind));
        MapT src = map;
        TSpin tspin = (c.spin == 0) ? TSpin::None : (c.spin == 1 ? TSpin::TSpin : TSpin::TSpinMini);
        ASpin aspin = (c.spin == 0) ? ASpin::None : ASpin::ASpin;
        const char *seq = "TISJOLZ";
        std::memset(nextbuf, 0, sizeof(nextbuf));
        for (int k = 0; k < c.env_len; ++k) nextbuf[k] = seq[k % 7];
        m_tetris2::AIEnv env = make_env(nextbuf, (size_t)c.env_len, c.node, c.hold);

        auto lt = make_landing<DetT>(c.col, c.land_height, tspin, 'T');
        auto la = make_landing<DetT>(c.col, c.land_height, aspin, 'T');
        auto lm = make_landing<DetI>(c.col, c.land_height, std::monostate{}, 'I');

        // ---- TOJ_v08 ----
        {
            auto er = v08.eval(map);
            TOJ_v08::Status parent{};
            parent.combo = 1; parent.b2b = 1; parent.under_attack = 2; parent.attack = 1;
            auto st = v08.get(er, c.clear, lt, parent, (size_t)c.depth, env);
            std::printf("V case=%d eval.value=%.17g eval.count=%d eval.t2=%d eval.t3=%d eval.roof=%d "
                        "get.attack=%d get.like=%.17g get.combo=%d get.b2b=%d get.death=%d get.maprise=%d get.value=%.17g\n",
                        i, er.value, er.count, er.t2_value, er.t3_value, er.roof,
                        st.attack, st.like, st.combo, (int)st.b2b, (int)st.death, st.map_rise, st.value);
        }
        // ---- Botris ----
        {
            auto er = bot.eval(map);
            Botris::Status parent{};
            parent.combo = 1; parent.b2b = 1; parent.under_attack = 2; parent.attack = 1;
            auto st = bot.get(er, c.clear, la, parent, (size_t)c.depth, env);
            std::printf("R case=%d eval.value=%.17g eval.count=%d eval.roof=%d "
                        "get.attack=%d get.like=%.17g get.combo=%d get.b2b=%d get.clear=%d get.death=%d get.value=%.17g\n",
                        i, er.value, er.count, er.roof,
                        st.attack, st.like, st.combo, (int)st.b2b, st.clear, (int)st.death, st.value);
        }
        // ---- TOJ ----
        {
            auto er = toj.eval(map);
            TOJ::Status parent{};
            parent.combo = 1; parent.b2b = 1; parent.under_attack = 2; parent.t2_value = er.t2_value; parent.t3_value = er.t3_value;
            auto st = toj.get(er, c.clear, lt, parent, env);
            std::printf("O case=%d eval.value=%.17g eval.count=%d eval.t2=%d eval.t3=%d eval.roof=%d "
                        "get.under=%d get.like=%.17g get.combo=%d get.b2b=%d get.death=%d get.acc=%.17g get.value=%.17g\n",
                        i, er.value, er.count, er.t2_value, er.t3_value, er.roof,
                        st.under_attack, st.like, (int)st.combo, (int)st.b2b, (int)st.death, st.acc_value, st.value);
        }
        // ---- C2 ----
        {
            auto er = c2.eval(map);
            C2::Status parent{};
            parent.combo = 1; parent.map = 0; parent.attack = 1; parent.combo_limit = 1.0;
            auto st = c2.get(er, lm, c.clear, src, parent, c.node, (size_t)c.env_len);
            std::printf("C case=%d eval.map=%.17g eval.attack=%.17g eval.fill=%.17g eval.cells=%d "
                        "get.attack=%.17g get.map=%.17g get.combo=%zu get.combolimit=%.17g get.value=%.17g\n",
                        i, er.map, er.attack, er.fill, er.cells,
                        st.attack, st.map, st.combo, st.combo_limit, st.value);
        }
    }

    // ---- 10x28 production rule_c2 独立对拍: C2::get<MapT> 模板在 10x28 实例化链接 ----
    {
        using C2Spec = rule_c2::TetrisRule::rule_spec;
        using C2MapT = m_tetris2::TetrisMap<C2Spec::width, C2Spec::height>;
        using C2DetI = m_tetris2::TetrisNodeDetails<C2Spec, 'I', (uint8_t)0>;
        C2::Config cfg28{};
        cfg28.mode = 0; cfg28.safe = 4; cfg28.planning_bpm = 120.0;  // BPM=120 -> dt=0.5s, 触发 remaining<dt 分支
        C2 c2_28; c2_28.init<C2Spec>(&cfg28);

        // 4 个 10x28 盘形: (board rows[0..27], land_height, clear, next_len)
        struct C28 { uint32_t rows[28]; int land; int clear; int nlen; };
        C28 cases[4] = {};
        // case 0: 空底 + 平地 (cells=4, 无洞)
        cases[0].rows[0]=0x3ff; cases[0].rows[1]=0x3ff; cases[0].land=2; cases[0].clear=0; cases[0].nlen=2;
        // case 1: 含洞 (row2 col5 空), clear=1
        cases[1].rows[0]=0x3ff; cases[1].rows[1]=0x3ff; cases[1].rows[2]=(0x3ff ^ (1u<<5)); cases[1].rows[3]=0x3ff;
        cases[1].land=4; cases[1].clear=1; cases[1].nlen=3;
        // case 2: 高栈到 row 20 (近顶), old_combo==0 分支
        for (int y=0;y<16;++y) cases[2].rows[y]=0x3ff; cases[2].rows[16]=0x3ff;
        cases[2].land=17; cases[2].clear=2; cases[2].nlen=1;
        // case 3: 顶置 (row 24..27 占满, topout/soft_drop 路径)
        cases[3].rows[24]=0x3ff; cases[3].rows[25]=0x3ff; cases[3].rows[26]=0x3ff; cases[3].rows[27]=0x3ff;
        cases[3].land=24; cases[3].clear=0; cases[3].nlen=0;

        for (int i=0;i<4;++i) {
            C2MapT m{}; C2MapT src{};
            for (int y=0;y<C2Spec::height;++y) {
                typename C2MapT::row_t r = static_cast<typename C2MapT::row_t>(C2MapT::row_full ^ cases[i].rows[y]);
                m.set_row(y, r); src.set_row(y, r);
            }
            auto er = c2_28.eval(m);
            auto lm28 = make_landing<C2DetI>(3, cases[i].land, std::monostate{}, 'I');
            C2::Status parent{}; parent.combo = (i==2)?0:1; parent.map=0; parent.attack=1; parent.combo_limit=0.3;
            auto st = c2_28.get(er, lm28, cases[i].clear, src, parent, 'T', (size_t)cases[i].nlen);
            std::printf("C28 case=%d eval.map=%.17g eval.attack=%.17g eval.fill=%.17g eval.cells=%d "
                        "get.attack=%.17g get.map=%.17g get.combo=%zu get.combolimit=%.17g get.value=%.17g\n",
                        i, er.map, er.attack, er.fill, er.cells,
                        st.attack, st.map, st.combo, st.combo_limit, st.value);
        }
    }
    return mismatches;
}
