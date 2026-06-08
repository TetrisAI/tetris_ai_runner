#ifndef TETRIS_AI_RUNNER_RULE_EXTREME_H_
#define TETRIS_AI_RUNNER_RULE_EXTREME_H_

//==========================================================================
// rule_extreme: 13×30 / 4x4 / 10 piece 极端规则.
//
// 用途: 给 emit-time filter index 框架 (kFilteredIndex / kFilteredCount)
//   做通用性压测. 主线 SRS 规则 piece 都是 4 cell × ≤4 旋转, master
//   tetris_core.cpp 里 block locate / TetrisMapSnap / piece_filter_index.h
//   等多处对 R_count<=4 / cell_count==4 的隐式假设, 单看 SRS 看不出来.
//   本规则故意:
//     - W=13 (非 10), H=30 (非 40), spawn 居中 ((13-4)/2=4 -> spawn x=4,
//       y 取 H-5=25, 与 SRS 1g spawn 思路同形 — Map::all_mask 有效行 [0, 30)).
//     - 10 个 piece 'A'..'J'.
//     - piece 'A' 1 cell × 1 rot, 'B' 2 cell × 2 rot, ..., 'J' 10 cell × 10 rot,
//       共 1+2+..+10 = 55 个 OpDesc, 任意 (T, R) cell 数恒 = piece 索引 (1..10).
//       覆盖 1..10 cell × 1..10 rotation 的乘积空间, 部分 piece 内 4×4 也接近
//       16 cell 的几何上限 (e.g. 'J' R0/R1 各只差 5/6 个 cell 满矩阵).
//     - 旋转链 cw r -> (r+1) % R_count, ccw r -> (r-1+R_count) % R_count,
//       180 全部 kOpRotateNone, 踢墙表全空 (NoKick).
//     - 部分 piece (e.g. 'B', 'C') 不同旋转下几何完全不同 — 不像 SRS S/Z 那样
//       r0=r2 同形, 真正压测 emit-time 去重表的 footprint 等价类合并.
//     - 高 cell count piece (G/H/I/J) 在小盘宽 13 上必然在多个 (xb, yb) 落到
//       重叠 cell mask, 触发 filter merge.
//
// 折衷: 任务原文要求"覆盖 1..16 cell", 但 4x4 矩阵 + 单 piece cell 数恒一
//   的约束下, 只 10 个 piece 装不下 11..16 cell 形态 — 实际折衷为 1..10 cell,
//   commit body 已注明.
//
// kicks / 180: 全空, 让落点集合完全由 BFS 平移 + 90° 旋转决定, 与 master
//   注册阶段 BFS rotate_clockwise/counterclockwise 等价.
//==========================================================================

#include "tetris_core.h"
#include "tetris_rule_spec.h"
#include <map>

namespace rule_extreme
{
    namespace detail
    {
        using namespace m_tetris2;

#define X(a, b, c, d) (((a) ? 1 : 0) | ((b) ? 2 : 0) | ((c) ? 4 : 0) | ((d) ? 8 : 0))

        //----- piece 'A': 1 cell × 1 rot -----
        using A0_lines = OpLines<X(1, 0, 0, 0), X(0, 0, 0, 0), X(0, 0, 0, 0), X(0, 0, 0, 0)>;
        using op_A0 = OpDesc<'A', 0, A0_lines, 0, 0>;

        //----- piece 'B': 2 cell × 2 rot -----
        using B0_lines = OpLines<X(1, 1, 0, 0), X(0, 0, 0, 0), X(0, 0, 0, 0), X(0, 0, 0, 0)>;
        using B1_lines = OpLines<X(1, 0, 0, 0), X(1, 0, 0, 0), X(0, 0, 0, 0), X(0, 0, 0, 0)>;
        using op_B0 = OpDesc<'B', 0, B0_lines, 0, 0, /*cw*/ 1, /*ccw*/ 1, kOpRotateNone>;
        using op_B1 = OpDesc<'B', 1, B1_lines, 0, 0, /*cw*/ 0, /*ccw*/ 0, kOpRotateNone>;

