#ifndef TETRIS_AI_RUNNER_BB_STATE_H_
#define TETRIS_AI_RUNNER_BB_STATE_H_

//==========================================================================
// 位板 BFS 共享数据结构 + piece-aware 静态 helper.
//
// 抽自 movegen_search.h (commit 1: 行为零变化的 helper 抽离).
// 后续 commit 2 起, 位板 BFS 引擎 / 各 search 接位板的实现都会 include 这里.
//
// 设计:
//   - bb 命名空间下的"自由结构" (KickDir / BBState / CellsKey) 与 RuleSpec 无关,
//     直接共享.
//   - bb::Helpers<RuleSpec> 模板把所有依赖 RuleSpec 编译期表的 helper 收纳为
//     static 方法; MoveGenSearch private 继承之, 把名字 using 进类作用域,
//     原 BFS 主循环的调用点零改动.
//==========================================================================

#include "tetris_map.h"
#include "tetris_rule_spec.h"
#include "tetris_shape.h"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <optional>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace m_tetris2
{
    struct TetrisNode;
    struct TetrisMap;

    namespace bb
    {
        struct BBState
        {
            std::uint8_t t;
            std::uint8_t r;
            std::int8_t xb;
            std::int8_t yb;
        };

        static_assert(sizeof(BBState) == sizeof(std::uint32_t),
                      "BBState must be exactly 4 bytes for bit_cast hash");

        struct BBStateHash
        {
            std::size_t operator()(BBState const &s) const noexcept
            {
                return static_cast<std::size_t>(std::bit_cast<std::uint32_t>(s));
            }
        };

        struct BBStateEqual
        {
            bool operator()(BBState const &a, BBState const &b) const noexcept
            {
                return a.t == b.t && a.r == b.r && a.xb == b.xb && a.yb == b.yb;
            }
        };

        template<class RuleSpec>
        Map<RuleSpec::width, RuleSpec::height> build_board_for_search(TetrisMap const &map);

        template<class RuleSpec>
        BBState state_from_node_for_search(TetrisNode const *node);
    }
}

namespace m_tetris2
{
    namespace movegen
    {
        template<class Spec, char T, std::uint8_t R>
        constexpr auto usable_map(Map<static_cast<int>(Spec::width), static_cast<int>(Spec::height)> const &board) noexcept;

        template<class Spec, char T, std::uint8_t R>
        constexpr auto inbounds_map() noexcept;
    }

    namespace bb
    {
        //RuleSpec::ops -> piece-type 元数据 (count / index_of(t) / type_at(i)).
        //  扫 OpDesc::type 去重, 按"首次出现顺序"分配 [0, count). 与 oracle 系
        //  原 piece_index switch 同形 (SRS rule_*.h 顺序: O, I, L, J, S, Z, T).
        template<class Tuple>
        struct PieceIndexInfo;

        template<class... Ops>
        struct PieceIndexInfo<std::tuple<Ops...>>
        {
            struct Result
            {
                std::array<int, 256> table;
                //+1 防止空 pack 触发零长数组 (RuleSpec 里 ops 至少 1 个, 但保险)
                std::array<char, sizeof...(Ops) + 1> types;
                int count;
            };

            static constexpr Result compute()
            {
                Result r{};
                for (int &v : r.table)
                    v = -1;
                r.count = 0;
                (((r.table[static_cast<unsigned char>(Ops::type)] < 0)
                      ? (r.types[r.count] = Ops::type,
                         r.table[static_cast<unsigned char>(Ops::type)] = r.count,
                         ++r.count, void())
                      : void()),
                 ...);
                return r;
            }

            static constexpr Result data = compute();
            static_assert(data.table[0] == -1,
                          "piece type '\\0' (value 0) is reserved as BBState sentinel; "
                          "no OpDesc::type may use '\\0'");
            static constexpr int count = data.count;
            static constexpr char type_at(std::size_t i) noexcept
            {
                return data.types[i];
            }
            static constexpr int index_of(char t) noexcept
            {
                return data.table[static_cast<unsigned char>(t)];
            }
        };

        //跨所有 piece 的最大 rotation_count: kMaxR LUT 第二维.
        //  各 piece 的 rotation_count 不同 (e.g. cobra J=4, future 10-piece 5-rot),
        //  以 max 为 LUT 容量上限, 单 piece 实际只填到自己 rcount.
        template<class Spec, class Tuple>
        struct MaxRotation;

        template<class Spec, class... Ops>
        struct MaxRotation<Spec, std::tuple<Ops...>>
        {
            static constexpr std::size_t value = []()
            {
                std::size_t m = 1;
                ((m = (shape::rotation_count<Spec, Ops::type> > m
                           ? shape::rotation_count<Spec, Ops::type>
                           : m)),
                 ...);
                return m;
            }();
        };

        //BBState 定义已前置到文件顶部，供 tetris_core.h 循环依赖场景复用。

        //commit 5f: 用位板版 "绝对 cells 集合" 等价类替换 master IndexFilter
        //  (tetris_core.cpp:296-321 把 (node.data[0..3], node.row) 当签名).
        //  master IndexFilter 把 4 行 row mask + base row 当 5x32 bit 整体 memcmp;
        //  几何上等价于 piece 在 board 上"实际占据的 4 cells 集合相同". 因此把
        //  所有 cells 投到绝对坐标 ((yb+cy)*kW + (xb+cx)), 排序后逐元素比较, 与
        struct CellsKey
        {
            //SRS 7 piece 全部正好 4 cell. 不足 4 cell 用 0xFFFF 填充以保留排序顺序.
            std::uint16_t c[4] = {0xFFFFu, 0xFFFFu, 0xFFFFu, 0xFFFFu};

            bool operator==(CellsKey const &o) const
            {
                return c[0] == o.c[0] && c[1] == o.c[1] && c[2] == o.c[2] && c[3] == o.c[3];
            }
            bool operator!=(CellsKey const &o) const
            {
                return !(*this == o);
            }
        };

        //commit 5b: 替换 BFS 内 cur->wall_kick_*/move_*/rotate_* 指针图查询的
        //  helper. 内部按 (T, R) 派发到 shape::wk_*<RuleSpec, T, R> 静态表 +
        //  位板 usable_arr 检查; 命中后续 BFS 全程走 BBState, 不再产生 master
        //  反查.
        enum class KickDir : std::uint8_t
        {
            Cw,
            Ccw,
            Opp
        };

        //==================================================================
        // Helpers<RuleSpec>: piece-aware 静态 helper / 数据结构 / 编译期常量.
        // MoveGenSearch private 继承之, using Base::xxx 把名字注入类作用域,
        // BFS 主循环的调用点不需要 BB:: 前缀.
        //==================================================================
        template<class RuleSpec>
        struct Helpers
        {
            //=== 编译期校验 ===
            static_assert(static_cast<int>(RuleSpec::width) > 0,
                          "bb::Helpers: RuleSpec must expose static constexpr width > 0");
            static_assert(static_cast<int>(RuleSpec::height) > 0,
                          "bb::Helpers: RuleSpec must expose static constexpr height > 0");

