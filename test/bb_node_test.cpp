// bb_node_test.cpp — 验证 BBNode<Details,SpinT> 的编译期字段、坐标语义、
//                    monostate 偏特化无 spin 成员。
//
// 编译期验证（static_assert）：
//   1. BBNodeBase::kHeight == 4（SRS 全量 4 行 OpLines）
//   2. BBNodeBase::kOriginY == piece_cells.origin.y（编译期对拍）
//   3. .t / .r / .height / .origin_y 均为 integral_constant，值正确
//   4. BBNode<monostate> sizeof == 2，BBNode<SpinT> sizeof == 3
//   5. has_spin 探针：monostate 无 spin 成员，SpinT 特化有
//   9. kWidth > 0（各 piece/rotation）
//  10. kLines 编译期非空
//
// 运行时验证（assert）：
//   6. row() == y - kOriginY  （各 piece/rotation 对拍）
//   7. col() == x
//   8. SpinT 特化可正常赋值读取 spin
//  11. land_height() == row() + kHeight
//  12. width() == kWidth
//  13. line(i) 与 OpLines::data 对拍（y-down 翻转验证）

#include "bb_node.h"
#include "rule_srs.h"

#include <cassert>
#include <type_traits>
#include <cstdio>

using Spec = rule_srs::TetrisRule::rule_spec;

// 便捷别名：隐藏 BBNodeDetails 细节，与新 API 风格一致
template<char T, uint8_t R>
using Det = m_tetris2::BBNodeDetails<Spec, T, R>;

// ── 模拟 SpinType 用于测试 ──────────────────────────────────────────────────
enum class FakeSpinType : uint8_t { None = 0, Mini = 1, Full = 2 };

// ── 1. kHeight == 4（SRS 所有 piece 全部 4 行 OpLines）──────────────────────
static_assert(m_tetris2::BBNodeBase<Det<'T', 0>>::kHeight == 4, "T R=0 kHeight==4");
static_assert(m_tetris2::BBNodeBase<Det<'I', 0>>::kHeight == 4, "I R=0 kHeight==4");
static_assert(m_tetris2::BBNodeBase<Det<'O', 0>>::kHeight == 4, "O R=0 kHeight==4");
static_assert(m_tetris2::BBNodeBase<Det<'S', 1>>::kHeight == 4, "S R=1 kHeight==4");

// ── 2. kOriginY == piece_cells.origin.y（编译期对拍）────────────────────────
static_assert(
    m_tetris2::BBNodeBase<Det<'T', 0>>::kOriginY ==
        (int)m_tetris2::shape::piece_cells<Spec, 'T', 0>.origin.y,
    "T R=0 kOriginY must match piece_cells.origin.y");
static_assert(
    m_tetris2::BBNodeBase<Det<'T', 2>>::kOriginY ==
        (int)m_tetris2::shape::piece_cells<Spec, 'T', 2>.origin.y,
    "T R=2 kOriginY must match piece_cells.origin.y");
static_assert(
    m_tetris2::BBNodeBase<Det<'I', 1>>::kOriginY ==
        (int)m_tetris2::shape::piece_cells<Spec, 'I', 1>.origin.y,
    "I R=1 kOriginY must match piece_cells.origin.y");

// ── 3. integral_constant 字段的编译期值 ─────────────────────────────────────
static_assert(m_tetris2::BBNode<Det<'T', 0>>{}.t      == 'T', ".t must equal 'T'");
static_assert(m_tetris2::BBNode<Det<'T', 0>>{}.r      ==  0,  ".r must equal 0");
static_assert(m_tetris2::BBNode<Det<'T', 2>>{}.r      ==  2,  ".r must equal 2");
static_assert(m_tetris2::BBNode<Det<'I', 1>>{}.t      == 'I', ".t must equal 'I'");
static_assert(m_tetris2::BBNode<Det<'T', 0>>{}.height ==  4,  ".height must equal 4");
static_assert(
    m_tetris2::BBNode<Det<'T', 0>>{}.origin_y ==
        m_tetris2::BBNodeBase<Det<'T', 0>>::kOriginY,
    ".origin_y must equal kOriginY");

// ── 4. sizeof 验证 ───────────────────────────────────────────────────────────
using NodeNoSpin   = m_tetris2::BBNode<Det<'T', 0>, std::monostate>;
using NodeWithSpin = m_tetris2::BBNode<Det<'T', 0>, FakeSpinType>;

