#ifndef TETRIS_AI_RUNNER_TETRIS_RULE_SPEC_H_
#define TETRIS_AI_RUNNER_TETRIS_RULE_SPEC_H_

//==========================================================================
// Compile-time rule primitives.
//
// 定义规则系统共用的编译期数据结构，包括行类型、踢墙表、操作描述与 RuleSpec。
//==========================================================================

#include <cstddef>
#include <cstdint>
#include <tuple>
#include <utility>

namespace m_tetris2
{
    //场景行/方块行的存储类型，当前统一为 uint64_t。
    using row_t = std::uint64_t;

    //踢墙序列: 平展为 [x0, y0, x1, y1, ...] 的编译期数组
    template<std::int8_t... XYs>
    struct WallKickList
    {
        static_assert(sizeof...(XYs) % 2 == 0, "WallKickList expects (x,y) pairs");
        static constexpr std::size_t length = sizeof...(XYs) / 2;
        //+1 防止空 pack 触发零长数组, N=0 时 data 永不会被读
        static constexpr std::int8_t data[sizeof...(XYs) + 1] = {XYs..., 0};
    };
    template<std::int8_t... XYs>
    constexpr std::int8_t WallKickList<XYs...>::data[sizeof...(XYs) + 1];
    using NoKick = WallKickList<>;

    //op 行模板: N 完全可变, 与 RuleSpec::N 一致性由 RuleSpec 校验
    template<std::uint32_t... Ls>
    struct OpLines
    {
        static constexpr std::size_t size = sizeof...(Ls);
        //+1 防止空 pack 触发零长数组
        static constexpr std::uint32_t data[sizeof...(Ls) + 1] = {Ls..., 0};
    };
    template<std::uint32_t... Ls>
    constexpr std::uint32_t OpLines<Ls...>::data[sizeof...(Ls) + 1];

    //旋转目标 R 为 0xFF 表示 nullptr (该方向无旋转)
    static constexpr std::uint8_t kOpRotateNone = 0xFF;

    //单条方块操作的编译期描述.
    //  T: 方块字符 ('I'/'O'/'T'/...)
    //  R: 旋转编号 (0..3, 不必连续, 但 shape::rotation_count 要求连续)
    //  Lines: OpLines<...>, N 行 W 位 mask
    //  X, Y: piece-local 初始化锚点 (注意: **不是** 游戏出生位置).
    //         作者写 OpLines 时使用的可能是"以 piece 某个特定 cell 为原点"的坐标系,
    //         X/Y 用来描述该锚点 cell 在 NxN 矩阵里的位置, 是给 rule 作者
    //         "把脑子里的坐标系对齐到框架内部坐标系"的便利字段.
    //         真正的游戏出生位置由 RuleSpec::spawn(PT, W, H) 提供, 与 X/Y 正交.
    //         链路上 X/Y 仅在 init 期被 op_create_bridge -> create_node 读取一次,
    //         用于建立每个 piece 的 base 节点; 运行期 movegen / search 一律走
    //         RuleSpec::spawn.
    //  TgtCW / TgtCCW / TgtOpp: 旋转后的目标 R, kOpRotateNone 表示无旋转
    //  WkCW / WkCCW / WkOpp: WallKickList<...> 三张踢墙表
    template<char T, std::uint8_t R, class Lines,
             std::int8_t X = 0, std::int8_t Y = 0,
             std::uint8_t TgtCW = kOpRotateNone,
             std::uint8_t TgtCCW = kOpRotateNone,
             std::uint8_t TgtOpp = kOpRotateNone,
             class WkCW = NoKick,
             class WkCCW = NoKick,
             class WkOpp = NoKick>
    struct OpDesc
    {
        static constexpr char type = T;
        static constexpr std::uint8_t rotation = R;
        //piece-local 初始化锚点; 见上方 OpDesc 注释. 不要与
        //  RuleSpec::spawn(PT, W, H) (游戏出生位置) 混用.
        static constexpr std::int8_t spawn_x = X;
        static constexpr std::int8_t spawn_y = Y;
        using lines = Lines;
        static constexpr std::uint8_t target_cw = TgtCW;
        static constexpr std::uint8_t target_ccw = TgtCCW;
        static constexpr std::uint8_t target_opp = TgtOpp;
        using wk_cw = WkCW;
        using wk_ccw = WkCCW;
        using wk_opp = WkOpp;
    };

    namespace detail
    {
        template<std::size_t N, class... Ops>
        struct AllLinesEqual;

        template<std::size_t N>
        struct AllLinesEqual<N>
        {
            static constexpr bool value = true;
        };

        template<std::size_t N, class O0, class... Rest>
        struct AllLinesEqual<N, O0, Rest...>
        {
            static constexpr bool value = (O0::lines::size == N) && AllLinesEqual<N, Rest...>::value;
        };
    }

    //整套规则的编译期清单
    //  W: 盘面宽度 (<= 64)
    //  H: 盘面高度
    //  N: note 矩阵的行数 (4x4 -> 4, 5x5 -> 5, 任意 N)
    //  Ops: OpDesc<...> 列表
    template<std::size_t W, std::size_t H, std::size_t N, class... Ops>
    struct RuleSpec
    {
        static_assert(W <= 64, "RuleSpec W must be <= 64");
        static_assert(detail::AllLinesEqual<N, Ops...>::value,
                      "Every OpDesc::lines::size must equal RuleSpec::N");
        static constexpr std::size_t width = W;
        static constexpr std::size_t height = H;
        static constexpr std::size_t note = N;

        using row_t = m_tetris2::row_t;

        using ops = std::tuple<Ops...>;

        //游戏出生位置接口；默认返回 SRS 系规则常用的 (3, 21)。
        //非 SRS 规则可在各自的 RuleSpec specialization 中覆盖该函数。
        static constexpr std::pair<int, int> spawn(char /*PT*/, int /*W*/, int /*H*/)
        {
            return {3, 21};
        }
    };
}

#endif // TETRIS_AI_RUNNER_TETRIS_RULE_SPEC_H_