            static constexpr int kW = static_cast<int>(RuleSpec::width);
            static constexpr int kH = static_cast<int>(RuleSpec::height);
            //piece 池容量与 LUT 第二维, 全部由 RuleSpec 推导.
            //  kPieceCount: RuleSpec::ops 中 OpDesc::type 去重个数.
            //  kMaxR     : max over T of shape::rotation_count<RuleSpec, T>.
            using piece_info_t = PieceIndexInfo<typename RuleSpec::ops>;
            static constexpr int kPieceCount = piece_info_t::count;
            static constexpr int kMaxR =
                static_cast<int>(MaxRotation<RuleSpec, typename RuleSpec::ops>::value);

            using map_t = Map<kW, kH>;

            //  走 piece_info_t::index_of(t) 256 桶 LUT, 与原 switch 等价但去掉了
            //  SRS-7 piece 的硬编码列表; 字符未在 RuleSpec::ops 中出现时返回 -1.
            static constexpr int piece_index(char t) noexcept
            {
                return piece_info_t::index_of(t);
            }

            template<char T>
            static constexpr int rcount_v()
            {
                return static_cast<int>(shape::rotation_count<RuleSpec, T>);
            }

            //  key = (r, x_bbox, y_bbox), 与 5b 的 usable_arr / check_TR 同源 bbox 公式.
            //  master 的 node_mark_ 走 std::vector<Mark> 按 node->index 寻址,
            //  作用域是单一 piece 的 BFS. 我们这里同等局部, 但维度直接位板, 不需要
            //  context->node_max() 预分配, 也不再需要 master 的 index 字段.
            //
            //  build_path 的链表回溯只用 op 字符 + 前驱坐标, 至此 PathMark 内部完全
            //
            //commit B+1: 不再用 ++version_ 做 O(1) 复位. "已访问"位图直接内联
            //  PathMarkBit 的一维 bitset 形式 (bits_[kWords] + 1 bit/cell) — 调用
            //  方看不到 used_ vs bits_ 的差异, 但 PathMark 内部 "存在性" 与
            //  PathMarkBit 共用同一种存储, 类型层面保持自洽. 默认构造
            //  (default member initializer = {}) 即合法空表; 仅在 disable_d
            //  二次 BFS (search_path::make_path_1g_native, 复用同一对象) 时
            //  才需要 clear() 显式复位. clear() 直接 memset(this) — PathMark
            //  全部字段 trivial, 全 0 是合法默认状态 (PrevKey/char/uint64_t 都
            //  天然支持).
            struct PathMark
            {
                static constexpr int kCells = kW * kH;
                //kR: PathMark 所支持的最大 rotation 维度. 与 LUT 同口径, 由 RuleSpec 推导.
                static constexpr int kR = kMaxR;
                static constexpr int kBits = kR * kCells;
                static constexpr int kWords = (kBits + 63) / 64;
                //prev cell: 起点协议 = "self prev" — 起点把自己 (r, xb, yb) 写入
                //  cell_prev_, build_path 反向回溯比对 prev == cur 即可终止. 与 oracle
                //  build_path "cur == start" 终止协议完全等价, 不再使用 0xFF 哨兵.
                struct PrevKey
                {
                    std::uint8_t r;
                    std::int8_t xb;
                    std::int8_t yb;
                };
                std::uint64_t bits_[kWords] = {};
                PrevKey cell_prev_[kR][kCells] = {};
                char cell_op_[kR][kCells] = {};

                //默认构造 (上面的 = {} 初始化器) 即合法空表, 调用方栈分配后
                //  无需调用 clear(); 仅当需要在同一对象上复位启动新一轮 BFS 时
                //  才调用. PathMark 所有字段都是 trivial POD, memset 整个对象
                //  到全 0 是合法 default state, 编译器会一次性发出最优 memset.
                void clear()
                {
                    std::memset(this, 0, sizeof(*this));
                }

                bool set_bbox(int r, int x, int y, PrevKey prev, char op)
                {
                    if (r < 0 || r >= kR || x < 0 || x >= kW || y < 0 || y >= kH)
                        return false;
                    int i = y * kW + x;
                    int idx = r * kCells + i;
                    std::uint64_t mask = std::uint64_t{1} << (idx & 63);
                    std::uint64_t &w = bits_[idx >> 6];
                    if (w & mask)
                        return false;
                    w |= mask;
                    cell_prev_[r][i] = prev;
                    cell_op_[r][i] = op;
                    return true;
                }

                std::pair<PrevKey, char> get_bbox(int r, int x, int y) const
                {
                    PrevKey none{0u, 0, 0};
                    if (r < 0 || r >= kR || x < 0 || x >= kW || y < 0 || y >= kH)
                        return {none, ' '};
                    int i = y * kW + x;
                    int idx = r * kCells + i;
                    std::uint64_t mask = std::uint64_t{1} << (idx & 63);
                    if (!(bits_[idx >> 6] & mask))
                        return {none, ' '};
                    return {cell_prev_[r][i], cell_op_[r][i]};
                }

                // 三态语义 (与 oracle/tetris_core.cpp:239-250 完全一致):
                //   * 越界 -> false (不写).
                //   * 该 cell 在本轮已写入, 且当前 op != ck -> false (不覆盖).
                //   * 否则 (未写过 / 已写过且 op == ck) -> 写入 (prev, op), 返回 true.
                //
                // 用于 search_tag T-spin 旋转邻居的 ' '→'z'/'c' 升级路径: 首次到达
                // 写入 (parent, ' '); 后续旋转邻居用 cover_if_bbox(prev, ' ', op)
                // 把 op 升级为 'z'/'c' 并允许重新展开. 在 T 的单 piece BFS 中,
                // 双射), 因此 cover_if_bbox 与 oracle node_mark_.cover_if 共用同样的
                // TagStrategy 中拔掉, 与 PathStrategy / SimulateStrategy 选用同款 PathMark.
                bool cover_if_bbox(int r, int x, int y, PrevKey prev, char ck, char op)
                {
                    if (r < 0 || r >= kR || x < 0 || x >= kW || y < 0 || y >= kH)
                        return false;
                    int i = y * kW + x;
                    int idx = r * kCells + i;
                    std::uint64_t mask = std::uint64_t{1} << (idx & 63);
                    std::uint64_t &w = bits_[idx >> 6];
                    if ((w & mask) && cell_op_[r][i] != ck)
                        return false;
                    w |= mask;
                    cell_prev_[r][i] = prev;
                    cell_op_[r][i] = op;
                    return true;
                }
            };

