// Movegen perft / dump driver.
//
// 目标:
//   - 在不依赖 tetris_core.h (因此不依赖 chash) 的前提下, 用最小化 SRS T/L/J/S/Z/I/O OpDesc
//     喂 MoveGen, 在空盘上把每种方块的可达落点全部 dump 出来.
//   - 输出 (rotation, x, y, spin) -> 总数, 既能跑 perft 比对, 也能在以后接通 cobra
//     时按行 diff.
//
// 用法:
//   perft_movegen <piece>     例如 perft_movegen T
//   perft_movegen all         dump O I T L J S Z 全集
//
// 设计取舍:
//   - SRS 的 OpDesc 整套已经在 src/rule_srs.h 里, 但它 #include "tetris_core.h"
//     拖入 chash. 这里直接照抄 rule_srs::detail 命名空间内 T/L/J/S/Z/I/O 的
//     OpDesc, 仅需要 RuleSpec / OpDesc / OpLines / WallKickList / kOpRotateNone,
//     这些都来自 tetris_rule_spec.h, 自包含, 不引 chash.
//   - 输出格式: 每行 "<piece>,<r>,<x>,<y>,<spin>" 便于 sort | uniq | diff.

#include "../src/tetris_movegen.h"
#include "../src/tetris_rule_spec.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

namespace srs_minimal
{
    using namespace m_tetris2;

#define R(a, b, c, d) (((a) ? 1 : 0) | ((b) ? 2 : 0) | ((c) ? 4 : 0) | ((d) ? 8 : 0))

    using IKickR0CW = WallKickList<-2, 0, +1, 0, -2, -1, +1, +2>;
    using IKickR0CCW = WallKickList<-1, 0, +2, 0, -1, +2, +2, -1>;
    using IKickR1CW = WallKickList<-1, 0, +2, 0, -1, +2, +2, -1>;
    using IKickR1CCW = WallKickList<+2, 0, -1, 0, +2, +1, -1, -2>;
    using IKickR2CW = WallKickList<+2, 0, -1, 0, +2, +1, -1, -2>;
    using IKickR2CCW = WallKickList<+1, 0, -2, 0, +1, -2, -2, +1>;
    using IKickR3CW = WallKickList<+1, 0, -2, 0, +1, -2, -2, +1>;
    using IKickR3CCW = WallKickList<-2, 0, +1, 0, -2, -1, +1, +2>;

    using JlstzKickR0CW = WallKickList<-1, 0, -1, +1, 0, -2, -1, -2>;
    using JlstzKickR0CCW = WallKickList<+1, 0, +1, +1, 0, -2, +1, -2>;
    using JlstzKickR1CW = WallKickList<+1, 0, +1, -1, 0, +2, +1, +2>;
    using JlstzKickR1CCW = WallKickList<+1, 0, +1, -1, 0, +2, +1, +2>;
    using JlstzKickR2CW = WallKickList<+1, 0, +1, +1, 0, -2, +1, -2>;
    using JlstzKickR2CCW = WallKickList<-1, 0, -1, +1, 0, -2, -1, -2>;
    using JlstzKickR3CW = WallKickList<-1, 0, -1, -1, 0, +2, -1, +2>;
    using JlstzKickR3CCW = WallKickList<-1, 0, -1, -1, 0, +2, -1, +2>;

    using O1_lines = OpLines<R(0, 1, 1, 0), R(0, 1, 1, 0), R(0, 0, 0, 0), R(0, 0, 0, 0)>;
    using op_O1 = OpDesc<'O', 0, O1_lines, 0, 0>;

    using I1_lines = OpLines<R(0, 0, 0, 0), R(1, 1, 1, 1), R(0, 0, 0, 0), R(0, 0, 0, 0)>;
    using I2_lines = OpLines<R(0, 0, 1, 0), R(0, 0, 1, 0), R(0, 0, 1, 0), R(0, 0, 1, 0)>;
    using I3_lines = OpLines<R(0, 0, 0, 0), R(0, 0, 0, 0), R(1, 1, 1, 1), R(0, 0, 0, 0)>;
    using I4_lines = OpLines<R(0, 1, 0, 0), R(0, 1, 0, 0), R(0, 1, 0, 0), R(0, 1, 0, 0)>;
    using op_I1 = OpDesc<'I', 0, I1_lines, 0, 0, 1, 3, kOpRotateNone, IKickR0CW, IKickR0CCW>;
    using op_I2 = OpDesc<'I', 1, I2_lines, 0, 0, 2, 0, kOpRotateNone, IKickR1CW, IKickR1CCW>;
    using op_I3 = OpDesc<'I', 2, I3_lines, 0, 0, 3, 1, kOpRotateNone, IKickR2CW, IKickR2CCW>;
    using op_I4 = OpDesc<'I', 3, I4_lines, 0, 0, 0, 2, kOpRotateNone, IKickR3CW, IKickR3CCW>;

