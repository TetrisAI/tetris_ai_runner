// cases.h — A组对拍共享 case 表。old_main / new_main 都包含它，
// 按同一确定性参数表展开，保证两侧输入逐 case 完全一致。
#pragma once
#include <cstdint>

namespace aicases
{
    constexpr int kW = 10;
    constexpr int kH = 40;

    // 6 张固定盘面：rows[y] 为 1=occupied 位掩码（y=0 底行）。
    struct Board
    {
        uint32_t rows[kH];
    };

    inline Board make_board(int kind)
    {
        Board b{};
        for (int i = 0; i < kH; ++i)
            b.rows[i] = 0u;
        auto fill = [&](int y, uint32_t mask) { b.rows[y] = mask; };
        switch (kind)
        {
        case 0: // 最小盘（落块后必有块；避免 roof=0 触发 occupied_row(-1)）
            fill(0, 0x1);
            break;
        case 1: // 平底 4 行
            fill(0, 0x3ff);
            fill(1, 0x3ff);
            fill(2, 0x3ff);
            fill(3, 0x3ff);
            break;
        case 2: // 平底 + 一列洞
            fill(0, 0x3ff);
            fill(1, 0x3ff);
            fill(2, 0x3ff ^ (1u << 5)); // row2 第5列空（洞）
            fill(3, 0x3ff);
            break;
        case 3: // 高栈（roof 大，触发 roof>4 / Danger）
            for (int y = 0; y < 18; ++y)
                fill(y, 0x3ff ^ (1u << 2));
            fill(18, 0x3ff);
            break;
        case 4: // 井（well）：右列高、左列低
            fill(0, 0x3ff);
            fill(1, 0x3ff);
            fill(2, 0x3ff ^ (1u << 0));
            fill(3, 0x3ff ^ (1u << 0));
            fill(4, 0x3ff);
            break;
        case 5: // 近顶：占满 row 36..39
            fill(36, 0x3ff);
            fill(37, 0x3ff);
            fill(38, 0x3ff);
            fill(39, 0x3ff);
            break;
        }
        return b;
    }

    // 单个 case 的全部输入。
    struct Case
    {
        int board_kind;
        int clear;        // 0..4
        int spin;         // 0=None,1=TSpin/ASpin,2=TSpinMini
        int attack_mode;  // Attack Config.mode
        std::size_t depth;
        int land_height;  // 期望 landing.land_height()
        int col;         // 期望 landing.col() (bbox 左列)
        // parent Status（跨深度累积）各字段
        double p_land_point;
        double p_attack;
        double p_rubbish;
        int p_under_attack;
        int p_recv_attack;
        int p_attack_int;
        int p_like;
        int p_combo;
        bool p_b2b;
        bool p_pc;
        double p_value;
    };

    constexpr int kNumCases = 96;

    // 确定性展开：idx -> Case。两 binary 用同一公式。
    inline Case decode(int idx)
    {
        Case c{};
        c.board_kind = idx % 6;
        int clear = (idx / 6) % 5;
        int spin = (idx / (6 * 5)) % 3;
        int mode = (idx / (6 * 5 * 3)) % 4;
        int depthsel = (idx / (6 * 5 * 3 * 4)) % 3;
        int lhsel = (idx / (6 * 5 * 3 * 4 * 3)) % 3;
        int colsel = (idx / (6 * 5 * 3 * 4 * 3 * 3)) % 3;

        c.clear = clear;
        c.spin = spin;
        c.attack_mode = mode;
        c.depth = depthsel;
        c.land_height = (lhsel == 0) ? 4 : (lhsel == 1 ? 12 : 40); // 40 == map_height 触发 DeadZone
        c.col = (colsel == 0) ? 0 : (colsel == 1 ? 4 : 9);

        // parent 累积状态，随 idx 变化以覆盖组合/b2b/under_attack/pc 路径
        c.p_under_attack = (idx & 1) ? 5 : 0;
        c.p_recv_attack = (idx & 2) ? 3 : 0;
        c.p_attack_int = (idx & 4) ? 2 : 0;
        c.p_like = (idx & 8) ? 50 : 0;
        c.p_combo = (idx & 16) ? 2 : 0;
        c.p_b2b = (idx & 32) != 0;
        c.p_pc = (idx & 64) != 0;
        c.p_land_point = (idx % 7) * 1.5;
        c.p_attack = (idx % 5) * 2.0;
        c.p_rubbish = (idx % 3) * 1.0;
        c.p_value = c.p_land_point;
        return c;
    }

    // PC combo 表（两侧同用）
    inline const int *combo_table()
    {
        static const int t[16] = {0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7};
        return t;
    }
    constexpr int combo_table_max = 16;
}