            //==============================================================
            // PathMarkBit (L1): 1 bit/cell 的位图, memset O(N) clear.
            //   tag search 阶段 1g/20g BFS 用 — visitor 不读 prev/op,
            //   不需要回溯路径. 一维 bitset 比 PathMark 省 64x 内存
            //   (~200B vs ~12.5KB), cache 局部性显著更好; clear 用 memset
            //   单 cache line 级别开销.
            //   index 公式: bit_idx = r * kCells + y * kW + x.
            //   默认构造 (`bits_ = {}`) 即合法空集, 调用方栈分配后无需 clear().
            //==============================================================
            struct PathMarkBit
            {
                static constexpr int kCells = kW * kH;
                static constexpr int kR = kMaxR;
                static constexpr int kBits = kR * kCells;
                static constexpr int kWords = (kBits + 63) / 64;
                std::uint64_t bits_[kWords] = {};

                void clear()
                {
                    std::memset(bits_, 0, sizeof(bits_));
                }

                //首次访问 -> true (置位); 已访问 -> false.
                bool mark_bbox(int r, int x, int y)
                {
                    if (r < 0 || r >= kR || x < 0 || x >= kW || y < 0 || y >= kH)
                        return false;
                    int idx = r * kCells + y * kW + x;
                    std::uint64_t mask = std::uint64_t{1} << (idx & 63);
                    std::uint64_t &w = bits_[idx >> 6];
                    if (w & mask)
                        return false;
                    w |= mask;
                    return true;
                }
            };

            //==============================================================
            // usable_arr 构造与查询.
            //==============================================================
            //usable_arr[r] = "(T,r) 在 bbox 基准点 (x,y) 处不越界且不碰撞 board".
            //  仅写 rcount<T> 项, 其余项保持默认空 map (BFS 不会查这些 r).
            template<char T>
            static void build_usable_T(map_t const &board, std::array<map_t, kMaxR> &out)
            {
                [&]<std::size_t... Rs>(std::index_sequence<Rs...>)
                {
                    ((out[Rs] = movegen::usable_map<RuleSpec, T, static_cast<std::uint8_t>(Rs)>(board)), ...);
                }(std::make_index_sequence<rcount_v<T>()>{});
            }

            static void build_usable_for_piece(char t, map_t const &board, std::array<map_t, kMaxR> &out)
            {
                //C-C: piece-switch 用 fold expression 在 piece_info_t 上派发.
                [&]<std::size_t... Is>(std::index_sequence<Is...>)
                {
                    bool hit = false;
                    (((!hit && piece_info_t::type_at(Is) == t)
                          ? (build_usable_T<piece_info_t::type_at(Is)>(board, out), hit = true)
                          : false),
                     ...);
                }(std::make_index_sequence<kPieceCount>{});
            }

            //==============================================================
            // 空盘 in-bounds 位图 = wall_kick_*[0] / rotate_* 等价的"空盘 board".
            //   master 旧初始化阶段填 wall_kick_clockwise 等数组时,
            //   逐 cur_op.func 在空盘 copy 上做 in-bounds 检查 (cur_op.func 是
            //   piece-shape 在 spawn 处用的 build, 仅与 (T, R) 偏移有关, 与 board
            //   无关). 因此 master cur->rotate_clockwise (= wall_kick_clockwise[0])
            //   = "把 (T, R) 当成空盘可放置位图后, kick 链中第一个 in-bounds 命中
            //   的目标 (T', R', xb', yb')". 位板等价: usable_arr 用 inbounds_map
            //   预算的"空盘等价" 数组 inbounds_arr 喂给 first_passing_kick_bb,
            //   出来的 BBState 即与 master rotate_* 完全等价 (board check 由调用
            //   方在 emit 前再补一次 usable_at_bb(实际 board) 做, 与 oracle
            //   "rotate_* != nullptr && rotate_*->check(map)" 二段判定一致).
            //==============================================================
            template<char T>
            static void build_inbounds_T(std::array<map_t, kMaxR> &out)
            {
                [&]<std::size_t... Rs>(std::index_sequence<Rs...>)
                {
                    ((out[Rs] = movegen::inbounds_map<RuleSpec, T, static_cast<std::uint8_t>(Rs)>()), ...);
                }(std::make_index_sequence<rcount_v<T>()>{});
            }

            static void build_inbounds_for_piece(char t, std::array<map_t, kMaxR> &out)
            {
                [&]<std::size_t... Is>(std::index_sequence<Is...>)
                {
                    bool hit = false;
                    (((!hit && piece_info_t::type_at(Is) == t)
                          ? (build_inbounds_T<piece_info_t::type_at(Is)>(out), hit = true)
                          : false),
                     ...);
                }(std::make_index_sequence<kPieceCount>{});
            }

            //把 master status (x,y) 转 bbox (x_bbox,y_bbox) 后查 usable_arr.
            //  master_x = bbox_x - origin.x, master_y = bbox_y + origin.y
            //  (与 tetris_movegen.h emit 阶段反向公式一致)
            template<char T, std::uint8_t R>
            static bool check_TR(int status_x, int status_y, std::array<map_t, kMaxR> const &usable_arr)
            {
                constexpr auto orig = shape::piece_cells<RuleSpec, T, R>.origin;
                int x_bbox = status_x + orig.x;
                int y_bbox = status_y - orig.y;
                if (x_bbox < 0 || x_bbox >= kW || y_bbox < 0 || y_bbox >= kH)
                    return false;
                return usable_arr[R].get(x_bbox, y_bbox);
            }

            template<char T>
            static bool check_T(int status_x, int status_y, std::uint8_t r,
                                std::array<map_t, kMaxR> const &usable_arr)
            {
                bool ret = false;
                [&]<std::size_t... Rs>(std::index_sequence<Rs...>)
                {
                    ((Rs == r ? (ret = check_TR<T, static_cast<std::uint8_t>(Rs)>(status_x, status_y, usable_arr), true) : false) || ...);
                }(std::make_index_sequence<rcount_v<T>()>{});
                return ret;
            }

            static bool check_T(char t, int status_x, int status_y, std::uint8_t r,
                                std::array<map_t, kMaxR> const &usable_arr)
            {
                bool ret = false;
                bool hit = false;
                [&]<std::size_t... Is>(std::index_sequence<Is...>)
                {
                    (((!hit && piece_info_t::type_at(Is) == t)
                          ? (ret = check_T<piece_info_t::type_at(Is)>(status_x, status_y, r, usable_arr), hit = true)
                          : false),
                     ...);
                }(std::make_index_sequence<kPieceCount>{});
                return ret;
            }

            //commit 5g cleanup-3: BFS 完全位板化后, node_check_bb 已无调用方, 移除.
            //commit 5g-3b (slice 5): bbox 直查版本. 调用方持有 BBState 时直接查
            //  usable_arr[r].get, 与 check_TR 等价 (后者只是为指针入口提供折回).
            static bool usable_at_bb(std::uint8_t r, int xb, int yb,
                                     std::array<map_t, kMaxR> const &usable_arr)
            {
                if (xb < 0 || xb >= kW || yb < 0 || yb >= kH)
                    return false;
                return usable_arr[r].get(xb, yb);
            }

            //==============================================================
            //==============================================================
            static BBState state_from_node(TetrisNode const *n);

