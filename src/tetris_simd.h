#ifndef TETRIS_AI_RUNNER_TETRIS_SIMD_H_
#define TETRIS_AI_RUNNER_TETRIS_SIMD_H_

//==========================================================================
// Portable SIMD-friendly bitboard primitives.
//
// 设计目标:
// - 与 cobra-movegen 思路对齐: 把整张棋盘塞进若干个 64bit 通道里, 让所有
//   row 级位运算在一次"宽寄存器"操作中完成, 编译器自动向量化.
// - 规避 gcc 扩展 [[gnu::vector_size]], 改用纯标准 C++20 的 std::array
//   包装. 现代编译器(clang/gcc/msvc)对长度已知的 std::array 上的
//   element-wise 循环, 配合 -O3 / /O2 都能向量化到 SSE2/AVX2/NEON.
// - 接口最小化: 只暴露 cobra Board 真正用到的算子(|, &, ~, ^, == ,
//   shifted, popcount, max_bit, set/get/store/load).
// - 保留 cobra 的所有静态形状: Tbits / Tlines / Tn 在编译期推导.
//
// 与 cobra Board<H> 的对应:
//   cobra::Board<H>::Bitboard      -> m_tetris2::BoardVec<uint64_t, Tn>
//   cobra::Board<H>::T             -> uint64_t (固定, 单独一个 lane 的位宽)
//
// 注意:
// - 跨 lane 的 shift_h (整行移位) 在这里只提供"整 lane"粒度的接口; 半 lane
//   粒度的 cross-lane shift 由后续 Map<W, H> 在编译期组合实现, 避免在
//   BoardVec 这一层引入耦合 H 几何的逻辑.
// - 所有方法都标记 constexpr/inline; 头文件 only, 无符号外泄.
//==========================================================================

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace m_tetris2
{
    namespace simd
    {
        //一个长度固定的 64bit 通道数组.
        //Lanes 在编译期已知, 便于编译器展开/向量化.
        template<class T, std::size_t Lanes>
        struct BoardVec
        {
            static_assert(std::is_unsigned_v<T>, "BoardVec lane type must be unsigned");
            static_assert(Lanes > 0, "BoardVec must have at least one lane");

            using lane_t = T;
            static constexpr std::size_t lanes = Lanes;
            static constexpr std::size_t lane_bits = sizeof(T) * 8;

            std::array<T, Lanes> data;

            //----- 构造 / 工厂 -----
            constexpr BoardVec() noexcept : data{} {}

            //单 lane 立即数广播到全部通道.
            static constexpr BoardVec splat(T v) noexcept
            {
                BoardVec b{};
                for (std::size_t i = 0; i < Lanes; ++i)
                    b.data[i] = v;
                return b;
            }

            //每条通道独立 mask, Lanes 长度需相同.
            static constexpr BoardVec from_array(std::array<T, Lanes> const &arr) noexcept
            {
                BoardVec b{};
                b.data = arr;
                return b;
            }

            //----- 索引 -----
            constexpr T &operator[](std::size_t i) noexcept
            {
                return data[i];
            }
            constexpr T const &operator[](std::size_t i) const noexcept
            {
                return data[i];
            }

            //----- 元素级位运算 -----
            constexpr BoardVec operator|(BoardVec const &o) const noexcept
            {
                BoardVec r{};
                for (std::size_t i = 0; i < Lanes; ++i)
                    r.data[i] = data[i] | o.data[i];
                return r;
            }
            constexpr BoardVec operator&(BoardVec const &o) const noexcept
            {
                BoardVec r{};
                for (std::size_t i = 0; i < Lanes; ++i)
                    r.data[i] = data[i] & o.data[i];
                return r;
            }
            constexpr BoardVec operator^(BoardVec const &o) const noexcept
            {
                BoardVec r{};
                for (std::size_t i = 0; i < Lanes; ++i)
                    r.data[i] = data[i] ^ o.data[i];
                return r;
            }
            constexpr BoardVec operator~() const noexcept
            {
                BoardVec r{};
                for (std::size_t i = 0; i < Lanes; ++i)
                    r.data[i] = ~data[i];
                return r;
            }

            constexpr BoardVec &operator|=(BoardVec const &o) noexcept
            {
                *this = *this | o;
                return *this;
            }
            constexpr BoardVec &operator&=(BoardVec const &o) noexcept
            {
                *this = *this & o;
                return *this;
            }
            constexpr BoardVec &operator^=(BoardVec const &o) noexcept
            {
                *this = *this ^ o;
                return *this;
            }

            //----- 整通道粒度的左/右移 (跨 lane) -----
            //把整个 BoardVec 看作 Lanes*lane_bits 位的大整数, 向高位推 Bits 位.
            //Bits 必须是 lane_bits 的整数倍, 这是 cobra 里整 lane 平移的常见情况.
            //不对齐到 lane_bits 的位移交由 Map 自己组合: 先 shift_lane, 再 shift_in_lane.
            template<int LaneShift>
            constexpr BoardVec shifted_lanes() const noexcept
            {
                BoardVec r{};
                if constexpr (LaneShift > 0)
                {
                    for (std::size_t i = 0; i < Lanes; ++i)
                    {
                        std::size_t src = i >= static_cast<std::size_t>(LaneShift)
                                              ? i - static_cast<std::size_t>(LaneShift)
                                              : Lanes;
                        r.data[i] = (src < Lanes) ? data[src] : T(0);
                    }
                }
                else if constexpr (LaneShift < 0)
                {
                    constexpr std::size_t k = static_cast<std::size_t>(-LaneShift);
                    for (std::size_t i = 0; i < Lanes; ++i)
                    {
                        std::size_t src = i + k;
                        r.data[i] = (src < Lanes) ? data[src] : T(0);
                    }
                }
                else
                {
                    r = *this;
                }
                return r;
            }

            //每条通道内独立的左/右位移. 不跨 lane.
            template<int Bits>
            constexpr BoardVec shifted_in_lane_left() const noexcept
            {
                BoardVec r{};
                if constexpr (Bits == 0)
                    r = *this;
                else if constexpr (Bits > 0 && Bits < static_cast<int>(lane_bits))
                {
                    for (std::size_t i = 0; i < Lanes; ++i)
                        r.data[i] = data[i] << Bits;
                }
                return r;
            }
            template<int Bits>
            constexpr BoardVec shifted_in_lane_right() const noexcept
            {
                BoardVec r{};
                if constexpr (Bits == 0)
                    r = *this;
                else if constexpr (Bits > 0 && Bits < static_cast<int>(lane_bits))
                {
                    for (std::size_t i = 0; i < Lanes; ++i)
                        r.data[i] = data[i] >> Bits;
                }
                return r;
            }

            //----- 比较 -----
            constexpr bool operator==(BoardVec const &o) const noexcept
            {
                for (std::size_t i = 0; i < Lanes; ++i)
                    if (data[i] != o.data[i])
                        return false;
                return true;
            }
            constexpr bool operator!=(BoardVec const &o) const noexcept
            {
                return !(*this == o);
            }

            //任意位置非零?
            constexpr bool any() const noexcept
            {
                T acc = 0;
                for (std::size_t i = 0; i < Lanes; ++i)
                    acc |= data[i];
                return acc != 0;
            }
            constexpr bool none() const noexcept
            {
                return !any();
            }

            //总置位数.
            constexpr int popcount() const noexcept
            {
                int total = 0;
                for (std::size_t i = 0; i < Lanes; ++i)
                    total += std::popcount(data[i]);
                return total;
            }

            //最高位的 (lane_index, bit_index_within_lane). 如果全 0 返回 {Lanes, 0}.
            //返回 std::pair 而不是 std::optional, 避免运行期开销.
            struct HighestBit
            {
                std::size_t lane; //== Lanes 表示找不到
                std::size_t bit; //该 lane 内的位下标
                constexpr bool valid() const noexcept
                {
                    return lane < Lanes;
                }
            };
            constexpr HighestBit highest_bit() const noexcept
            {
                for (std::size_t i = Lanes; i > 0; --i)
                {
                    std::size_t lane = i - 1;
                    T bits = data[lane];
                    if (bits != 0)
                    {
                        std::size_t pos = static_cast<std::size_t>(lane_bits - 1) - static_cast<std::size_t>(std::countl_zero(bits));
                        return HighestBit{lane, pos};
                    }
                }
                return HighestBit{Lanes, 0};
            }
        };

        //方便外部按 lane 类型 + 通道数显式实例化.
        using BoardVec64x1 = BoardVec<uint64_t, 1>;
        using BoardVec64x2 = BoardVec<uint64_t, 2>;
        using BoardVec64x3 = BoardVec<uint64_t, 3>;
        using BoardVec64x4 = BoardVec<uint64_t, 4>;
    }
}

#endif // TETRIS_AI_RUNNER_TETRIS_SIMD_H_