        //----- piece 'C': 3 cell × 3 rot -----
        using C0_lines = OpLines<X(1, 1, 1, 0), X(0, 0, 0, 0), X(0, 0, 0, 0), X(0, 0, 0, 0)>;
        using C1_lines = OpLines<X(1, 0, 0, 0), X(1, 0, 0, 0), X(1, 0, 0, 0), X(0, 0, 0, 0)>;
        using C2_lines = OpLines<X(1, 1, 0, 0), X(1, 0, 0, 0), X(0, 0, 0, 0), X(0, 0, 0, 0)>;
        using op_C0 = OpDesc<'C', 0, C0_lines, 0, 0, /*cw*/ 1, /*ccw*/ 2, kOpRotateNone>;
        using op_C1 = OpDesc<'C', 1, C1_lines, 0, 0, /*cw*/ 2, /*ccw*/ 0, kOpRotateNone>;
        using op_C2 = OpDesc<'C', 2, C2_lines, 0, 0, /*cw*/ 0, /*ccw*/ 1, kOpRotateNone>;

        //----- piece 'D': 4 cell × 4 rot ----- (T-shape variants, 故意几何全不同)
        using D0_lines = OpLines<X(0, 1, 0, 0), X(1, 1, 1, 0), X(0, 0, 0, 0), X(0, 0, 0, 0)>;
        using D1_lines = OpLines<X(0, 1, 0, 0), X(1, 1, 0, 0), X(0, 1, 0, 0), X(0, 0, 0, 0)>;
        using D2_lines = OpLines<X(0, 0, 0, 0), X(1, 1, 1, 0), X(0, 1, 0, 0), X(0, 0, 0, 0)>;
        using D3_lines = OpLines<X(0, 1, 0, 0), X(0, 1, 1, 0), X(0, 1, 0, 0), X(0, 0, 0, 0)>;
        using op_D0 = OpDesc<'D', 0, D0_lines, 0, 0, 1, 3, kOpRotateNone>;
        using op_D1 = OpDesc<'D', 1, D1_lines, 0, 0, 2, 0, kOpRotateNone>;
        using op_D2 = OpDesc<'D', 2, D2_lines, 0, 0, 3, 1, kOpRotateNone>;
        using op_D3 = OpDesc<'D', 3, D3_lines, 0, 0, 0, 2, kOpRotateNone>;

        //----- piece 'E': 5 cell × 5 rot -----
        using E0_lines = OpLines<X(1, 1, 1, 1), X(1, 0, 0, 0), X(0, 0, 0, 0), X(0, 0, 0, 0)>;
        using E1_lines = OpLines<X(1, 0, 0, 0), X(1, 0, 0, 0), X(1, 0, 0, 0), X(1, 1, 0, 0)>;
        using E2_lines = OpLines<X(1, 1, 0, 0), X(1, 1, 0, 0), X(1, 0, 0, 0), X(0, 0, 0, 0)>;
        using E3_lines = OpLines<X(0, 1, 1, 1), X(1, 0, 0, 0), X(1, 0, 0, 0), X(0, 0, 0, 0)>;
        using E4_lines = OpLines<X(0, 1, 0, 0), X(1, 1, 1, 0), X(0, 1, 0, 0), X(0, 0, 0, 0)>;
        using op_E0 = OpDesc<'E', 0, E0_lines, 0, 0, 1, 4, kOpRotateNone>;
        using op_E1 = OpDesc<'E', 1, E1_lines, 0, 0, 2, 0, kOpRotateNone>;
        using op_E2 = OpDesc<'E', 2, E2_lines, 0, 0, 3, 1, kOpRotateNone>;
        using op_E3 = OpDesc<'E', 3, E3_lines, 0, 0, 4, 2, kOpRotateNone>;
        using op_E4 = OpDesc<'E', 4, E4_lines, 0, 0, 0, 3, kOpRotateNone>;