            // 运行时 (t, r, master_x, master_y) -> BBState.
            // 等价于 state_from_node 但直接接受坐标，无需 TetrisNode*.
            static BBState state_from_status(char t, std::uint8_t r, int mx, int my)
            {
                BBState s{0, 0, 0, 0};
                auto build_for_piece = [&]<char T>()
                {
                    [&]<std::size_t... Rs>(std::index_sequence<Rs...>)
                    {
                        ((r == static_cast<std::uint8_t>(Rs)
                              ? (s = build_state_from_master<T, static_cast<std::uint8_t>(Rs)>(mx, my), true)
                              : false) ||
                         ...);
                    }(std::make_index_sequence<rcount_v<T>()>{});
                };
                bool dispatched = false;
                [&]<std::size_t... Is>(std::index_sequence<Is...>)
                {
                    (((!dispatched && piece_info_t::type_at(Is) == t)
                          ? (build_for_piece.template operator()<piece_info_t::type_at(Is)>(), dispatched = true, true)
                          : false),
                     ...);
                }(std::make_index_sequence<kPieceCount>{});
                return s;
            }

            // BBState (bbox 坐标) -> master (status.x, status.y).
            // master_x = xb - orig.x,  master_y = yb + orig.y
            static std::pair<int, int> master_xy_from_state(BBState const &cs)
            {
                int mx = 0, my = 0;
                char t = static_cast<char>(cs.t);
                std::uint8_t r = cs.r;
                auto build_for_piece = [&]<char T>()
                {
                    [&]<std::size_t... Rs>(std::index_sequence<Rs...>)
                    {
                        ((r == static_cast<std::uint8_t>(Rs)
                              ? ([&]()
                                 {
                                     constexpr auto orig =
                                         shape::piece_cells<RuleSpec, T, static_cast<std::uint8_t>(Rs)>.origin;
                                     mx = static_cast<int>(cs.xb) - static_cast<int>(orig.x);
                                     my = static_cast<int>(cs.yb) + static_cast<int>(orig.y);
                                 }(),
                                 true)
                              : false) ||
                         ...);
                    }(std::make_index_sequence<rcount_v<T>()>{});
                };
                bool dispatched = false;
                [&]<std::size_t... Is>(std::index_sequence<Is...>)
                {
                    (((!dispatched && piece_info_t::type_at(Is) == t)
                          ? (build_for_piece.template operator()<piece_info_t::type_at(Is)>(), dispatched = true, true)
                          : false),
                     ...);
                }(std::make_index_sequence<kPieceCount>{});
                return {mx, my};
            }

            //==============================================================
            // CellsKey: master IndexFilter 的位板等价.
            //==============================================================
            template<char T, std::uint8_t R>
            static CellsKey cells_key_TR(int xb, int yb)
            {
                CellsKey k{};
                constexpr auto const &cells = shape::piece_cells<RuleSpec, T, R>;
                std::size_t n = cells.count > 4 ? 4 : cells.count;
                for (std::size_t i = 0; i < n; ++i)
                {
                    int abs_x = xb + static_cast<int>(cells.cells[i].x);
                    int abs_y = yb + static_cast<int>(cells.cells[i].y);
                    k.c[i] = static_cast<std::uint16_t>(abs_y * kW + abs_x);
                }
                //排序 (n<=4, 直接小型 bubble).
                for (std::size_t i = 0; i < 4; ++i)
                    for (std::size_t j = i + 1; j < 4; ++j)
                        if (k.c[j] < k.c[i])
                        {
                            std::uint16_t tmp = k.c[i];
                            k.c[i] = k.c[j];
                            k.c[j] = tmp;
                        }
                return k;
            }

            //commit 7a-2: BBState 入口的 cells_key. 与 cells_key_for(node) 等价 (node
            //  侧也是 status_to_bbox -> cells_key_TR), 但跳过 context_->get + 反推.
            template<char T>
            static CellsKey cells_key_T_state(BBState const &s)
            {
                int xb = static_cast<int>(s.xb);
                int yb = static_cast<int>(s.yb);
                CellsKey ret{};
                std::uint8_t cur_r = s.r;
                [&]<std::size_t... Rs>(std::index_sequence<Rs...>)
                {
                    ((Rs == cur_r ? (ret = cells_key_TR<T, static_cast<std::uint8_t>(Rs)>(xb, yb), true) : false) || ...);
                }(std::make_index_sequence<rcount_v<T>()>{});
                return ret;
            }

            static CellsKey cells_key_for_state(BBState const &s)
            {
                CellsKey ret{};
                char t = static_cast<char>(s.t);
                [&]<std::size_t... Is>(std::index_sequence<Is...>)
                {
                    bool hit = false;
                    (((!hit && piece_info_t::type_at(Is) == t)
                          ? (ret = cells_key_T_state<piece_info_t::type_at(Is)>(s), hit = true)
                          : false),
                     ...);
                }(std::make_index_sequence<kPieceCount>{});
                return ret;
            }

            static bool index_filtered_eq_state(BBState const &s, CellsKey const &target)
            {
                return cells_key_for_state(s) == target;
            }

            // Compute CellsKey from oracle-coordinate (status.x / status.y) without
            // Equivalent to: cells_key_for_state(build_state_from_master<T,R>(x, y))
            // but accepts runtime t/r values, making it usable from diff-test drivers
            static CellsKey cells_key_from_oracle(char t, std::uint8_t r, int x, int y)
            {
                CellsKey ret{};
                auto build_for_piece = [&]<char T>()
                {
                    [&]<std::size_t... Rs>(std::index_sequence<Rs...>)
                    {
                        ((Rs == r
                              ? (ret = cells_key_TR<T, static_cast<std::uint8_t>(Rs)>(
                                     x + static_cast<int>(shape::piece_cells<RuleSpec, T, static_cast<std::uint8_t>(Rs)>.origin.x),
                                     y - static_cast<int>(shape::piece_cells<RuleSpec, T, static_cast<std::uint8_t>(Rs)>.origin.y)),
                                 true)
                              : false) ||
                         ...);
                    }(std::make_index_sequence<rcount_v<T>()>{});
                };
                bool dispatched = false;
                [&]<std::size_t... Is>(std::index_sequence<Is...>)
                {
                    (((!dispatched && piece_info_t::type_at(Is) == t)
                          ? (build_for_piece.template operator()<piece_info_t::type_at(Is)>(),
                             dispatched = true, true)
                          : false),
                     ...);
                }(std::make_index_sequence<kPieceCount>{});
                return ret;
            }

            //==============================================================
            // disable_d 三谓词位板化 (master search_tspin.cpp:139).
            //==============================================================
            template<char T, std::uint8_t R>
            static int min_cell_y_TR()
            {
                constexpr auto const &cells = shape::piece_cells<RuleSpec, T, R>;
                int m = 0x7fffffff;
                for (std::size_t i = 0; i < cells.count; ++i)
                {
                    int cy = static_cast<int>(cells.cells[i].y);
                    if (cy < m)
                        m = cy;
                }
                return m;
            }

