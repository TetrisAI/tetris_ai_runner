// tag_landpoint_collision_dump: one-off investigation tool to determine
// whether oracle/search_tag::Search produces multiple land-point cache entries
// that share index_filtered but differ in (is_last_rotate, is_ready). The
// output guides the dedup-key choice for the new BfsEngine-based tag search
// (single key vs composite key).

#include "../oracle/search_tag.h"
#define search_tspin search_tspin_oracle
#include "../oracle/search_tspin.h"
#undef search_tspin
#define rule_srs rule_srs_oracle
#include "../oracle/rule_srs.h"
#undef rule_srs
#include "../src/tetris_core.h"
#include "../oracle/ai_easy.h"
#include "../src/ai_zzz.h"
#include "../src/rule_srs.h"
#include "../src/search_tspin.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <map>
#include <set>
#include <string>
#include <tuple>
#include <vector>

namespace
{
    using OracleEngine = m_tetris::TetrisEngine<rule_srs_oracle::TetrisRule, ai_easy::AI, search_tspin_oracle::Search>;

    OracleEngine &oracle_engine()
    {
        static OracleEngine engine;
        static bool prepared = []()
        { return engine.prepare(10, 40); }();
        (void)prepared;
        return engine;
    }

    search_tag_oracle::Search &oracle_tag_search()
    {
        static search_tag_oracle::Search s;
        static bool inited = []()
        {
            m_tetris::TetrisContext const *ctx = oracle_engine().context().get();
            if (!ctx)
                return false;
            s.init(ctx);
            return true;
        }();
        (void)inited;
        return s;
    }

    struct BoardMask
    {
        uint64_t lo = 0;
        uint64_t hi[6] = {};
        void set(int x, int y)
        {
            int bit = y * 10 + x;
            if (bit < 64)
                lo |= (1ull << bit);
            else
                hi[(bit - 64) / 64] |= (1ull << ((bit - 64) % 64));
        }
        bool get(int x, int y) const
        {
            int bit = y * 10 + x;
            if (bit < 64)
                return (lo >> bit) & 1ull;
            return (hi[(bit - 64) / 64] >> ((bit - 64) % 64)) & 1ull;
        }
    };

    void build_oracle_map(BoardMask const &src, m_tetris::TetrisMap &dst)
    {
        dst = m_tetris::TetrisMap(10, 40);
        uint32_t empty_row = (1u << 10) - 1u;
        for (int y = 0; y < 40; ++y)
            dst.row[y] = empty_row;
        for (int y = 0; y < 40; ++y)
        {
            uint32_t occ = 0;
            for (int x = 0; x < 10; ++x)
                if (src.get(x, y))
                    occ |= (1u << x);
            dst.row[y] &= ~occ;
            if (occ != 0)
            {
                dst.roof = std::max(dst.roof, y + 1);
                for (int x = 0; x < 10; ++x)
                    if ((occ >> x) & 1)
                        dst.top[x] = std::max(dst.top[x], y + 1);
            }
            dst.count += __builtin_popcount(occ);
        }
    }

    BoardMask from_rows(std::initializer_list<char const *> rows)
    {
        BoardMask m{};
        int y = 0;
        for (auto *row : rows)
        {
            for (int x = 0; x < 10 && row[x]; ++x)
                if (row[x] == '#')
                    m.set(x, y);
            ++y;
        }
        return m;
    }