    using S1_lines = OpLines<R(0, 1, 1, 0), R(1, 1, 0, 0), R(0, 0, 0, 0), R(0, 0, 0, 0)>;
    using S2_lines = OpLines<R(0, 1, 0, 0), R(0, 1, 1, 0), R(0, 0, 1, 0), R(0, 0, 0, 0)>;
    using S3_lines = OpLines<R(0, 0, 0, 0), R(0, 1, 1, 0), R(1, 1, 0, 0), R(0, 0, 0, 0)>;
    using S4_lines = OpLines<R(1, 0, 0, 0), R(1, 1, 0, 0), R(0, 1, 0, 0), R(0, 0, 0, 0)>;
    using op_S1 = OpDesc<'S', 0, S1_lines, 0, 0, 1, 3, kOpRotateNone, JlstzKickR0CW, JlstzKickR0CCW>;
    using op_S2 = OpDesc<'S', 1, S2_lines, 0, 0, 2, 0, kOpRotateNone, JlstzKickR1CW, JlstzKickR1CCW>;
    using op_S3 = OpDesc<'S', 2, S3_lines, 0, 0, 3, 1, kOpRotateNone, JlstzKickR2CW, JlstzKickR2CCW>;
    using op_S4 = OpDesc<'S', 3, S4_lines, 0, 0, 0, 2, kOpRotateNone, JlstzKickR3CW, JlstzKickR3CCW>;

    using Z1_lines = OpLines<R(1, 1, 0, 0), R(0, 1, 1, 0), R(0, 0, 0, 0), R(0, 0, 0, 0)>;
    using Z2_lines = OpLines<R(0, 0, 1, 0), R(0, 1, 1, 0), R(0, 1, 0, 0), R(0, 0, 0, 0)>;
    using Z3_lines = OpLines<R(0, 0, 0, 0), R(1, 1, 0, 0), R(0, 1, 1, 0), R(0, 0, 0, 0)>;
    using Z4_lines = OpLines<R(0, 1, 0, 0), R(1, 1, 0, 0), R(1, 0, 0, 0), R(0, 0, 0, 0)>;
    using op_Z1 = OpDesc<'Z', 0, Z1_lines, 0, 0, 1, 3, kOpRotateNone, JlstzKickR0CW, JlstzKickR0CCW>;
    using op_Z2 = OpDesc<'Z', 1, Z2_lines, 0, 0, 2, 0, kOpRotateNone, JlstzKickR1CW, JlstzKickR1CCW>;
    using op_Z3 = OpDesc<'Z', 2, Z3_lines, 0, 0, 3, 1, kOpRotateNone, JlstzKickR2CW, JlstzKickR2CCW>;
    using op_Z4 = OpDesc<'Z', 3, Z4_lines, 0, 0, 0, 2, kOpRotateNone, JlstzKickR3CW, JlstzKickR3CCW>;

    using L1_lines = OpLines<R(0, 0, 1, 0), R(1, 1, 1, 0), R(0, 0, 0, 0), R(0, 0, 0, 0)>;
    using L2_lines = OpLines<R(0, 1, 0, 0), R(0, 1, 0, 0), R(0, 1, 1, 0), R(0, 0, 0, 0)>;
    using L3_lines = OpLines<R(0, 0, 0, 0), R(1, 1, 1, 0), R(1, 0, 0, 0), R(0, 0, 0, 0)>;
    using L4_lines = OpLines<R(1, 1, 0, 0), R(0, 1, 0, 0), R(0, 1, 0, 0), R(0, 0, 0, 0)>;
    using op_L1 = OpDesc<'L', 0, L1_lines, 0, 0, 1, 3, kOpRotateNone, JlstzKickR0CW, JlstzKickR0CCW>;
    using op_L2 = OpDesc<'L', 1, L2_lines, 0, 0, 2, 0, kOpRotateNone, JlstzKickR1CW, JlstzKickR1CCW>;
    using op_L3 = OpDesc<'L', 2, L3_lines, 0, 0, 3, 1, kOpRotateNone, JlstzKickR2CW, JlstzKickR2CCW>;
    using op_L4 = OpDesc<'L', 3, L4_lines, 0, 0, 0, 2, kOpRotateNone, JlstzKickR3CW, JlstzKickR3CCW>;

