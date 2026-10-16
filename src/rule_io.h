#pragma once

#include "tetris_core.h"

#include <cstdint>
#include <tuple>
#include <utility>

// TETR.IO SRS+ (.research/tetrio-s2-rules/rules.md §2, §4). Kick offsets are y-up; the movegen tests the plain
// rotation (0,0) before the listed kicks, so (0,0) is not listed.
namespace rule_io
{
    namespace detail
    {
        using namespace m_tetris2;

#define T(a, b, c, d) (((a) ? 1 : 0) | ((b) ? 2 : 0) | ((c) ? 4 : 0) | ((d) ? 8 : 0))

        using JlstzKick01 = WallKickList<-1, 0, -1, +1, 0, -2, -1, -2>;
        using JlstzKick10 = WallKickList<+1, 0, +1, -1, 0, +2, +1, +2>;
        using JlstzKick12 = WallKickList<+1, 0, +1, -1, 0, +2, +1, +2>;
        using JlstzKick21 = WallKickList<-1, 0, -1, +1, 0, -2, -1, -2>;
        using JlstzKick23 = WallKickList<+1, 0, +1, +1, 0, -2, +1, -2>;
        using JlstzKick32 = WallKickList<-1, 0, -1, -1, 0, +2, -1, +2>;
        using JlstzKick30 = WallKickList<-1, 0, -1, -1, 0, +2, -1, +2>;
        using JlstzKick03 = WallKickList<+1, 0, +1, +1, 0, -2, +1, -2>;
        using JlstzKick02 = WallKickList<0, +1, +1, +1, -1, +1, +1, 0, -1, 0>;
        using JlstzKick20 = WallKickList<0, -1, -1, -1, +1, -1, -1, 0, +1, 0>;
        using JlstzKick13 = WallKickList<+1, 0, +1, +2, +1, +1, 0, +2, 0, +1>;
        using JlstzKick31 = WallKickList<-1, 0, -1, +2, -1, +1, 0, +2, 0, +1>;

        using IKick01 = WallKickList<+1, 0, -2, 0, -2, -1, +1, +2>;
        using IKick10 = WallKickList<-1, 0, +2, 0, -1, -2, +2, +1>;
        using IKick12 = WallKickList<-1, 0, +2, 0, -1, +2, +2, -1>;
        using IKick21 = WallKickList<-2, 0, +1, 0, -2, +1, +1, -2>;
        using IKick23 = WallKickList<+2, 0, -1, 0, +2, +1, -1, -2>;
        using IKick32 = WallKickList<+1, 0, -2, 0, +1, +2, -2, -1>;
        using IKick30 = WallKickList<+1, 0, -2, 0, +1, -2, -2, +1>;
        using IKick03 = WallKickList<-1, 0, +2, 0, +2, -1, -1, +2>;
        using IKick02 = WallKickList<0, +1>;
        using IKick20 = WallKickList<0, -1>;
        using IKick13 = WallKickList<+1, 0>;
        using IKick31 = WallKickList<-1, 0>;

        // Kick tables of rotation state R, in (CW, CCW, 180) order.
        using JlstzKicks = std::tuple<std::tuple<JlstzKick01, JlstzKick03, JlstzKick02>,
                                      std::tuple<JlstzKick12, JlstzKick10, JlstzKick13>,
                                      std::tuple<JlstzKick23, JlstzKick21, JlstzKick20>,
                                      std::tuple<JlstzKick30, JlstzKick32, JlstzKick31>>;
        using IKicks = std::tuple<std::tuple<IKick01, IKick03, IKick02>,
                                  std::tuple<IKick12, IKick10, IKick13>,
                                  std::tuple<IKick23, IKick21, IKick20>,
                                  std::tuple<IKick30, IKick32, IKick31>>;

        template<char P, std::uint8_t R, class Lines, class Kicks>
        using SrsPlusOp = OpDesc<P, R, Lines, 0, 0, (R + 1) % 4, (R + 3) % 4, (R + 2) % 4,
                                 std::tuple_element_t<0, std::tuple_element_t<R, Kicks>>,
                                 std::tuple_element_t<1, std::tuple_element_t<R, Kicks>>,
                                 std::tuple_element_t<2, std::tuple_element_t<R, Kicks>>>;

        using op_O = OpDesc<'O', 0, OpLines<T(0, 1, 1, 0), T(0, 1, 1, 0), T(0, 0, 0, 0), T(0, 0, 0, 0)>>;

        using op_I0 = SrsPlusOp<'I', 0, OpLines<T(0, 0, 0, 0), T(1, 1, 1, 1), T(0, 0, 0, 0), T(0, 0, 0, 0)>, IKicks>;
        using op_I1 = SrsPlusOp<'I', 1, OpLines<T(0, 0, 1, 0), T(0, 0, 1, 0), T(0, 0, 1, 0), T(0, 0, 1, 0)>, IKicks>;
        using op_I2 = SrsPlusOp<'I', 2, OpLines<T(0, 0, 0, 0), T(0, 0, 0, 0), T(1, 1, 1, 1), T(0, 0, 0, 0)>, IKicks>;
        using op_I3 = SrsPlusOp<'I', 3, OpLines<T(0, 1, 0, 0), T(0, 1, 0, 0), T(0, 1, 0, 0), T(0, 1, 0, 0)>, IKicks>;