    BoardMask make_empty_board()
    {
        return BoardMask{};
    }
    BoardMask make_tst_triple_board()
    {
        return from_rows({
            "##########",
            "##.#######",
            "##.#######",
            "##..######",
            "..##......",
            "..........",
        });
    }
    BoardMask make_stsd_board()
    {
        return from_rows({
            "##########",
            "##########",
            "###.######",
            "##..######",
            "######.###",
            "#####..###",
            "#####.####",
            "..........",
        });
    }
    BoardMask make_sspin_board()
    {
        return from_rows({
            "##########",
            "##.#######",
            "#..#######",
            "#.########",
            "##########",
            "..........",
        });
    }
    BoardMask make_tsd_board()
    {
        return from_rows({
            "##########",
            "####.#####",
            "###...####",
            "..........",
        });
    }
    BoardMask make_tss_left_board()
    {
        return from_rows({
            "##########",
            ".#########",
            "..########",
            "#.########",
            "..........",
        });
    }
    BoardMask make_tss_right_board()
    {
        return from_rows({
            "##########",
            "#########.",
            "########..",
            "#########.",
            "..........",
        });
    }
    BoardMask make_donation_board()
    {
        BoardMask m{};
        for (int y = 0; y < 14; ++y)
            for (int x = 0; x < 9; ++x)
                m.set(x, y);
        for (int x = 0; x < 7; ++x)
            m.set(x, 14);
        for (int x = 0; x < 4; ++x)
            m.set(x, 15);
        m.set(0, 16);
        m.set(1, 16);
        return m;
    }
    BoardMask make_pc_opener_board()
    {
        return from_rows({
            "###....###",
            "##......##",
            "#........#",
            "..........",
        });
    }
    BoardMask make_sealed_top_board()
    {
        BoardMask m{};
        for (int y = 18; y < 40; ++y)
            for (int x = 0; x < 10; ++x)
                m.set(x, y);
        return m;
    }
    BoardMask make_i_well_left_board()
    {
        return from_rows({
            "##########",
            ".#########",
            ".#########",
            ".#########",
            ".#########",
            "..........",
        });
    }
    BoardMask make_i_well_right_board()
    {
        return from_rows({
            "##########",
            "#########.",
            "#########.",
            "#########.",
            "#########.",
            "..........",
        });
    }
    BoardMask make_t_kick_left_board()
    {
        return from_rows({
            "##########",
            "#.########",
            "#.########",
            "#..#######",
            "#.########",
            "##.#######",
            "..........",
        });
    }
    BoardMask make_j_left_well_board()
    {
        return from_rows({
            "##########",
            ".#########",
            ".#########",
            ".#########",
            "..########",
            "..........",
        });
    }
    BoardMask make_bottom_pocket_board()
    {
        return from_rows({
            "###.######",
            "###.######",
            "###.######",
            "##...#####",
            "..........",
        });
    }
    BoardMask make_sz_wall_spin_board()
    {
        return from_rows({
            "##########",
            "..########",
            ".#######..",
            "##......##",
            "..........",
        });
    }
    BoardMask make_opp_chamber_board()
    {
        return from_rows({
            "##########",
            "#........#",
            "#........#",
            "##########",
            "....#.....",
            "..........",
        });
    }

    struct PerPiece
    {
        size_t total = 0;
        size_t unique_index = 0;
        size_t collision_rotate = 0;
        size_t collision_ready = 0;
        size_t collision_both = 0;
    };

    struct CollisionExample
    {
        char piece;
        char const *board;
        char const *flavor;
        size_t index_filtered;
        std::vector<std::tuple<int, int, int, int, int>> entries;
    };

    void scan_one(char piece, BoardMask const &board, char const *board_name, bool is_20g,
                  PerPiece &agg, std::vector<CollisionExample> &examples)
    {
        m_tetris::TetrisContext const *ctx = oracle_engine().context().get();
        if (!ctx)
            return;
        m_tetris::TetrisMap map;
        build_oracle_map(board, map);
        auto spawn = ctx->generate(piece);
        if (!spawn)
            return;
        auto start = is_20g ? spawn->drop(map) : spawn;
        if (!start)
            return;

        auto *cache = oracle_tag_search().search(map, start, 0);
        if (!cache)
            return;

        using OracleLP = std::decay_t<decltype((*cache)[0])>;
        std::map<size_t, std::vector<OracleLP>> by_index;
        for (auto const &entry : *cache)
        {
            by_index[entry->index_filtered].push_back(entry);
        }

        agg.total += cache->size();
        agg.unique_index += by_index.size();

        for (auto const &kv : by_index)
        {
            auto const &group = kv.second;
            if (group.size() <= 1)
                continue;
            std::set<int> rotate_set;
            std::set<int> ready_set;
            std::set<std::pair<int, int>> both_set;
            for (auto const &e : group)
            {
                rotate_set.insert(e.is_last_rotate ? 1 : 0);
                ready_set.insert(e.is_ready ? 1 : 0);
                both_set.insert({e.is_last_rotate ? 1 : 0, e.is_ready ? 1 : 0});
            }
            bool diff_rotate = rotate_set.size() > 1;
            bool diff_ready = ready_set.size() > 1;
            bool diff_both = both_set.size() > 1;
            if (diff_rotate)
                ++agg.collision_rotate;
            if (diff_ready)
                ++agg.collision_ready;
            if (diff_both)
                ++agg.collision_both;
            if ((diff_rotate || diff_ready) && examples.size() < 12)
            {
                CollisionExample ex;
                ex.piece = piece;
                ex.board = board_name;
                ex.flavor = is_20g ? "20g" : "1g";
                ex.index_filtered = kv.first;
                for (auto const &e : group)
                {
                    ex.entries.emplace_back(e->status.r, e->status.x, e->status.y,
                                            e.is_last_rotate ? 1 : 0,
                                            e.is_ready ? 1 : 0);
                }
                examples.push_back(std::move(ex));
            }
        }
    }
} // namespace

