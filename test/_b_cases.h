// _b_cases.h — B组(TOJ_v08/Botris/TOJ/C2)对拍共享确定性 case 表.
// old_golden_main / new_diff_main 都包含它，两侧输入逐 case 完全一致.
#pragma once
#include <cstdint>

namespace bicases
{
    // 6 张盘面: rows[y] 位掩码, 1=occupied, y=0 底行. 复用 A 组盘形语义.
    struct Board { uint32_t rows[40]; };

    inline Board make_board(int kind)
    {
        Board b{};
        auto fill = [&](int y, uint32_t mask) { b.rows[y] = mask; };
        switch (kind)
        {
        case 0: fill(0, 0x1); break;                                   // 最小
        case 1: fill(0, 0x3ff); fill(1, 0x3ff); fill(2, 0x3ff); fill(3, 0x3ff); break; // 平底
        case 2: fill(0, 0x3ff); fill(1, 0x3ff); fill(2, 0x3ff ^ (1u << 5)); fill(3, 0x3ff); break; // 含洞
        case 3: for (int y = 0; y < 18; ++y) fill(y, 0x3ff ^ (1u << 2)); fill(18, 0x3ff); break; // 高栈
        case 4: fill(0, 0x3ff); fill(1, 0x3ff); fill(2, 0x3ff ^ (1u << 0)); fill(3, 0x3ff ^ (1u << 0)); fill(4, 0x3ff); break; // 井
        case 5: fill(36, 0x3ff); fill(37, 0x3ff); fill(38, 0x3ff); fill(39, 0x3ff); break; // 近顶(topout)
        }
        return b;
    }

    struct Case
    {
        int board_kind;
        int col;           // 落块 x
        int land_height;   // land_height() = y + kHeight
        int clear;         // 消行数 0..4
        int spin;         // 0=None 1=TSpin/ASpin 2=TSpinMini
        int depth;        // 0..2
        int env_len;      // next 队列长度
        char node;        // 新块 (C2 NextNode)
        char hold;        // hold 块 (v08/Botris/TOJ env.hold)
    };

    inline Case const *table()
    {
        static Case const t[] = {
            {0, 3, 4, 0, 0, 0, 0, ' ', ' '},
            {1, 3, 4, 0, 0, 1, 2, 'T', ' '},
            {1, 3, 4, 1, 1, 0, 3, 'I', 'T'},   // TSpin single
            {1, 3, 4, 1, 2, 0, 1, 'T', 'I'},   // TSpinMini
            {2, 5, 4, 2, 0, 0, 2, 'T', ' '},   // 洞 + double
            {3, 2, 10, 3, 0, 2, 4, 'I', 'T'},  // 高栈 triple
            {4, 8, 5, 4, 0, 0, 0, ' ', ' '},   // well tetris
            {5, 3, 38, 0, 0, 1, 1, 'T', ' '},  // 近顶 topout
            {1, 0, 4, 1, 0, 0, 5, 'T', 'I'},   // 左列 edge
            {1, 9, 4, 0, 1, 2, 0, ' ', ' '},   // 右列 edge spin
            {2, 5, 4, 0, 0, 1, 3, 'T', 'T'},   // hold=T, t_expect=0
            {3, 2, 10, 2, 1, 0, 6, 'I', ' '},  // 高栈 spin double
        };
        return t;
    }
    inline int kNumCases() { return 12; }

    inline int const *combo_table()
    {
        static int const ct[] = {0, 0, 1, 1, 1, 2, 2, 3, 3, 4, 4, 4, 5};
        return ct;
    }
    inline int combo_table_max() { return 12; }
} // namespace bicases