    using J1_lines = OpLines<R(1, 0, 0, 0), R(1, 1, 1, 0), R(0, 0, 0, 0), R(0, 0, 0, 0)>;
    using J2_lines = OpLines<R(0, 1, 1, 0), R(0, 1, 0, 0), R(0, 1, 0, 0), R(0, 0, 0, 0)>;
    using J3_lines = OpLines<R(0, 0, 0, 0), R(1, 1, 1, 0), R(0, 0, 1, 0), R(0, 0, 0, 0)>;
    using J4_lines = OpLines<R(0, 1, 0, 0), R(0, 1, 0, 0), R(1, 1, 0, 0), R(0, 0, 0, 0)>;
    using op_J1 = OpDesc<'J', 0, J1_lines, 0, 0, 1, 3, kOpRotateNone, JlstzKickR0CW, JlstzKickR0CCW>;
    using op_J2 = OpDesc<'J', 1, J2_lines, 0, 0, 2, 0, kOpRotateNone, JlstzKickR1CW, JlstzKickR1CCW>;
    using op_J3 = OpDesc<'J', 2, J3_lines, 0, 0, 3, 1, kOpRotateNone, JlstzKickR2CW, JlstzKickR2CCW>;
    using op_J4 = OpDesc<'J', 3, J4_lines, 0, 0, 0, 2, kOpRotateNone, JlstzKickR3CW, JlstzKickR3CCW>;

    using T1_lines = OpLines<R(0, 1, 0, 0), R(1, 1, 1, 0), R(0, 0, 0, 0), R(0, 0, 0, 0)>;
    using T2_lines = OpLines<R(0, 1, 0, 0), R(0, 1, 1, 0), R(0, 1, 0, 0), R(0, 0, 0, 0)>;
    using T3_lines = OpLines<R(0, 0, 0, 0), R(1, 1, 1, 0), R(0, 1, 0, 0), R(0, 0, 0, 0)>;
    using T4_lines = OpLines<R(0, 1, 0, 0), R(1, 1, 0, 0), R(0, 1, 0, 0), R(0, 0, 0, 0)>;
    using op_T1 = OpDesc<'T', 0, T1_lines, 0, 0, 1, 3, kOpRotateNone, JlstzKickR0CW, JlstzKickR0CCW>;
    using op_T2 = OpDesc<'T', 1, T2_lines, 0, 0, 2, 0, kOpRotateNone, JlstzKickR1CW, JlstzKickR1CCW>;
    using op_T3 = OpDesc<'T', 2, T3_lines, 0, 0, 3, 1, kOpRotateNone, JlstzKickR2CW, JlstzKickR2CCW>;
    using op_T4 = OpDesc<'T', 3, T4_lines, 0, 0, 0, 2, kOpRotateNone, JlstzKickR3CW, JlstzKickR3CCW>;

#undef R

    using SrsRule = RuleSpec<10, 40, 4,
                             op_O1,
                             op_I1, op_I2, op_I3, op_I4,
                             op_S1, op_S2, op_S3, op_S4,
                             op_Z1, op_Z2, op_Z3, op_Z4,
                             op_L1, op_L2, op_L3, op_L4,
                             op_J1, op_J2, op_J3, op_J4,
                             op_T1, op_T2, op_T3, op_T4>;
} // namespace srs_minimal

namespace
{
    using Rule = srs_minimal::SrsRule;
    using BoardT = m_tetris2::Map<10, 40>;

