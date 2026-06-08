// _diff_harness.h — 七 driver 共享的对拍骨架.
//
// 二阶段比对协议 (用户 C5 调研后裁定):
//
//   Phase A — `oracle ⊆ candidate` (subset).
//     oracle.search() 与 candidate.search() 各拿一份 LandPoint 集. 用
//     (index_filtered, spin_type, last_idx) 作为 LpKey 去重 / 排序后, 严格
//     语义是 `set_difference(oracle, candidate)` 必须为空 — 即 candidate
//     不能漏吐 oracle 探到的任何 key. candidate-only (即候选多吐 oracle 未
//     吐的 key) 仅作为 [INFO] 行列出, 不计 fail. 这条 subset 协议出自
//     C5 阶段对 oracle/search_path.cpp:187 起 fast-path 的 phase_a_truth
//     调研: oracle fast-path 在 BFS 邻居上加 `!X->open(map)` 过滤, 强制
//     中间节点必须 supported (即 hard-drop only), 因此漏吐 tuck 落点;
//     candidate (位板 PathStrategy / SimulateStrategy / TSpinStrategy /
//     TagStrategy) 走完整 BFS 可达性, 包含全部 tuck 落点. 这些 tuck 落点
//     在游戏规则下严格可达 — 实战 master engine 上层一般也不消费它们,
//     所以 oracle "漏吐" 不算 bug, 只是结果集语义比 candidate 窄. 用户
//     裁定 (C5): 接受 candidate ⊋ oracle, 不要求反向. 详见
//     `.research/flip-bits-cleanup/phase_a_truth.md`.
//
//   Phase B — make_path byte-equal (尾部 D/d 剥离后).
//     仅对 Phase A 双方都吐了同一 key 的交集 key 跑. 用户原话: "make_path
//     末尾的 D/d 都直接 pop 掉, 这是等价的, 我们不在乎." — 比对前对两侧
//     输出做相同的尾部 D/d 剥离 (规范化), 然后再字节比对. 这一步消除
//     candidate make_path 在 oracle 走 "隐式 hard-drop 命中谓词" 时多 emit
//     的末尾 'D' / 'd' 系统差.
//
//   driver 的 fail = Phase A fail (subset 违反) + Phase B fail (规范化后
//   仍不等). 严格 driver 任一非零则 exit 1; 非严格 driver (simple_diff)
//   仅打 stats.
//
// 棋面 fixture: 18 个命名 + 16 个 splitmix64 固定 seed 随机 = 34. Fixtures
// 从旧 oracle_diff / aspin_dump 收敛而来. ASpin "all-spin" 综合棋面在
// make_aspin_all_spin_board() 嵌入 (来源 commit ef582c3, 由用户提供).

#ifndef TETRIS_AI_RUNNER_TESTS_DIFF_HARNESS_H_
#define TETRIS_AI_RUNNER_TESTS_DIFF_HARNESS_H_

#include "../oracle/tetris_core.h"
#include "../src/tetris_core.h"
#include "../src/tetris_map.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <initializer_list>
#include <set>
#include <string>
#include <tuple>
#include <vector>

namespace tetris_diff
{
    //=======================================================================
    // NewMap: 与 oracle_diff 历史选型一致, 10 列 × 40 行 (master 端 spawn 高度
    //   与 SRS 下落空间). 所有 driver 共用同一棋面尺寸.
    //=======================================================================
    using NewMap = m_tetris2::Map<10, 40>;
    static constexpr int kBoardW = 10;
    static constexpr int kBoardH = 40;

    //=======================================================================
    // LpKey: search 集合的等价类 id.
    //
    //   cells_key      : bb::CellsKey (4×uint16 格子坐标) 的小端 memcpy 打包
    //                    值, 作为落点几何等价类 id. 直接从 BBState 或 oracle
    //                    坐标算, 不经 TetrisNode 指针或 state_to_node.
    //   spin_type      : NoSpin 路径恒 0; TSpin 三态 (None=0/TSpin=1/Mini=2);
    //                    ASpin 二态 (None=0/ASpin=1); tag 二态 (None=0/Spin=1).
    //                    各 driver 自行约定语义并填入.
    //   last_idx       : TSpin 路径下 lp.last 的 cells_key 打包值; 否则 -1
    //                    (当 last 不参与比对时固定为 -1 填入).
    //=======================================================================
    struct LpKey
    {
        std::uint64_t index_filtered;
        int spin_type;
        int last_idx;