            template<char T, std::uint8_t R>
            static int spawn_min_cell_y_TR_state(BBState const &spawn)
            {
                constexpr auto orig_cur = shape::piece_cells<RuleSpec, T, R>.origin;
                constexpr auto orig_spawn = shape::piece_cells<RuleSpec, T, 0>.origin;
                int yb0 = static_cast<int>(spawn.yb) + static_cast<int>(orig_cur.y) - static_cast<int>(orig_spawn.y);
                return yb0 + min_cell_y_TR<T, 0>();
            }

            template<char T>
            static int spawn_min_cell_y_T_state(BBState const &spawn)
            {
                int best = 0x7fffffff;
                std::uint8_t cur_r = spawn.r;
                [&]<std::size_t... Rs>(std::index_sequence<Rs...>)
                {
                    ((Rs == cur_r
                          ? (best = spawn_min_cell_y_TR_state<T, static_cast<std::uint8_t>(Rs)>(spawn), true)
                          : false) ||
                     ...);
                }(std::make_index_sequence<rcount_v<T>()>{});
                return best;
            }

            //计算 board 在指定列 x 的 "首个未占据 y" 等价值.
            //master TetrisMap.top[x] = "该列最高已占据 y + 1". 我们逐 y 扫:
            //  从高到低找第一个 board.get(x, y) = 1, 返回 y+1; 全空返 0.
            static int board_col_top(map_t const &board, int x)
            {
                if (x < 0 || x >= kW)
                    return 0;
                for (int y = kH - 1; y >= 0; --y)
                    if (board.get(x, y))
                        return y + 1;
                return 0;
            }

            //land_point->open(map) 位板等价.
            template<char T, std::uint8_t R>
            static bool open_bb_TR(int xb, int yb, map_t const &board)
            {
                constexpr auto const &cells = shape::piece_cells<RuleSpec, T, R>;
                int mn[4];
                for (int i = 0; i < 4; ++i)
                    mn[i] = 0x7fffffff;
                int min_cx = 0x7fffffff, max_cx = -0x7fffffff;
                for (std::size_t i = 0; i < cells.count; ++i)
                {
                    int cx = static_cast<int>(cells.cells[i].x);
                    int cy = static_cast<int>(cells.cells[i].y);
                    if (cx < min_cx)
                        min_cx = cx;
                    if (cx > max_cx)
                        max_cx = cx;
                    int idx = cx - 0;
                    if (idx >= 0 && idx < 4 && cy < mn[idx])
                        mn[idx] = cy;
                }
                for (int cx = min_cx; cx <= max_cx; ++cx)
                {
                    int idx = cx - 0;
                    if (idx < 0 || idx >= 4 || mn[idx] == 0x7fffffff)
                        continue;
                    int abs_x = xb + cx;
                    int abs_y_min = yb + mn[idx];
                    int top = board_col_top(board, abs_x);
                    if (abs_y_min >= top)
                        return true;
                }
                return false;
            }

            template<char T>
            static bool open_bb_T_state(BBState const &lp, map_t const &board)
            {
                bool ret = false;
                std::uint8_t cur_r = lp.r;
                [&]<std::size_t... Rs>(std::index_sequence<Rs...>)
                {
                    ((Rs == cur_r
                          ? (ret = open_bb_TR<T, static_cast<std::uint8_t>(Rs)>(lp.xb, lp.yb, board), true)
                          : false) ||
                     ...);
                }(std::make_index_sequence<rcount_v<T>()>{});
                return ret;
            }

            static bool open_bb_state(BBState const &lp, map_t const &board)
            {
                bool ret = false;
                char t = static_cast<char>(lp.t);
                [&]<std::size_t... Is>(std::index_sequence<Is...>)
                {
                    bool hit = false;
                    (((!hit && piece_info_t::type_at(Is) == t)
                          ? (ret = open_bb_T_state<piece_info_t::type_at(Is)>(lp, board), hit = true)
                          : false),
                     ...);
                }(std::make_index_sequence<kPieceCount>{});
                return ret;
            }

            static int spawn_min_cell_y_dispatch_state(BBState const &spawn)
            {
                int ret = 0x7fffffff;
                char t = static_cast<char>(spawn.t);
                [&]<std::size_t... Is>(std::index_sequence<Is...>)
                {
                    bool hit = false;
                    (((!hit && piece_info_t::type_at(Is) == t)
                          ? (ret = spawn_min_cell_y_T_state<piece_info_t::type_at(Is)>(spawn), hit = true)
                          : false),
                     ...);
                }(std::make_index_sequence<kPieceCount>{});
                return ret;
            }

            static int board_roof(map_t const &board)
            {
                if (board.popcount() == 0)
                    return 0;
                return board.max_y() + 1;
            }

            //==============================================================
            // is_above_roof_bb: BBState 入口的 "piece 完全悬空 + 在屋顶以上"
            //   位板谓词. 与 master `node->low >= map.roof` 在 spawn entry
            //   语义对齐 (位板侧用当前 R 的 cells 最低 y = yb +
            //   min_cell_y_TR<T,R>()). 用于 search_tag / search_simple 的
            //   1g vs 20g/fast 分支 — entry 完全悬空时走 fast/20g 路径,
            //   不需要 BFS 列出落点.
            //==============================================================
            template<char T, std::uint8_t R>
            static int state_piece_min_y_TR(BBState const &s)
            {
                return static_cast<int>(s.yb) + min_cell_y_TR<T, R>();
            }

            template<char T>
            static int state_piece_min_y_T(BBState const &s)
            {
                int ret = 0x7fffffff;
                std::uint8_t cur_r = s.r;
                [&]<std::size_t... Rs>(std::index_sequence<Rs...>)
                {
                    ((Rs == cur_r
                          ? (ret = state_piece_min_y_TR<T, static_cast<std::uint8_t>(Rs)>(s), true)
                          : false) ||
                     ...);
                }(std::make_index_sequence<rcount_v<T>()>{});
                return ret;
            }

            static bool is_above_roof_bb(BBState const &s, int roof)
            {
                int min_y = 0x7fffffff;
                char t = static_cast<char>(s.t);
                [&]<std::size_t... Is>(std::index_sequence<Is...>)
                {
                    bool hit = false;
                    (((!hit && piece_info_t::type_at(Is) == t)
                          ? (min_y = state_piece_min_y_T<piece_info_t::type_at(Is)>(s), hit = true)
                          : false),
                     ...);
                }(std::make_index_sequence<kPieceCount>{});
                return min_y >= roof;
            }

            //==============================================================
            // 起点 BBState / wall-kick / rotate / drop / build_board.
            //==============================================================
            template<char T, std::uint8_t Target>
            static BBState build_state_from_master(int status_x, int status_y)
            {
                constexpr auto orig = shape::piece_cells<RuleSpec, T, Target>.origin;
                BBState s;
                s.t = static_cast<std::uint8_t>(T);
                s.r = Target;
                s.xb = static_cast<std::int8_t>(status_x + static_cast<int>(orig.x));
                s.yb = static_cast<std::int8_t>(status_y - static_cast<int>(orig.y));
                return s;
            }

