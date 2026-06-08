
#pragma once

#include "tetris_core.h"
#include <map>

namespace rule_st
{
    namespace detail
    {
        using namespace m_tetris2;

#define T(a, b, c, d) (((a) ? 1 : 0) | ((b) ? 2 : 0) | ((c) ? 4 : 0) | ((d) ? 8 : 0))

        //O:不旋转,spawn 偏移 (2,1)
        using O1_lines = OpLines<T(0, 0, 0, 0), T(0, 1, 1, 0), T(0, 1, 1, 0), T(0, 0, 0, 0)>;
        using op_O1 = OpDesc<'O', 0, O1_lines, 2, 1>;

        //I:仅 2 个旋转
        using I1_lines = OpLines<T(0, 0, 0, 0), T(1, 1, 1, 1), T(0, 0, 0, 0), T(0, 0, 0, 0)>;
        using I2_lines = OpLines<T(0, 0, 1, 0), T(0, 0, 1, 0), T(0, 0, 1, 0), T(0, 0, 1, 0)>;
        using op_I1 = OpDesc<'I', 0, I1_lines, 2, 1, 1, 1>;
        using op_I2 = OpDesc<'I', 1, I2_lines, 2, 1, 0, 0>;

        //S:仅 2 个旋转
        using S1_lines = OpLines<T(0, 0, 0, 0), T(0, 0, 1, 1), T(0, 1, 1, 0), T(0, 0, 0, 0)>;
        using S2_lines = OpLines<T(0, 0, 1, 0), T(0, 0, 1, 1), T(0, 0, 0, 1), T(0, 0, 0, 0)>;
        using op_S1 = OpDesc<'S', 0, S1_lines, 2, 1, 1, 1>;
        using op_S2 = OpDesc<'S', 1, S2_lines, 2, 1, 0, 0>;

        //Z:仅 2 个旋转
        using Z1_lines = OpLines<T(0, 0, 0, 0), T(0, 1, 1, 0), T(0, 0, 1, 1), T(0, 0, 0, 0)>;
        using Z2_lines = OpLines<T(0, 0, 0, 1), T(0, 0, 1, 1), T(0, 0, 1, 0), T(0, 0, 0, 0)>;
        using op_Z1 = OpDesc<'Z', 0, Z1_lines, 2, 1, 1, 1>;
        using op_Z2 = OpDesc<'Z', 1, Z2_lines, 2, 1, 0, 0>;

        //L
        using L1_lines = OpLines<T(0, 0, 0, 0), T(0, 1, 1, 1), T(0, 1, 0, 0), T(0, 0, 0, 0)>;
        using L2_lines = OpLines<T(0, 0, 1, 0), T(0, 0, 1, 0), T(0, 0, 1, 1), T(0, 0, 0, 0)>;
        using L3_lines = OpLines<T(0, 0, 0, 1), T(0, 1, 1, 1), T(0, 0, 0, 0), T(0, 0, 0, 0)>;
        using L4_lines = OpLines<T(0, 1, 1, 0), T(0, 0, 1, 0), T(0, 0, 1, 0), T(0, 0, 0, 0)>;
        using op_L1 = OpDesc<'L', 0, L1_lines, 2, 1, 3, 1>;
        using op_L2 = OpDesc<'L', 1, L2_lines, 2, 1, 0, 2>;
        using op_L3 = OpDesc<'L', 2, L3_lines, 2, 1, 1, 3>;
        using op_L4 = OpDesc<'L', 3, L4_lines, 2, 1, 2, 0>;

        //J
        using J1_lines = OpLines<T(0, 0, 0, 0), T(0, 1, 1, 1), T(0, 0, 0, 1), T(0, 0, 0, 0)>;
        using J2_lines = OpLines<T(0, 0, 1, 1), T(0, 0, 1, 0), T(0, 0, 1, 0), T(0, 0, 0, 0)>;
        using J3_lines = OpLines<T(0, 1, 0, 0), T(0, 1, 1, 1), T(0, 0, 0, 0), T(0, 0, 0, 0)>;
        using J4_lines = OpLines<T(0, 0, 1, 0), T(0, 0, 1, 0), T(0, 1, 1, 0), T(0, 0, 0, 0)>;
        using op_J1 = OpDesc<'J', 0, J1_lines, 2, 1, 3, 1>;
        using op_J2 = OpDesc<'J', 1, J2_lines, 2, 1, 0, 2>;
        using op_J3 = OpDesc<'J', 2, J3_lines, 2, 1, 1, 3>;
        using op_J4 = OpDesc<'J', 3, J4_lines, 2, 1, 2, 0>;

        //T
        using T1_lines = OpLines<T(0, 0, 0, 0), T(0, 1, 1, 1), T(0, 0, 1, 0), T(0, 0, 0, 0)>;
        using T2_lines = OpLines<T(0, 0, 1, 0), T(0, 0, 1, 1), T(0, 0, 1, 0), T(0, 0, 0, 0)>;
        using T3_lines = OpLines<T(0, 0, 1, 0), T(0, 1, 1, 1), T(0, 0, 0, 0), T(0, 0, 0, 0)>;
        using T4_lines = OpLines<T(0, 0, 1, 0), T(0, 1, 1, 0), T(0, 0, 1, 0), T(0, 0, 0, 0)>;
        using op_T1 = OpDesc<'T', 0, T1_lines, 2, 1, 3, 1>;
        using op_T2 = OpDesc<'T', 1, T2_lines, 2, 1, 0, 2>;
        using op_T3 = OpDesc<'T', 2, T3_lines, 2, 1, 1, 3>;
        using op_T4 = OpDesc<'T', 3, T4_lines, 2, 1, 2, 0>;

#undef T

        using StRuleBase = RuleSpec<10, 40, 4,
                                    op_O1,
                                    op_I1, op_I2,
                                    op_S1, op_S2,
                                    op_Z1, op_Z2,
                                    op_L1, op_L2, op_L3, op_L4,
                                    op_J1, op_J2, op_J3, op_J4,
                                    op_T1, op_T2, op_T3, op_T4>;

        struct StRule : StRuleBase
        {
            //rule_st 的 spawn 公式 (与原 spawn helper 一致): (W/2, H-2)
            static constexpr std::pair<int, int> spawn(char /*PT*/, int W, int H)
            {
                return {W / 2, H - 2};
            }
        };
    }

    class TetrisRule
    {
    public:
        using rule_spec = detail::StRule;
    };
}
