// old_main.cpp — 旧基线（snapshot HEAD@7af0537a 真实 ai_zzz 代码）对拍 driver。
// 不重写任何旧公式；直接调用旧 eval/get，逐 case 打印字段值作为黄金基线。
#include "ai_zzz.h"  // from frozen snapshot src (via -I)
#include "rule_srs.h"
#include "tetris_node.h"

#include <cstdio>
#include <cstdint>

#include "_a_cases.h"

using Spec = rule_srs::TetrisRule::rule_spec;
using MapT = m_tetris2::TetrisMap<Spec::width, Spec::height>;

static MapT board_from(const aicases::Board &b)
{
    MapT m{};
    for (int y = 0; y < Spec::height; ++y)
        m.set_row(y, static_cast<MapT::row_t>(MapT::row_full ^ b.rows[y]));
    return m;
}

template<class Details, class SpinT>
static m_tetris2::TetrisNode<Details, SpinT> make_node(int col, int land_height, SpinT spin)
{
    using NB = m_tetris2::TetrisNodeBase<Details>;
    m_tetris2::TetrisNode<Details, SpinT> n;
    n.x = static_cast<int8_t>(col);
    // land_height() = (y - kOriginY) + kHeight
    n.y = static_cast<int8_t>(land_height - NB::kHeight + NB::kOriginY);
    if constexpr (!std::is_same_v<SpinT, std::monostate>)
        n.spin = spin;
    return n;
}

int main()
{
    using namespace ai_zzz;
    using TSpin = search_tspin::Search::TSpinType;
    using ASpin = search_aspin::Search::ASpinType;

    // ---- init 4 AIs ----
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

    using DetMS = m_tetris2::TetrisNodeDetails<Spec, 'T', (uint8_t)0>;
    using DetT = m_tetris2::TetrisNodeDetails<Spec, 'T', (uint8_t)0>;

    for (int i = 0; i < aicases::kNumCases; ++i)
    {
        auto c = aicases::decode(i);
        attack_cfg.mode = c.attack_mode;

        MapT map = board_from(aicases::make_board(c.board_kind));
        MapT src = map;

        TSpin tspin = (c.spin == 0) ? TSpin::None : (c.spin == 1 ? TSpin::TSpin : TSpin::TSpinMini);
        ASpin aspin = (c.spin == 0) ? ASpin::None : ASpin::ASpin;

        auto node_ms = make_node<DetMS, std::monostate>(c.col, c.land_height, std::monostate{});
        auto node_toj = make_node<DetT, TSpin>(c.col, c.land_height, tspin);
        auto node_bot = make_node<DetT, ASpin>(c.col, c.land_height, aspin);

        // ---- Attack ----
        {
            auto er = attack.eval(node_ms, map, src, (size_t)c.clear);
            qq::Attack::Status parent{c.p_land_point, c.p_attack, c.p_rubbish, c.p_value};
            auto st = attack.get(er, c.depth, parent);
            std::printf("A case=%d eval.land_point=%.17g eval.map=%.17g eval.danger=%d get.land_point=%.17g get.attack=%.17g get.rubbish=%.17g get.value=%.17g\n",
                        i, er.land_point, er.map, (int)er.danger, st.land_point, st.attack, st.rubbish, st.value);
        }
        // ---- Dig ----
        {
            double er = dig.eval(node_ms, map, src, c.clear);
            double st = dig.get(er, c.depth);
            std::printf("D case=%d eval.value=%.17g get.value=%.17g\n", i, er, st);
        }
        // ---- TOJ_PC ----
        {
            auto er = toj.eval(node_toj, map, src, (size_t)c.clear);
            TOJ_PC::Status parent{c.p_under_attack, c.p_recv_attack, c.p_attack_int, c.p_like, c.p_combo, c.p_b2b, c.p_pc, c.p_value};
            auto st = toj.get(er, c.depth, parent);
            std::printf("T case=%d eval.value=%.17g eval.roof=%d eval.tspin=%d get.attack=%d get.like=%d get.combo=%d get.b2b=%d get.pc=%d get.recv=%d get.under=%d get.value=%.17g\n",
                        i, er.value, er.roof, (int)er.t_spin, st.attack, st.like, st.combo, (int)st.b2b, (int)st.pc, st.recv_attack, st.under_attack, st.value);
        }
        // ---- Botris_PC ----
        {
            auto er = bot.eval(node_bot, map, src, (size_t)c.clear);
            Botris_PC::Status parent{c.p_under_attack, c.p_recv_attack, c.p_attack_int, c.p_like, c.p_combo, c.p_b2b, c.p_pc, c.p_value};
            auto st = bot.get(er, c.depth, parent);
            std::printf("B case=%d eval.value=%.17g eval.roof=%d eval.aspin=%d get.attack=%d get.like=%d get.combo=%d get.b2b=%d get.pc=%d get.recv=%d get.under=%d get.value=%.17g\n",
                        i, er.value, er.roof, (int)er.a_spin, st.attack, st.like, st.combo, (int)st.b2b, (int)st.pc, st.recv_attack, st.under_attack, st.value);
        }
    }
    return 0;
}
