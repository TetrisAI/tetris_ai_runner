#ifndef TETRIS_AI_RUNNER_TETRIS_MOVEGEN_H_
#define TETRIS_AI_RUNNER_TETRIS_MOVEGEN_H_

//==========================================================================
// Cobra-style bitboard BFS move generator.
//
// 与 cobra::MoveList 等价: 给定一张棋盘 TetrisMap<W,H> 和一个方块类型 T,
// 通过 BFS 在位图上扩展左/右/下/旋转, 输出所有可达的落点.
//
// 形状数据从 shape::piece_cells / shape::piece_line 取.
// 旋转目标/踢墙表从 shape::target_*/wk_* 取.
//==========================================================================

#include "piece_filter_index.h"
#include "movegen_hook.h"
#include "tetris_map.h"
#include "tetris_shape.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace m_tetris2
{
    // 落点描述：基础几何由框架持有，额外元数据由 Hook::Payload 扩展。
    template<class Hook>
    struct LandingPosT
    {
        std::int8_t x;
        std::int8_t y;
        std::uint8_t r;
        [[no_unique_address]] typename Hook::Payload extra;
    };

    // 默认的落点类型别名。
    using LandingPos = LandingPosT<DefaultTSpinHook>;

    namespace movegen
    {
        //MoveGen 的按键能力. soft_drop_step=false: 软降不能停在半空 (instant soft drop),
        //  只能一次落到底.
        struct MoveSet
        {
            bool rotate_180 = true;
            bool soft_drop_step = true;
        };

        //=== usable_map 构造 ===
        //对给定旋转 R 和棋盘 board, 构造"基准点可放置位图":
        //即 piece 的每个 cell(相对偏移 (cx, cy)) 都不与 board 碰撞.
        //  usable[r] = ~board.shifted<-cx0,-cy0> & ~board.shifted<-cx1,-cy1> & ...
        //
        //由于 shifted<Dx,Dy> 是模板参数必须编译期, 但 cell 也是编译期(来自
        //piece_cells), 所以可以用 index_sequence 展开.
        //

        namespace detail
        {
            //in-bounds mask: 与 usable_for_rotation_impl 同一公式, 但去掉 board 碰撞 AND,
            //  仅保留每个 cell 的"棋盘内"位移. 用于在 0-kick mini 检查中沿 master kick 链
            //  挑选 "第一条 in-bounds kick" — master init 时用空盘 check 选出 rotate_*
            //  cache 链, 运行时只在最末一段做 board check.
            template<class MapT, class Op, std::size_t... Is>
            constexpr MapT inbounds_for_rotation_impl(std::index_sequence<Is...>) noexcept
            {
                constexpr auto cells = shape::op_cells<Op>;
                MapT result = MapT(MapT::all_mask());
                MapT const all_ones = MapT(MapT::all_mask());
                ((result &= all_ones.template shifted<
                            -cells.cells[Is].x, -cells.cells[Is].y>()),
                 ...);
                return result;
            }

            template<class MapT, class Op, std::size_t... Is>
            constexpr MapT usable_for_rotation_impl(MapT const &board,
                                                    std::index_sequence<Is...>) noexcept
            {
                constexpr auto cells = shape::op_cells<Op>;
                //all_mask 对应"整个棋盘全空"
                MapT result = MapT(MapT::all_mask());
                //per-cell in-bounds mask: 把 "全 1 棋盘" 按 (-cx, -cy) 平移后,
                //  shift_col_keep_mask 会把"从棋盘外平移进来的位置"清 0, 因此结果
                //  恰好是"基准点 (px, py) 使该 cell 仍在棋盘内"的位置集合.
                //  对所有 cell AND 起来 = 整个 piece 在棋盘内的合法基准点集合.
                MapT const all_ones = MapT(MapT::all_mask());
                ((result &= all_ones.template shifted<
                            -cells.cells[Is].x, -cells.cells[Is].y>()),
                 ...);
                //对每个 cell, 将 board 反向平移后取反再与运算.
                //shifted<-cx, -cy>(board) 的含义: 在某位置 (px,py) 放 piece 后,
                //cell (cx,cy) 落到 (px+cx, py+cy); 等价于 board 向 (-cx,-cy) 移动
                //然后对 (px,py) 做碰撞检测.
                //
                //注意: shift 把"从棋盘外平移进来的位置"填 0, 所以 ~ 后这些位置成 1,
                //      看起来像 "free"; 但它们其实是越界, 属于"不可放置". 上面已经
                //      用 in-bounds mask 处理掉了, 这里只关心 board 内的碰撞.
                ((result &= ~board.template shifted<
                             -cells.cells[Is].x, -cells.cells[Is].y>()),
                 ...);
                return result;
            }
        }

        //对单个旋转 R 构造 usable map (需要 Op 类型做编译期).
        template<class Spec, char T, std::uint8_t R>
        constexpr auto usable_map(TetrisMap<static_cast<int>(Spec::width),
                                      static_cast<int>(Spec::height)> const &board) noexcept
        {
            using MapT = TetrisMap<static_cast<int>(Spec::width), static_cast<int>(Spec::height)>;
            using Op = shape::find_op<Spec, T, R>;
            constexpr auto cnt = shape::piece_cells<Spec, T, R>.count;
            return detail::usable_for_rotation_impl<MapT, Op>(
                board, std::make_index_sequence<cnt>{});
        }

        template<class Spec, char T, std::uint8_t R>
        constexpr auto inbounds_map() noexcept
        {
            using MapT = TetrisMap<static_cast<int>(Spec::width), static_cast<int>(Spec::height)>;
            using Op = shape::find_op<Spec, T, R>;
            constexpr auto cnt = shape::piece_cells<Spec, T, R>.count;
            return detail::inbounds_for_rotation_impl<MapT, Op>(
                std::make_index_sequence<cnt>{});
        }

        //=== landable_map 构造 ===
        //落地点 = 可放置且下方一格不可放置.
        //  我们的 Map y 增加 = 向上, 因此 shifted<0, +1>(usable)(x, y) = usable(x, y - 1),
        //  即"下方一格的 usable". 落地条件 = 当前 usable 且下方 NOT usable.
        //  对应 cobra Board 的 landable = usable & ~usable.shifted<0, +1>(),
        //  方向已根据本仓库 y-up 约定校正.
        template<class MapT>
        constexpr MapT landable_map(MapT const &usable) noexcept
        {
            return usable & ~usable.template shifted<0, 1>();
        }

        //=== BFS 平移扩展 (不含旋转) ===
        //从 seed 出发, 用 usable 做碰撞检测, 扩展左/右/下直到不再变化.
        //返回可达位图 reachable.
        template<class MapT>
        constexpr MapT bfs_translate(MapT const &seed, MapT const &usable) noexcept
        {
            MapT search = seed & usable;
            for (;;)
            {
                MapT expand = (search.template shifted<-1, 0>() |
                               search.template shifted<1, 0>() |
                               search.template shifted<0, -1>()) &
                              usable;
                MapT next = search | expand;
                if (next == search)
                    break;
                search = next;
            }
            return search;
        }

        //=== 含旋转的 BFS ===
        //cobra 的 generate: 维护 per-rotation 的 search 数组,
        //每轮同时执行平移+旋转扩展直到所有 rotation 不再变化.
        //
        //数组尺寸贴合实际旋转数 R_count = shape::rotation_count<Spec, T>.
        //rotation 编号必须是 [0, R_count) 连续 (shape::rotation_count 已 static_assert).
        //
        //EnableMini = true 时启用 T-Spin / Mini T-Spin 检测.
        //  判定规则对齐 ai_zzz / search_tspin (本仓库原版语义), 不照搬 cobra:
        //    is_ready       = "到达该格子的最后一步是旋转" & "≥3 corner 占用"(per-r)
        //    is_mini_ready  = is_ready & 当前位置 CW/CCW/OPP 三个目标 rotation 都不可放置
        //    spin           = is_ready ? (is_mini_ready ? Mini : Full) : None
        //  不需要 spin 信息的 search (simple/path/cautious/simulate/aspin) 走默认 false,
        //  if constexpr 在编译期裁掉所有 last_rotate / corners3 维护逻辑, 0 开销.
        //
        // 具体行为由 Hook trait 决定。
        //  框架本身不再认 piece 字面量 'T', 由 Hook::active_for_piece<T> 编译期决定
        //  本 piece 是否走 spin/last_rotate 维护. NoHook 等价原 EnableMini=false,
        //  TSpinEnableHook 等价原 (EnableMini=true && T=='T').

        template<class Spec, char T, class Hook = NoHook>
        class MoveGen
        {
            static constexpr int W = static_cast<int>(Spec::width);
            static constexpr int H = static_cast<int>(Spec::height);
            using map_t = TetrisMap<W, H>;

            //旋转数量编译期常量, 来自 RuleSpec::ops 中类型 T 的 OpDesc 数量.
            static constexpr int R_count = static_cast<int>(shape::rotation_count<Spec, T>);
            static_assert(R_count > 0, "MoveGen: piece T has no rotations in RuleSpec");
            static_assert(R_count <= 32, "MoveGen: rotation count > 32 not supported");

            // T-spin 检查由 Hook 控制。
            //  与原 `EnableMini && (T == 'T')` 字面量等价: TSpinEnableHook 只对 'T' 开启,
            //  其它 piece / 其它 hook 保持关闭.
            static constexpr bool kCheckTSpin = Hook::template active_for_piece<T>;

            // 旋转元数据由 Hook 自行维护。

            //origin 表生成: piece_cells<>.origin = (cx_min, H-1-cy_min) 对每个 R 各异.
            //  emit 阶段输出 (master_x, master_y) = (new_x - origin.x, new_y + origin.y),
            template<std::size_t... Rs>
            static constexpr auto build_origin_xs(std::index_sequence<Rs...>)
            {
                return std::array<std::int8_t, sizeof...(Rs)>{
                    shape::piece_cells<Spec, T, static_cast<std::uint8_t>(Rs)>.origin.x...};
            }
            template<std::size_t... Rs>
            static constexpr auto build_origin_ys(std::index_sequence<Rs...>)
            {
                return std::array<std::int8_t, sizeof...(Rs)>{
                    shape::piece_cells<Spec, T, static_cast<std::uint8_t>(Rs)>.origin.y...};
            }

            static constexpr auto kOriginXs =
                build_origin_xs(std::make_index_sequence<R_count>{});
            static constexpr auto kOriginYs =
                build_origin_ys(std::make_index_sequence<R_count>{});

            // emit 阶段使用 filtered_idx 去重等价落点。
            static constexpr std::uint16_t kFilteredCount =
                movegen::kFilteredCount<Spec, T>;
            static constexpr std::size_t kFilteredWords =
                (static_cast<std::size_t>(kFilteredCount) + 63u) / 64u + 1u;

            //CCW emit 顺序: master 注册阶段从 r=0 沿 rotate_counterclockwise
            //  扩展, IndexFilter 让 CCW 链上更早的 r 拿走 canonical idx.
            //  filtered index 表也按 CCW 序分配 idx; 运行时 emit 必须用同一
            //  顺序, 否则 r=1/r=3 共享同一 footprint 时会在 filtered_mark 上
            //  抢到 *不同* 的 r — 实际等价类相同, 但落点 (x,y,r) 字段会与
            //  master 偏离 (oracle_diff: r=1 vs r=3).
            template<std::size_t I>
            static constexpr std::uint8_t ccw_visit_at()
            {
                if constexpr (I == 0)
                    return 0;
                else
                {
                    constexpr std::uint8_t prev = ccw_visit_at<I - 1>();
                    constexpr std::uint8_t nx = shape::target_ccw<Spec, T, prev>;
                    if constexpr (nx == kOpRotateNone)
                        return prev;
                    else
                        return nx;
                }
            }
            template<std::size_t... Os>
            static constexpr auto build_ccw_visit(std::index_sequence<Os...>)
            {
                return std::array<std::uint8_t, sizeof...(Os)>{ccw_visit_at<Os>()...};
            }
            static constexpr auto kCcwVisit =
                build_ccw_visit(std::make_index_sequence<R_count>{});

        public:
            //输出回调: fn(LandingPos) for each reachable landing.
            //
            //start: 活动块的起始位板状态 (rotation + bbox 左下角), 即 Helpers::state_from_status
            //  对宿主给出的 TetrisBlockStatus 的换算; 新块出生时取 RuleSpec::spawn 换算的状态.
            //找不到合法起点 (例如顶部塞死) 时直接返回, 不输出任何落点.
            //moves: 默认 = 单行软降 + 180 旋转 (全部现有 search 的语义).
            template<class Fn>
            static void generate(map_t const &board, State const &start, Fn &&fn, MoveSet moves = {})
            {
                assert(start.r < R_count);
                //1. 构建 usable / landable (每个旋转独立)
                std::array<map_t, R_count> usable_arr{};
                std::array<map_t, R_count> landable_arr{};
                build_usable_landable(board, usable_arr, landable_arr);

                //1b. spin 元数据 (仅 kCheckTSpin 路径维护): per-rotation 的 last_rotate /
                //  corners3 位图统一交给 Hook::RotState 持有, 算法实现 (corners3 公式 /
                //  emit 时 ready/mini 判定 / last 前驱反查) 全部搬到 hook 静态成员上.
                //  NoHook 走 EmptyRotState (0 字节), if constexpr 守护下连 init 都不调.
                typename Hook::template RotState<map_t, R_count> rot_state{};
                if constexpr (kCheckTSpin)
                {
                    Hook::template on_init_rotations<Spec, T, map_t, R_count>(board, rot_state);
                }

                //2. seed: 起点只在 start.r 处点亮.
                //   若起点不可放, 沿 y 向上探至棋盘顶部 (cobra Slow init 思路).
                map_t seed{};
                if (map_t::is_ok_x(start.xb))
                {
                    int sy = start.yb;
                    while (sy < H && !usable_arr[start.r].get(start.xb, sy))
                        ++sy;
                    if (sy < H)
                        seed.set(start.xb, sy);
                }
                if (seed.none())
                    return;

                std::array<map_t, R_count> search{};
                search[start.r] = seed & usable_arr[start.r];

                //3. BFS 主循环: 平移收敛 + 旋转扩展, 反复直到不再变化.
                //  *关键* 每轮先把所有 r 的平移扩展到收敛 (内层 do/while), 再做一次
                //  旋转扩展. 这样旋转扩展时 search[r] 已是当前的 "所有可由当前已知
                //  source 平移到的位置", 与 master cover_if 在 BFS 序里"先到先得 ' ', 旋转
                //  cover_if 把已 ' ' 标记的格子升级为 'z'/'c'/'x'" 的语义对齐.
                for (;;)
                {
                    bool changed = false;
                    bool inner_changed;
                    do
                    {
                        inner_changed = false;
                        for (int r = 0; r < R_count; ++r)
                        {
                            if (search[r].none())
                                continue;
                            map_t expand = (search[r].template shifted<-1, 0>() |
                                            search[r].template shifted<1, 0>() |
                                            soft_drop(moves, search[r], usable_arr[r], landable_arr[r])) &
                                           usable_arr[r];
                            map_t next = search[r] | expand;
                            if (next != search[r])
                            {
                                search[r] = next;
                                inner_changed = true;
                                changed = true;
                            }
                        }
                    } while (inner_changed);
                    bool rot_changed = expand_rotations(search, usable_arr, rot_state, moves.rotate_180);
                    changed = changed || rot_changed;
                    if (!changed)
                        break;
                }

                //4. 输出: 落点 = search[r] & landable[r]
                //   filtered-index 去重: 把 (r, xb, yb) 折成 cell-footprint 等价
                //   类后, 同一 footprint 在 r 外循环里 *先到先得* 一次性 emit.
                //   master `node_mark_filtered_` 在 IndexFilter 维度做的等价类
                //   合并 (例如 SRS S piece 在 (r=0, xb=k) 与 (r=2, xb=k') 落同一
                //   棋盘 cell 集合) 与本表严格同义, 不依赖 SRS-7 / pivot 归一化
                //   巧合. 详见 src/piece_filter_index.h.
                //   spin 字段:
                //     - kCheckTSpin = false: 框架不写任何 spin 元数据 (Payload 为空,
                //       LandingPos.{x,y,r} 即落点几何坐标).
                //     - kCheckTSpin = true:  调用 Hook::on_emit, 由 hook 自己分类
                //       Full / Mini / None 并回填 last 前驱.
                std::array<std::uint64_t, kFilteredWords> filtered_mark{};
                //hook 需要区分 "最后一步是旋转" 与 "经平移/软降/出生到达" 的格子.
                std::array<map_t, R_count> moved{};
                if constexpr (kCheckTSpin)
                {
                    for (int r = 0; r < R_count; ++r)
                        moved[r] = search[r] & (search[r].template shifted<-1, 0>() |
                                                search[r].template shifted<1, 0>() |
                                                soft_drop(moves, search[r], usable_arr[r], landable_arr[r]));
                    moved[start.r] |= search[start.r] & seed;
                }
                for (int k = 0; k < R_count; ++k)
                {
                    int r = static_cast<int>(kCcwVisit[k]);
                    map_t landings = search[r] & landable_arr[r];
                    //逐 cell 查 filtered_idx, 同一 idx 二次出现时从 landings 里
                    //  剥掉 — 这样 hook 在 ready/mini/none 三类的 AND 划分里
                    //  自动被同步剔除, 不需要 hook 持 dedup 状态.
                    map_t fresh{};
                    landings.for_each_set_bit([&](int x, int y)
                                              {
                        std::size_t lin = (static_cast<std::size_t>(r) *
                                               static_cast<std::size_t>(W) +
                                           static_cast<std::size_t>(x)) *
                                              static_cast<std::size_t>(H) +
                                          static_cast<std::size_t>(y);
                        std::uint16_t idx = movegen::kFilteredIndex<Spec, T>[lin];
                        if (idx == movegen::kFilteredIndexInvalid)
                            return;
                        std::size_t word = static_cast<std::size_t>(idx) >> 6u;
                        std::uint64_t mask = std::uint64_t{1} << (idx & 63u);
                        if (filtered_mark[word] & mask)
                            return;
                        filtered_mark[word] |= mask;
                        fresh.set(x, y); });
                    landings = fresh;
                    if (landings.none())
                        continue;
                    if constexpr (kCheckTSpin)
                    {
                        Hook::template on_emit<Spec, T, LandingPosT<Hook>, map_t, R_count,
                                               decltype(kOriginXs)>(
                            landings, r, rot_state, search, usable_arr, moved,
                            kOriginXs, kOriginYs, fn);
                    }
                    else
                    {
                        // NoHook 路径下不写额外 spin 元数据。
                        landings.for_each_set_bit([&](int x, int y)
                                                  {
                                                      LandingPosT<Hook> lp{};
                                                      lp.x = static_cast<std::int8_t>(x - kOriginXs[r]);
                                                      lp.y = static_cast<std::int8_t>(y + kOriginYs[r]);
                                                      lp.r = static_cast<std::uint8_t>(r);
                                                      fn(lp); });
                    }
                }
            }

        private:
            //构建 usable / landable -- 数组尺寸 == R_count, 每个旋转都存在.
            static void build_usable_landable(map_t const &board,
                                              std::array<map_t, R_count> &usable_out,
                                              std::array<map_t, R_count> &landable_out)
            {
                [&]<std::size_t... Rs>(std::index_sequence<Rs...>)
                {
                    (build_one_rotation<static_cast<std::uint8_t>(Rs)>(board, usable_out, landable_out), ...);
                }(std::make_index_sequence<R_count>{});
            }

            template<std::uint8_t R>
            static void build_one_rotation(map_t const &board,
                                           std::array<map_t, R_count> &usable_out,
                                           std::array<map_t, R_count> &landable_out)
            {
                usable_out[R] = usable_map<Spec, T, R>(board);
                landable_out[R] = landable_map(usable_out[R]);
            }

            //软降一步可到达的格子: 单行软降, 或 (instant soft drop) 只能直落到底.
            static map_t soft_drop(MoveSet const &moves, map_t const &from, map_t const &usable,
                                   map_t const &landable) noexcept
            {
                map_t fall = from.template shifted<0, -1>() & usable;
                if (moves.soft_drop_step)
                    return fall;
                for (;;)
                {
                    map_t next = fall | (fall.template shifted<0, -1>() & usable);
                    if (next == fall)
                        return fall & landable;
                    fall = next;
                }
            }

            //旋转扩展: 对每个 rotation, 尝试 CW/CCW/OPP 踢墙.
            //  rot_state: per-rotation spin 元数据由 hook 维护, MoveGen 仅在
            //  apply_kicks 内按 (方向, 测试序号) 通过 Hook::on_rotate_reach 把本次旋转到达的 cell 抛给它.
            //  NoHook 路径下 rot_state 是 EmptyRotState, on_rotate_reach 空实现.
            template<class RotState>
            static bool expand_rotations(std::array<map_t, R_count> &search,
                                         std::array<map_t, R_count> const &usable,
                                         RotState &rot_state, bool rotate_180)
            {
                bool changed = false;
                [&]<std::size_t... Rs>(std::index_sequence<Rs...>)
                {
                    ((changed |= try_rotate_all<static_cast<std::uint8_t>(Rs)>(search, usable, rot_state, rotate_180)), ...);
                }(std::make_index_sequence<R_count>{});
                return changed;
            }

            template<std::uint8_t R, class RotState>
            static bool try_rotate_all(std::array<map_t, R_count> &search,
                                       std::array<map_t, R_count> const &usable,
                                       RotState &rot_state, bool rotate_180)
            {
                bool changed = false;
                //CW
                constexpr std::uint8_t cw = shape::target_cw<Spec, T, R>;
                if constexpr (cw != kOpRotateNone)
                {
                    changed |= apply_kicks<R, cw, shape::wk_cw<Spec, T, R>, KickDir::Cw>(
                        search, usable, rot_state);
                }
                //CCW
                constexpr std::uint8_t ccw = shape::target_ccw<Spec, T, R>;
                if constexpr (ccw != kOpRotateNone)
                {
                    changed |= apply_kicks<R, ccw, shape::wk_ccw<Spec, T, R>, KickDir::Ccw>(
                        search, usable, rot_state);
                }
                //OPP (180)
                constexpr std::uint8_t opp = shape::target_opp<Spec, T, R>;
                if constexpr (opp != kOpRotateNone)
                {
                    if (rotate_180)
                        changed |= apply_kicks<R, opp, shape::wk_opp<Spec, T, R>, KickDir::Opp>(
                            search, usable, rot_state);
                }
                return changed;
            }

            //应用踢墙: 把 search[src_r] 按踢墙 offset 平移, 与 usable[dst_r] 做交集,
            //加入 search[dst_r]. 返回是否有新增.
            //
            //KickList 的 (dx, dy) 是 master `status` (4x4 top-left) 坐标系下的 kick 偏移
            //("0-kick" 表示 status 不变); 而 new MoveGen BFS 用 bbox 左下角. 两个坐标
            //系差一个 origin 偏移 (origin = (cx_min, H-1-cy_min) 来自 cells_impl 归一化),
            //  必须补到 dx/dy 里, 否则 rotate 后的落点几何错位 -> last_rotate_arr 点亮
            //  到错位置, T-spin 标签错误.
            //
            //  master_x = new_x - origin.x   ⇒  new_dx = kick.x + (origin_dst.x - origin_src.x)
            //  master_y = new_y + origin.y   ⇒  new_dy = kick.y + (origin_src.y - origin_dst.y)
            //  注意 x / y 的 origin 公式符号相反 (cells_impl 里 origin.y = H-1-cy_min).
            template<std::uint8_t SrcR, std::uint8_t DstR, class KickList, KickDir Dir, class RotState>
            static bool apply_kicks(std::array<map_t, R_count> &search,
                                    std::array<map_t, R_count> const &usable,
                                    RotState &rot_state)
            {
                constexpr int dx_origin = shape::piece_cells<Spec, T, DstR>.origin.x -
                                          shape::piece_cells<Spec, T, SrcR>.origin.x;
                constexpr int dy_origin = shape::piece_cells<Spec, T, SrcR>.origin.y -
                                          shape::piece_cells<Spec, T, DstR>.origin.y;
                map_t src = search[SrcR];
                if (src.none())
                    return false;
                map_t added{};
                //master 在 BFS 时把 wall_kick_*[0] 设为 0-kick (即 rotate_##func 缓存),
                //  随后才 append KickList 内的 (dx, dy). 这里也要把 0-kick 当作
                //  KickList::length+1 个 kick 中的第 0 个先 try, 否则像 r=1→r=0 (JLSTZ
                //  CCW 表 +1,0 / +1,-1 / 0,+2 / +1,+2) 全都不含 0-kick, 0-kick 可达的格子
                //  在新 BFS 上不会被 last_rotate 标记 -> spin 漏标.
                //
                //另外 master 的 cover_if(' ' -> 'z') 允许把先前由平移到达 (tag=' ') 的
                //  目标格升级为旋转 (tag='z'); 因此 hook 收到的是 *所有* 经由本次旋转
                //  kick 到达的格子 (按测试序号 k 分开: 0 = 无踢墙, k = 第 k-1 个列表 kick),
                //  而不是仅 search[DstR] 之外的新格.
                std::size_t total_kicks = KickList::length + 1;
                for (std::size_t k = 0; k < total_kicks; ++k)
                {
                    int dx, dy;
                    if (k == 0)
                    {
                        dx = dx_origin;
                        dy = dy_origin;
                    }
                    else
                    {
                        dx = KickList::data[(k - 1) * 2] + dx_origin;
                        dy = KickList::data[(k - 1) * 2 + 1] + dy_origin;
                    }
                    map_t shifted = shift_runtime(src, dx, dy);
                    map_t legal = shifted & usable[DstR];
                    if constexpr (kCheckTSpin)
                        Hook::template on_rotate_reach<map_t, R_count>(
                            Dir, DstR, static_cast<std::uint8_t>(k), legal, rot_state);
                    added |= legal & ~search[DstR];
                    //first-kick-wins: 只要某 src cell 经过本 kick 几何可放, 后续 kick 不再
                    //  尝试该 src cell (与 master "成功一次就 break" 等价).
                    src &= ~shift_runtime(legal, -dx, -dy);
                }
                if (added.any())
                {
                    search[DstR] |= added;
                    return true;
                }
                return false;
            }

            //运行期 Map shift -- 逐行双循环, 适用于踢墙 (offset 编译期未知).
            static map_t shift_runtime(map_t const &m, int dx, int dy) noexcept
            {
                map_t result{};
                using row_t = typename map_t::row_t;
                constexpr row_t full = map_t::row_full;
                for (int y = 0; y < H; ++y)
                {
                    int src_y = y - dy;
                    if (src_y < 0 || src_y >= H)
                        continue;
                    row_t r = m.row(src_y);
                    if (dx > 0)
                        r = static_cast<row_t>((r << dx) & full);
                    else if (dx < 0)
                        r = static_cast<row_t>(r >> (-dx));
                    result.set_row(y, r);
                }
                return result;
            }
        };
    }
}

#endif // TETRIS_AI_RUNNER_TETRIS_MOVEGEN_H_
