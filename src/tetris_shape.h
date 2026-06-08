#ifndef TETRIS_AI_RUNNER_TETRIS_SHAPE_H_
#define TETRIS_AI_RUNNER_TETRIS_SHAPE_H_

//==========================================================================
// Compile-time piece shape lookup over a RuleSpec.
//
// 设计目标:
// - 为每个 (RuleSpec, piece-type, rotation) 提供一份编译期可访问的形状位图.
// - cobra 用 piece_table<P, R>() 现场算坐标; 但本仓库的形状定义已经在
//   RuleSpec::OpDesc::lines 里, 是 OpLines<L0, L1, ...> 形式的 N 行 W 位 mask.
//   所以这里不重新定义形状, 而是直接从 OpDesc 抽出.
// - 同时把 OpDesc 里的 spawn_x / spawn_y / rotation targets / wallkick 也
//   暴露给后续 movegen 阶段, 让 cobra 风格的 SIMD movegen 能够拿到完整的
//   规则数据.
//
// 关键 API:
//   m_tetris2::shape::find_op<Spec, T, R>          - 查找 OpDesc 类型(static_assert 找不到时报错)
//   m_tetris2::shape::piece_line<Spec, T, R, I>()  - 取第 I 行 mask (uint32_t)
//   m_tetris2::shape::piece_h<Spec, T, R>          - 形状高度 = N
//   m_tetris2::shape::piece_spawn<Spec, T, R>      - 返回 {x, y}
//
// 不在本头文件做的事:
// - 不构造 Map<W, H>; 后续 movegen 才会把 lines 写入 Map.
// - 不计算 bbox 紧缩 / pretty-print; 那些是 debug helper 不影响热路径.
//==========================================================================

#include "tetris_rule_spec.h"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <tuple>
#include <type_traits>
#include <utility>

namespace m_tetris2
{
    namespace shape
    {
        //----- 编译期: 在 RuleSpec::ops (std::tuple<Ops...>) 中查找 (T, R) -----
        namespace detail
        {
            //tuple 类型按下标取元素类型, 同时 + 找不到时报错的占位.
            struct NotFound
            {
                static constexpr bool found = false;
            };

            template<class Op, char T, std::uint8_t R>
            struct OpMatches
            {
                static constexpr bool value = (Op::type == T) && (Op::rotation == R);
            };

            template<class Tuple, char T, std::uint8_t R, std::size_t I, std::size_t N>
            struct FindImpl
            {
                using head = std::tuple_element_t<I, Tuple>;
                using type = std::conditional_t<
                    OpMatches<head, T, R>::value,
                    head,
                    typename FindImpl<Tuple, T, R, I + 1, N>::type>;
            };

            template<class Tuple, char T, std::uint8_t R, std::size_t N>
            struct FindImpl<Tuple, T, R, N, N>
            {
                using type = NotFound;
            };

            template<class Spec, char T, std::uint8_t R>
            struct Find
            {
                using ops_tuple = typename Spec::ops;
                static constexpr std::size_t op_count = std::tuple_size<ops_tuple>::value;
                using type = typename FindImpl<ops_tuple, T, R, 0, op_count>::type;
                static_assert(!std::is_same_v<type, NotFound>,
                              "shape::find_op: OpDesc<T, R> not present in RuleSpec::ops");
            };
        }

        //找到的 OpDesc 类型. 静态错误若不存在.
        template<class Spec, char T, std::uint8_t R>
        using find_op = typename detail::Find<Spec, T, R>::type;

        // 安全检测：(T,R) 是否存在于 RuleSpec::ops，不触发 static_assert.
        // 用于 BBCallEval::call_eval_typed 的编译期合法性门控.
        template<class Spec, char T, std::uint8_t R>
        inline constexpr bool has_op =
            !std::is_same_v<
                typename detail::FindImpl<
                    typename Spec::ops, T, R,
                    0, std::tuple_size_v<typename Spec::ops>
                >::type,
                detail::NotFound>;

        //----- 编译期: 数 RuleSpec::ops 中类型 T 的旋转数量 -----
        //要求 OpDesc::rotation 是 [0, count) 连续编号 (现有所有 rule 都满足).
        //返回的是 std::size_t, 同时静态校验 rotation 编号无重复且为连续区间.
        namespace detail
        {
            template<class Spec, char T, class Ops>
            struct CountRotations;

            template<class Spec, char T, class... Ops>
            struct CountRotations<Spec, T, std::tuple<Ops...>>
            {
                static constexpr std::size_t value =
                    ((Ops::type == T ? std::size_t(1) : std::size_t(0)) + ... + std::size_t(0));

                //静态校验: 所有 rotation in [0, value) 都能找到.
                //  has_rotation<I>(): Ops 包里是否存在 (type==T, rotation==I) 的 OpDesc
                template<std::size_t I>
                static constexpr bool has_rotation()
                {
                    return ((Ops::type == T && Ops::rotation == I) || ...);
                }
                template<std::size_t... Is>
                static constexpr bool check_dense(std::index_sequence<Is...>)
                {
                    return (has_rotation<Is>() && ...);
                }
                static_assert(value == 0 || check_dense(std::make_index_sequence<value>{}),
                              "shape::rotation_count: piece T's rotations must be densely numbered [0, count)");
            };
        }