        //----- piece 'F': 6 cell × 6 rot -----
        using F0_lines = OpLines<X(1, 1, 1, 1), X(1, 1, 0, 0), X(0, 0, 0, 0), X(0, 0, 0, 0)>;
        using F1_lines = OpLines<X(1, 0, 0, 0), X(1, 1, 0, 0), X(1, 1, 0, 0), X(1, 0, 0, 0)>;
        using F2_lines = OpLines<X(1, 1, 1, 0), X(0, 1, 0, 0), X(1, 1, 0, 0), X(0, 0, 0, 0)>;
        using F3_lines = OpLines<X(0, 1, 1, 1), X(1, 0, 0, 0), X(0, 1, 0, 0), X(0, 1, 0, 0)>;
        using F4_lines = OpLines<X(1, 1, 0, 0), X(1, 1, 0, 0), X(1, 1, 0, 0), X(0, 0, 0, 0)>;
        using F5_lines = OpLines<X(0, 1, 0, 0), X(1, 1, 1, 0), X(1, 1, 0, 0), X(0, 0, 0, 0)>;
        using op_F0 = OpDesc<'F', 0, F0_lines, 0, 0, 1, 5, kOpRotateNone>;
        using op_F1 = OpDesc<'F', 1, F1_lines, 0, 0, 2, 0, kOpRotateNone>;
        using op_F2 = OpDesc<'F', 2, F2_lines, 0, 0, 3, 1, kOpRotateNone>;
        using op_F3 = OpDesc<'F', 3, F3_lines, 0, 0, 4, 2, kOpRotateNone>;
        using op_F4 = OpDesc<'F', 4, F4_lines, 0, 0, 5, 3, kOpRotateNone>;
        using op_F5 = OpDesc<'F', 5, F5_lines, 0, 0, 0, 4, kOpRotateNone>;

        //----- piece 'G': 7 cell × 7 rot -----
        using G0_lines = OpLines<X(1, 1, 1, 1), X(1, 1, 1, 0), X(0, 0, 0, 0), X(0, 0, 0, 0)>;
        using G1_lines = OpLines<X(1, 0, 0, 0), X(1, 1, 0, 0), X(1, 1, 0, 0), X(1, 1, 0, 0)>;
        using G2_lines = OpLines<X(1, 1, 1, 0), X(1, 1, 0, 0), X(1, 1, 0, 0), X(0, 0, 0, 0)>;
        using G3_lines = OpLines<X(1, 1, 0, 0), X(1, 1, 0, 0), X(1, 1, 1, 0), X(0, 0, 0, 0)>;
        using G4_lines = OpLines<X(0, 1, 1, 1), X(1, 1, 0, 0), X(1, 1, 0, 0), X(0, 0, 0, 0)>;
        using G5_lines = OpLines<X(1, 0, 0, 0), X(1, 1, 1, 0), X(1, 1, 1, 0), X(0, 0, 0, 0)>;
        using G6_lines = OpLines<X(0, 1, 0, 0), X(1, 1, 1, 0), X(1, 1, 0, 0), X(1, 0, 0, 0)>;
        using op_G0 = OpDesc<'G', 0, G0_lines, 0, 0, 1, 6, kOpRotateNone>;
        using op_G1 = OpDesc<'G', 1, G1_lines, 0, 0, 2, 0, kOpRotateNone>;
        using op_G2 = OpDesc<'G', 2, G2_lines, 0, 0, 3, 1, kOpRotateNone>;
        using op_G3 = OpDesc<'G', 3, G3_lines, 0, 0, 4, 2, kOpRotateNone>;
        using op_G4 = OpDesc<'G', 4, G4_lines, 0, 0, 5, 3, kOpRotateNone>;
        using op_G5 = OpDesc<'G', 5, G5_lines, 0, 0, 6, 4, kOpRotateNone>;
        using op_G6 = OpDesc<'G', 6, G6_lines, 0, 0, 0, 5, kOpRotateNone>;

