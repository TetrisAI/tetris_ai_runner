#ifndef TETRIS_AI_RUNNER_TETRIS_MAP_H_
#define TETRIS_AI_RUNNER_TETRIS_MAP_H_

//==========================================================================
// Compile-time-sized tetris board container.
//
// 设计目标:
// - 与 cobra::Board<H> 思路一致: 把整张棋盘按 W*H 的 packed bits 塞进一个
//   BoardVec<uint64_t, Tn> 里, 以便所有 row 级位运算都能向量化.
// - W / H 都是模板参数; row_t 在 Map 内部按 W 选最窄的无符号类型.
//   * W <= 8 用 uint8_t, <= 16 用 uint16_t, <= 32 用 uint32_t, <= 64 用 uint64_t.
//   * 上限 64 = lane_t (uint64) 全宽; W>64 需要拆 lane, 不在本容器范畴.
// - 对外提供与现有 TetrisMap 等价的"按行视图":
//     row_t row(int y) const;          //取第 y 行(低位是 x=0)
//     void  set_row(int y, row_t v);   //写第 y 行
//   这样 Phase 2 在重写 Search/AI 之前, Map 与 TetrisMap 之间可以做对拍.
// - 不引入 cobra 的 do_move / clear_lines 加速版本; 这些留给 movegen 阶段.
//   本头文件只负责"棋盘的存储与基础位运算", 不包含规则/形状.
//
// 与 cobra::Board<H> 的对应:
//   cobra::Board<H>::T            -> uint64_t  (内部 lane 类型)
//   cobra::Board<H>::Tbits        -> 64
//   cobra::Board<H>::Tlines       -> Tbits / W
//   cobra::Board<H>::Tn           -> ((H - 1) / Tlines) + 1
//   cobra::Board<H>::Bitboard     -> simd::BoardVec<uint64_t, Tn>
//   cobra::Board<H>::all()        -> Map<W,H>::all_mask()
//   cobra::Board<H>::col_mask<x>  -> Map<W,H>::col_mask<x>()
//==========================================================================

#include "tetris_simd.h"