// monostate 特化：4 个 no_unique_address（零尺寸）+ int8_t x + int8_t y = 2 字节
static_assert(sizeof(NodeNoSpin) == 2,
    "BBNode<monostate>: size must be 2 (x + y only)");
// FakeSpinType(uint8_t) 追加 1 字节 = 3 字节
static_assert(sizeof(NodeWithSpin) == 3,
    "BBNode<FakeSpinType>: size must be 3 (x + y + spin)");

// ── 5. has_spin 探针 ─────────────────────────────────────────────────────────
namespace detail {
    template<class, class = void>
    struct has_spin : std::false_type {};
    template<class T>
    struct has_spin<T, std::void_t<decltype(std::declval<T>().spin)>> : std::true_type {};
}
static_assert(!detail::has_spin<NodeNoSpin>::value,
    "BBNode<monostate> must NOT have a spin member");
static_assert( detail::has_spin<NodeWithSpin>::value,
    "BBNode<FakeSpinType> must have a spin member");

// ── 9. kWidth > 0（编译期）──────────────────────────────────────────────────
static_assert(m_tetris2::BBNodeBase<Det<'T', 0>>::kWidth > 0, "T R=0 kWidth > 0");
static_assert(m_tetris2::BBNodeBase<Det<'I', 0>>::kWidth > 0, "I R=0 kWidth > 0");
static_assert(m_tetris2::BBNodeBase<Det<'I', 1>>::kWidth > 0, "I R=1 kWidth > 0");
static_assert(m_tetris2::BBNodeBase<Det<'O', 0>>::kWidth > 0, "O R=0 kWidth > 0");
static_assert(m_tetris2::BBNodeBase<Det<'S', 0>>::kWidth > 0, "S R=0 kWidth > 0");

// ── 10. kLines 编译期非空（顶行 mask 是 constexpr）──────────────────────────
static_assert(m_tetris2::BBNodeBase<Det<'T', 0>>::kLines.size() == 4,
    "T R=0 kLines must have 4 entries");
static_assert(m_tetris2::BBNodeBase<Det<'I', 0>>::kLines.size() == 4,
    "I R=0 kLines must have 4 entries");