            template<char T, std::uint8_t Target, class WkList>
            static std::optional<BBState> try_kick_chain_TR_bb(int status_x, int status_y,
                                                               std::array<map_t, kMaxR> const &usable_arr)
            {
                if (check_TR<T, Target>(status_x, status_y, usable_arr))
                    return build_state_from_master<T, Target>(status_x, status_y);
                for (std::size_t i = 0; i < WkList::length; ++i)
                {
                    int nx = status_x + WkList::data[2 * i];
                    int ny = status_y + WkList::data[2 * i + 1];
                    if (check_TR<T, Target>(nx, ny, usable_arr))
                        return build_state_from_master<T, Target>(nx, ny);
                }
                return std::nullopt;
            }

            template<char T, std::uint8_t R, KickDir Dir>
            static std::optional<BBState> try_kick_TRD_bb(BBState const &cs,
                                                          std::array<map_t, kMaxR> const &usable_arr)
            {
                constexpr auto orig = shape::piece_cells<RuleSpec, T, R>.origin;
                int mx = static_cast<int>(cs.xb) - static_cast<int>(orig.x);
                int my = static_cast<int>(cs.yb) + static_cast<int>(orig.y);
                if constexpr (Dir == KickDir::Cw)
                {
                    constexpr std::uint8_t tgt = shape::target_cw<RuleSpec, T, R>;
                    if constexpr (tgt != kOpRotateNone)
                        return try_kick_chain_TR_bb<T, tgt, shape::wk_cw<RuleSpec, T, R>>(
                            mx, my, usable_arr);
                    else
                        return std::nullopt;
                }
                else if constexpr (Dir == KickDir::Ccw)
                {
                    constexpr std::uint8_t tgt = shape::target_ccw<RuleSpec, T, R>;
                    if constexpr (tgt != kOpRotateNone)
                        return try_kick_chain_TR_bb<T, tgt, shape::wk_ccw<RuleSpec, T, R>>(
                            mx, my, usable_arr);
                    else
                        return std::nullopt;
                }
                else
                {
                    constexpr std::uint8_t tgt = shape::target_opp<RuleSpec, T, R>;
                    if constexpr (tgt != kOpRotateNone)
                        return try_kick_chain_TR_bb<T, tgt, shape::wk_opp<RuleSpec, T, R>>(
                            mx, my, usable_arr);
                    else
                        return std::nullopt;
                }
            }

            template<char T, KickDir Dir>
            static std::optional<BBState> try_kick_TD_bb(BBState const &cs,
                                                         std::array<map_t, kMaxR> const &usable_arr)
            {
                std::optional<BBState> ret;
                std::uint8_t cur_r = cs.r;
                [&]<std::size_t... Rs>(std::index_sequence<Rs...>)
                {
                    ((Rs == cur_r ? (ret = try_kick_TRD_bb<T, static_cast<std::uint8_t>(Rs), Dir>(cs, usable_arr), true) : false) || ...);
                }(std::make_index_sequence<rcount_v<T>()>{});
                return ret;
            }

            template<KickDir Dir>
            static std::optional<BBState> first_passing_kick_dir_bb(char t, BBState const &cs,
                                                                    std::array<map_t, kMaxR> const &usable_arr)
            {
                std::optional<BBState> ret;
                [&]<std::size_t... Is>(std::index_sequence<Is...>)
                {
                    bool hit = false;
                    (((!hit && piece_info_t::type_at(Is) == t)
                          ? (ret = try_kick_TD_bb<piece_info_t::type_at(Is), Dir>(cs, usable_arr), hit = true)
                          : false),
                     ...);
                }(std::make_index_sequence<kPieceCount>{});
                return ret;
            }

            static std::optional<BBState> first_passing_kick_bb(char t,
                                                                KickDir dir,
                                                                BBState const &cs,
                                                                std::array<map_t, kMaxR> const &usable_arr)
            {
                switch (dir)
                {
                case KickDir::Cw:
                    return first_passing_kick_dir_bb<KickDir::Cw>(t, cs, usable_arr);
                case KickDir::Ccw:
                    return first_passing_kick_dir_bb<KickDir::Ccw>(t, cs, usable_arr);
                case KickDir::Opp:
                    return first_passing_kick_dir_bb<KickDir::Opp>(t, cs, usable_arr);
                }
                return std::nullopt;
            }

            //commit 7b: rotate_no_kick 的 BBState 出口. 旋转目标 r 由 (T, R, Dir) 静态
            //  解出, 不存在时返回 nullopt; 否则按 build_state_from_master 公式直接折出
            //  (xb', yb'). 与 rotate_no_kick_TRD(cs) 完全等价.
            template<char T, std::uint8_t R, KickDir Dir>
            static std::optional<BBState> rotate_no_kick_TRD_bb(BBState const &cs)
            {
                if constexpr (Dir == KickDir::Cw)
                {
                    constexpr std::uint8_t tgt = shape::target_cw<RuleSpec, T, R>;
                    if constexpr (tgt == kOpRotateNone)
                        return std::nullopt;
                    else
                    {
                        constexpr auto orig = shape::piece_cells<RuleSpec, T, R>.origin;
                        int mx = static_cast<int>(cs.xb) - static_cast<int>(orig.x);
                        int my = static_cast<int>(cs.yb) + static_cast<int>(orig.y);
                        return build_state_from_master<T, tgt>(mx, my);
                    }
                }
                else if constexpr (Dir == KickDir::Ccw)
                {
                    constexpr std::uint8_t tgt = shape::target_ccw<RuleSpec, T, R>;
                    if constexpr (tgt == kOpRotateNone)
                        return std::nullopt;
                    else
                    {
                        constexpr auto orig = shape::piece_cells<RuleSpec, T, R>.origin;
                        int mx = static_cast<int>(cs.xb) - static_cast<int>(orig.x);
                        int my = static_cast<int>(cs.yb) + static_cast<int>(orig.y);
                        return build_state_from_master<T, tgt>(mx, my);
                    }
                }
                else
                {
                    constexpr std::uint8_t tgt = shape::target_opp<RuleSpec, T, R>;
                    if constexpr (tgt == kOpRotateNone)
                        return std::nullopt;
                    else
                    {
                        constexpr auto orig = shape::piece_cells<RuleSpec, T, R>.origin;
                        int mx = static_cast<int>(cs.xb) - static_cast<int>(orig.x);
                        int my = static_cast<int>(cs.yb) + static_cast<int>(orig.y);
                        return build_state_from_master<T, tgt>(mx, my);
                    }
                }
            }

            template<char T, KickDir Dir>
            static std::optional<BBState> rotate_no_kick_TD_bb(BBState const &cs)
            {
                std::optional<BBState> ret;
                std::uint8_t cur_r = cs.r;
                [&]<std::size_t... Rs>(std::index_sequence<Rs...>)
                {
                    ((Rs == cur_r ? (ret = rotate_no_kick_TRD_bb<T, static_cast<std::uint8_t>(Rs), Dir>(cs), true) : false) || ...);
                }(std::make_index_sequence<rcount_v<T>()>{});
                return ret;
            }