#include <bit>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace m_tetris2
{
    namespace board
    {
        //按 W 选最窄的无符号 row_t.
        //保留 1 个高位余量, 避免 W==32 时 (1u << W) 溢出造成的歧义; 同理 W==64
        //  落到 uint64_t 时 row_full 的全 1 掩码也走 ~row_t(0) 分支.
        template<int W>
        struct row_type_selector
        {
            using type =
                std::conditional_t<(W <= 7), std::uint8_t,
                                   std::conditional_t<(W <= 15), std::uint16_t,
                                                      std::conditional_t<(W <= 31), std::uint32_t,
                                                                         std::uint64_t>>>;
        };

        //内部 lane 类型固定 uint64_t, 向 cobra 的 64bit 通道对齐.
        using lane_t = std::uint64_t;
        static constexpr int lane_bits = sizeof(lane_t) * 8;

        //每条通道能放下多少完整的行.
        constexpr int compute_lane_lines(int W) noexcept
        {
            return lane_bits / W;
        }
        //铺满整个棋盘需要多少条通道.
        constexpr int compute_lane_count(int W, int H) noexcept
        {
            int lines_per_lane = compute_lane_lines(W);
            return ((H - 1) / lines_per_lane) + 1;
        }
    }

    //W: 棋盘宽度(列数), 必须 1..32
    //H: 棋盘高度(行数), 必须 >= 1
    template<int W, int H>
    struct Map
    {
        static_assert(W >= 1 && W <= 64, "Map<W, H> requires 1 <= W <= 64");
        static_assert(H >= 1, "Map<W, H> requires H >= 1");

        static constexpr int width = W;
        static constexpr int height = H;
        static constexpr int lane_bits = board::lane_bits;
        static constexpr int lane_lines = board::compute_lane_lines(W); //每条 lane 容纳行数
        static constexpr int lane_count = board::compute_lane_count(W, H);
        static constexpr board::lane_t lane_full = (lane_lines * W >= lane_bits)
                                                       ? ~board::lane_t(0)
                                                       : (board::lane_t(1) << (lane_lines * W)) - 1;
        //单行 W 位的全 1 mask.
        using row_t = typename board::row_type_selector<W>::type;
        static constexpr row_t row_full = (W >= int(sizeof(row_t) * 8))
                                              ? row_t(~row_t(0))
                                              : row_t((row_t(1) << W) - row_t(1));

        using vec_t = simd::BoardVec<board::lane_t, static_cast<std::size_t>(lane_count)>;

        //主存储: lane_count 个 64bit 通道, 每条通道里塞 lane_lines 行(每行 W 位).
        vec_t data;

        //----- 构造 -----
        constexpr Map() noexcept : data{} {}
        explicit constexpr Map(vec_t const &v) noexcept : data(v) {}

        //----- 几何工具 -----
        static constexpr bool is_ok_x(int x) noexcept
        {
            return x >= 0 && x < W;
        }
        static constexpr bool is_ok_y(int y) noexcept
        {
            return y >= 0 && y < H;
        }

        //把 (x, y) 翻译成 (lane_index, bit_index_within_lane).
        struct CellAddr
        {
            int lane;
            int bit;
        };
        static constexpr CellAddr cell_addr(int x, int y) noexcept
        {
            int lane = y / lane_lines;
            int row_in_lane = y - lane * lane_lines;
            int bit = row_in_lane * W + x;
            return CellAddr{lane, bit};
        }

        //----- 单格 -----
        constexpr void set(int x, int y) noexcept
        {
            assert(is_ok_x(x) && is_ok_y(y));
            CellAddr a = cell_addr(x, y);
            data.data[static_cast<std::size_t>(a.lane)] |= (board::lane_t(1) << a.bit);
        }
        constexpr void clear(int x, int y) noexcept
        {
            assert(is_ok_x(x) && is_ok_y(y));
            CellAddr a = cell_addr(x, y);
            data.data[static_cast<std::size_t>(a.lane)] &= ~(board::lane_t(1) << a.bit);
        }
        constexpr bool get(int x, int y) const noexcept
        {
            assert(is_ok_x(x) && is_ok_y(y));
            CellAddr a = cell_addr(x, y);
            return (data.data[static_cast<std::size_t>(a.lane)] >> a.bit) & board::lane_t(1);
        }

        //----- 整行视图 (兼容现有 TetrisMap 的 row_t 接口) -----
        constexpr row_t row(int y) const noexcept
        {
            assert(is_ok_y(y));
            CellAddr a = cell_addr(0, y);
            board::lane_t bits = data.data[static_cast<std::size_t>(a.lane)] >> a.bit;
            return static_cast<row_t>(bits & board::lane_t(row_full));
        }
        constexpr void set_row(int y, row_t v) noexcept
        {
            assert(is_ok_y(y));
            CellAddr a = cell_addr(0, y);
            board::lane_t mask = board::lane_t(row_full) << a.bit;
            board::lane_t v_ext = (board::lane_t(v) & board::lane_t(row_full)) << a.bit;
            data.data[static_cast<std::size_t>(a.lane)] =
                (data.data[static_cast<std::size_t>(a.lane)] & ~mask) | v_ext;
        }

        //----- 静态 mask -----
        //整个棋盘的有效位 mask (低 lane_lines*W 位 = 1 的多 lane 组合).
        static consteval vec_t all_mask() noexcept
        {
            vec_t v{};
            //每个 lane 都填到 lane_full, 但最后一 lane 可能只用了一部分.
            //总有效行数 H, 已用 (lane_count - 1) * lane_lines 行覆盖前面所有 lane.
            //最后一个 lane 用 (H - (lane_count - 1) * lane_lines) 行.
            int lines_in_last = H - (lane_count - 1) * lane_lines;
            int last_bits = lines_in_last * W;
            board::lane_t last_mask = (last_bits >= lane_bits)
                                          ? ~board::lane_t(0)
                                          : (board::lane_t(1) << last_bits) - 1;
            for (int i = 0; i < lane_count - 1; ++i)
                v.data[static_cast<std::size_t>(i)] = lane_full;
            v.data[static_cast<std::size_t>(lane_count - 1)] = last_mask;
            return v;
        }

        //第 x 列 mask (在每一行的 x 位置都置 1).
        template<int X>
        static consteval vec_t col_mask() noexcept
        {
            static_assert(X >= 0 && X < W);
            Map m{};
            for (int y = 0; y < H; ++y)
                m.set(X, y);
            return m.data;
        }

        //----- 位运算 -----
        constexpr Map operator|(Map const &o) const noexcept
        {
            return Map(data | o.data);
        }
        constexpr Map operator&(Map const &o) const noexcept
        {
            return Map(data & o.data);
        }
        constexpr Map operator^(Map const &o) const noexcept
        {
            return Map(data ^ o.data);
        }
        constexpr Map operator~() const noexcept
        {
            return Map((~data) & all_mask());
        }
        constexpr Map &operator|=(Map const &o) noexcept
        {
            data |= o.data;
            return *this;
        }
        constexpr Map &operator&=(Map const &o) noexcept
        {
            data &= o.data;
            return *this;
        }
        constexpr Map &operator^=(Map const &o) noexcept
        {
            data ^= o.data;
            return *this;
        }

        constexpr bool operator==(Map const &o) const noexcept
        {
            return data == o.data;
        }
        constexpr bool operator!=(Map const &o) const noexcept
        {
            return data != o.data;
        }

        constexpr bool any() const noexcept
        {
            return data.any();
        }
        constexpr bool none() const noexcept
        {
            return data.none();
        }
        constexpr int popcount() const noexcept
        {
            return data.popcount();
        }

        //最顶端有方块的行号(从 0 起算, 全空返回 0).
        //对应 cobra Board<H>::max_y.
        constexpr int max_y() const noexcept
        {
            auto h = data.highest_bit();
            if (!h.valid())
                return 0;
            int lane = static_cast<int>(h.lane);
            int bit = static_cast<int>(h.bit);
            return lane * lane_lines + bit / W;
        }

        //----- 整图位移 -----
        //把整张棋盘按 (Dx, Dy) 平移. Dx 是列(向右为正), Dy 是行(向上为正).
        //越界的部分丢弃, 留出的位置填 0. 对应 cobra Board<H>::shifted<dx, dy>.
        //
        //思路: 与 cobra 一致, 把 (Dx, Dy) 拆成
        //   - 跨 lane 的整 lane 平移 (LaneShift = (|Dy| 中已对齐到 lane_lines 的部分))
        //   - lane 内的 bit 平移 ((Dy 余数) * W + Dx)
        //最后用 all_mask() 截断溢出位, 再用 shift_col_mask<Dx>() 砍掉横向越界.
    private:
        //生成"对 Dx 列平移后, 仍然落在棋盘内"的列 mask.
        //Dx > 0 时, 最左 Dx 列变成无效(因为它们是从棋盘外平移进来的); 反之亦然.
        template<int Dx>
        static consteval vec_t shift_col_keep_mask() noexcept
        {
            Map m{};
            if constexpr (Dx >= 0)
            {
                for (int x = Dx; x < W; ++x)
                    for (int y = 0; y < H; ++y)
                        m.set(x, y);
            }
            else
            {
                for (int x = 0; x < W + Dx; ++x)
                    for (int y = 0; y < H; ++y)
                        m.set(x, y);
            }
            return m.data;
        }

    public:
        template<int Dx, int Dy>
        constexpr Map shifted() const noexcept
        {
            if constexpr (Dx == 0 && Dy == 0)
                return *this;

            //先做行向位移 (Dy), 后做列向位移 (Dx); cobra 也是这个顺序.
            vec_t v = data;

            if constexpr (Dy > 0)
            {
                constexpr int lane_step = Dy / lane_lines;
                constexpr int row_in_lane = Dy - lane_step * lane_lines;
                constexpr int bit_step = row_in_lane * W;
                if constexpr (lane_step != 0)
                    v = v.template shifted_lanes<lane_step>();
                if constexpr (bit_step > 0)
                {
                    //跨 lane 的"小数行"平移: 把每条 lane 左移 bit_step 位, 同时把
                    //溢出到 lane 顶部的高位补到下一条 lane 的低位.
                    //  注意: 一条 lane 实际只用 (lane_lines * W) 位 (= lane_used),
                    //  剩余 lane_bits - lane_used 个 bit 是 padding, 必须先用
                    //  all_mask() 截掉它们. 把 lane[i] 顶部的 bit_step 位
                    //  (range [lane_used - bit_step, lane_used)) 提取出来, 放到
                    //  lane[i+1] 的 bit 0..bit_step-1.
                    constexpr int lane_used = lane_lines * W;
                    vec_t low = v.template shifted_in_lane_left<bit_step>();
                    vec_t high = v.template shifted_in_lane_right<lane_used - bit_step>();
                    high = high.template shifted_lanes<1>();
                    v = low | high;
                }
            }
            else if constexpr (Dy < 0)
            {
                constexpr int neg = -Dy;
                constexpr int lane_step = neg / lane_lines;
                constexpr int row_in_lane = neg - lane_step * lane_lines;
                constexpr int bit_step = row_in_lane * W;
                if constexpr (lane_step != 0)
                    v = v.template shifted_lanes<-lane_step>();
                if constexpr (bit_step > 0)
                {
                    //同上: lane[i] 底部 bit_step 位被推到 lane[i-1] 顶部
                    //  (range [lane_used - bit_step, lane_used)).
                    constexpr int lane_used = lane_lines * W;
                    vec_t high = v.template shifted_in_lane_right<bit_step>();
                    vec_t low = v.template shifted_in_lane_left<lane_used - bit_step>();
                    low = low.template shifted_lanes<-1>();
                    v = high | low;
                }
            }

            //截断: 行向溢出会被 all_mask 砍掉.
            v &= all_mask();

            //再做列向 (Dx) 平移.
            if constexpr (Dx > 0)
                v = v.template shifted_in_lane_left<Dx>();
            else if constexpr (Dx < 0)
                v = v.template shifted_in_lane_right<-Dx>();

            //横向溢出: 进出列范围的位置归 0.
            if constexpr (Dx != 0)
                v &= shift_col_keep_mask<Dx>();
            //再次和 all_mask 与, 防止 Dx 平移把 lane 内多余高位带回来.
            v &= all_mask();
            return Map(v);
        }

        //----- 枚举所有置位的格子 -----
        //对每个为 1 的 cell 调用 fn(int x, int y). 顺序: lane 升序, lane 内 bit 升序.
        //对应 cobra Board<H>::for_each_set_bit. 用于 movegen 输出落点列表.
        template<class Fn>
        constexpr void for_each_set_bit(Fn &&fn) const noexcept(noexcept(fn(0, 0)))
        {
            for (int i = 0; i < lane_count; ++i)
            {
                board::lane_t bits = data.data[static_cast<std::size_t>(i)];
                while (bits != 0)
                {
                    int bit = std::countr_zero(bits);
                    int row_in_lane = bit / W;
                    int x = bit - row_in_lane * W;
                    int y = i * lane_lines + row_in_lane;
                    if (y < H && x < W)
                        fn(x, y);
                    bits &= bits - 1;
                }
            }
        }

        //----- 满行检测 -----
        //返回一张 Map: 在所有"行 == row_full"的 (x, y) 都置 1, 其余 0.
        //对应 cobra Board<H>::line_clears, 但实现走 row 逐行遍历, 避免 cobra 的
        //packed-row 加法技巧对 W 的特殊依赖, 同时保持 W 任意 1..32 的正确性.
        constexpr Map line_clears() const noexcept
        {
            Map result{};
            for (int y = 0; y < H; ++y)
                if (row(y) == row_full)
                    for (int x = 0; x < W; ++x)
                        result.set(x, y);
            return result;
        }

        //----- 消行 -----
        //输入: line_clears() 的输出 (满行处全 row_full, 其余全 0).
        //语义: 把 lines 标记的行抹掉, 上方所有行向下塌陷, 顶部空出的行补 0.
        //对应 cobra Board<H>::clear_lines.
        //
        //实现: 走逐行 row()/set_row() 双指针. W=10 H=40 时最多 40 次行复制,
        //不是热点(每次 do_move 才调一次), 不为它做 SIMD 特化.
        constexpr Map clear_lines(Map const &lines) const noexcept
        {
            Map result{};
            int dst = 0;
            for (int y = 0; y < H; ++y)
            {
                if (lines.row(y) != row_t(0))
                    continue;
                result.set_row(dst, row(y));
                ++dst;
            }
            return result;
        }
    };
}

#endif // TETRIS_AI_RUNNER_TETRIS_MAP_H_
