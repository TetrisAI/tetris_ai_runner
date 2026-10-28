// _b_old_golden_main.cpp — 冻结旧基线(snapshot 真实旧 ai_zzz) B组对拍 driver.
// 不重写旧公式；直接调用旧 4 参 eval / 旧 get，逐 case 打印字段作为黄金基线.
#include "ai_zzz.h" // from frozen snapshot src (via -I)
#include "rule_srs.h"
#include "rule_c2.h"
#include "tetris_node.h"

#include <cstdio>
#include <cstdint>
#include <cstring>

#include "_b_cases.h"

using Spec = rule_srs::TetrisRule::rule_spec;
using MapT = m_tetris2::TetrisMap<Spec::width, Spec::height>;

static MapT board_from(const bicases::Board &b)
{
    MapT m{};
    for (int y = 0; y < Spec::height; ++y)
        m.set_row(y, static_cast<MapT::row_t>(MapT::row_full ^ b.rows[y]));
    return m;
}

template <class Details, class SpinT>
static m_tetris2::TetrisNode<Details, SpinT> make_node(int col, int land_height, SpinT spin)
{
    using NB = m_tetris2::TetrisNodeBase<Details>;
    m_tetris2::TetrisNode<Details, SpinT> n;
    n.x = static_cast<int8_t>(col);
    n.y = static_cast<int8_t>(land_height - NB::kHeight + NB::kOriginY);
    if constexpr (!std::is_same_v<SpinT, std::monostate>)
        n.spin = spin;
    return n;
}

static m_tetris2::AIEnv make_env(const char *next, size_t len, char node, char hold)
{
    m_tetris2::AIEnv e{};
    e.next = next;
    e.length = len;
    e.node = node;
    e.hold = hold;
    e.is_hold = (hold != ' ');
    return e;
}

