
#pragma once

#include "tetris_core.h"
#include <map>

//for SRSX
namespace rule_srsx
{
    namespace detail
    {
        using namespace m_tetris2;

#define T(a, b, c, d) (((a) ? 1 : 0) | ((b) ? 2 : 0) | ((c) ? 4 : 0) | ((d) ? 8 : 0))

        //I 方块的 CW/CCW 踢墙(与 SRS 完全一致)
        using IKickR0CW = WallKickList<-2, 0, +1, 0, -2, -1, +1, +2>;
        using IKickR0CCW = WallKickList<-1, 0, +2, 0, -1, +2, +2, -1>;
        using IKickR1CW = WallKickList<-1, 0, +2, 0, -1, +2, +2, -1>;
        using IKickR1CCW = WallKickList<+2, 0, -1, 0, +2, +1, -1, -2>;
        using IKickR2CW = WallKickList<+2, 0, -1, 0, +2, +1, -1, -2>;
        using IKickR2CCW = WallKickList<+1, 0, -2, 0, +1, -2, -2, +1>;
        using IKickR3CW = WallKickList<+1, 0, -2, 0, +1, -2, -2, +1>;
        using IKickR3CCW = WallKickList<-2, 0, +1, 0, -2, -1, +1, +2>;

        //I 方块 180° 踢墙(SRSX 独有,长度 5)
        using IKickR0Opp = WallKickList<-1, 0, -2, 0, +1, 0, +2, 0, 0, -1>;
        using IKickR1Opp = WallKickList<0, -1, 0, -2, 0, +1, 0, +2, -1, 0>;
        using IKickR2Opp = WallKickList<+1, 0, +2, 0, -1, 0, -2, 0, 0, +1>;
        using IKickR3Opp = WallKickList<0, +1, 0, +2, 0, -1, 0, -2, +1, 0>;

        //J/L/S/T/Z 方块共享的 CW/CCW 踢墙(与 SRS 完全一致)
        using JlstzKickR0CW = WallKickList<-1, 0, -1, +1, 0, -2, -1, -2>;
        using JlstzKickR0CCW = WallKickList<+1, 0, +1, +1, 0, -2, +1, -2>;
        using JlstzKickR1CW = WallKickList<+1, 0, +1, -1, 0, +2, +1, +2>;
        using JlstzKickR1CCW = WallKickList<+1, 0, +1, -1, 0, +2, +1, +2>;
        using JlstzKickR2CW = WallKickList<+1, 0, +1, +1, 0, -2, +1, -2>;
        using JlstzKickR2CCW = WallKickList<-1, 0, -1, +1, 0, -2, -1, -2>;
        using JlstzKickR3CW = WallKickList<-1, 0, -1, -1, 0, +2, -1, +2>;
        using JlstzKickR3CCW = WallKickList<-1, 0, -1, -1, 0, +2, -1, +2>;

        //J/L/S/T/Z 方块共享的 180° 踢墙(SRSX 独有,长度 11)
        using JlstzKickR0Opp = WallKickList<+1, 0, +2, 0, +1, -1, +2, -1, -1, 0, -2, 0, -1, -1, -2, -1, 0, +1, +3, 0, -3, 0>;
        using JlstzKickR1Opp = WallKickList<0, -1, 0, -2, -1, -1, -1, -2, 0, +1, 0, +2, -1, +1, -1, +2, +1, 0, 0, -3, 0, +3>;
        using JlstzKickR2Opp = WallKickList<-1, 0, -2, 0, -1, +1, -2, +1, +1, 0, +2, 0, +1, +1, +2, +1, 0, -1, -3, 0, +3, 0>;
        using JlstzKickR3Opp = WallKickList<0, -1, 0, -2, +1, -1, +1, -2, 0, +1, 0, +2, +1, +1, +1, +2, -1, 0, 0, -3, 0, +3>;

        //O:不旋转
        using O1_lines = OpLines<T(0, 1, 1, 0), T(0, 1, 1, 0), T(0, 0, 0, 0), T(0, 0, 0, 0)>;
        using op_O1 = OpDesc<'O', 0, O1_lines, 0, 0>;

