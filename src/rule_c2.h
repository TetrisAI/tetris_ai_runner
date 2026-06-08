
#pragma once

#include "tetris_core.h"
#include <map>

//for Cultris II
namespace rule_c2
{
    namespace detail
    {
        using namespace m_tetris2;

#define T(a, b, c, d) (((a) ? 1 : 0) | ((b) ? 2 : 0) | ((c) ? 4 : 0) | ((d) ? 8 : 0))

        //C2 全部方向共用同一张 7 项踢墙表
        using C2Kick = WallKickList<-1, 0, +1, 0, 0, -1, -1, -1, +1, -1, -2, 0, +2, 0>;

        //O:不旋转
        using O1_lines = OpLines<T(0, 0, 0, 0), T(0, 1, 1, 0), T(0, 1, 1, 0), T(0, 0, 0, 0)>;
        using op_O1 = OpDesc<'O', 0, O1_lines, 0, 0>;

        //I
        using I1_lines = OpLines<T(0, 0, 0, 0), T(1, 1, 1, 1), T(0, 0, 0, 0), T(0, 0, 0, 0)>;
        using I2_lines = OpLines<T(0, 0, 1, 0), T(0, 0, 1, 0), T(0, 0, 1, 0), T(0, 0, 1, 0)>;
        using I3_lines = OpLines<T(0, 0, 0, 0), T(0, 0, 0, 0), T(1, 1, 1, 1), T(0, 0, 0, 0)>;
        using I4_lines = OpLines<T(0, 1, 0, 0), T(0, 1, 0, 0), T(0, 1, 0, 0), T(0, 1, 0, 0)>;
        using op_I1 = OpDesc<'I', 0, I1_lines, 0, 0, 1, 3, 2, C2Kick, C2Kick, C2Kick>;
        using op_I2 = OpDesc<'I', 1, I2_lines, 0, 0, 2, 0, 3, C2Kick, C2Kick, C2Kick>;
        using op_I3 = OpDesc<'I', 2, I3_lines, 0, 0, 3, 1, 0, C2Kick, C2Kick, C2Kick>;
        using op_I4 = OpDesc<'I', 3, I4_lines, 0, 0, 0, 2, 1, C2Kick, C2Kick, C2Kick>;

        //S(C2 形态:行数据有顶部 0 行偏移)
        using S1_lines = OpLines<T(0, 0, 0, 0), T(0, 1, 1, 0), T(1, 1, 0, 0), T(0, 0, 0, 0)>;
        using S2_lines = OpLines<T(0, 0, 0, 0), T(0, 1, 0, 0), T(0, 1, 1, 0), T(0, 0, 1, 0)>;
        using S3_lines = OpLines<T(0, 0, 0, 0), T(0, 0, 0, 0), T(0, 1, 1, 0), T(1, 1, 0, 0)>;
        using S4_lines = OpLines<T(0, 0, 0, 0), T(1, 0, 0, 0), T(1, 1, 0, 0), T(0, 1, 0, 0)>;
        using op_S1 = OpDesc<'S', 0, S1_lines, 0, 0, 1, 3, 2, C2Kick, C2Kick, C2Kick>;
        using op_S2 = OpDesc<'S', 1, S2_lines, 0, 0, 2, 0, 3, C2Kick, C2Kick, C2Kick>;
        using op_S3 = OpDesc<'S', 2, S3_lines, 0, 0, 3, 1, 0, C2Kick, C2Kick, C2Kick>;
        using op_S4 = OpDesc<'S', 3, S4_lines, 0, 0, 0, 2, 1, C2Kick, C2Kick, C2Kick>;

        //Z
        using Z1_lines = OpLines<T(0, 0, 0, 0), T(1, 1, 0, 0), T(0, 1, 1, 0), T(0, 0, 0, 0)>;
        using Z2_lines = OpLines<T(0, 0, 0, 0), T(0, 0, 1, 0), T(0, 1, 1, 0), T(0, 1, 0, 0)>;
        using Z3_lines = OpLines<T(0, 0, 0, 0), T(0, 0, 0, 0), T(1, 1, 0, 0), T(0, 1, 1, 0)>;
        using Z4_lines = OpLines<T(0, 0, 0, 0), T(0, 1, 0, 0), T(1, 1, 0, 0), T(1, 0, 0, 0)>;
        using op_Z1 = OpDesc<'Z', 0, Z1_lines, 0, 0, 1, 3, 2, C2Kick, C2Kick, C2Kick>;
        using op_Z2 = OpDesc<'Z', 1, Z2_lines, 0, 0, 2, 0, 3, C2Kick, C2Kick, C2Kick>;
        using op_Z3 = OpDesc<'Z', 2, Z3_lines, 0, 0, 3, 1, 0, C2Kick, C2Kick, C2Kick>;
        using op_Z4 = OpDesc<'Z', 3, Z4_lines, 0, 0, 0, 2, 1, C2Kick, C2Kick, C2Kick>;