            template<KickDir Dir>
            static std::optional<BBState> rotate_no_kick_dir_bb(char t, BBState const &cs)
            {
                std::optional<BBState> ret;
                [&]<std::size_t... Is>(std::index_sequence<Is...>)
                {
                    bool hit = false;
                    (((!hit && piece_info_t::type_at(Is) == t)
                          ? (ret = rotate_no_kick_TD_bb<piece_info_t::type_at(Is), Dir>(cs), hit = true)
                          : false),
                     ...);
                }(std::make_index_sequence<kPieceCount>{});
                return ret;
            }

            static std::optional<BBState> rotate_no_kick_bb(char t, KickDir dir, BBState const &cs)
            {
                switch (dir)
                {
                case KickDir::Cw:
                    return rotate_no_kick_dir_bb<KickDir::Cw>(t, cs);
                case KickDir::Ccw:
                    return rotate_no_kick_dir_bb<KickDir::Ccw>(t, cs);
                case KickDir::Opp:
                    return rotate_no_kick_dir_bb<KickDir::Opp>(t, cs);
                }
                return std::nullopt;
            }

            //try_kick_chain_to: build_path 末段 T-spin 重放. master 是 "枚举
            //  last->wall_kick_dir 数组中第一个 wk->check(map) 命中且 wk == target
            //  的", 我们位板版做同样的事: 顺着 kick 链找第一个 fits 且其 cells_key
            //  == target_key 的.
            //  cells_key 与 (T, R, xb, yb) 是双射, 与原 hit == target 指针对比等价.
            //commit 7a-2: 整条链改 BBState, 命中后直接 cells_key_for_state 比对.
            static bool try_kick_chain_to(char t, KickDir dir, BBState const &last_state, CellsKey const &target_key,
                                          std::array<map_t, kMaxR> const &usable_arr)
            {
                auto hit = first_passing_kick_bb(t, dir, last_state, usable_arr);
                if (!hit)
                    return false;
                return cells_key_for_state(*hit) == target_key;
            }

            //drop 位板化: 沿当前 (T, r, x) column 找最低 still-usable 的 y.
            //  master cur->drop(map) 走 move_down_multi 缓存, 等价 "while (move_down &&
            //  move_down->check(map)) cur = move_down". 位板等价: 在 usable_arr[r] 上
            //  从 y_bbox 向下递减直到 usable 不成立; 最后一个仍成立的 y 即终点.
            //  out-of-bounds 时返回 nullptr (理论不会发生, 调用前 cur 必在 BFS 已可达).
            //commit 7a-2: drop_bb 的 BBState-only 版本. 与原 drop_bb 完全相同的 column
            //  扫描语义, 唯一差异是直接返回新 BBState (xb 不变, yb 落到 column 最低
            //  still-usable). out-of-bounds 时返回 nullopt.
            static std::optional<BBState> drop_bb_state(BBState const &cs,
                                                        std::array<map_t, kMaxR> const &usable_arr)
            {
                int xb = static_cast<int>(cs.xb);
                int yb = static_cast<int>(cs.yb);
                std::uint8_t r = cs.r;
                if (xb < 0 || xb >= kW || yb < 0 || yb >= kH)
                    return std::nullopt;
                if (!usable_arr[r].get(xb, yb))
                    return std::nullopt;
                while (true)
                {
                    int try_yb = yb - 1;
                    if (try_yb < 0)
                        break;
                    if (!usable_arr[r].get(xb, try_yb))
                        break;
                    yb = try_yb;
                }
                BBState ns = cs;
                ns.yb = static_cast<std::int8_t>(yb);
                return ns;
            }

            //==============================================================
            // spawn-row land_point 枚举的位板等价 (master 旧初始化阶段
            //   tetris_core.cpp:414-428 行为复刻).
            //   起点 BBState entry, 沿 rotate_ccw (优先 ccw, 没 ccw 用 cw) 链
            //   跑遍每个 r; 每 r 上在 yb 行用 inbounds_arr 扫"可放置基准点"
            //   (与 master move_left/move_right in-bounds 链表等价), leftmost→
            //   rightmost 顺序 push 出 BBState. drop 由调用方再做.
            //   ccw 链与 master rotate_* 出口完全等价 (master 端 rotate_* =
            //   wall_kick_*[0] = 空盘 inbounds 第一个命中).
            //==============================================================
            static void enumerate_spawn_row_states(char piece_t,
                                                   BBState const &entry,
                                                   std::array<map_t, kMaxR> const &inbounds_arr,
                                                   std::vector<BBState> &out)
            {
                BBState rot = entry;
                std::uint8_t start_r = rot.r;
                bool first = true;
                while (true)
                {
                    if (!first && rot.r == start_r)
                        break;
                    first = false;
                    int yb = static_cast<int>(rot.yb);
                    //先 move_left 到底.
                    int left_xb = static_cast<int>(rot.xb);
                    while (left_xb - 1 >= 0 && inbounds_arr[rot.r].get(left_xb - 1, yb))
                        --left_xb;
                    //再 move_right 扫到底, 全部 push.
                    int xb = left_xb;
                    while (xb < kW && inbounds_arr[rot.r].get(xb, yb))
                    {
                        BBState s = rot;
                        s.xb = static_cast<std::int8_t>(xb);
                        out.push_back(s);
                        ++xb;
                    }
                    //rotate_ccw 优先, 否则 rotate_cw (master prepare 行 427).
                    auto nxt = first_passing_kick_bb(piece_t, KickDir::Ccw, rot, inbounds_arr);
                    if (!nxt)
                        nxt = first_passing_kick_bb(piece_t, KickDir::Cw, rot, inbounds_arr);
                    if (!nxt)
                        break;
                    rot = *nxt;
                }
            }

                        // TetrisMap.row[y] 极性: 低 W 位中 1=空, 0=占含墙外.

                        // 位板 map_t 极性: 1=占. 转换 = (~row[y]) & line, line 为低 W 位全 1.

                        // out-of-line 定义在 #include "tetris_core.h" 之后（文件末尾），

                        // 以确保 TetrisMap 成员在方法体中是完整类型.

                        static map_t build_board(TetrisMap const &m);

            

                        // 纯位板版 attach：把 BBState 对应的方块写入 TetrisMap，消行，更新 top/roof.

                        // 不需要 TetrisNode*，完全替代 TetrisNode::attach(context, map).

                        // 返回消行数.

                        // out-of-line 定义在 #include "tetris_core.h" 之后（文件末尾）.

                        static size_t attach_to_map(BBState const &s, TetrisMap &map);

            

                    private:

                        // 编译期 (T, R) 路径：写方块、消行、更新 top/roof.

                        // out-of-line 定义在 #include "tetris_core.h" 之后（文件末尾）.

                        template<char T, std::uint8_t R>

                        static size_t attach_TR(BBState const &s, TetrisMap &map);

            

                    public:

                    };
        template<class RuleSpec>
        inline Map<RuleSpec::width, RuleSpec::height> build_board_for_search(TetrisMap const &map)
        {
            return Helpers<RuleSpec>::build_board(map);
        }

