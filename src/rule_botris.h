#ifndef TETRIS_AI_RUNNER_RULE_BOTRIS_H_
#define TETRIS_AI_RUNNER_RULE_BOTRIS_H_

#include "tetris_core.h"
#include <map>

//for Botris (SRS 变体,O 方块也会旋转并自带特殊踢墙表)
namespace rule_botris
{
    namespace detail
    {
        using namespace m_tetris2;

#define T(a, b, c, d) (((a) ? 1 : 0) | ((b) ? 2 : 0) | ((c) ? 4 : 0) | ((d) ? 8 : 0))

        //O 方块四个旋转独立踢墙
        using OKickR0CW = WallKickList<+1, 0, 0, -1, -1, 0, -1, +1>;
        using OKickR0CCW = WallKickList<-1, 0, 0, -1, +1, 0, +1, +1>;
        using OKickR1CW = WallKickList<-1, 0, 0, -1, +1, 0, +1, +1>;
        using OKickR1CCW = WallKickList<-1, 0, 0, +1, +1, 0, +1, -1>;
        using OKickR2CW = WallKickList<-1, 0, 0, +1, +1, 0, +1, -1>;
        using OKickR2CCW = WallKickList<+1, 0, 0, +1, -1, 0, -1, -1>;
        using OKickR3CW = WallKickList<+1, 0, 0, +1, -1, 0, -1, -1>;
        using OKickR3CCW = WallKickList<+1, 0, 0, -1, -1, 0, -1, +1>;

        //I 方块踢墙
        using IKickR0CW = WallKickList<-2, 0, +1, 0, -2, -1, +1, +2>;
        using IKickR0CCW = WallKickList<-1, 0, +2, 0, -1, +2, +2, -1>;
        using IKickR1CW = WallKickList<-1, 0, +2, 0, -1, +2, +2, -1>;
        using IKickR1CCW = WallKickList<+2, 0, -1, 0, +2, +1, -1, -2>;
        using IKickR2CW = WallKickList<+2, 0, -1, 0, +2, +1, -1, -2>;
        using IKickR2CCW = WallKickList<+1, 0, -2, 0, +1, -2, -2, +1>;
        using IKickR3CW = WallKickList<+1, 0, -2, 0, +1, -2, -2, +1>;
        using IKickR3CCW = WallKickList<-2, 0, +1, 0, -2, -1, +1, +2>;

        //J/L/S/T/Z 共享踢墙
        using JlstzKickR0CW = WallKickList<-1, 0, -1, +1, 0, -2, -1, -2>;
        using JlstzKickR0CCW = WallKickList<+1, 0, +1, +1, 0, -2, +1, -2>;
        using JlstzKickR1CW = WallKickList<+1, 0, +1, -1, 0, +2, +1, +2>;
        using JlstzKickR1CCW = WallKickList<+1, 0, +1, -1, 0, +2, +1, +2>;
        using JlstzKickR2CW = WallKickList<+1, 0, +1, +1, 0, -2, +1, -2>;
        using JlstzKickR2CCW = WallKickList<-1, 0, -1, +1, 0, -2, -1, -2>;
        using JlstzKickR3CW = WallKickList<-1, 0, -1, -1, 0, +2, -1, +2>;
        using JlstzKickR3CCW = WallKickList<-1, 0, -1, -1, 0, +2, -1, +2>;

        //O:四个旋转(spawn X=1)
        using O1_lines = OpLines<T(0, 1, 1, 0), T(0, 1, 1, 0), T(0, 0, 0, 0), T(0, 0, 0, 0)>;
        using O2_lines = OpLines<T(0, 0, 0, 0), T(0, 0, 1, 1), T(0, 0, 1, 1), T(0, 0, 0, 0)>;
        using O3_lines = OpLines<T(0, 0, 0, 0), T(0, 0, 0, 0), T(0, 1, 1, 0), T(0, 1, 1, 0)>;
        using O4_lines = OpLines<T(0, 0, 0, 0), T(1, 1, 0, 0), T(1, 1, 0, 0), T(0, 0, 0, 0)>;
        using op_O1 = OpDesc<'O', 0, O1_lines, 1, 0, 1, 3, kOpRotateNone, OKickR0CW, OKickR0CCW>;
        using op_O2 = OpDesc<'O', 1, O2_lines, 1, 0, 2, 0, kOpRotateNone, OKickR1CW, OKickR1CCW>;
        using op_O3 = OpDesc<'O', 2, O3_lines, 1, 0, 3, 1, kOpRotateNone, OKickR2CW, OKickR2CCW>;
        using op_O4 = OpDesc<'O', 3, O4_lines, 1, 0, 0, 2, kOpRotateNone, OKickR3CW, OKickR3CCW>;