        //L
        using L1_lines = OpLines<T(0, 0, 0, 0), T(0, 0, 1, 0), T(1, 1, 1, 0), T(0, 0, 0, 0)>;
        using L2_lines = OpLines<T(0, 0, 0, 0), T(0, 1, 0, 0), T(0, 1, 0, 0), T(0, 1, 1, 0)>;
        using L3_lines = OpLines<T(0, 0, 0, 0), T(0, 0, 0, 0), T(1, 1, 1, 0), T(1, 0, 0, 0)>;
        using L4_lines = OpLines<T(0, 0, 0, 0), T(1, 1, 0, 0), T(0, 1, 0, 0), T(0, 1, 0, 0)>;
        using op_L1 = OpDesc<'L', 0, L1_lines, 0, 0, 1, 3, 2, C2Kick, C2Kick, C2Kick>;
        using op_L2 = OpDesc<'L', 1, L2_lines, 0, 0, 2, 0, 3, C2Kick, C2Kick, C2Kick>;
        using op_L3 = OpDesc<'L', 2, L3_lines, 0, 0, 3, 1, 0, C2Kick, C2Kick, C2Kick>;
        using op_L4 = OpDesc<'L', 3, L4_lines, 0, 0, 0, 2, 1, C2Kick, C2Kick, C2Kick>;

        //J
        using J1_lines = OpLines<T(0, 0, 0, 0), T(1, 0, 0, 0), T(1, 1, 1, 0), T(0, 0, 0, 0)>;
        using J2_lines = OpLines<T(0, 0, 0, 0), T(0, 1, 1, 0), T(0, 1, 0, 0), T(0, 1, 0, 0)>;
        using J3_lines = OpLines<T(0, 0, 0, 0), T(0, 0, 0, 0), T(1, 1, 1, 0), T(0, 0, 1, 0)>;
        using J4_lines = OpLines<T(0, 0, 0, 0), T(0, 1, 0, 0), T(0, 1, 0, 0), T(1, 1, 0, 0)>;
        using op_J1 = OpDesc<'J', 0, J1_lines, 0, 0, 1, 3, 2, C2Kick, C2Kick, C2Kick>;
        using op_J2 = OpDesc<'J', 1, J2_lines, 0, 0, 2, 0, 3, C2Kick, C2Kick, C2Kick>;
        using op_J3 = OpDesc<'J', 2, J3_lines, 0, 0, 3, 1, 0, C2Kick, C2Kick, C2Kick>;
        using op_J4 = OpDesc<'J', 3, J4_lines, 0, 0, 0, 2, 1, C2Kick, C2Kick, C2Kick>;

        //T
        using T1_lines = OpLines<T(0, 0, 0, 0), T(0, 1, 0, 0), T(1, 1, 1, 0), T(0, 0, 0, 0)>;
        using T2_lines = OpLines<T(0, 0, 0, 0), T(0, 1, 0, 0), T(0, 1, 1, 0), T(0, 1, 0, 0)>;
        using T3_lines = OpLines<T(0, 0, 0, 0), T(0, 0, 0, 0), T(1, 1, 1, 0), T(0, 1, 0, 0)>;
        using T4_lines = OpLines<T(0, 0, 0, 0), T(0, 1, 0, 0), T(1, 1, 0, 0), T(0, 1, 0, 0)>;
        using op_T1 = OpDesc<'T', 0, T1_lines, 0, 0, 1, 3, 2, C2Kick, C2Kick, C2Kick>;
        using op_T2 = OpDesc<'T', 1, T2_lines, 0, 0, 2, 0, 3, C2Kick, C2Kick, C2Kick>;
        using op_T3 = OpDesc<'T', 2, T3_lines, 0, 0, 3, 1, 0, C2Kick, C2Kick, C2Kick>;
        using op_T4 = OpDesc<'T', 3, T4_lines, 0, 0, 0, 2, 1, C2Kick, C2Kick, C2Kick>;

#undef T

        using C2Rule = RuleSpec<10, 21, 4,
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
        using rule_spec = detail::C2Rule;
    };
}