        template<class Spec, char T>
        inline constexpr std::size_t rotation_count = detail::CountRotations<Spec, T, typename Spec::ops>::value;

        //----- 形状几何 -----
        //形状高度 (== Spec::note, 同 N)
        template<class Spec, char T, std::uint8_t R>
        inline constexpr std::size_t piece_h = find_op<Spec, T, R>::lines::size;

        //形状宽度 (== Spec::width, 因为 OpLines 的每行 mask 是 W 位)
        template<class Spec, char T, std::uint8_t R>
        inline constexpr std::size_t piece_w = Spec::width;

        //piece-local 初始化锚点 (转发自 OpDesc::spawn_x/spawn_y).
        //  注意: 这是给 rule 作者用的"作者坐标系 -> 矩阵坐标系"映射, 不是游戏
        //  出生位置. 真正的 spawn 位置请走 RuleSpec::spawn(PT, W, H).
        //  仅在 init 期被 op_create_bridge -> create_node 读取一次.
        template<class Spec, char T, std::uint8_t R>
        inline constexpr std::int8_t piece_spawn_x = find_op<Spec, T, R>::spawn_x;

        template<class Spec, char T, std::uint8_t R>
        inline constexpr std::int8_t piece_spawn_y = find_op<Spec, T, R>::spawn_y;

        //取第 I 行 (0 == 最底行)的 W 位 mask. uint32_t (RuleSpec 当前 W <= 32 时可用).
        //I 必须严格小于 piece_h, 否则会读到 OpLines 末尾的 0 占位 (而非真正的形状数据).
        namespace detail
        {
            template<class Spec, char T, std::uint8_t R, std::size_t I>
            constexpr std::uint32_t piece_line_value()
            {
                static_assert(I < find_op<Spec, T, R>::lines::size,
                              "shape::piece_line: row index out of range");
                return find_op<Spec, T, R>::lines::data[I];
            }
        }
        template<class Spec, char T, std::uint8_t R, std::size_t I>
        inline constexpr std::uint32_t piece_line = detail::piece_line_value<Spec, T, R, I>();

        //----- 旋转目标 / 踢墙表 -----
        //旋转后的目标 R; kOpRotateNone 表示无旋转可用.
        template<class Spec, char T, std::uint8_t R>
        inline constexpr std::uint8_t target_cw = find_op<Spec, T, R>::target_cw;

        template<class Spec, char T, std::uint8_t R>
        inline constexpr std::uint8_t target_ccw = find_op<Spec, T, R>::target_ccw;

        template<class Spec, char T, std::uint8_t R>
        inline constexpr std::uint8_t target_opp = find_op<Spec, T, R>::target_opp;

        //wallkick (CW / CCW / OPP). 类型是 WallKickList<XYs...> 的特化, 通过 typedef 暴露.
        template<class Spec, char T, std::uint8_t R>
        using wk_cw = typename find_op<Spec, T, R>::wk_cw;

        template<class Spec, char T, std::uint8_t R>
        using wk_ccw = typename find_op<Spec, T, R>::wk_ccw;

        template<class Spec, char T, std::uint8_t R>
        using wk_opp = typename find_op<Spec, T, R>::wk_opp;

        //----- 编译期单 cell 列表 (x, y) -----
        //把一个 OpDesc 的行 mask 展开成一组 (int8_t x, int8_t y) 坐标对.
        //x = 列号 (0 = 左), y = 行号 (0 = 底). 最多 4*W 个, 实际一般 4 个.
        //用于 movegen 构造 usable_map: 对每个 cell 执行一次 Map::shifted<-x, -y>().
        struct Cell
        {
            std::int8_t x, y;

            consteval bool operator==(Cell const &o) const
            {
                return x == o.x && y == o.y;
            }
            consteval bool operator!=(Cell const &o) const
            {
                return !(*this == o);
            }
        };

