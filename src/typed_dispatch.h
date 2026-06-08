#pragma once

//==========================================================================
// typed_dispatch.h
//
// 编译期 r → 编译期 R 的 dispatch 工具，以及 TypedLandPoint<T, R> 落点结构体。
//
// 用途（Commit 4 Phase 1）：
//   在 Search 层 run_piece<T> 的 collect lambda 里，对运行时 lp.r 做编译期展开：
//
//     for_each_typed_r<rcount>(lp.r, [&]<size_t R>() {
//         // T, R 均为编译期，可构造 TypedLandPoint<T, R> 传给 eval
//     });
//
// 约定：
//   - R_count 通常 <= 4（SRS 七种方块最多 4 旋转状态）
//   - runtime_r 必须在 [0, R_count) 内；超界则不调用 fn，返回 false（debug 可用 assert）
//   - fn 必须是泛型 lambda（或带模板 operator() 的 functor），签名 []<size_t R>() { ... }
//==========================================================================

#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <utility>

namespace m_tetris2
{
    //----------------------------------------------------------------------
    // for_each_typed_r<R_count>(runtime_r, fn)
    //
    // 当 runtime_r == R 时，以 R 作为编译期 NTTP 调用 fn.template operator()<R>()。
    // 保证只调用一次（short-circuit ||）。
    //
    // 返回值：是否命中（runtime_r 在 [0, R_count) 范围内则为 true）。
    //----------------------------------------------------------------------
    template<std::size_t R_count, class Fn>
    bool for_each_typed_r(std::size_t runtime_r, Fn &&fn)
    {
        return [&]<std::size_t... Rs>(std::index_sequence<Rs...>) -> bool
        {
            return ((Rs == runtime_r
                         ? (std::forward<Fn>(fn).template operator()<Rs>(), true)
                         : false) ||
                    ...);
        }(std::make_index_sequence<R_count>{});
    }

    //----------------------------------------------------------------------
    // TypedLandPoint<T, R>
    //
    // 编译期持有 piece 类型 T 和旋转状态 R 的落点。
    // x / y：master 坐标（TetrisBlockStatus.x / .y 的同形语义）
    // spin：SpinType 底层值（0=None, 1=Mini, 2=Full, 3=ASpin）
    //
    // 只存几何数据，不含 eval 结果。eval 结果由调用方写入 TreeNode。
    //----------------------------------------------------------------------
    template<char T, std::uint8_t R>
    struct TypedLandPoint
    {
        static constexpr char     piece    = T;
        static constexpr uint8_t  rotation = R;

        int8_t  x    = 0;
        int8_t  y    = 0;
        uint8_t spin = 0;   ///< SpinType 底层值
    };

} // namespace m_tetris2
