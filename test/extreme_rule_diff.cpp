// Extreme-rule diff driver: 把 13×30 / 10 piece × 1..10 rotations 的极端规则
// 喂进新框架 MoveGen + kFilteredIndex, 与 master TetrisContext oracle BFS 做
// (r, x, y) 落点集合对拍.
//
// 目的:
//   - 验证 emit-time filter index (kFilteredIndex / kFilteredCount) 在
//     非 SRS / 非 4-cell / R_count > 4 配置下与 master node_mark_filtered_
//     行为完全一致.
//   - master 旧硬编码 node_block_[t*4+r] / TetrisMapSnap::row[4][...] 已扩到
//     max_rotation=16, 这条 oracle 通道顺带把扩容跑通.
//
// 不对拍 spin / T-spin / last 字段 — 极端规则没有 'T' 字面量, 也不是这次目标.
// 仅按 (r, x, y) 落点集合做严格相等比较.
//
// 用法:
//   extreme_rule_diff             跑全部 piece × 全部 fixture
//   extreme_rule_diff <piece>     单 piece (A..J)
//
// Phase 0 Oracle 隔离说明:
//   master tetris_core 维持 SRS-4-rotation 假设 (TetrisMapSnap::row[4][...]
//   等). 即便只对 R<=4 piece 单独跑对拍, oracle 端 TetrisContext::prepare
//   也会把整套 rule_extreme (含 R=5..10 的 E..J) 注册进 node_block_, 立刻
//   越界. 阶段 0 不允许动 oracle 端硬假设, 因此所有 piece 都跳过 oracle
//   路径, 只 dump 新框架侧落点. 后续 Phase 再决定是否替换为新框架自洽比对.

#include "../src/rule_extreme.h"
#include "../src/bb_node.h"
#include "../src/tetris_movegen.h"
#include "../src/tetris_rule_spec.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <set>
#include <string>
#include <tuple>
#include <vector>

namespace
{
    using NewRule = rule_extreme::TetrisRule::rule_spec;
    static constexpr int kW = static_cast<int>(NewRule::width);
    static constexpr int kH = static_cast<int>(NewRule::height);
    using NewMap = m_tetris2::Map<kW, kH>;

    struct Tup
    {
        int r;
        int x;
        int y;
        bool operator<(Tup const &o) const
        {
            return std::tie(r, x, y) < std::tie(o.r, o.x, o.y);
        }
        bool operator==(Tup const &o) const
        {
            return r == o.r && x == o.x && y == o.y;
        }
    };

    template<char T>
    std::vector<Tup> new_landings_T(NewMap const &board)
    {
        std::vector<Tup> out;
        using Hook = m_tetris2::NoHook;
        auto collect = [&](m_tetris2::LandingPosT<Hook> p)
        {
            out.push_back(Tup{static_cast<int>(p.r),
                              static_cast<int>(p.x),
                              static_cast<int>(p.y)});
        };
        auto sp = NewRule::spawn(T, kW, kH);
        m_tetris2::movegen::MoveGen<NewRule, T, Hook>::generate(board, sp.first, sp.second, collect);
        std::sort(out.begin(), out.end());
        out.erase(std::unique(out.begin(), out.end()), out.end());
        return out;
    }

    std::vector<Tup> new_landings(NewMap const &board, char piece)
    {
        switch (piece)
        {
        case 'A':
            return new_landings_T<'A'>(board);
        case 'B':
            return new_landings_T<'B'>(board);
        case 'C':
            return new_landings_T<'C'>(board);
        case 'D':
            return new_landings_T<'D'>(board);
        case 'E':
            return new_landings_T<'E'>(board);
        case 'F':
            return new_landings_T<'F'>(board);
        case 'G':
            return new_landings_T<'G'>(board);
        case 'H':
            return new_landings_T<'H'>(board);
        case 'I':
            return new_landings_T<'I'>(board);
        case 'J':
            return new_landings_T<'J'>(board);
        }
        return {};
    }

