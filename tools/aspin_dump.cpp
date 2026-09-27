// ASpin baseline dump driver: 跑 aspin::Search 在 rule_botris 上对一系列
// fixture board × 7 piece × {1g, 20g} 的输出 (status.x / y / r, type), 排序后
// 写到指定文件. commit 5 (DefaultASpinHook) 迁移前后跑两遍, byte-level diff
// 用来守 ASpin 行为不变.
//
// 用法: aspin_dump <out_file>

#include "../src/ai_zzz.h"
#include "../src/rule_botris.h"
#include "../src/search_aspin.h"
#include "../src/search_path.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <string>
#include <tuple>
#include <vector>

namespace
{
    // 位板极性: 1=occupied, 0=empty (与旧 TetrisMap 的 1=empty 相反)
    using NewMap = m_tetris2::TetrisMap<10, 40>;

    // 从 ascii 行 (bottom-up) 拼一张 TetrisMap<10,40>.
    // '#' / 非'.' 非' ' 字符 = 占用格 (bit=1).
    NewMap from_rows(std::initializer_list<char const *> rows_bottom_up)
    {
        NewMap m{};
        int y = 0;
        for (char const *row : rows_bottom_up)
        {
            std::uint32_t occ = 0;
            for (int x = 0; x < 10 && row[x] != '\0'; ++x)
            {
                char c = row[x];
                if (c != '.' && c != ' ')
                    occ |= (1u << x);
            }
            m.set_row(y, static_cast<NewMap::row_t>(occ));
            ++y;
        }
        return m;
    }

    // 几个常见 fixture (来自 oracle_diff 的现成 board, 这里抠 ASpin 关心的几张).
    NewMap make_empty()
    {
        return NewMap{};
    }

    NewMap make_donation()
    {
        // 与 oracle_diff::make_donation_board 等价.
        // y=0..13: col 0..8 占用 (col 9 空)
        // y=14:    col 0..6 占用
        // y=15:    col 0..3 占用
        // y=16:    col 0..1 占用
        NewMap m{};
        for (int y = 0; y < 14; ++y)
            m.set_row(y, static_cast<NewMap::row_t>((1u << 9) - 1u)); // col 0..8
        m.set_row(14, static_cast<NewMap::row_t>((1u << 7) - 1u));    // col 0..6
        m.set_row(15, static_cast<NewMap::row_t>((1u << 4) - 1u));    // col 0..3
        m.set_row(16, static_cast<NewMap::row_t>((1u << 2) - 1u));    // col 0..1
        return m;
    }

    NewMap make_pc_opener()
    {
        return from_rows({
            "###....###", // y=0
            "##......##", // y=1
            "#........#", // y=2
            "..........", // y=3
        });
    }

    NewMap make_sealed_top()
    {
        // y=18..39 全行填满，y=0..17 全空。
        NewMap m{};
        NewMap::row_t full_row = NewMap::row_full;
        for (int y = 18; y < 40; ++y)
            m.set_row(y, full_row);
        return m;
    }

    NewMap make_sspin()
    {
        return from_rows({
            "##########", // y=0
            "##.#######", // y=1
            "#..#######", // y=2
            "#.########", // y=3
            "##########", // y=4
            "..........", // y=5
        });
    }

    NewMap make_sz_wall_spin()
    {
        return from_rows({
            "##########", // y=0
            "..########", // y=1
            ".#######..", // y=2
            "##......##", // y=3
            "..........", // y=4
        });
    }

    NewMap make_opp_chamber()
    {
        return from_rows({
            "##########", // y=0
            "#........#", // y=1
            "#........#", // y=2
            "##########", // y=3
            "....#.....", // y=4
            "..........", // y=5
        });
    }

    NewMap make_bottom_pocket()
    {
        return from_rows({
            "###.######", // y=0
            "###.######", // y=1
            "###.######", // y=2
            "##...#####", // y=3
            "..........", // y=4
        });
    }

    // 紧贴左墙的"L 形 1 宽 4 深井": col 0 留竖直通道, 顶部 col 1 把口子封死.
    // I 块 R1 (竖向) 钻入后 4 个邻位都被阻塞 -> ASpin.
    NewMap make_aspin_lwell()
    {
        return from_rows({
            "##########", // y=0
            ".#########", // y=1  col 0 空
            ".#########", // y=2  col 0 空
            ".#########", // y=3  col 0 空
            ".#########", // y=4  col 0 空
            "##########", // y=5  顶盖
            "..........", // y=6
        });
    }

    // 顶盖封口的右侧 1 宽井: 镜像 lwell.
    NewMap make_aspin_rwell()
    {
        return from_rows({
            "##########", // y=0
            "#########.", // y=1
            "#########.", // y=2
            "#########.", // y=3
            "#########.", // y=4
            "##########", // y=5
            "..........", // y=6
        });
    }

    // 中间一个 1 宽 3 深口袋, 顶盖封死: 任意 piece R 朝向落入井底都四面被封.
    NewMap make_aspin_center_pocket()
    {
        return from_rows({
            "##########", // y=0
            "####.#####", // y=1
            "####.#####", // y=2
            "####.#####", // y=3
            "##########", // y=4  顶盖
            "..........", // y=5
        });
    }

