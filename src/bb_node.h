#pragma once

//==========================================================================
// bb_node.h — 编译期 piece 落点节点，供 AI eval 接口使用。
//
// BBNodeDetails<Spec, T, R>
//   将 <Spec, T, R> 三参数打包为单一类型，供 BBNode 第一模板参数使用。
//   AI 编写者通过 Details 的静态成员访问编译期信息：
//     Details::t_value  — char，piece 类型字符 ('T' / 'I' / ...)
//     Details::r_value  — uint8_t，旋转索引 (0 / 1 / 2 / 3)
//     Details::spec_type — Spec 类型本身
//
// BBNodeBase<Details>
//   核心字段：
//     编译期（[[no_unique_address]]，零尺寸）：
//       t        — piece 类型字符 ('T' / 'I' / ...)
//       r        — 旋转索引 (0 / 1 / 2 / 3)
//       height   — bbox NxN 全高（= piece_h<Spec,T,R> = OpLines::size）
//       origin_y — bbox 底行到矩阵底行偏移（= H - 1 - cy_min）
//     运行时：
//       x        — bbox 左下角列（= 旧 node.col / status.x）
//       y        — bbox 顶行绝对坐标（y-up，= 旧 node.status.y）
//     静态（编译期计算，inline constexpr）：
//       kWidth   — bbox 实际列数（cx_max - cx_min + 1）
//       kLines   — [kHeight] 每行 mask（y-down，row=0 为顶行）
//   方法：
//     row()         — bbox 底行（= y - kOriginY）           旧 node.row
//     col()         — 列（= x）                             旧 node.col
//     land_height() — 落点高度（= row() + kHeight）         旧 node.row + node.height
//     width()       — bbox 列数（= kWidth）                 旧 node.width
//     line(int i)   — 第 i 行 mask（y-down，i=0 为顶行）   旧 node.data[i]
//
// BBNode<Details, SpinT>
//   在 BBNodeBase 基础上追加 SpinT spin 字段（TSpinType / ASpinType）。
//
// BBNode<Details, std::monostate>（偏特化）
//   NoSpinHook 路径，移除 spin 成员，尺寸与 BBNodeBase 等同。
//
// 坐标语义（与 LandingPosT 一致）：
//   x        = bbox 左下角列（= master status.x - origin.x，即旧 node.col）
//   y        = bbox 顶行绝对坐标（y-up，= yb + origin_y，即旧 node.status.y）
//   origin_y = piece_cells<Spec,T,R>.origin.y = H - 1 - cy_min
//   row()    = bbox 底行 = y - origin_y   （= 旧 node.row）
//   col()    = x                           （= 旧 node.col）
//
// height / kHeight 语义：
//   kHeight = piece_h<Spec,T,R> = OpLines::size，原始 NxN box 全高（含空行）。
//   origin_y = H - 1 - cy_min，≠ kHeight（cy_min 通常不为 0）。
//   land_height() = row() + kHeight（= 旧 node.row + node.height）。
//   注意：y + 1（旧 status.y + 1）与 land_height() 不等，仅当 cy_min == 0 时相同。
//
// line(i) 语义：
//   kLines[i] = 第 i 行 mask（y-down，i=0 为顶行，i=kHeight-1 为底行）。
//   对应旧 node.data[i]。mask 的 bit j 表示第 j 列有方块，绝对列坐标 = col() + j。
//
// 使用示例（AI 编写者视角）：
//   template<class Details, class SpinType>
//   double eval(BBNode<Details, SpinType> node, SpinType spin, ...) const {
//       // 编译期信息
//       constexpr char    T = Details::t_value;
//       constexpr uint8_t R = Details::r_value;
//       // 运行时信息
//       int row = node.row();
//       int col = node.col();
//   }
//==========================================================================

#include "tetris_shape.h"
#include "tetris_map.h"

#include <array>
#include <cstdint>
#include <type_traits>
#include <variant> // std::monostate

namespace m_tetris2
{
    //----------------------------------------------------------------------
    // detail helpers：编译期推导 kWidth 和 kLines
    //----------------------------------------------------------------------
    namespace detail
    {
        // bbox 宽度 = 归一化后 cells x 最大值 + 1
        // （cells_impl 已经把 cx_min 减掉，所以 x in [0, cx_max-cx_min]）
        template<class Spec, char T, uint8_t R>
        consteval int calc_kwidth()
        {
            constexpr auto cl = shape::piece_cells<Spec, T, R>;
            int w = 0;
            for (size_t i = 0; i < cl.count; ++i)
                if (cl.cells[i].x + 1 > w)
                    w = cl.cells[i].x + 1;
            return w;
        }

        // kLines[i]：第 i 行 mask，y-down（i=0 为顶行）
        // piece_line<Spec,T,R,I> 是 y-up（I=0 为底行），所以：
        //   kLines[i] = piece_line<..., kHeight - 1 - i>
        // 用运行期数组缓存（每个 (Spec,T,R) 实例化一次）。
        template<class Spec, char T, uint8_t R, size_t... Is>
        consteval auto make_lines_impl(std::index_sequence<Is...>)
        {
            constexpr size_t H = shape::piece_h<Spec, T, R>;
            // Is 是 0..H-1（top-down），第 i 行对应 piece_line row index = H-1-i
            return std::array<uint64_t, H>{
                static_cast<uint64_t>(
                    shape::find_op<Spec, T, R>::lines::data[H - 1 - Is])...};
        }