int main()
{
    struct NamedBoard
    {
        char const *name;
        BoardMask (*build)();
    };
    NamedBoard boards[] = {
        {"empty", &make_empty_board},
        {"tst_triple", &make_tst_triple_board},
        {"stsd", &make_stsd_board},
        {"sspin", &make_sspin_board},
        {"tsd", &make_tsd_board},
        {"tss_left", &make_tss_left_board},
        {"tss_right", &make_tss_right_board},
        {"donation", &make_donation_board},
        {"pc_opener", &make_pc_opener_board},
        {"sealed_top", &make_sealed_top_board},
        {"i_well_left", &make_i_well_left_board},
        {"i_well_right", &make_i_well_right_board},
        {"t_kick_left", &make_t_kick_left_board},
        {"j_left_well", &make_j_left_well_board},
        {"bottom_pocket", &make_bottom_pocket_board},
        {"sz_wall_spin", &make_sz_wall_spin_board},
        {"opp_chamber", &make_opp_chamber_board},
    };
    char const *pieces = "OITLJSZ";

    std::map<char, PerPiece> per_piece;
    PerPiece global;
    std::vector<CollisionExample> examples;

    for (char p : std::string(pieces))
    {
        for (bool is_20g : {false, true})
        {
            for (auto const &nb : boards)
            {
                BoardMask m = nb.build();
                PerPiece local;
                scan_one(p, m, nb.name, is_20g, local, examples);
                global.total += local.total;
                global.unique_index += local.unique_index;
                global.collision_rotate += local.collision_rotate;
                global.collision_ready += local.collision_ready;
                global.collision_both += local.collision_both;
                auto &acc = per_piece[p];
                acc.total += local.total;
                acc.unique_index += local.unique_index;
                acc.collision_rotate += local.collision_rotate;
                acc.collision_ready += local.collision_ready;
                acc.collision_both += local.collision_both;
            }
        }
    }

    std::printf("PSM Summary:\n");
    std::printf("  total_land_points = %zu\n", global.total);
    std::printf("  unique_index_count = %zu\n", global.unique_index);
    std::printf("  collisions(same_index_filtered, different is_last_rotate) = %zu\n", global.collision_rotate);
    std::printf("  collisions(same_index_filtered, different is_ready) = %zu\n", global.collision_ready);
    std::printf("  collisions(same_index_filtered, different (is_last_rotate, is_ready)) = %zu\n", global.collision_both);
    std::printf("  per-piece breakdown:\n");
    for (char p : std::string(pieces))
    {
        auto const &acc = per_piece[p];
        std::printf("    piece=%c total=%zu unique=%zu coll_rot=%zu coll_ready=%zu coll_both=%zu\n",
                    p, acc.total, acc.unique_index, acc.collision_rotate, acc.collision_ready, acc.collision_both);
    }
    if (!examples.empty())
    {
        std::printf("Examples (up to %zu):\n", examples.size());
        for (auto const &ex : examples)
        {
            std::printf("  piece=%c board=%s flavor=%s index_filtered=%zu\n",
                        ex.piece, ex.board, ex.flavor, ex.index_filtered);
            for (auto const &t : ex.entries)
            {
                std::printf("    r=%d x=%d y=%d is_last_rotate=%d is_ready=%d\n",
                            std::get<0>(t), std::get<1>(t), std::get<2>(t),
                            std::get<3>(t), std::get<4>(t));
            }
        }
    }
    return 0;
}