    // 用户提供的 all-spin 综合场地: 一张 10x16 board, 同时具备 7 piece 各自的
    // wall-kick spin pocket. 原始 0/1 表 (1=占, 0=空), 上→下 16 行, 这里转成
    // y=0 (bottom) → y=15 (top) 的 ASCII.
    NewMap make_aspin_all_spin_board()
    {
        return from_rows({
            "####.#####", // y=0  原 row 15
            "####..####", // y=1
            "###....###", // y=2
            "#####..###", // y=3
            "##.#....##", // y=4
            "#......###", // y=5
            "##.....###", // y=6
            "###...####", // y=7
            "###....###", // y=8
            "####..####", // y=9
            "###....###", // y=10
            "#.##..##.#", // y=11
            "#.........", // y=12
            "#.........", // y=13
            "..........", // y=14
            "..........", // y=15 原 row 0 (top)
        });
    }

    using AspinSearcher = aspin::Search::type<rule_botris::TetrisRule>;

    AspinSearcher::Config &config()
    {
        static AspinSearcher::Config cfg{};
        return cfg;
    }

    AspinSearcher &search_obj()
    {
        static AspinSearcher s;
        static bool inited = []()
        {
            s.init(&config());
            return true;
        }();
        (void)inited;
        return s;
    }

    using Helpers = m_tetris2::Helpers<rule_botris::TetrisRule::rule_spec>;

    inline std::uint64_t cells_key_to_u64(m_tetris2::CellsKey const &k)
    {
        std::uint64_t v = 0;
        static_assert(sizeof(k) == sizeof(v), "CellsKey size mismatch");
        std::memcpy(&v, &k, sizeof(v));
        return v;
    }

    struct Tup
    {
        //index_filtered: cells_key 等价类索引, 既包含 R 等价折叠也唯一确定占据的
        //cell-mask -> row, 所以无需再单独存 row.
        std::uint64_t idx;
        int type; //ASpin / None.
        bool operator<(Tup const &o) const
        {
            return std::tie(idx, type) < std::tie(o.idx, o.type);
        }
        bool operator==(Tup const &o) const
        {
            return idx == o.idx && type == o.type;
        }
    };

    std::vector<Tup> dump_one(NewMap const &map, char piece)
    {
        std::vector<Tup> out;
        auto sp = rule_botris::TetrisRule::rule_spec::spawn(piece,
            rule_botris::TetrisRule::rule_spec::width,
            rule_botris::TetrisRule::rule_spec::height);
        auto spawn = Helpers::state_from_status(piece,
            static_cast<std::uint8_t>(0),
            sp.first, sp.second);
        auto *result = search_obj().search(map, spawn, 0);
        if (!result)
            return out;
        out.reserve(result->size());
        for (auto const &lp : *result)
        {
            if (lp.state.t == 0)
                continue;
            //index_filtered 在新框架与 master 完全等价 (extreme_rule_diff 已证),
            //同一 idx 唯一确定 piece 在棋盘上占据的 cell-mask, 因此不再额外记录
            //status.r / row -- idx + type 已是等价类的最小完备 key.
            out.push_back(Tup{cells_key_to_u64(Helpers::cells_key_for_state(lp.state)),
                              static_cast<int>(lp.type)});
        }
        std::sort(out.begin(), out.end());
        //commit 5: 对 (idx, type) 元组做唯一化, 把 master canonical-R 等价
        //R 的 alias 落点也压平, 让 baseline / after diff 直接 byte-level 对比.
        out.erase(std::unique(out.begin(), out.end()), out.end());
        return out;
    }
}

int main(int argc, char **argv)
{
    char const *out_path = argc >= 2 ? argv[1] : "aspin_dump.txt";
    std::FILE *fp = std::fopen(out_path, "w");
    if (!fp)
    {
        std::fprintf(stderr, "cannot open %s\n", out_path);
        return 2;
    }
    struct Named
    {
        char const *name;
        NewMap (*build)();
    };
    Named boards[] = {
        {"empty", &make_empty},
        {"donation", &make_donation},
        {"pc_opener", &make_pc_opener},
        {"sealed_top", &make_sealed_top},
        {"sspin", &make_sspin},
        {"sz_wall_spin", &make_sz_wall_spin},
        {"opp_chamber", &make_opp_chamber},
        {"bottom_pocket", &make_bottom_pocket},
        {"aspin_lwell", &make_aspin_lwell},
        {"aspin_rwell", &make_aspin_rwell},
        {"aspin_center_pocket", &make_aspin_center_pocket},
        {"aspin_all_spin", &make_aspin_all_spin_board},
    };
    char const *pieces = "OITLJSZ";
    for (bool is_20g : {false, true})
    {
        config().is_20g = is_20g;
        for (auto const &nb : boards)
        {
            NewMap m = nb.build();
            for (char const *p = pieces; *p != '\0'; ++p)
            {
                std::vector<Tup> tups = dump_one(m, *p);
                std::fprintf(fp, "# board=%s piece=%c flavor=%s count=%zu\n",
                             nb.name, *p, is_20g ? "20g" : "1g", tups.size());
                for (Tup const &t : tups)
                {
                    std::fprintf(fp, "  idx=%llu type=%d\n",
                                 static_cast<unsigned long long>(t.idx), t.type);
                }
            }
        }
    }
    std::fclose(fp);
    std::fprintf(stderr, "# aspin_dump -> %s ok\n", out_path);
    return 0;
}
