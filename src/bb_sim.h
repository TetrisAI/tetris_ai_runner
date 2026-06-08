#pragma once

//==========================================================================
// bb_sim.h — 纯位板路径字符串模拟
//
// sim_path_bb<H>(cs, usable_arr, move, move_end)
//   按 path 字符串驱动 BBState cs 移动，不接触任何 TetrisNode 指针。
//   H = Helpers<RuleSpec> 实例化类型（调用方透过 TetrisEngine::TetrisSearch
//   可获取对应 RuleSpec，进而得到 H）。
//
// 支持的字符：
//   'l'/'r'/'d' — 单步左/右/下
//   'L'/'R'     — 贴壁左/右
//   'D'         — 硬降（drop）
//   'z'         — 逆时针旋转踢墙
//   'c'         — 顺时针旋转踢墙
//   'x'         — 180° 旋转踢墙
//   其他        — 终止循环（等同于字符串结束）
//
// 移动语义：
//   - 单步平移：检查 usable_at_bb(r, xb±1/yb-1, usable_arr)；合法则更新。
//   - 旋转：调用 first_passing_kick_bb；成功则替换 cs。
//   - drop：调用 drop_bb_state；返回 optional，有值则替换 cs。
//   - 'l'/'r' roof 优化被省略（usable_at_bb 本身对空区必返回 true，
//     功能完全等价，无需单独判断 row >= roof）。
//==========================================================================

#include "bb_state.h"

namespace m_tetris2::bb
{

template<class H>
inline void sim_path_bb(BBState &cs,
                        std::array<typename H::map_t, H::kMaxR> const &usable_arr,
                        char const *move,
                        char const *move_end)
{
    while (move != move_end && *move != '\0')
    {
        switch (*move++)
        {
        case 'l':
        {
            int nxb = static_cast<int>(cs.xb) - 1;
            if (H::usable_at_bb(cs.r, nxb, static_cast<int>(cs.yb), usable_arr))
                cs.xb = static_cast<std::int8_t>(nxb);
            break;
        }
        case 'r':
        {
            int nxb = static_cast<int>(cs.xb) + 1;
            if (H::usable_at_bb(cs.r, nxb, static_cast<int>(cs.yb), usable_arr))
                cs.xb = static_cast<std::int8_t>(nxb);
            break;
        }
        case 'd':
        {
            int nyb = static_cast<int>(cs.yb) - 1;
            if (H::usable_at_bb(cs.r, static_cast<int>(cs.xb), nyb, usable_arr))
                cs.yb = static_cast<std::int8_t>(nyb);
            break;
        }
        case 'L':
        {
            int yb = static_cast<int>(cs.yb);
            int xb = static_cast<int>(cs.xb);
            while (H::usable_at_bb(cs.r, xb - 1, yb, usable_arr))
                --xb;
            cs.xb = static_cast<std::int8_t>(xb);
            break;
        }
        case 'R':
        {
            int yb = static_cast<int>(cs.yb);
            int xb = static_cast<int>(cs.xb);
            while (H::usable_at_bb(cs.r, xb + 1, yb, usable_arr))
                ++xb;
            cs.xb = static_cast<std::int8_t>(xb);
            break;
        }
        case 'D':
        {
            if (auto dropped = H::drop_bb_state(cs, usable_arr))
                cs = *dropped;
            break;
        }
        case 'z':
        {
            if (auto kicked = H::first_passing_kick_bb(static_cast<char>(cs.t), KickDir::Ccw, cs, usable_arr))
                cs = *kicked;
            break;
        }
        case 'c':
        {
            if (auto kicked = H::first_passing_kick_bb(static_cast<char>(cs.t), KickDir::Cw, cs, usable_arr))
                cs = *kicked;
            break;
        }
        case 'x':
        {
            if (auto kicked = H::first_passing_kick_bb(static_cast<char>(cs.t), KickDir::Opp, cs, usable_arr))
                cs = *kicked;
            break;
        }
        default:
            move = move_end;
            break;
        }
    }
}

} // namespace m_tetris2::bb
