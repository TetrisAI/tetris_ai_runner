// typed_dispatch_test: 验证 for_each_typed_r<> 的 dispatch 正确性
//                      以及 TypedLandPoint<T,R> 的编译期常量字段。
//
// 编译期验证（static_assert）：
//   1. TypedLandPoint<'T', 2>::piece     == 'T'
//   2. TypedLandPoint<'T', 2>::rotation  == 2
//   3. TypedLandPoint<'I', 0>::piece     == 'I'，rotation == 0
//
// 运行时验证（assert 退出码 1 = 失败）：
//   4. for_each_typed_r<4>(r, fn): 只有 r==R 时调用 fn<R>，其余不调
//   5. for_each_typed_r<1>(0, fn): R_count=1 单态也能正确命中
//   6. for_each_typed_r<4>(99, fn): 超界时返回 false，fn 不被调

#include "typed_dispatch.h"

#include <cassert>
#include <array>
#include <cstddef>

using namespace m_tetris2;

// ─── 1-3. TypedLandPoint 编译期常量 ─────────────────────────────────────────

static_assert(TypedLandPoint<'T', 2>::piece    == 'T',
    "TypedLandPoint<'T',2>::piece must be 'T'");
static_assert(TypedLandPoint<'T', 2>::rotation == 2,
    "TypedLandPoint<'T',2>::rotation must be 2");
static_assert(TypedLandPoint<'I', 0>::piece    == 'I',
    "TypedLandPoint<'I',0>::piece must be 'I'");
static_assert(TypedLandPoint<'I', 0>::rotation == 0,
    "TypedLandPoint<'I',0>::rotation must be 0");
static_assert(TypedLandPoint<'S', 3>::piece    == 'S',
    "TypedLandPoint<'S',3>::piece must be 'S'");
static_assert(TypedLandPoint<'S', 3>::rotation == 3,
    "TypedLandPoint<'S',3>::rotation must be 3");

// ─── 4-6. for_each_typed_r 运行时行为 ───────────────────────────────────────

int main()
{
    // ── 4. R_count=4：每个 r 值只触发对应的 R ──
    for (std::size_t target = 0; target < 4; ++target)
    {
        std::array<int, 4> hit{};
        bool ok = for_each_typed_r<4>(target, [&]<std::size_t R>()
        {
            hit[R]++;
        });
        assert(ok && "for_each_typed_r<4>: in-range r must return true");
        // 只有 target 位被命中，其余为 0
        for (std::size_t r = 0; r < 4; ++r)
            assert(hit[r] == (r == target ? 1 : 0)
                && "for_each_typed_r<4>: exactly one slot must be hit");
    }

    // ── 5. R_count=1：只有 r=0 合法 ──
    {
        int hit = 0;
        bool ok = for_each_typed_r<1>(0, [&]<std::size_t R>()
        {
            static_assert(R == 0, "R_count=1: R must be 0");
            hit++;
        });
        assert(ok && hit == 1 && "for_each_typed_r<1>(0): must hit exactly once");
    }

    // ── 6. 超界：r >= R_count → fn 不调，返回 false ──
    {
        int hit = 0;
        bool ok = for_each_typed_r<4>(99, [&]<std::size_t R>()
        {
            hit++;
        });
        assert(!ok  && "for_each_typed_r<4>(99): out-of-range must return false");
        assert(hit == 0 && "for_each_typed_r<4>(99): fn must NOT be called");
    }

    // ── 7. 验证 TypedLandPoint 数据字段可正常赋值与读取 ──
    {
        TypedLandPoint<'T', 2> lp;
        lp.x    = 3;
        lp.y    = 18;
        lp.spin = 2; // SpinType::Full
        assert(lp.x    ==  3 && "TypedLandPoint.x");
        assert(lp.y    == 18 && "TypedLandPoint.y");
        assert(lp.spin ==  2 && "TypedLandPoint.spin");
        static_assert(TypedLandPoint<'T', 2>::piece    == 'T');
        static_assert(TypedLandPoint<'T', 2>::rotation ==  2);
    }

    return 0;
}