int main()
{
    // ── 6. row() 对拍：row() == y - kOriginY == yb ───────────────────────────
    //
    // emit 公式：lp.y = yb + kOriginY
    //   => yb = lp.y - kOriginY
    //   => row() 应返回 yb
    //
    // T R=0
    {
        constexpr int oy = m_tetris2::BBNodeBase<Det<'T', 0>>::kOriginY;
        for (int yb = 0; yb < 20; ++yb)
        {
            m_tetris2::BBNode<Det<'T', 0>> node{};
            node.x = 3;
            node.y = static_cast<int8_t>(yb + oy);
            assert(node.row() == yb && "T R=0: row() must equal yb");
            assert(node.col() == 3  && "T R=0: col() must equal x");
        }
    }
    // T R=2
    {
        constexpr int oy = m_tetris2::BBNodeBase<Det<'T', 2>>::kOriginY;
        for (int yb = 0; yb < 20; ++yb)
        {
            m_tetris2::BBNode<Det<'T', 2>> node{};
            node.x = 2;
            node.y = static_cast<int8_t>(yb + oy);
            assert(node.row() == yb && "T R=2: row() must equal yb");
        }
    }
    // I R=1（水平 I，cy_min 不同）
    {
        constexpr int oy = m_tetris2::BBNodeBase<Det<'I', 1>>::kOriginY;
        for (int yb = 0; yb < 20; ++yb)
        {
            m_tetris2::BBNode<Det<'I', 1>> node{};
            node.x = 5;
            node.y = static_cast<int8_t>(yb + oy);
            assert(node.row() == yb && "I R=1: row() must equal yb");
        }
    }
    // O R=0
    {
        constexpr int oy = m_tetris2::BBNodeBase<Det<'O', 0>>::kOriginY;
        for (int yb = 0; yb < 20; ++yb)
        {
            m_tetris2::BBNode<Det<'O', 0>> node{};
            node.x = 4;
            node.y = static_cast<int8_t>(yb + oy);
            assert(node.row() == yb && "O R=0: row() must equal yb");
        }
    }

    // ── 7. SpinT 特化：spin 成员可正常赋值读取 ──────────────────────────────
    {
        m_tetris2::BBNode<Det<'T', 2>, FakeSpinType> node{};
        node.x    = 4;
        node.y    = 18;
        node.spin = FakeSpinType::Full;
        assert(node.x    ==  4                  && "spin node: x");
        assert(node.y    == 18                  && "spin node: y");
        assert(node.spin == FakeSpinType::Full  && "spin node: spin");
        assert(static_cast<char>(node.t)    == 'T' && "spin node: t");
        assert(static_cast<uint8_t>(node.r) ==  2  && "spin node: r");
        constexpr int oy = m_tetris2::BBNodeBase<Det<'T', 2>>::kOriginY;
        assert(node.row() == 18 - oy && "spin node: row()");
    }

    // ── 11. land_height() == row() + kHeight ────────────────────────────────
    {
        // T R=0（cy_min != 0，验证 land_height != y + 1 的情况）
        constexpr int oy = m_tetris2::BBNodeBase<Det<'T', 0>>::kOriginY;
        constexpr int kH = m_tetris2::BBNodeBase<Det<'T', 0>>::kHeight;
        for (int yb = 0; yb < 20; ++yb)
        {
            m_tetris2::BBNode<Det<'T', 0>> node{};
            node.x = 3;
            node.y = static_cast<int8_t>(yb + oy);
            assert(node.land_height() == node.row() + kH &&
                "T R=0: land_height() must equal row() + kHeight");
        }
    }
    {
        // I R=1（水平条，典型 kOriginY 值）
        constexpr int oy = m_tetris2::BBNodeBase<Det<'I', 1>>::kOriginY;
        constexpr int kH = m_tetris2::BBNodeBase<Det<'I', 1>>::kHeight;
        m_tetris2::BBNode<Det<'I', 1>> node{};
        node.x = 3;
        node.y = static_cast<int8_t>(5 + oy);
        assert(node.land_height() == node.row() + kH &&
            "I R=1: land_height() must equal row() + kHeight");
    }

    // ── 12. width() == kWidth ────────────────────────────────────────────────
    {
        using Base_T0 = m_tetris2::BBNodeBase<Det<'T', 0>>;
        using Base_I1 = m_tetris2::BBNodeBase<Det<'I', 1>>;
        using Base_O0 = m_tetris2::BBNodeBase<Det<'O', 0>>;
        m_tetris2::BBNode<Det<'T', 0>> nt{};
        m_tetris2::BBNode<Det<'I', 1>> ni{};
        m_tetris2::BBNode<Det<'O', 0>> no{};
        assert(nt.width() == Base_T0::kWidth && "T R=0: width() == kWidth");
        assert(ni.width() == Base_I1::kWidth && "I R=1: width() == kWidth");
        assert(no.width() == Base_O0::kWidth && "O R=0: width() == kWidth");
        // 输出供人工核查
        std::printf("T R=0 kWidth=%d  I R=1 kWidth=%d  O R=0 kWidth=%d\n",
            Base_T0::kWidth, Base_I1::kWidth, Base_O0::kWidth);
    }

    // ── 13. line(i) 与 OpLines::data 对拍（y-down 翻转）───────────────────────
    // piece_line<Spec,T,R,I> = OpLines::data[I]（y-up，I=0 为底行）
    // line(i) 应为 OpLines::data[kHeight - 1 - i]（y-down，i=0 为顶行）
    {
        using Base = m_tetris2::BBNodeBase<Det<'T', 0>>;
        constexpr int H = Base::kHeight;
        m_tetris2::BBNode<Det<'T', 0>> node{};
        for (int i = 0; i < H; ++i)
        {
            uint64_t expected = m_tetris2::shape::find_op<Spec, 'T', 0>::lines::data[H - 1 - i];
            assert(node.line(i) == expected && "T R=0: line(i) must match OpLines y-down flip");
        }
        // I R=0（水平 I，形状直观）
        using BaseI = m_tetris2::BBNodeBase<Det<'I', 0>>;
        constexpr int HI = BaseI::kHeight;
        m_tetris2::BBNode<Det<'I', 0>> inode{};
        for (int i = 0; i < HI; ++i)
        {
            uint64_t expected = m_tetris2::shape::find_op<Spec, 'I', 0>::lines::data[HI - 1 - i];
            assert(inode.line(i) == expected && "I R=0: line(i) must match OpLines y-down flip");
        }
        // 输出供人工核查：T R=0 的 kLines（kLines[0]=顶行，kLines[3]=底行）
        std::printf("T R=0 kLines (top->bottom): ");
        for (int i = 0; i < H; ++i)
            std::printf("0x%08llx ", (unsigned long long)node.line(i));
        std::printf("\n");
    }

    std::printf("All bb_node tests PASSED\n");
    return 0;
}