        //----- piece 'H': 8 cell × 8 rot -----
        using H0_lines = OpLines<X(1, 1, 1, 1), X(1, 1, 1, 1), X(0, 0, 0, 0), X(0, 0, 0, 0)>;
        using H1_lines = OpLines<X(1, 1, 0, 0), X(1, 1, 0, 0), X(1, 1, 0, 0), X(1, 1, 0, 0)>;
        using H2_lines = OpLines<X(0, 1, 1, 1), X(1, 1, 0, 0), X(1, 1, 0, 0), X(1, 0, 0, 0)>;
        using H3_lines = OpLines<X(1, 1, 0, 0), X(1, 1, 1, 0), X(1, 1, 1, 0), X(0, 0, 0, 0)>;
        using H4_lines = OpLines<X(1, 0, 0, 0), X(1, 1, 1, 0), X(1, 1, 0, 0), X(1, 1, 0, 0)>;
        using H5_lines = OpLines<X(1, 1, 1, 1), X(1, 1, 0, 0), X(1, 1, 0, 0), X(0, 0, 0, 0)>;
        using H6_lines = OpLines<X(0, 1, 0, 0), X(1, 1, 1, 0), X(1, 1, 1, 0), X(0, 1, 0, 0)>;
        using H7_lines = OpLines<X(0, 0, 1, 1), X(1, 1, 0, 0), X(0, 0, 1, 1), X(1, 1, 0, 0)>;
        using op_H0 = OpDesc<'H', 0, H0_lines, 0, 0, 1, 7, kOpRotateNone>;
        using op_H1 = OpDesc<'H', 1, H1_lines, 0, 0, 2, 0, kOpRotateNone>;
        using op_H2 = OpDesc<'H', 2, H2_lines, 0, 0, 3, 1, kOpRotateNone>;
        using op_H3 = OpDesc<'H', 3, H3_lines, 0, 0, 4, 2, kOpRotateNone>;
        using op_H4 = OpDesc<'H', 4, H4_lines, 0, 0, 5, 3, kOpRotateNone>;
        using op_H5 = OpDesc<'H', 5, H5_lines, 0, 0, 6, 4, kOpRotateNone>;
        using op_H6 = OpDesc<'H', 6, H6_lines, 0, 0, 7, 5, kOpRotateNone>;
        using op_H7 = OpDesc<'H', 7, H7_lines, 0, 0, 0, 6, kOpRotateNone>;

        //----- piece 'I': 9 cell × 9 rot -----
        using I0_lines = OpLines<X(1, 1, 1, 1), X(1, 1, 1, 1), X(1, 0, 0, 0), X(0, 0, 0, 0)>;
        using I1_lines = OpLines<X(1, 0, 0, 0), X(1, 1, 0, 0), X(1, 1, 1, 0), X(1, 1, 1, 0)>;
        using I2_lines = OpLines<X(1, 1, 0, 0), X(1, 1, 0, 0), X(1, 1, 1, 0), X(1, 1, 0, 0)>;
        using I3_lines = OpLines<X(1, 1, 1, 0), X(1, 1, 1, 0), X(1, 1, 1, 0), X(0, 0, 0, 0)>;
        using I4_lines = OpLines<X(0, 1, 1, 1), X(1, 1, 1, 0), X(1, 1, 0, 0), X(1, 0, 0, 0)>;
        using I5_lines = OpLines<X(1, 0, 0, 0), X(1, 1, 1, 1), X(1, 1, 1, 1), X(0, 0, 0, 0)>;
        using I6_lines = OpLines<X(1, 1, 1, 1), X(1, 1, 0, 0), X(1, 1, 0, 0), X(1, 0, 0, 0)>;
        using I7_lines = OpLines<X(1, 0, 0, 0), X(1, 0, 0, 0), X(1, 1, 1, 0), X(1, 1, 1, 1)>;
        using I8_lines = OpLines<X(0, 1, 0, 0), X(1, 1, 1, 0), X(1, 1, 1, 0), X(1, 1, 0, 0)>;
        using op_I0 = OpDesc<'I', 0, I0_lines, 0, 0, 1, 8, kOpRotateNone>;
        using op_I1 = OpDesc<'I', 1, I1_lines, 0, 0, 2, 0, kOpRotateNone>;
        using op_I2 = OpDesc<'I', 2, I2_lines, 0, 0, 3, 1, kOpRotateNone>;
        using op_I3 = OpDesc<'I', 3, I3_lines, 0, 0, 4, 2, kOpRotateNone>;
        using op_I4 = OpDesc<'I', 4, I4_lines, 0, 0, 5, 3, kOpRotateNone>;
        using op_I5 = OpDesc<'I', 5, I5_lines, 0, 0, 6, 4, kOpRotateNone>;
        using op_I6 = OpDesc<'I', 6, I6_lines, 0, 0, 7, 5, kOpRotateNone>;
        using op_I7 = OpDesc<'I', 7, I7_lines, 0, 0, 8, 6, kOpRotateNone>;
        using op_I8 = OpDesc<'I', 8, I8_lines, 0, 0, 0, 7, kOpRotateNone>;