        using op_S0 = SrsPlusOp<'S', 0, OpLines<T(0, 1, 1, 0), T(1, 1, 0, 0), T(0, 0, 0, 0), T(0, 0, 0, 0)>, JlstzKicks>;
        using op_S1 = SrsPlusOp<'S', 1, OpLines<T(0, 1, 0, 0), T(0, 1, 1, 0), T(0, 0, 1, 0), T(0, 0, 0, 0)>, JlstzKicks>;
        using op_S2 = SrsPlusOp<'S', 2, OpLines<T(0, 0, 0, 0), T(0, 1, 1, 0), T(1, 1, 0, 0), T(0, 0, 0, 0)>, JlstzKicks>;
        using op_S3 = SrsPlusOp<'S', 3, OpLines<T(1, 0, 0, 0), T(1, 1, 0, 0), T(0, 1, 0, 0), T(0, 0, 0, 0)>, JlstzKicks>;

        using op_Z0 = SrsPlusOp<'Z', 0, OpLines<T(1, 1, 0, 0), T(0, 1, 1, 0), T(0, 0, 0, 0), T(0, 0, 0, 0)>, JlstzKicks>;
        using op_Z1 = SrsPlusOp<'Z', 1, OpLines<T(0, 0, 1, 0), T(0, 1, 1, 0), T(0, 1, 0, 0), T(0, 0, 0, 0)>, JlstzKicks>;
        using op_Z2 = SrsPlusOp<'Z', 2, OpLines<T(0, 0, 0, 0), T(1, 1, 0, 0), T(0, 1, 1, 0), T(0, 0, 0, 0)>, JlstzKicks>;
        using op_Z3 = SrsPlusOp<'Z', 3, OpLines<T(0, 1, 0, 0), T(1, 1, 0, 0), T(1, 0, 0, 0), T(0, 0, 0, 0)>, JlstzKicks>;

        using op_L0 = SrsPlusOp<'L', 0, OpLines<T(0, 0, 1, 0), T(1, 1, 1, 0), T(0, 0, 0, 0), T(0, 0, 0, 0)>, JlstzKicks>;
        using op_L1 = SrsPlusOp<'L', 1, OpLines<T(0, 1, 0, 0), T(0, 1, 0, 0), T(0, 1, 1, 0), T(0, 0, 0, 0)>, JlstzKicks>;
        using op_L2 = SrsPlusOp<'L', 2, OpLines<T(0, 0, 0, 0), T(1, 1, 1, 0), T(1, 0, 0, 0), T(0, 0, 0, 0)>, JlstzKicks>;
        using op_L3 = SrsPlusOp<'L', 3, OpLines<T(1, 1, 0, 0), T(0, 1, 0, 0), T(0, 1, 0, 0), T(0, 0, 0, 0)>, JlstzKicks>;

        using op_J0 = SrsPlusOp<'J', 0, OpLines<T(1, 0, 0, 0), T(1, 1, 1, 0), T(0, 0, 0, 0), T(0, 0, 0, 0)>, JlstzKicks>;
        using op_J1 = SrsPlusOp<'J', 1, OpLines<T(0, 1, 1, 0), T(0, 1, 0, 0), T(0, 1, 0, 0), T(0, 0, 0, 0)>, JlstzKicks>;
        using op_J2 = SrsPlusOp<'J', 2, OpLines<T(0, 0, 0, 0), T(1, 1, 1, 0), T(0, 0, 1, 0), T(0, 0, 0, 0)>, JlstzKicks>;
        using op_J3 = SrsPlusOp<'J', 3, OpLines<T(0, 1, 0, 0), T(0, 1, 0, 0), T(1, 1, 0, 0), T(0, 0, 0, 0)>, JlstzKicks>;

        using op_T0 = SrsPlusOp<'T', 0, OpLines<T(0, 1, 0, 0), T(1, 1, 1, 0), T(0, 0, 0, 0), T(0, 0, 0, 0)>, JlstzKicks>;
        using op_T1 = SrsPlusOp<'T', 1, OpLines<T(0, 1, 0, 0), T(0, 1, 1, 0), T(0, 1, 0, 0), T(0, 0, 0, 0)>, JlstzKicks>;
        using op_T2 = SrsPlusOp<'T', 2, OpLines<T(0, 0, 0, 0), T(1, 1, 1, 0), T(0, 1, 0, 0), T(0, 0, 0, 0)>, JlstzKicks>;
        using op_T3 = SrsPlusOp<'T', 3, OpLines<T(0, 1, 0, 0), T(1, 1, 0, 0), T(0, 1, 0, 0), T(0, 0, 0, 0)>, JlstzKicks>;

#undef T

        using IoRuleBase = RuleSpec<10, 40, 4,
                                    op_O,
                                    op_I0, op_I1, op_I2, op_I3,
                                    op_S0, op_S1, op_S2, op_S3,
                                    op_Z0, op_Z1, op_Z2, op_Z3,
                                    op_L0, op_L1, op_L2, op_L3,
                                    op_J0, op_J1, op_J2, op_J3,
                                    op_T0, op_T1, op_T2, op_T3>;

        struct IoRule : IoRuleBase
        {
            // Master coordinates (top-left cell of the 4x4 box): every flat spawn row is board row 21 (0-based from
            // the bottom); columns T/S/Z/L/J 3-5, I 3-6, O 4-5. O never rotates, so its spins are unreachable.
            static constexpr std::pair<int, int> spawn(char /*PT*/, int /*W*/, int /*H*/)
            {
                return {3, 22};
            }
        };
    } // namespace detail

    class TetrisRule
    {
    public:
        using rule_spec = detail::IoRule;
    };
} // namespace rule_io