    // Spawn 选择: cobra Gen::SPAWN_X = 4, RulesT::SPAWN_Y = 19. 但 cobra piece 锚点
    // 是 piece-center, 我们的 piece-local (0,0) 是 OpLines 最底行最左列. 不同方块基准
    // 列差距通过 BFS slow-init 自动消化 (沿 y 向上探), 因此这里只要选一个安全的
    // 高位 y 即可. 取 y = 20 (留 4 行空间), x = 3 (cobra SPAWN_X = 4 减 1, 因为我们的
    // 基准点偏左一格).
    constexpr int kSpawnX = 3;
    constexpr int kSpawnY = 20;

    template<char T>
    int dump_piece(bool enable_mini)
    {
        BoardT board{}; // 全空
        if (enable_mini && T == 'T')
        {
            using Hook = m_tetris2::DefaultTSpinHook;
            std::vector<m_tetris2::LandingPosT<Hook>> landings;
            landings.reserve(64);
            auto collect = [&](m_tetris2::LandingPosT<Hook> p)
            { landings.push_back(p); };
            m_tetris2::movegen::MoveGen<Rule, T, Hook>::generate(board, kSpawnX, kSpawnY, collect);
            std::sort(landings.begin(), landings.end(),
                      [](auto const &a, auto const &b)
                      {
                          if (a.r != b.r)
                              return a.r < b.r;
                          if (a.x != b.x)
                              return a.x < b.x;
                          return a.y < b.y;
                      });
            for (auto const &p : landings)
            {
                std::printf("%c,%u,%d,%d,%u\n",
                            T,
                            static_cast<unsigned>(p.r),
                            static_cast<int>(p.x),
                            static_cast<int>(p.y),
                            static_cast<unsigned>(p.extra.type));
            }
            return static_cast<int>(landings.size());
        }
        else
        {
            using Hook = m_tetris2::NoHook;
            std::vector<m_tetris2::LandingPosT<Hook>> landings;
            landings.reserve(64);
            auto collect = [&](m_tetris2::LandingPosT<Hook> p)
            { landings.push_back(p); };
            m_tetris2::movegen::MoveGen<Rule, T, Hook>::generate(board, kSpawnX, kSpawnY, collect);
            std::sort(landings.begin(), landings.end(),
                      [](auto const &a, auto const &b)
                      {
                          if (a.r != b.r)
                              return a.r < b.r;
                          if (a.x != b.x)
                              return a.x < b.x;
                          return a.y < b.y;
                      });
            for (auto const &p : landings)
            {
                //NoHook 路径无 spin 字段, 输出常 0 占位 (与原行为一致).
                std::printf("%c,%u,%d,%d,%u\n",
                            T,
                            static_cast<unsigned>(p.r),
                            static_cast<int>(p.x),
                            static_cast<int>(p.y),
                            0u);
            }
            return static_cast<int>(landings.size());
        }
    }
}

int main(int argc, char **argv)
{
    bool enable_mini = false;
    char piece = 0;
    if (argc < 2)
    {
        std::fprintf(stderr, "usage: %s <O|I|T|L|J|S|Z|all> [--mini]\n", argv[0]);
        return 1;
    }
    if (std::strlen(argv[1]) >= 1)
        piece = argv[1][0];
    for (int i = 2; i < argc; ++i)
        if (std::strcmp(argv[i], "--mini") == 0)
            enable_mini = true;

    auto run_one = [&](char p)
    {
        int n = 0;
        switch (p)
        {
        case 'O':
            n = dump_piece<'O'>(enable_mini);
            break;
        case 'I':
            n = dump_piece<'I'>(enable_mini);
            break;
        case 'T':
            n = dump_piece<'T'>(enable_mini);
            break;
        case 'L':
            n = dump_piece<'L'>(enable_mini);
            break;
        case 'J':
            n = dump_piece<'J'>(enable_mini);
            break;
        case 'S':
            n = dump_piece<'S'>(enable_mini);
            break;
        case 'Z':
            n = dump_piece<'Z'>(enable_mini);
            break;
        default:
            std::fprintf(stderr, "unknown piece: %c\n", p);
            return 0;
        }
        std::fprintf(stderr, "# %c total = %d\n", p, n);
        return n;
    };

    if (piece == 'a' /* "all" */)
    {
        const char *all = "OITLJSZ";
        int total = 0;
        for (const char *p = all; *p; ++p)
            total += run_one(*p);
        std::fprintf(stderr, "# grand total = %d\n", total);
    }
    else
    {
        run_one(piece);
    }
    return 0;
}