        //op_cells 的返回值: cells + pivot + origin.
        //  pivot 是 OpLines 矩阵中心 (col=1, row=1, 即 4x4 SRS 矩阵的"中央 cell")
        //  归一化到 bbox-原点坐标系后的位置. SRS 系所有 piece 都把旋转中心放在
        //  4x4 矩阵的 (1,1), 因此 pivot 直接由 (1 - cx_min, 1 - cy_min) 给出
        //  (cy_min 取 y-up 翻转后的最小值, 对应 OpLines 的 row = N-2).
        //  T-spin corner offset (基准点 = bbox 左下角时) = pivot ± (1, 1).
        //
        //  origin: bbox 左下角在原始 4xN 矩阵中的偏移 (cx_min, N-1-cy_min).
        //  master 老 movegen 用 4x4 矩阵左上角作 status 基准, 因此把新框架 (Map y-up,
        //  bbox 左下角) 输出的 (x, y) 转成 master.status 时需加上该偏移:
        //    master_x = new_x - cx_min       = new_x - origin.x_offset
        //    master_y = new_y + (H-1-cy_min) = new_y + origin.y_offset
        //  origin 同时承担: bbox-原点坐标系下"原 4xN 矩阵原点"的位置.
        //
        //  Cap: cells 数组容量, 由 op_cell_count<Op> = sum popcount(OpLines) 自动推导,
        //  即每个 (T, R) 形状的实际 cell 数. SRS 4-cell 系即 Cap=4; 5-cell / 6-cell
        //  / 任意大块自动 fit, 不再需要 32 cell 上限. 对外接口 (.count / .cells[i] /
        //  .pivot / .origin) 形态保持不变.
        template<std::size_t Cap>
        struct CellList
        {
            std::size_t count;
            std::array<Cell, Cap> cells;
            Cell pivot;
            Cell origin;

            //cells_impl 按 row 升序 + countr_zero 列扫描生成, 几何相同的两个 (Op, R)
            //  在同一规范化坐标系内必然得到相同的有序序列, 因此直接逐元素比较即可
            //  做几何等价判定, 不需要再排序. 跨 Cap 比较 (理论上同 piece 跨 R 的
            //  cell 数相等; 模板化只是为接口稳健).
            template<std::size_t Cap2>
            consteval bool same_geometry(CellList<Cap2> const &other) const
            {
                if (count != other.count)
                    return false;
                for (std::size_t i = 0; i < count; ++i)
                    if (cells[i] != other.cells[i])
                        return false;
                return true;
            }
        };

        namespace detail
        {
            //单个 OpDesc 实际 cell 数 = sum popcount(OpLines.data[row]).
            //  consteval, 仅作为 cells_impl 的 Cap 推导使用; 任意 OpLines 自然准确,
            //  无需用户手填上限.
            template<class Op>
            consteval std::size_t op_cell_count_impl()
            {
                constexpr std::size_t H = Op::lines::size;
                std::size_t cnt = 0;
                for (std::size_t row = 0; row < H; ++row)
                {
                    std::uint32_t mask = Op::lines::data[row];
                    while (mask != 0)
                    {
                        ++cnt;
                        mask &= mask - 1;
                    }
                }
                return cnt;
            }
        }

        template<class Op>
        inline constexpr std::size_t op_cell_count = detail::op_cell_count_impl<Op>();

        namespace detail
        {
            template<class Op>
            consteval auto cells_impl()
            {
                constexpr std::size_t H = Op::lines::size;
                constexpr std::size_t Cap = op_cell_count<Op>;
                std::array<Cell, Cap> buf{};
                std::size_t cnt = 0;
                for (std::size_t row = 0; row < H; ++row)
                {
                    std::uint32_t mask = Op::lines::data[row];
                    while (mask != 0)
                    {
                        int col = std::countr_zero(mask);
                        buf[cnt++] = Cell{static_cast<std::int8_t>(col),
                                          static_cast<std::int8_t>(H - 1 - row)};
                        mask &= mask - 1;
                    }
                }
                //归一化: 把所有 cell 减去 (cx_min, cy_min). 同时把 4x4/NxN 矩阵中心
                //(col=1, y-up row = H-1-1 = H-2) 也减去同样的偏移得到 pivot.
                std::int8_t cx_min = 0, cy_min = 0;
                if (cnt > 0)
                {
                    cx_min = buf[0].x;
                    cy_min = buf[0].y;
                    for (std::size_t i = 1; i < cnt; ++i)
                    {
                        if (buf[i].x < cx_min)
                            cx_min = buf[i].x;
                        if (buf[i].y < cy_min)
                            cy_min = buf[i].y;
                    }
                    if (cx_min != 0 || cy_min != 0)
                        for (std::size_t i = 0; i < cnt; ++i)
                        {
                            buf[i].x = static_cast<std::int8_t>(buf[i].x - cx_min);
                            buf[i].y = static_cast<std::int8_t>(buf[i].y - cy_min);
                        }
                }
                Cell pivot{static_cast<std::int8_t>(1 - cx_min),
                           static_cast<std::int8_t>(static_cast<int>(H) - 2 - cy_min)};
                Cell origin{cx_min,
                            static_cast<std::int8_t>(static_cast<int>(H) - 1 - cy_min)};
                return CellList<Cap>{cnt, buf, pivot, origin};
            }
        }

        //op_cells<Op>: 编译期 constexpr, 返回 CellList{count, cells, pivot}.
        template<class Op>
        inline constexpr auto op_cells = detail::cells_impl<Op>();

        //piece_cells<Spec, T, R>: 三元组接口, 等价于 op_cells<find_op<Spec, T, R>>.
        template<class Spec, char T, std::uint8_t R>
        inline constexpr auto piece_cells = op_cells<find_op<Spec, T, R>>;
    }
}

#endif // TETRIS_AI_RUNNER_TETRIS_SHAPE_H_