        //I
        using I1_lines = OpLines<T(0, 0, 0, 0), T(1, 1, 1, 1), T(0, 0, 0, 0), T(0, 0, 0, 0)>;
        using I2_lines = OpLines<T(0, 0, 1, 0), T(0, 0, 1, 0), T(0, 0, 1, 0), T(0, 0, 1, 0)>;
        using I3_lines = OpLines<T(0, 0, 0, 0), T(0, 0, 0, 0), T(1, 1, 1, 1), T(0, 0, 0, 0)>;
        using I4_lines = OpLines<T(0, 1, 0, 0), T(0, 1, 0, 0), T(0, 1, 0, 0), T(0, 1, 0, 0)>;
        using op_I1 = OpDesc<'I', 0, I1_lines, 0, 0, 1, 3, kOpRotateNone, IKickR0CW, IKickR0CCW>;
        using op_I2 = OpDesc<'I', 1, I2_lines, 0, 0, 2, 0, kOpRotateNone, IKickR1CW, IKickR1CCW>;
        using op_I3 = OpDesc<'I', 2, I3_lines, 0, 0, 3, 1, kOpRotateNone, IKickR2CW, IKickR2CCW>;
        using op_I4 = OpDesc<'I', 3, I4_lines, 0, 0, 0, 2, kOpRotateNone, IKickR3CW, IKickR3CCW>;

        //S
        using S1_lines = OpLines<T(0, 1, 1, 0), T(1, 1, 0, 0), T(0, 0, 0, 0), T(0, 0, 0, 0)>;
        using S2_lines = OpLines<T(0, 1, 0, 0), T(0, 1, 1, 0), T(0, 0, 1, 0), T(0, 0, 0, 0)>;
        using S3_lines = OpLines<T(0, 0, 0, 0), T(0, 1, 1, 0), T(1, 1, 0, 0), T(0, 0, 0, 0)>;
        using S4_lines = OpLines<T(1, 0, 0, 0), T(1, 1, 0, 0), T(0, 1, 0, 0), T(0, 0, 0, 0)>;
        using op_S1 = OpDesc<'S', 0, S1_lines, 0, 0, 1, 3, kOpRotateNone, JlstzKickR0CW, JlstzKickR0CCW>;
        using op_S2 = OpDesc<'S', 1, S2_lines, 0, 0, 2, 0, kOpRotateNone, JlstzKickR1CW, JlstzKickR1CCW>;
        using op_S3 = OpDesc<'S', 2, S3_lines, 0, 0, 3, 1, kOpRotateNone, JlstzKickR2CW, JlstzKickR2CCW>;
        using op_S4 = OpDesc<'S', 3, S4_lines, 0, 0, 0, 2, kOpRotateNone, JlstzKickR3CW, JlstzKickR3CCW>;

        //Z
        using Z1_lines = OpLines<T(1, 1, 0, 0), T(0, 1, 1, 0), T(0, 0, 0, 0), T(0, 0, 0, 0)>;
        using Z2_lines = OpLines<T(0, 0, 1, 0), T(0, 1, 1, 0), T(0, 1, 0, 0), T(0, 0, 0, 0)>;
        using Z3_lines = OpLines<T(0, 0, 0, 0), T(1, 1, 0, 0), T(0, 1, 1, 0), T(0, 0, 0, 0)>;
        using Z4_lines = OpLines<T(0, 1, 0, 0), T(1, 1, 0, 0), T(1, 0, 0, 0), T(0, 0, 0, 0)>;
        using op_Z1 = OpDesc<'Z', 0, Z1_lines, 0, 0, 1, 3, kOpRotateNone, JlstzKickR0CW, JlstzKickR0CCW>;
        using op_Z2 = OpDesc<'Z', 1, Z2_lines, 0, 0, 2, 0, kOpRotateNone, JlstzKickR1CW, JlstzKickR1CCW>;
        using op_Z3 = OpDesc<'Z', 2, Z3_lines, 0, 0, 3, 1, kOpRotateNone, JlstzKickR2CW, JlstzKickR2CCW>;
        using op_Z4 = OpDesc<'Z', 3, Z4_lines, 0, 0, 0, 2, kOpRotateNone, JlstzKickR3CW, JlstzKickR3CCW>;