        bool operator<(LpKey const &o) const
        {
            return std::tie(index_filtered, spin_type, last_idx) <
                   std::tie(o.index_filtered, o.spin_type, o.last_idx);
        }
        bool operator==(LpKey const &o) const
        {
            return index_filtered == o.index_filtered && spin_type == o.spin_type &&
                   last_idx == o.last_idx;
        }
    };

    inline std::string format_lpkey(LpKey const &k)
    {
        char buf[96];
        std::snprintf(buf, sizeof(buf), "idx=%llu spin=%d last=%d",
                      static_cast<unsigned long long>(k.index_filtered),
                      k.spin_type, k.last_idx);
        return buf;
    }

    //=======================================================================
    // LpEntry: driver 把 oracle / candidate search 输出折叠成 LpEntry 列表.
    //   key    : 上面的等价类 id, 集合比对单位.
    //   index  : driver 内部的私有索引 (例如指向 vector<NodeWithSpin> 的 idx),
    //            供 Phase B make_path 回调反查代表元. 不参与比对.
    //   repr   : 可选的人读字符串 — driver 选填 (例如 "r=0,x=4,y=7,open=0"),
    //            harness 在 phase_a fail / [INFO] 行打印时用它给 candidate-only /
    //            oracle-only 的 key 配上具体几何位置, 便于 phase_a_truth 调研.
    //            为空时 harness 仅打 idx/spin/last 不打几何.
    //=======================================================================
    struct LpEntry
    {
        LpKey key;
        std::size_t index;
        std::string repr;
    };

    //=======================================================================
    // Fixture: 命名棋面 + 固定 seed 随机棋面.
    //=======================================================================
    struct Fixture
    {
        std::string name;
        NewMap board;
    };

    //--- ascii-row 构造 (bottom-up) ---
    inline NewMap from_rows(std::initializer_list<char const *> rows_bottom_up)
    {
        NewMap m{};
        int y = 0;
        for (char const *row : rows_bottom_up)
        {
            for (int x = 0; x < kBoardW && row[x] != '\0'; ++x)
            {
                char c = row[x];
                if (c != '.' && c != ' ')
                    m.set(x, y);
            }
            ++y;
        }
        return m;
    }

    //--- 命名棋面 (从 oracle_diff.cpp 等价搬来) ---
    inline NewMap make_empty()
    {
        return NewMap{};
    }

    inline NewMap make_tst_board()
    {
        NewMap m{};
        for (int x = 0; x < kBoardW; ++x)
        {
            m.set(x, 0);
            m.set(x, 1);
        }
        m.clear(4, 0);
        m.clear(4, 1);
        m.clear(3, 1);
        return m;
    }

    inline NewMap make_tst_triple_board()
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

    inline NewMap make_stsd_board()
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

    inline NewMap make_sspin_board()
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

    inline NewMap make_tsd_board()
    {
        return from_rows({
            "##########",
            "####.#####",
            "###...####",
            "..........",
        });
    }

    inline NewMap make_tss_left_board()
    {
        return from_rows({
            "##########",
            ".#########",
            "..########",
            "#.########",
            "..........",
        });
    }

    inline NewMap make_tss_right_board()
    {
        return from_rows({
            "##########",
            "#########.",
            "########..",
            "#########.",
            "..........",
        });
    }