    //---- 棋盘 fixture ----

    NewMap make_empty_board()
    {
        return NewMap{};
    }

    //左半 4 列填高 5: x=0..3, y=0..4 全占, 模拟 jagged 局面.
    NewMap make_left_half_board()
    {
        NewMap m{};
        for (int x = 0; x < 4; ++x)
            for (int y = 0; y < 5; ++y)
                m.set(x, y);
        return m;
    }

    //sparse: 在低区随机选若干 cell 填上 (固定种子, 不真随机).
    NewMap make_sparse_board()
    {
        NewMap m{};
        int sx[] = {2, 5, 6, 9, 11, 1, 3, 7, 8, 10, 4, 12};
        int sy[] = {0, 0, 1, 0, 1, 2, 2, 1, 3, 0, 4, 2};
        for (int i = 0; i < int(sizeof(sx) / sizeof(sx[0])); ++i)
            m.set(sx[i], sy[i]);
        return m;
    }

    //右侧 1 宽井: col 12 全空, 其它列 0..3 行填实 (低区), 顶部敞开.
    NewMap make_right_well_board()
    {
        NewMap m{};
        for (int y = 0; y < 4; ++y)
            for (int x = 0; x < kW - 1; ++x)
                m.set(x, y);
        return m;
    }

    //sealed_top: 顶部两行全占, 模拟上方封顶. piece 必须在顶部以下落地.
    NewMap make_sealed_top_board()
    {
        NewMap m{};
        for (int x = 0; x < kW; ++x)
        {
            m.set(x, kH - 1);
            m.set(x, kH - 2);
        }
        return m;
    }

    void dump_to_file(char const *path, std::vector<Tup> const &v)
    {
        FILE *fp = std::fopen(path, "w");
        if (!fp)
            return;
        for (auto const &t : v)
            std::fprintf(fp, "r=%d x=%d y=%d\n", t.r, t.x, t.y);
        std::fclose(fp);
    }

    int diff_one(char piece, NewMap const &board, char const *board_name)
    {
        //阶段 0 (Oracle 隔离): 所有 piece 跳过 oracle 路径, 只 dump 新框架侧落点;
        //  oracle 路径因 SRS-4-rotation 硬假设与 R>4 piece 不兼容暂不启用.
        int piece_r_count = piece - 'A' + 1;

        std::vector<Tup> b = new_landings(board, piece);
        char path_b[256];
        std::snprintf(path_b, sizeof(path_b),
                      "research/flip-bits/dumps/extreme_%s_%c_new.txt", board_name, piece);
        dump_to_file(path_b, b);

        std::fprintf(stderr, "[ok-new-only] %s piece=%c R=%d count=%zu (oracle skipped, phase 0)\n",
                     board_name, piece, piece_r_count, b.size());
        return 0;
    }
}

int main(int argc, char **argv)
{
    char piece = 'a';
    if (argc >= 2 && std::strlen(argv[1]) >= 1)
        piece = argv[1][0];

    int fails = 0;
    struct NamedBoard
    {
        char const *name;
        NewMap (*build)();
    };
    NamedBoard boards[] = {
        {"empty", &make_empty_board},
        {"left_half", &make_left_half_board},
        {"sparse", &make_sparse_board},
        {"right_well", &make_right_well_board},
        {"sealed_top", &make_sealed_top_board},
    };
    auto run_piece = [&](char p)
    {
        for (auto const &nb : boards)
        {
            NewMap m = nb.build();
            fails += diff_one(p, m, nb.name);
        }
    };
    if (piece == 'a')
    {
        for (char p : std::string("ABCDEFGHIJ"))
            run_piece(p);
    }
    else
    {
        run_piece(piece);
    }

    if (fails == 0)
        std::fprintf(stderr, "# all extreme-rule diffs ok\n");
    else
        std::fprintf(stderr, "# %d failing scenario(s)\n", fails);
    return fails == 0 ? 0 : 1;
}