        //L
        using L1_lines = OpLines<T(0, 0, 1, 0), T(1, 1, 1, 0), T(0, 0, 0, 0), T(0, 0, 0, 0)>;
        using L2_lines = OpLines<T(0, 1, 0, 0), T(0, 1, 0, 0), T(0, 1, 1, 0), T(0, 0, 0, 0)>;
        using L3_lines = OpLines<T(0, 0, 0, 0), T(1, 1, 1, 0), T(1, 0, 0, 0), T(0, 0, 0, 0)>;
        using L4_lines = OpLines<T(1, 1, 0, 0), T(0, 1, 0, 0), T(0, 1, 0, 0), T(0, 0, 0, 0)>;
        using op_L1 = OpDesc<'L', 0, L1_lines, 0, 0, 1, 3, kOpRotateNone, JlstzKickR0CW, JlstzKickR0CCW>;
        using op_L2 = OpDesc<'L', 1, L2_lines, 0, 0, 2, 0, kOpRotateNone, JlstzKickR1CW, JlstzKickR1CCW>;
        using op_L3 = OpDesc<'L', 2, L3_lines, 0, 0, 3, 1, kOpRotateNone, JlstzKickR2CW, JlstzKickR2CCW>;
        using op_L4 = OpDesc<'L', 3, L4_lines, 0, 0, 0, 2, kOpRotateNone, JlstzKickR3CW, JlstzKickR3CCW>;

        //J
        using J1_lines = OpLines<T(1, 0, 0, 0), T(1, 1, 1, 0), T(0, 0, 0, 0), T(0, 0, 0, 0)>;
        using J2_lines = OpLines<T(0, 1, 1, 0), T(0, 1, 0, 0), T(0, 1, 0, 0), T(0, 0, 0, 0)>;
        using J3_lines = OpLines<T(0, 0, 0, 0), T(1, 1, 1, 0), T(0, 0, 1, 0), T(0, 0, 0, 0)>;
        using J4_lines = OpLines<T(0, 1, 0, 0), T(0, 1, 0, 0), T(1, 1, 0, 0), T(0, 0, 0, 0)>;
        using op_J1 = OpDesc<'J', 0, J1_lines, 0, 0, 1, 3, kOpRotateNone, JlstzKickR0CW, JlstzKickR0CCW>;
        using op_J2 = OpDesc<'J', 1, J2_lines, 0, 0, 2, 0, kOpRotateNone, JlstzKickR1CW, JlstzKickR1CCW>;
        using op_J3 = OpDesc<'J', 2, J3_lines, 0, 0, 3, 1, kOpRotateNone, JlstzKickR2CW, JlstzKickR2CCW>;
        using op_J4 = OpDesc<'J', 3, J4_lines, 0, 0, 0, 2, kOpRotateNone, JlstzKickR3CW, JlstzKickR3CCW>;

        //T
        using T1_lines = OpLines<T(0, 1, 0, 0), T(1, 1, 1, 0), T(0, 0, 0, 0), T(0, 0, 0, 0)>;
        using T2_lines = OpLines<T(0, 1, 0, 0), T(0, 1, 1, 0), T(0, 1, 0, 0), T(0, 0, 0, 0)>;
        using T3_lines = OpLines<T(0, 0, 0, 0), T(1, 1, 1, 0), T(0, 1, 0, 0), T(0, 0, 0, 0)>;
        using T4_lines = OpLines<T(0, 1, 0, 0), T(1, 1, 0, 0), T(0, 1, 0, 0), T(0, 0, 0, 0)>;
        using op_T1 = OpDesc<'T', 0, T1_lines, 0, 0, 1, 3, kOpRotateNone, JlstzKickR0CW, JlstzKickR0CCW>;
        using op_T2 = OpDesc<'T', 1, T2_lines, 0, 0, 2, 0, kOpRotateNone, JlstzKickR1CW, JlstzKickR1CCW>;
        using op_T3 = OpDesc<'T', 2, T3_lines, 0, 0, 3, 1, kOpRotateNone, JlstzKickR2CW, JlstzKickR2CCW>;
        using op_T4 = OpDesc<'T', 3, T4_lines, 0, 0, 0, 2, kOpRotateNone, JlstzKickR3CW, JlstzKickR3CCW>;

#undef T

        using BotrisRuleBase = RuleSpec<10, 40, 4,
                                        op_O1, op_O2, op_O3, op_O4,
                                        op_I1, op_I2, op_I3, op_I4,
                                        op_S1, op_S2, op_S3, op_S4,
                                        op_Z1, op_Z2, op_Z3, op_Z4,
                                        op_L1, op_L2, op_L3, op_L4,
                                        op_J1, op_J2, op_J3, op_J4,
                                        op_T1, op_T2, op_T3, op_T4>;

        struct BotrisRule : BotrisRuleBase
        {
            //rule_botris 的 spawn 公式 (与原 spawn helper 一致):
            //  O 块 (4, 20), 其它 piece (3, 20)
            static constexpr std::pair<int, int> spawn(char PT, int /*W*/, int /*H*/)
            {
                return {PT == 'O' ? 4 : 3, 20};
            }
        };
    }

    class TetrisRule
    {
    public:
        using rule_spec = detail::BotrisRule;
    };
}

#endif // TETRIS_AI_RUNNER_RULE_BOTRIS_H_