        //I:四个旋转(含 180°)
        using I1_lines = OpLines<T(0, 0, 0, 0), T(1, 1, 1, 1), T(0, 0, 0, 0), T(0, 0, 0, 0)>;
        using I2_lines = OpLines<T(0, 0, 1, 0), T(0, 0, 1, 0), T(0, 0, 1, 0), T(0, 0, 1, 0)>;
        using I3_lines = OpLines<T(0, 0, 0, 0), T(0, 0, 0, 0), T(1, 1, 1, 1), T(0, 0, 0, 0)>;
        using I4_lines = OpLines<T(0, 1, 0, 0), T(0, 1, 0, 0), T(0, 1, 0, 0), T(0, 1, 0, 0)>;
        using op_I1 = OpDesc<'I', 0, I1_lines, 0, 0, 1, 3, 2, IKickR0CW, IKickR0CCW, IKickR0Opp>;
        using op_I2 = OpDesc<'I', 1, I2_lines, 0, 0, 2, 0, 3, IKickR1CW, IKickR1CCW, IKickR1Opp>;
        using op_I3 = OpDesc<'I', 2, I3_lines, 0, 0, 3, 1, 0, IKickR2CW, IKickR2CCW, IKickR2Opp>;
        using op_I4 = OpDesc<'I', 3, I4_lines, 0, 0, 0, 2, 1, IKickR3CW, IKickR3CCW, IKickR3Opp>;

        //S
        using S1_lines = OpLines<T(0, 1, 1, 0), T(1, 1, 0, 0), T(0, 0, 0, 0), T(0, 0, 0, 0)>;
        using S2_lines = OpLines<T(0, 1, 0, 0), T(0, 1, 1, 0), T(0, 0, 1, 0), T(0, 0, 0, 0)>;
        using S3_lines = OpLines<T(0, 0, 0, 0), T(0, 1, 1, 0), T(1, 1, 0, 0), T(0, 0, 0, 0)>;
        using S4_lines = OpLines<T(1, 0, 0, 0), T(1, 1, 0, 0), T(0, 1, 0, 0), T(0, 0, 0, 0)>;
        using op_S1 = OpDesc<'S', 0, S1_lines, 0, 0, 1, 3, 2, JlstzKickR0CW, JlstzKickR0CCW, JlstzKickR0Opp>;
        using op_S2 = OpDesc<'S', 1, S2_lines, 0, 0, 2, 0, 3, JlstzKickR1CW, JlstzKickR1CCW, JlstzKickR1Opp>;
        using op_S3 = OpDesc<'S', 2, S3_lines, 0, 0, 3, 1, 0, JlstzKickR2CW, JlstzKickR2CCW, JlstzKickR2Opp>;
        using op_S4 = OpDesc<'S', 3, S4_lines, 0, 0, 0, 2, 1, JlstzKickR3CW, JlstzKickR3CCW, JlstzKickR3Opp>;

        //Z
        using Z1_lines = OpLines<T(1, 1, 0, 0), T(0, 1, 1, 0), T(0, 0, 0, 0), T(0, 0, 0, 0)>;
        using Z2_lines = OpLines<T(0, 0, 1, 0), T(0, 1, 1, 0), T(0, 1, 0, 0), T(0, 0, 0, 0)>;
        using Z3_lines = OpLines<T(0, 0, 0, 0), T(1, 1, 0, 0), T(0, 1, 1, 0), T(0, 0, 0, 0)>;
        using Z4_lines = OpLines<T(0, 1, 0, 0), T(1, 1, 0, 0), T(1, 0, 0, 0), T(0, 0, 0, 0)>;
        using op_Z1 = OpDesc<'Z', 0, Z1_lines, 0, 0, 1, 3, 2, JlstzKickR0CW, JlstzKickR0CCW, JlstzKickR0Opp>;
        using op_Z2 = OpDesc<'Z', 1, Z2_lines, 0, 0, 2, 0, 3, JlstzKickR1CW, JlstzKickR1CCW, JlstzKickR1Opp>;
        using op_Z3 = OpDesc<'Z', 2, Z3_lines, 0, 0, 3, 1, 0, JlstzKickR2CW, JlstzKickR2CCW, JlstzKickR2Opp>;
        using op_Z4 = OpDesc<'Z', 3, Z4_lines, 0, 0, 0, 2, 1, JlstzKickR3CW, JlstzKickR3CCW, JlstzKickR3Opp>;