int main()
{
    using namespace ai_zzz;
    using TSpin = search_tspin::Search::TSpinType;
    using ASpin = search_aspin::Search::ASpinType;

    int const *ct = bicases::combo_table();

    TOJ_v08::Config v08_cfg{ct, bicases::combo_table_max()};
    TOJ_v08 v08; v08.init<Spec>(&v08_cfg);

    Botris::Config bot_cfg{ct, bicases::combo_table_max()};
    Botris bot; bot.init<Spec>(&bot_cfg);

    TOJ::Config toj_cfg{ct, bicases::combo_table_max()};
    toj_cfg.safe = 4;
    TOJ toj; toj.init<Spec>(&toj_cfg);

    C2::Config c2_cfg{};
    c2_cfg.mode = 0;
    c2_cfg.safe = 4;
    c2_cfg.planning_bpm = 60.0;
    C2 c2; c2.init<Spec>(&c2_cfg);

    using DetT = m_tetris2::TetrisNodeDetails<Spec, 'T', (uint8_t)0>;
    using DetM = m_tetris2::TetrisNodeDetails<Spec, 'I', (uint8_t)0>;

    static char nextbuf[16];

    for (int i = 0; i < bicases::kNumCases(); ++i)
    {
        auto c = bicases::table()[i];
        MapT map = board_from(bicases::make_board(c.board_kind));
        MapT src = map;

        TSpin tspin = (c.spin == 0) ? TSpin::None : (c.spin == 1 ? TSpin::TSpin : TSpin::TSpinMini);
        ASpin aspin = (c.spin == 0) ? ASpin::None : ASpin::ASpin;

        // next 队列：从 TISJO 轮转取 env_len 个
        const char *seq = "TISJOLZ";
        std::memset(nextbuf, 0, sizeof(nextbuf));
        for (int k = 0; k < c.env_len; ++k) nextbuf[k] = seq[k % 7];
        m_tetris2::AIEnv env = make_env(nextbuf, (size_t)c.env_len, c.node, c.hold);

        auto nt = make_node<DetT, TSpin>(c.col, c.land_height, tspin);
        auto na = make_node<DetT, ASpin>(c.col, c.land_height, aspin);
        auto nm = make_node<DetM, std::monostate>(c.col, c.land_height, std::monostate{});

        // ---- TOJ_v08 ----
        {
            auto er = v08.eval(nt, map, src, (size_t)c.clear);
            TOJ_v08::Status parent{};
            parent.combo = 1; parent.b2b = 1; parent.under_attack = 2; parent.attack = 1;
            auto st = v08.get(er, (size_t)c.depth, parent, env);
            std::printf("V case=%d eval.value=%.17g eval.count=%d eval.t2=%d eval.t3=%d eval.roof=%d "
                        "get.attack=%d get.like=%.17g get.combo=%d get.b2b=%d get.death=%d get.maprise=%d get.value=%.17g\n",
                        i, er.value, er.count, er.t2_value, er.t3_value, er.roof,
                        st.attack, st.like, st.combo, (int)st.b2b, (int)st.death, st.map_rise, st.value);
        }
        // ---- Botris ----
        {
            auto er = bot.eval(na, map, src, (size_t)c.clear);
            Botris::Status parent{};
            parent.combo = 1; parent.b2b = 1; parent.under_attack = 2; parent.attack = 1;
            auto st = bot.get(er, (size_t)c.depth, parent, env);
            std::printf("R case=%d eval.value=%.17g eval.count=%d eval.roof=%d "
                        "get.attack=%d get.like=%.17g get.combo=%d get.b2b=%d get.clear=%d get.death=%d get.value=%.17g\n",
                        i, er.value, er.count, er.roof,
                        st.attack, st.like, st.combo, (int)st.b2b, st.clear, (int)st.death, st.value);
        }
        // ---- TOJ ----
        {
            auto er = toj.eval(nt, map, src, (size_t)c.clear);
            TOJ::Status parent{};
            parent.combo = 1; parent.b2b = 1; parent.under_attack = 2; parent.t2_value = er.t2_value; parent.t3_value = er.t3_value;
            auto st = toj.get(er, (size_t)c.depth, parent, env);
            std::printf("O case=%d eval.value=%.17g eval.count=%d eval.t2=%d eval.t3=%d eval.roof=%d "
                        "get.under=%d get.like=%.17g get.combo=%d get.b2b=%d get.death=%d get.acc=%.17g get.value=%.17g\n",
                        i, er.value, er.count, er.t2_value, er.t3_value, er.roof,
                        st.under_attack, st.like, (int)st.combo, (int)st.b2b, (int)st.death, st.acc_value, st.value);
        }
        // ---- C2 ----
        {
            auto er = c2.eval(nm, map, src, (size_t)c.clear);
            C2::Status parent{};
            parent.combo = 1; parent.map = 0; parent.attack = 1; parent.combo_limit = 1.0;
            auto st = c2.get(er, (size_t)c.depth, parent, env);
            std::printf("C case=%d eval.map=%.17g eval.attack=%.17g eval.fill=%.17g eval.cells=%d "
                        "get.attack=%.17g get.map=%.17g get.combo=%zu get.combolimit=%.17g get.value=%.17g\n",
                        i, er.map, er.attack, er.fill, er.cells,
                        st.attack, st.map, st.combo, st.combo_limit, st.value);
        }
    }

    // ---- 10x28 production rule_c2 独立对拍: 旧 C2::eval(node,map,src,clear)+get(er,depth,parent,env) ----
    {
        using C2Spec = rule_c2::TetrisRule::rule_spec;
        using C2MapT = m_tetris2::TetrisMap<C2Spec::width, C2Spec::height>;
        using C2DetI = m_tetris2::TetrisNodeDetails<C2Spec, 'I', (uint8_t)0>;
        C2::Config cfg28{};
        cfg28.mode = 0; cfg28.safe = 4; cfg28.planning_bpm = 120.0;
        C2 c2_28; c2_28.init<C2Spec>(&cfg28);

        struct C28 { uint32_t rows[28]; int land; int clear; int nlen; };
        C28 cases[4] = {};
        cases[0].rows[0]=0x3ff; cases[0].rows[1]=0x3ff; cases[0].land=2; cases[0].clear=0; cases[0].nlen=2;
        cases[1].rows[0]=0x3ff; cases[1].rows[1]=0x3ff; cases[1].rows[2]=(0x3ff ^ (1u<<5)); cases[1].rows[3]=0x3ff;
        cases[1].land=4; cases[1].clear=1; cases[1].nlen=3;
        for (int y=0;y<16;++y) cases[2].rows[y]=0x3ff; cases[2].rows[16]=0x3ff;
        cases[2].land=17; cases[2].clear=2; cases[2].nlen=1;
        cases[3].rows[24]=0x3ff; cases[3].rows[25]=0x3ff; cases[3].rows[26]=0x3ff; cases[3].rows[27]=0x3ff;
        cases[3].land=24; cases[3].clear=0; cases[3].nlen=0;

        static char nb[16];
        for (int i=0;i<4;++i) {
            C2MapT m{}; C2MapT src{};
            for (int y=0;y<C2Spec::height;++y) {
                typename C2MapT::row_t r = static_cast<typename C2MapT::row_t>(C2MapT::row_full ^ cases[i].rows[y]);
                m.set_row(y, r); src.set_row(y, r);
            }
            auto nm28 = make_node<C2DetI, std::monostate>(3, cases[i].land, std::monostate{});
            auto er = c2_28.eval(nm28, m, src, (size_t)cases[i].clear);
            std::memset(nb,0,sizeof(nb));
            const char *seq="TISJOLZ";
            for (int k=0;k<cases[i].nlen;++k) nb[k]=seq[k%7];
            m_tetris2::AIEnv env28 = make_env(nb, (size_t)cases[i].nlen, 'T', ' ');
            C2::Status parent{}; parent.combo = (i==2)?0:1; parent.map=0; parent.attack=1; parent.combo_limit=0.3;
            auto st = c2_28.get(er, (size_t)0, parent, env28);
            std::printf("C28 case=%d eval.map=%.17g eval.attack=%.17g eval.fill=%.17g eval.cells=%d "
                        "get.attack=%.17g get.map=%.17g get.combo=%zu get.combolimit=%.17g get.value=%.17g\n",
                        i, er.map, er.attack, er.fill, er.cells,
                        st.attack, st.map, st.combo, st.combo_limit, st.value);
        }
    }
    return 0;
}