    inline NewMap make_donation_board()
    {
        NewMap m{};
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

    inline NewMap make_pc_opener_board()
    {
        return from_rows({
            "###....###",
            "##......##",
            "#........#",
            "..........",
        });
    }

    inline NewMap make_sealed_top_board()
    {
        NewMap m{};
        for (int y = 18; y < 40; ++y)
            for (int x = 0; x < kBoardW; ++x)
                m.set(x, y);
        return m;
    }

    inline NewMap make_i_well_left_board()
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

    inline NewMap make_i_well_right_board()
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

    inline NewMap make_t_kick_left_board()
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

    inline NewMap make_j_left_well_board()
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

    inline NewMap make_bottom_pocket_board()
    {
        return from_rows({
            "###.######",
            "###.######",
            "###.######",
            "##...#####",
            "..........",
        });
    }

    inline NewMap make_sz_wall_spin_board()
    {
        return from_rows({
            "##########",
            "..########",
            ".#######..",
            "##......##",
            "..........",
        });
    }

    inline NewMap make_opp_chamber_board()
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

    //=======================================================================
    // ASpin 关键 fixture (来自 ef582c3 commit, 由用户提供):
    //   `aspin_all_spin_board` 一张同时具备 7 piece 各自 wall-kick spin
    //   pocket 的综合场地. 原始来源 tools/aspin_dump.cpp::make_aspin_all_spin_board
    //   (用户注释 "用户提供的 all-spin 综合场地").
    //=======================================================================
    inline NewMap make_aspin_all_spin_board()
    {
        return from_rows({
            "####.#####", // y=0  原 row 15 (bottom)
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

    inline NewMap make_aspin_lwell_board()
    {
        return from_rows({
            "##########",
            ".#########",
            ".#########",
            ".#########",
            ".#########",
            "##########",
            "..........",
        });
    }

    inline NewMap make_aspin_rwell_board()
    {
        return from_rows({
            "##########",
            "#########.",
            "#########.",
            "#########.",
            "#########.",
            "##########",
            "..........",
        });
    }

    inline NewMap make_aspin_center_pocket_board()
    {
        return from_rows({
            "##########",
            "####.#####",
            "####.#####",
            "####.#####",
            "##########",
            "..........",
        });
    }

    //=======================================================================
    // 固定 seed 随机棋面. splitmix64 滚动种子, 与 cstdlib 的 rand() 完全无关.
    //=======================================================================
    inline std::uint64_t splitmix64(std::uint64_t &state)
    {
        state += 0x9E3779B97F4A7C15ull;
        std::uint64_t z = state;
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
        return z ^ (z >> 31);
    }

    inline NewMap make_random_board(std::uint64_t seed)
    {
        NewMap m{};
        std::uint64_t s = seed ^ 0xA5A5A5A5DEADBEEFull;
        int height = 3 + static_cast<int>(splitmix64(s) % 10); // 3..12
        for (int y = 0; y < height; ++y)
        {
            std::uint64_t r = splitmix64(s);
            std::uint32_t mask = 0;
            for (int x = 0; x < kBoardW; ++x)
            {
                if (((r >> x) & 1) && (((r >> (x + 16)) & 1) || ((r >> (x + 32)) & 0)))
                    mask |= (1u << x);
            }
            std::uint64_t r2 = splitmix64(s);
            for (int x = 0; x < kBoardW; ++x)
            {
                if (((r2 >> x) & 0x1) == 0)
                    mask &= ~(1u << x);
            }
            if (mask == ((1u << kBoardW) - 1))
                mask &= ~(1u << static_cast<int>(splitmix64(s) % kBoardW));
            for (int x = 0; x < kBoardW; ++x)
                if ((mask >> x) & 1)
                    m.set(x, y);
        }
        return m;
    }

    inline std::vector<Fixture> build_named_fixtures()
    {
        std::vector<Fixture> v;
        v.push_back({"empty", make_empty()});
        v.push_back({"tst", make_tst_board()});
        v.push_back({"tst_triple", make_tst_triple_board()});
        v.push_back({"stsd", make_stsd_board()});
        v.push_back({"sspin", make_sspin_board()});
        v.push_back({"tsd", make_tsd_board()});
        v.push_back({"tss_left", make_tss_left_board()});
        v.push_back({"tss_right", make_tss_right_board()});
        v.push_back({"donation", make_donation_board()});
        v.push_back({"pc_opener", make_pc_opener_board()});
        v.push_back({"sealed_top", make_sealed_top_board()});
        v.push_back({"i_well_left", make_i_well_left_board()});
        v.push_back({"i_well_right", make_i_well_right_board()});
        v.push_back({"t_kick_left", make_t_kick_left_board()});
        v.push_back({"j_left_well", make_j_left_well_board()});
        v.push_back({"bottom_pocket", make_bottom_pocket_board()});
        v.push_back({"sz_wall_spin", make_sz_wall_spin_board()});
        v.push_back({"opp_chamber", make_opp_chamber_board()});
        return v;
    }

    inline std::vector<Fixture> build_aspin_named_fixtures()
    {
        //ASpin driver 专属命名 fixture: 主集 18 个 + lwell/rwell/center_pocket
        //+ 用户提供的 all_spin 综合棋面.
        std::vector<Fixture> v = build_named_fixtures();
        v.push_back({"aspin_lwell", make_aspin_lwell_board()});
        v.push_back({"aspin_rwell", make_aspin_rwell_board()});
        v.push_back({"aspin_center_pocket", make_aspin_center_pocket_board()});
        v.push_back({"aspin_all_spin", make_aspin_all_spin_board()});
        return v;
    }

    inline std::vector<Fixture> build_random_fixtures(int count, std::uint64_t base_seed)
    {
        std::vector<Fixture> v;
        v.reserve(count);
        for (int i = 0; i < count; ++i)
        {
            std::uint64_t s = base_seed + static_cast<std::uint64_t>(i) * 0x100000001B3ull;
            char name[32];
            std::snprintf(name, sizeof(name), "rand_%03d", i);
            v.push_back({name, make_random_board(s)});
        }
        return v;
    }

    inline std::vector<Fixture> build_all_fixtures()
    {
        std::vector<Fixture> named = build_named_fixtures();
        std::vector<Fixture> rnd = build_random_fixtures(16, 0xC0FFEE0420250601ull);
        named.insert(named.end(), rnd.begin(), rnd.end());
        return named;
    }

    inline std::vector<Fixture> build_all_aspin_fixtures()
    {
        std::vector<Fixture> named = build_aspin_named_fixtures();
        std::vector<Fixture> rnd = build_random_fixtures(16, 0xC0FFEE0420250601ull);
        named.insert(named.end(), rnd.begin(), rnd.end());
        return named;
    }

    //=======================================================================
    // xfail 列表: 字符串 set + 简易尾部 '*' 通配. 命中 → 该 case 不计 fail
    //   count, 仅打 [XFAIL] 标记到 stderr.
    //
    //   xfail key 粒度: phase=A|B + piece=X + board=Y + mode=1g|20g.
    //   driver 可选择性地豁免 phase A 或 phase B (举例: tag 17 个 J 边界
    //   path-string fail 是已知历史 fail, 只豁免 phase=B 那一支即可).
    //=======================================================================
    struct XfailSet
    {
        std::vector<std::string> patterns;

        void add(std::string p)
        {
            patterns.push_back(std::move(p));
        }

        bool match(std::string const &key) const
        {
            for (std::string const &p : patterns)
            {
                if (!p.empty() && p.back() == '*')
                {
                    std::string prefix = p.substr(0, p.size() - 1);
                    if (key.compare(0, prefix.size(), prefix) == 0)
                        return true;
                }
                else if (p == key)
                {
                    return true;
                }
            }
            return false;
        }
    };

    inline std::string make_case_key(char const *phase, char piece,
                                     std::string const &board_name, bool is_20g)
    {
        char buf[192];
        std::snprintf(buf, sizeof(buf), "phase=%s,piece=%c,board=%s,mode=%s",
                      phase, piece, board_name.c_str(), is_20g ? "20g" : "1g");
        return buf;
    }

    //=======================================================================
    // CaseProbe: driver 把"一个 (piece, board, flavor) 上 oracle 与 candidate
    //  各自的 search 输出 + 各自的 make_path 闭包" 包成 CaseProbe 交给 harness.
    //
    //  oracle_lps / candidate_lps 必须是 LpKey 单值 sorted/unique 后的 set,
    //  driver 在 build 时调 sort + unique. 内嵌的 index 字段是 driver 内部
    //  私有 vector 下标, harness 不动它, 只在 phase B 把它原样喂回 make_path
    //  闭包.
    //
    //  oracle_make_path / candidate_make_path : (LpEntry) -> std::string. 二
    //  者的输出在 phase B byte-equal 比对.
    //=======================================================================
    struct CaseProbe
    {
        std::vector<LpEntry> oracle_lps;
        std::vector<LpEntry> candidate_lps;
        std::function<std::string(LpEntry const &)> oracle_make_path;
        std::function<std::string(LpEntry const &)> candidate_make_path;
    };

    using ProbeFn = std::function<CaseProbe(char piece,
                                            NewMap const &board,
                                            std::string const &board_name,
                                            bool is_20g)>;
    using FlavorFn = std::function<void(bool is_20g)>;

    struct DiffStats
    {
        int total = 0;
        int phase_a_ok = 0;
        int phase_a_fail = 0;
        int phase_a_xfail = 0;
        int phase_a_info = 0; // candidate-only > 0 但 oracle ⊆ candidate (subset 协议下不算 fail)
        int phase_b_ok = 0;
        int phase_b_fail = 0;
        int phase_b_xfail = 0;
        int phase_b_drift = 0; // phase B 路径串差但 driver 关 phase_b_strict (语义等价路径选择差)
    };

    //=======================================================================
    // PhaseBPolicy: driver 级 phase B 策略.
    //   * Strict — 路径串 mismatch 计入 phase_b_fail; xfail 通配仍生效.
    //   * IgnoreDrift — 路径串 mismatch 当作 drift, 计入 phase_b_drift,
    //     不算 fail. 前提是 Phase A subset 已严格成立 (oracle ⊆ candidate),
    //     且 strip_trailing_dD 后仍不一致 — 这种残留是 BFS 邻居枚举顺序差
    //     (oracle vs 位板) 引起的"语义等价路径选择差异", 非 BUG.
    //
    //   IgnoreDrift 的具体证据见 .research/flip-bits-cleanup/phase_a_truth.md
    //   §6 与 .research/flip-bits-cleanup/c5_followup.md.
    //
    //   tag_diff 保留 Strict — 历史 17 个 J 边界 fail (kick chain 维度) 在
    //   tag 路径上是真路径差异, 必须用具体 J×board xfail 列表豁免, 不能用
    //   IgnoreDrift 一锅吞掉 (会丢失对未来 J 路径回归的检测).
    //=======================================================================
    enum class PhaseBPolicy
    {
        Strict,
        IgnoreDrift,
    };

    //=======================================================================
    // 把 NewMap 拷贝成 master TetrisMap.
    //   master TetrisMap.row 极性: 1=空, 0=占; 棋盘外的 bit 必须填 0.
    //=======================================================================
    template<class TetrisMap>
    inline void copy_to_oracle_map_impl(NewMap const &src, TetrisMap &dst)
    {
        dst = TetrisMap(kBoardW, kBoardH);
        std::uint32_t empty_row = (1u << kBoardW) - 1u;
        for (int y = 0; y < kBoardH; ++y)
            dst.row[y] = empty_row;
        for (int y = 0; y < kBoardH; ++y)
        {
            std::uint32_t occ = 0;
            for (int x = 0; x < kBoardW; ++x)
                if (src.get(x, y))
                    occ |= (1u << x);
            dst.row[y] &= ~occ;
            if (occ != 0)
            {
                dst.roof = std::max(dst.roof, y + 1);
                for (int x = 0; x < kBoardW; ++x)
                    if ((occ >> x) & 1)
                        dst.top[x] = std::max(dst.top[x], y + 1);
            }
            dst.count += __builtin_popcount(occ);
        }
    }

    inline void copy_to_oracle_map(NewMap const &src, m_tetris::TetrisMap &dst)
    {
        copy_to_oracle_map_impl(src, dst);
    }

    inline void copy_to_oracle_map(NewMap const &src, m_tetris2::TetrisMap &dst)
    {
        copy_to_oracle_map_impl(src, dst);
    }

    //=======================================================================
    // 简化的 unified-diff 工具: phase A 的 LpKey set 差异 + phase B 的字符串
    //   差异. 都仅 stderr 输出, 上下文 ±3.
    //=======================================================================
    inline void print_lpkey_set_diff(FILE *fp, char const *header,
                                     std::vector<LpEntry> const &oracle_entries,
                                     std::vector<LpEntry> const &candidate_entries,
                                     bool oracle_only_is_fail,
                                     bool candidate_only_is_info)
    {
        std::fprintf(fp, "  --- oracle: %s\n", header);
        std::fprintf(fp, "  +++ candidate: %s\n", header);
        std::vector<LpKey> oracle_keys;
        oracle_keys.reserve(oracle_entries.size());
        for (LpEntry const &e : oracle_entries)
            oracle_keys.push_back(e.key);
        std::vector<LpKey> candidate_keys;
        candidate_keys.reserve(candidate_entries.size());
        for (LpEntry const &e : candidate_entries)
            candidate_keys.push_back(e.key);
        std::vector<LpKey> only_a;
        std::vector<LpKey> only_b;
        std::set_difference(oracle_keys.begin(), oracle_keys.end(),
                            candidate_keys.begin(), candidate_keys.end(),
                            std::back_inserter(only_a));
        std::set_difference(candidate_keys.begin(), candidate_keys.end(),
                            oracle_keys.begin(), oracle_keys.end(),
                            std::back_inserter(only_b));
        std::fprintf(fp, "  oracle-only=%zu candidate-only=%zu\n",
                     only_a.size(), only_b.size());
        auto find_repr = [](std::vector<LpEntry> const &v, LpKey const &k) -> std::string
        {
            for (LpEntry const &e : v)
                if (e.key == k)
                    return e.repr;
            return std::string();
        };
        char const *oracle_only_tag = oracle_only_is_fail ? "  - " : "  ! ";
        char const *cand_only_tag = candidate_only_is_info ? "  i " : "  + ";
        std::size_t shown = 0;
        for (LpKey const &k : only_a)
        {
            if (shown++ >= 16)
            {
                std::fprintf(fp, "%s... (truncated)\n", oracle_only_tag);
                break;
            }
            std::string repr = find_repr(oracle_entries, k);
            if (repr.empty())
                std::fprintf(fp, "%s%s\n", oracle_only_tag, format_lpkey(k).c_str());
            else
                std::fprintf(fp, "%s%s %s\n", oracle_only_tag,
                             format_lpkey(k).c_str(), repr.c_str());
        }
        shown = 0;
        for (LpKey const &k : only_b)
        {
            if (shown++ >= 16)
            {
                std::fprintf(fp, "%s... (truncated)\n", cand_only_tag);
                break;
            }
            std::string repr = find_repr(candidate_entries, k);
            if (repr.empty())
                std::fprintf(fp, "%s%s\n", cand_only_tag, format_lpkey(k).c_str());
            else
                std::fprintf(fp, "%s%s %s\n", cand_only_tag,
                             format_lpkey(k).c_str(), repr.c_str());
        }
    }

    inline std::string escape_path_chars(std::string const &s)
    {
        std::string out;
        out.reserve(s.size());
        for (char c : s)
        {
            if (c >= 0x20 && c < 0x7F)
                out.push_back(c);
            else
            {
                char buf[8];
                std::snprintf(buf, sizeof(buf), "\\x%02x", static_cast<unsigned>(c) & 0xFFu);
                out += buf;
            }
        }
        return out;
    }

    inline void print_make_path_diff(FILE *fp, char const *header, LpKey const &key,
                                     std::string const &oracle_path,
                                     std::string const &candidate_path)
    {
        std::fprintf(fp, "  [path-mismatch] %s key=%s\n", header, format_lpkey(key).c_str());
        std::fprintf(fp, "    - oracle    (%zu): \"%s\"\n",
                     oracle_path.size(), escape_path_chars(oracle_path).c_str());
        std::fprintf(fp, "    + candidate (%zu): \"%s\"\n",
                     candidate_path.size(), escape_path_chars(candidate_path).c_str());
    }

    //=======================================================================
    // strip_trailing_dD: 用户原话 (C5): "make_path 末尾的 D/d 都直接 pop 掉,
    //   这是等价的, 我们不在乎." — Phase B byte-equal 比对前对两侧做相同
    //   规范化. oracle/search_*::make_path 用"隐式 hard-drop 命中谓词" 在
    //   BFS 边上一旦发现 node->X->drop(map) 落在目标 cells_key 即立刻返回,
    //   所以 oracle path 不带最后一下 'D' (master engine 上层会无脑 drop);
    //   candidate (位板 PathStrategy / SimulateStrategy) 用 cells_key 严格
    //   相等命中, 'D' 是真实图边, 故 path 经常多一个 'D' 后缀 (有时多 'd'
    //   再 'D'). 剥离后语义等价.
    //=======================================================================
    inline std::string strip_trailing_dD(std::string s)
    {
        while (!s.empty() && (s.back() == 'D' || s.back() == 'd'))
            s.pop_back();
        return s;
    }

    //=======================================================================
    // run_diff_main: 主编排.
    //
    //   driver 提供 ProbeFn (单 case 的 oracle / candidate 双向 search +
    //   make_path 回调). harness 跑 fixtures × pieces × flavors, 对每个 case:
    //     1. probe(...) 拿到 CaseProbe.
    //     2. Phase A: 比 oracle_lps.key 与 candidate_lps.key 的 sorted set.
    //     3. Phase A pass 才走 Phase B: 对每个 key, 在 oracle_lps 与
    //        candidate_lps 各拿一个 LpEntry (取首个 match), 跑 make_path 闭包,
    //        byte-equal 比对.
    //     4. xfail 命中标 [XFAIL], 不计 fail.
    //   严格 driver fail>0 → exit 1.
    //=======================================================================
    inline int run_diff_main(char const *driver_name,
                             char const *pieces,
                             std::vector<Fixture> const &fixtures,
                             std::initializer_list<bool> flavors,
                             FlavorFn const &set_flavor,
                             ProbeFn const &probe,
                             XfailSet const &xfail,
                             bool strict,
                             PhaseBPolicy phase_b_policy = PhaseBPolicy::Strict)
    {
        DiffStats stats;
        std::fprintf(stderr,
                     "# driver=%s fixtures=%zu pieces=%s strict=%d\n",
                     driver_name, fixtures.size(), pieces, strict ? 1 : 0);
        for (bool is_20g : flavors)
        {
            if (set_flavor)
                set_flavor(is_20g);
            char const *flavor = is_20g ? "20g" : "1g";
            std::fprintf(stderr, "# flavor=%s\n", flavor);
            for (Fixture const &fx : fixtures)
            {
                for (char const *p = pieces; *p != '\0'; ++p)
                {
                    char piece = *p;
                    ++stats.total;
                    CaseProbe cp = probe(piece, fx.board, fx.name, is_20g);

                    //commit C4-followup: oracle / candidate 各自的 lp 列表
                    //  必须 sort+unique 才能 set diff. driver 通常返回未 dedup
                    //  的列表, 这里 harness 兜底.
                    auto sort_unique = [](std::vector<LpEntry> &v)
                    {
                        std::sort(v.begin(), v.end(),
                                  [](LpEntry const &a, LpEntry const &b)
                                  { return a.key < b.key; });
                        v.erase(std::unique(v.begin(), v.end(),
                                            [](LpEntry const &a, LpEntry const &b)
                                            { return a.key == b.key; }),
                                v.end());
                    };
                    sort_unique(cp.oracle_lps);
                    sort_unique(cp.candidate_lps);

                    std::vector<LpKey> oracle_keys;
                    oracle_keys.reserve(cp.oracle_lps.size());
                    for (LpEntry const &e : cp.oracle_lps)
                        oracle_keys.push_back(e.key);
                    std::vector<LpKey> candidate_keys;
                    candidate_keys.reserve(cp.candidate_lps.size());
                    for (LpEntry const &e : cp.candidate_lps)
                        candidate_keys.push_back(e.key);

                    //=== Phase A: oracle ⊆ candidate (subset) ===
                    //  C5 调研后 (.research/flip-bits-cleanup/phase_a_truth.md):
                    //  oracle/search_path.cpp:187 起 fast-path 在 BFS 邻居加
                    //  `!X->open(map)` 过滤 (强制 supported 中间节点),
                    //  漏吐 tuck 落点; candidate 走完整 BFS 含 tuck. 这些
                    //  tuck 落点在游戏规则下严格可达, oracle 漏吐不算 bug,
                    //  仅是 result-set 语义比 candidate 窄. 用户裁定: 接受
                    //  candidate ⊋ oracle, candidate-only 仅作 [INFO]. 反向
                    //  违反 (即 candidate 漏吐 oracle 探到的 key) 视为真
                    //  fail — candidate BFS 邻居枚举出问题, 必须停下来修.
                    std::vector<LpKey> oracle_only_keys;
                    std::set_difference(oracle_keys.begin(), oracle_keys.end(),
                                        candidate_keys.begin(), candidate_keys.end(),
                                        std::back_inserter(oracle_only_keys));
                    std::vector<LpKey> candidate_only_keys;
                    std::set_difference(candidate_keys.begin(), candidate_keys.end(),
                                        oracle_keys.begin(), oracle_keys.end(),
                                        std::back_inserter(candidate_only_keys));
                    bool subset_ok = oracle_only_keys.empty();
                    bool exact_eq = subset_ok && candidate_only_keys.empty();
                    std::string key_a = make_case_key("A", piece, fx.name, is_20g);
                    if (exact_eq)
                    {
                        ++stats.phase_a_ok;
                    }
                    else if (subset_ok)
                    {
                        //candidate ⊋ oracle: 多吐 tuck 不算 fail, 但仍输出
                        //  [INFO] 行让人 review. phase_a_ok 仍 +1 (subset 成立).
                        //  TETRIS_DIFF_VERBOSE_INFO 环境变量打开时, 把
                        //  candidate-only 的具体几何 (LpEntry::repr) 也展开,
                        //  用于 phase_a_truth 调研.
                        ++stats.phase_a_ok;
                        ++stats.phase_a_info;
                        std::fprintf(stderr,
                                     "[INFO] %s %s oracle=%zu candidate=%zu (oracle ⊆ candidate, candidate-only=%zu)\n",
                                     driver_name, key_a.c_str(),
                                     oracle_keys.size(), candidate_keys.size(),
                                     candidate_only_keys.size());
                        if (std::getenv("TETRIS_DIFF_VERBOSE_INFO"))
                            print_lpkey_set_diff(stderr, key_a.c_str(),
                                                 cp.oracle_lps, cp.candidate_lps,
                                                 /*oracle_only_is_fail=*/false,
                                                 /*candidate_only_is_info=*/true);
                    }
                    else if (xfail.match(key_a))
                    {
                        ++stats.phase_a_xfail;
                        std::fprintf(stderr,
                                     "[XFAIL] %s %s oracle=%zu candidate=%zu oracle-only=%zu\n",
                                     driver_name, key_a.c_str(),
                                     oracle_keys.size(), candidate_keys.size(),
                                     oracle_only_keys.size());
                    }
                    else
                    {
                        ++stats.phase_a_fail;
                        std::fprintf(stderr,
                                     "[FAIL] %s %s oracle=%zu candidate=%zu oracle-only=%zu\n",
                                     driver_name, key_a.c_str(),
                                     oracle_keys.size(), candidate_keys.size(),
                                     oracle_only_keys.size());
                        print_lpkey_set_diff(stderr, key_a.c_str(),
                                             cp.oracle_lps, cp.candidate_lps,
                                             /*oracle_only_is_fail=*/true,
                                             /*candidate_only_is_info=*/true);
                    }

                    //=== Phase B: make_path byte-equal (尾部 D/d 剥离后) ===
                    //  仅对两侧都有的 key 做; subset 协议下 oracle ⊆ candidate
                    //  即 intersect == oracle_keys, candidate-only 不参与
                    //  Phase B (oracle 没 path 可对). 比对前对两侧 path 做
                    //  相同的 strip_trailing_dD 规范化.
                    if (cp.oracle_make_path && cp.candidate_make_path)
                    {
                        std::string key_b = make_case_key("B", piece, fx.name, is_20g);
                        std::vector<LpKey> intersect;
                        std::set_intersection(oracle_keys.begin(), oracle_keys.end(),
                                              candidate_keys.begin(), candidate_keys.end(),
                                              std::back_inserter(intersect));
                        bool any_b_fail = false;
                        int b_fail_count = 0;
                        int b_total = 0;
                        for (LpKey const &k : intersect)
                        {
                            //commit C4-followup: 在 dedup 后的 LpEntry 列表里
                            //  各取首个匹配该 key 的代表元.
                            LpEntry const *oracle_entry = nullptr;
                            for (LpEntry const &e : cp.oracle_lps)
                                if (e.key == k)
                                {
                                    oracle_entry = &e;
                                    break;
                                }
                            LpEntry const *candidate_entry = nullptr;
                            for (LpEntry const &e : cp.candidate_lps)
                                if (e.key == k)
                                {
                                    candidate_entry = &e;
                                    break;
                                }
                            if (!oracle_entry || !candidate_entry)
                                continue;
                            ++b_total;
                            std::string oracle_path_raw = cp.oracle_make_path(*oracle_entry);
                            std::string candidate_path_raw = cp.candidate_make_path(*candidate_entry);
                            std::string oracle_path = strip_trailing_dD(oracle_path_raw);
                            std::string candidate_path = strip_trailing_dD(candidate_path_raw);
                            if (oracle_path == candidate_path)
                                continue;
                            any_b_fail = true;
                            if (b_fail_count < 4)
                                print_make_path_diff(stderr, key_b.c_str(), k,
                                                     oracle_path_raw, candidate_path_raw);
                            ++b_fail_count;
                        }
                        if (!any_b_fail)
                        {
                            if (b_total > 0)
                                ++stats.phase_b_ok;
                        }
                        else if (xfail.match(key_b))
                        {
                            ++stats.phase_b_xfail;
                            std::fprintf(stderr,
                                         "[XFAIL] %s %s mismatches=%d/%d\n",
                                         driver_name, key_b.c_str(),
                                         b_fail_count, b_total);
                        }
                        else if (phase_b_policy == PhaseBPolicy::IgnoreDrift)
                        {
                            //commit C5: BFS 邻居枚举顺序差导致"oracle 走 LrD,
                            //  candidate 走 lDL"这种字符序差, strip_trailing_dD
                            //  剥离后仍不等. Phase A subset 已严格成立 (即两侧
                            //  到达同一 cells_key), 路径只是不同序的合法移动,
                            //  落点结果完全等价. driver 选 IgnoreDrift, 把这
                            //  种残留计入 phase_b_drift 而非 phase_b_fail. 详
                            //  见 phase_a_truth.md §6 / c5_followup.md.
                            ++stats.phase_b_drift;
                            std::fprintf(stderr,
                                         "[DRIFT] %s %s mismatches=%d/%d\n",
                                         driver_name, key_b.c_str(),
                                         b_fail_count, b_total);
                        }
                        else
                        {
                            ++stats.phase_b_fail;
                            std::fprintf(stderr,
                                         "[FAIL] %s %s mismatches=%d/%d\n",
                                         driver_name, key_b.c_str(),
                                         b_fail_count, b_total);
                        }
                    }
                }
            }
        }
        std::fprintf(stderr,
                     "# %s done: total=%d phase_a(ok=%d info=%d xfail=%d fail=%d) "
                     "phase_b(ok=%d xfail=%d drift=%d fail=%d)\n",
                     driver_name, stats.total,
                     stats.phase_a_ok, stats.phase_a_info,
                     stats.phase_a_xfail, stats.phase_a_fail,
                     stats.phase_b_ok, stats.phase_b_xfail,
                     stats.phase_b_drift, stats.phase_b_fail);
        if (!strict)
            return 0;
        return (stats.phase_a_fail == 0 && stats.phase_b_fail == 0) ? 0 : 1;
    }

} // namespace tetris_diff

#endif // TETRIS_AI_RUNNER_TESTS_DIFF_HARNESS_H_