        //L
        using L1_lines = OpLines<T(0, 0, 1, 0), T(1, 1, 1, 0), T(0, 0, 0, 0), T(0, 0, 0, 0)>;
        using L2_lines = OpLines<T(0, 1, 0, 0), T(0, 1, 0, 0), T(0, 1, 1, 0), T(0, 0, 0, 0)>;
        using L3_lines = OpLines<T(0, 0, 0, 0), T(1, 1, 1, 0), T(1, 0, 0, 0), T(0, 0, 0, 0)>;
        using L4_lines = OpLines<T(1, 1, 0, 0), T(0, 1, 0, 0), T(0, 1, 0, 0), T(0, 0, 0, 0)>;
        using op_L1 = OpDesc<'L', 0, L1_lines, 0, 0, 1, 3, 2, JlstzKickR0CW, JlstzKickR0CCW, JlstzKickR0Opp>;
        using op_L2 = OpDesc<'L', 1, L2_lines, 0, 0, 2, 0, 3, JlstzKickR1CW, JlstzKickR1CCW, JlstzKickR1Opp>;
        using op_L3 = OpDesc<'L', 2, L3_lines, 0, 0, 3, 1, 0, JlstzKickR2CW, JlstzKickR2CCW, JlstzKickR2Opp>;
        using op_L4 = OpDesc<'L', 3, L4_lines, 0, 0, 0, 2, 1, JlstzKickR3CW, JlstzKickR3CCW, JlstzKickR3Opp>;

        //J
        using J1_lines = OpLines<T(1, 0, 0, 0), T(1, 1, 1, 0), T(0, 0, 0, 0), T(0, 0, 0, 0)>;
        using J2_lines = OpLines<T(0, 1, 1, 0), T(0, 1, 0, 0), T(0, 1, 0, 0), T(0, 0, 0, 0)>;
        using J3_lines = OpLines<T(0, 0, 0, 0), T(1, 1, 1, 0), T(0, 0, 1, 0), T(0, 0, 0, 0)>;
        using J4_lines = OpLines<T(0, 1, 0, 0), T(0, 1, 0, 0), T(1, 1, 0, 0), T(0, 0, 0, 0)>;
        using op_J1 = OpDesc<'J', 0, J1_lines, 0, 0, 1, 3, 2, JlstzKickR0CW, JlstzKickR0CCW, JlstzKickR0Opp>;
        using op_J2 = OpDesc<'J', 1, J2_lines, 0, 0, 2, 0, 3, JlstzKickR1CW, JlstzKickR1CCW, JlstzKickR1Opp>;
        using op_J3 = OpDesc<'J', 2, J3_lines, 0, 0, 3, 1, 0, JlstzKickR2CW, JlstzKickR2CCW, JlstzKickR2Opp>;
        using op_J4 = OpDesc<'J', 3, J4_lines, 0, 0, 0, 2, 1, JlstzKickR3CW, JlstzKickR3CCW, JlstzKickR3Opp>;

        //T
        using T1_lines = OpLines<T(0, 1, 0, 0), T(1, 1, 1, 0), T(0, 0, 0, 0), T(0, 0, 0, 0)>;
        using T2_lines = OpLines<T(0, 1, 0, 0), T(0, 1, 1, 0), T(0, 1, 0, 0), T(0, 0, 0, 0)>;
        using T3_lines = OpLines<T(0, 0, 0, 0), T(1, 1, 1, 0), T(0, 1, 0, 0), T(0, 0, 0, 0)>;
        using T4_lines = OpLines<T(0, 1, 0, 0), T(1, 1, 0, 0), T(0, 1, 0, 0), T(0, 0, 0, 0)>;
        using op_T1 = OpDesc<'T', 0, T1_lines, 0, 0, 1, 3, 2, JlstzKickR0CW, JlstzKickR0CCW, JlstzKickR0Opp>;
        using op_T2 = OpDesc<'T', 1, T2_lines, 0, 0, 2, 0, 3, JlstzKickR1CW, JlstzKickR1CCW, JlstzKickR1Opp>;
        using op_T3 = OpDesc<'T', 2, T3_lines, 0, 0, 3, 1, 0, JlstzKickR2CW, JlstzKickR2CCW, JlstzKickR2Opp>;
        using op_T4 = OpDesc<'T', 3, T4_lines, 0, 0, 0, 2, 1, JlstzKickR3CW, JlstzKickR3CCW, JlstzKickR3Opp>;

#undef T

        using SrsxRule = RuleSpec<10, 40, 4,
                                  op_O1,
                                  op_I1, op_I2, op_I3, op_I4,
                                  op_S1, op_S2, op_S3, op_S4,
                                  op_Z1, op_Z2, op_Z3, op_Z4,
                                  op_L1, op_L2, op_L3, op_L4,
                                  op_J1, op_J2, op_J3, op_J4,
                                  op_T1, op_T2, op_T3, op_T4>;
    }

    class TetrisRule
    {
    public:
        using rule_spec = detail::SrsxRule;
    };
}