        template<class RuleSpec>
        inline BBState state_from_node_for_search(TetrisNode const *node)
        {
            return Helpers<RuleSpec>::state_from_node(node);
        }

        // out-of-line definition of Helpers<RuleSpec>::state_from_node.
        // Placed after #include "tetris_core.h" so TetrisNode is complete.
        // These includes are deferred to here (from the old L74 position) so that
        // when bb_state.h is included before tetris_core.h, Helpers<> is fully
        // defined before tetris_core.h sees it (breaks the circular-include issue).
    } // namespace bb
} // namespace m_tetris2

#include "tetris_core.h"
#include "tetris_movegen.h"

namespace m_tetris2
{
    namespace bb
    {
        template<class RuleSpec>
        BBState Helpers<RuleSpec>::state_from_node(TetrisNode const *n)
        {
            BBState s{0, 0, 0, 0};
            if (!n)
                return s;
            char t = n->status.t;
            std::uint8_t r = static_cast<std::uint8_t>(n->status.r);
            auto build_for_piece = [&]<char T>()
            {
                [&]<std::size_t... Rs>(std::index_sequence<Rs...>)
                {
                    ((r == static_cast<std::uint8_t>(Rs)
                          ? (s = Helpers<RuleSpec>::template build_state_from_master<T, static_cast<std::uint8_t>(Rs)>(n->status.x, n->status.y), true)
                          : false) ||
                     ...);
                }(std::make_index_sequence<Helpers<RuleSpec>::template rcount_v<T>()>{});
            };
            bool dispatched = false;
            [&]<std::size_t... Is>(std::index_sequence<Is...>)
            {
                (((!dispatched && Helpers<RuleSpec>::piece_info_t::type_at(Is) == t)
                      ? (build_for_piece.template operator()<Helpers<RuleSpec>::piece_info_t::type_at(Is)>(), dispatched = true, true)
                      : false),
                 ...);
            }(std::make_index_sequence<Helpers<RuleSpec>::kPieceCount>{});
            return s;
        }

        // out-of-line: Helpers<RuleSpec>::build_board
        // TetrisMap is complete here (tetris_core.h already included above).
        template<class RuleSpec>
        typename Helpers<RuleSpec>::map_t
        Helpers<RuleSpec>::build_board(TetrisMap const &m)
        {
            map_t out{};
            using row_t_local = typename map_t::row_t;
            constexpr row_t_local row_mask = map_t::row_full;
            int h = m.height < kH ? m.height : kH;
            for (int y = 0; y < h; ++y)
            {
                std::uint32_t inv = (~m.row[y]) & static_cast<std::uint32_t>(row_mask);
                out.set_row(y, static_cast<row_t_local>(inv));
            }
            return out;
        }

        // out-of-line: Helpers<RuleSpec>::attach_to_map
        template<class RuleSpec>
        size_t Helpers<RuleSpec>::attach_to_map(BBState const &s, TetrisMap &map)
        {
            size_t result = 0;
            char t = static_cast<char>(s.t);
            std::uint8_t r = s.r;
            auto dispatch_T = [&]<char T>()
            {
                [&]<std::size_t... Rs>(std::index_sequence<Rs...>)
                {
                    ((Rs == r
                          ? (result = attach_TR<T, static_cast<std::uint8_t>(Rs)>(s, map), true)
                          : false) ||
                     ...);
                }(std::make_index_sequence<rcount_v<T>()>{});
            };
            bool dispatched = false;
            [&]<std::size_t... Is>(std::index_sequence<Is...>)
            {
                (((!dispatched && piece_info_t::type_at(Is) == t)
                      ? (dispatch_T.template operator()<piece_info_t::type_at(Is)>(), dispatched = true, true)
                      : false),
                 ...);
            }(std::make_index_sequence<kPieceCount>{});
            return result;
        }

        // out-of-line: Helpers<RuleSpec>::attach_TR<T, R>
        template<class RuleSpec>
        template<char T, std::uint8_t R>
        size_t Helpers<RuleSpec>::attach_TR(BBState const &s, TetrisMap &map)
        {
            constexpr auto &cells = shape::piece_cells<RuleSpec, T, R>;
            int xb = static_cast<int>(s.xb);
            int yb = static_cast<int>(s.yb);

            // 1. 写方块 cell 到 TetrisMap（TetrisMap 极性: 1=空, 0=占）.
            for (std::size_t i = 0; i < cells.count; ++i)
            {
                int ax = xb + static_cast<int>(cells.cells[i].x);
                int ay = yb + static_cast<int>(cells.cells[i].y);
                if (ay >= 0 && ay < map.height && ax >= 0 && ax < map.width)
                    map.row[ay] &= ~(row_t(1) << ax);
            }

            // 2. 确定方块行范围 [row_min, row_max].
            int row_min = map.height, row_max = -1;
            for (std::size_t i = 0; i < cells.count; ++i)
            {
                int ay = yb + static_cast<int>(cells.cells[i].y);
                if (ay >= 0 && ay < map.height)
                {
                    if (ay < row_min) row_min = ay;
                    if (ay > row_max) row_max = ay;
                }
            }

            // 3. 消行（满行 = row[y] == 0，即 1=空 全清）.
            row_t full_row = row_t(0);
            size_t clear = 0;
            if (row_min <= row_max)
            {
                for (int i = row_max; i >= row_min; --i)
                {
                    if (map.row[i] == full_row)
                    {
                        std::memmove(&map.row[i], &map.row[i + 1],
                                     static_cast<std::size_t>(map.height - i - 1) * sizeof(row_t));
                        map.row[map.height - 1] = map.empty_line();
                        ++clear;
                    }
                }
            }

            // 4. 更新 top/roof.
            if (clear > 0)
            {
                map.roof = 0;
                for (int x = 0; x < map.width; ++x)
                {
                    map.top[x] = 0;
                    for (int y = map.height - 1; y >= 0; --y)
                    {
                        if (map.full(static_cast<std::size_t>(x),
                                     static_cast<std::size_t>(y)))
                        {
                            map.top[x] = y + 1;
                            if (y + 1 > map.roof)
                                map.roof = y + 1;
                            break;
                        }
                    }
                }
            }
            else
            {
                for (std::size_t i = 0; i < cells.count; ++i)
                {
                    int ax = xb + static_cast<int>(cells.cells[i].x);
                    int ay = yb + static_cast<int>(cells.cells[i].y);
                    if (ax >= 0 && ax < map.width && ay >= 0 && ay < map.height)
                    {
                        int new_top = ay + 1;
                        if (new_top > map.top[ax])
                        {
                            map.top[ax] = new_top;
                            if (new_top > map.roof)
                                map.roof = new_top;
                        }
                    }
                }
            }

            // 5. 更新 count.
            map.count += static_cast<int32_t>(cells.count) -
                         static_cast<int32_t>(clear) * map.width;

            return clear;
        }

    } // namespace bb
} // namespace m_tetris2

#endif // TETRIS_AI_RUNNER_BB_STATE_H_