        //----- piece 'J': 10 cell × 10 rot ----- (R_count=10, 突破 SRS 4 旋转上限)
        using J0_lines = OpLines<X(1, 1, 1, 1), X(1, 1, 1, 1), X(1, 1, 0, 0), X(0, 0, 0, 0)>;
        using J1_lines = OpLines<X(1, 1, 0, 0), X(1, 1, 0, 0), X(1, 1, 1, 1), X(1, 1, 0, 0)>;
        using J2_lines = OpLines<X(1, 1, 1, 0), X(1, 1, 1, 0), X(1, 1, 0, 0), X(1, 1, 0, 0)>;
        using J3_lines = OpLines<X(1, 0, 0, 0), X(1, 1, 0, 0), X(1, 1, 1, 0), X(1, 1, 1, 1)>;
        using J4_lines = OpLines<X(1, 1, 1, 1), X(1, 1, 1, 0), X(1, 1, 0, 0), X(1, 0, 0, 0)>;
        using J5_lines = OpLines<X(1, 1, 0, 0), X(1, 1, 0, 0), X(1, 1, 1, 0), X(1, 1, 1, 0)>;
        using J6_lines = OpLines<X(1, 1, 0, 0), X(1, 1, 1, 0), X(1, 1, 1, 0), X(1, 1, 0, 0)>;
        using J7_lines = OpLines<X(0, 1, 1, 1), X(1, 1, 1, 0), X(1, 1, 0, 0), X(1, 1, 0, 0)>;
        using J8_lines = OpLines<X(1, 1, 1, 1), X(1, 1, 0, 0), X(1, 1, 0, 0), X(1, 1, 0, 0)>;
        using J9_lines = OpLines<X(1, 1, 0, 0), X(0, 1, 1, 1), X(1, 1, 1, 0), X(1, 1, 0, 0)>;
        using op_J0 = OpDesc<'J', 0, J0_lines, 0, 0, 1, 9, kOpRotateNone>;
        using op_J1 = OpDesc<'J', 1, J1_lines, 0, 0, 2, 0, kOpRotateNone>;
        using op_J2 = OpDesc<'J', 2, J2_lines, 0, 0, 3, 1, kOpRotateNone>;
        using op_J3 = OpDesc<'J', 3, J3_lines, 0, 0, 4, 2, kOpRotateNone>;
        using op_J4 = OpDesc<'J', 4, J4_lines, 0, 0, 5, 3, kOpRotateNone>;
        using op_J5 = OpDesc<'J', 5, J5_lines, 0, 0, 6, 4, kOpRotateNone>;
        using op_J6 = OpDesc<'J', 6, J6_lines, 0, 0, 7, 5, kOpRotateNone>;
        using op_J7 = OpDesc<'J', 7, J7_lines, 0, 0, 8, 6, kOpRotateNone>;
        using op_J8 = OpDesc<'J', 8, J8_lines, 0, 0, 9, 7, kOpRotateNone>;
        using op_J9 = OpDesc<'J', 9, J9_lines, 0, 0, 0, 8, kOpRotateNone>;

#undef X

        //13×30, N=4, 共 55 个 OpDesc.
        using ExtremeRuleBase = RuleSpec<13, 30, 4,
                                         op_A0,
                                         op_B0, op_B1,
                                         op_C0, op_C1, op_C2,
                                         op_D0, op_D1, op_D2, op_D3,
                                         op_E0, op_E1, op_E2, op_E3, op_E4,
                                         op_F0, op_F1, op_F2, op_F3, op_F4, op_F5,
                                         op_G0, op_G1, op_G2, op_G3, op_G4, op_G5, op_G6,
                                         op_H0, op_H1, op_H2, op_H3, op_H4, op_H5, op_H6, op_H7,
                                         op_I0, op_I1, op_I2, op_I3, op_I4, op_I5, op_I6, op_I7, op_I8,
                                         op_J0, op_J1, op_J2, op_J3, op_J4, op_J5, op_J6, op_J7, op_J8, op_J9>;

        struct ExtremeRule : ExtremeRuleBase
        {
            //spawn: 横向居中 (W=13 -> floor((13-4)/2)=4), 纵向 25 (H=30, master 风格
            //  H-5 = 25, 留 5 行余量给 4 行高 piece + 1 行裕量).
            static constexpr std::pair<int, int> spawn(char /*PT*/, int W, int H)
            {
                return {(W - 4) / 2, H - 5};
            }
        };
    }

    class TetrisRule
    {
    public:
        using rule_spec = detail::ExtremeRule;
    };
}

#endif // TETRIS_AI_RUNNER_RULE_EXTREME_H_