        template<class Spec, char T, uint8_t R>
        consteval auto make_lines()
        {
            constexpr size_t H = shape::piece_h<Spec, T, R>;
            return make_lines_impl<Spec, T, R>(std::make_index_sequence<H>{});
        }
    }

    //----------------------------------------------------------------------
    // BBNodeDetails<Spec, T, R>
    //
    // 将三个模板参数打包为单一类型，隐藏 AI 编写者视角中的细节。
    // AI eval 签名只需要 template<class Details, class SpinType>，
    // 内部通过 Details::t_value / Details::r_value / Details::spec_type 访问。
    //----------------------------------------------------------------------
    template<class Spec, char T, uint8_t R>
    struct BBNodeDetails
    {
        using spec_type = Spec;
        using map_type = Map<Spec::width, Spec::height>;
        static constexpr char t_value = T;
        static constexpr uint8_t r_value = R;
    };

    //----------------------------------------------------------------------
    // BBNodeBase<Details>
    //
    // 公共基类，持有编译期 t / r / height / origin_y 以及运行时 x / y。
    // 不含 spin 字段；spin 由派生类（BBNode 两个特化）按需添加。
    //----------------------------------------------------------------------
    template<class Details>
    struct BBNodeBase
    {
        using spec_type = typename Details::spec_type;
        static constexpr char T = Details::t_value;
        static constexpr uint8_t R = Details::r_value;

        // ── 编译期字段（零尺寸，隐式转换到基础类型）──────────────────────
        [[no_unique_address]] std::integral_constant<char, T> t;
        [[no_unique_address]] std::integral_constant<uint8_t, R> r;

        // kHeight = OpLines::size（原始 NxN box 高度，含空行）
        static constexpr int kHeight =
            static_cast<int>(shape::piece_h<spec_type, T, R>);
        [[no_unique_address]] std::integral_constant<int, kHeight> height;

        // kOriginY = H - 1 - cy_min（piece bbox 底行到 NxN 矩阵底行的偏移）
        // emit 时：lp.y = yb + kOriginY，因此 row() = y - kOriginY = yb。
        static constexpr int kOriginY =
            static_cast<int>(shape::piece_cells<spec_type, T, R>.origin.y);
        [[no_unique_address]] std::integral_constant<int, kOriginY> origin_y;

        // kWidth = bbox 实际列数（cx_max - cx_min + 1）
        static constexpr int kWidth = detail::calc_kwidth<spec_type, T, R>();

        // kLines[i]：第 i 行 mask，y-down（i=0 为顶行）= 旧 node.data[i]
        static constexpr auto kLines = detail::make_lines<spec_type, T, R>();

        // ── 运行时字段 ────────────────────────────────────────────────────
        int8_t x; ///< bbox 左下角列（= 旧 node.col / status.x - origin.x）
        int8_t y; ///< bbox 顶行绝对坐标 y-up（= 旧 node.status.y）

        /// bbox 底行，= 旧 node.row（= y - kOriginY）
        [[nodiscard]] int row() const noexcept
        {
            return y - kOriginY;
        }

        /// 列，= 旧 node.col（= x）
        [[nodiscard]] int col() const noexcept
        {
            return x;
        }

        /// 落点高度，= 旧 node.row + node.height（= row() + kHeight）
        /// 注意：≠ y + 1（除非 cy_min == 0）
        [[nodiscard]] int land_height() const noexcept
        {
            return row() + kHeight;
        }

        /// bbox 实际列数，= 旧 node.width
        [[nodiscard]] static constexpr int width() noexcept
        {
            return kWidth;
        }

        /// 第 i 行 mask（y-down，i=0 为顶行）= 旧 node.data[i]
        /// i 必须在 [0, kHeight)，不做边界检查。
        [[nodiscard]] uint64_t line(int i) const noexcept
        {
            return kLines[i];
        }
    };

    //----------------------------------------------------------------------
    // BBNode<Details, SpinT>（通用形态：含 spin 成员）
    //
    // SpinT 应为：
    //   TSpinType  （TSpinHook 路径）
    //   ASpinType  （ASpinHook 路径）
    //----------------------------------------------------------------------
    template<class Details, class SpinT = std::monostate>
    struct BBNode : BBNodeBase<Details>
    {
        SpinT spin; ///< TSpinType 或 ASpinType
    };

    //----------------------------------------------------------------------
    // BBNode<Details, std::monostate>（偏特化：NoSpinHook 路径）
    //
    // 移除 spin 成员，整体尺寸 = BBNodeBase（四个 no_unique_address 零尺寸
    // integral_constant + int8_t x + int8_t y = 2 字节）。
    //----------------------------------------------------------------------
    template<class Details>
    struct BBNode<Details, std::monostate> : BBNodeBase<Details>
    {
        // 不添加任何成员
    };

} // namespace m_tetris2
