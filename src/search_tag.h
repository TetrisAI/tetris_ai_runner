#ifndef TETRIS_AI_RUNNER_SEARCH_TAG_H_
#define TETRIS_AI_RUNNER_SEARCH_TAG_H_

//==========================================================================
// TagStrategy<SpinHook, RuleSpec>: oracle search_tag 的位板原生 strategy.
//
// 与 PathStrategy / SimulateStrategy 的本质差异 (源自 oracle/search_tag.cpp 与
// oracle/search_path.cpp 的算法分歧):
//
//   * 1g 邻居集 = z c l r d (没有 'x' (180), 没有 'L'/'R' (扫到尽头), 没有
//     'D' (drop), 没有 rotate_move 二级 X/Z/C). 旋转走 first_passing_kick
//     master 端 rotate_* 与 wall_kick_*[] 共用 union, rotate_clockwise 实
//     际 = 第一个通过的 wall-kick), 与 oracle/search_tag.cpp 行 91-119 / 228-242
//     / 278-292 / 351-365 一一对应.
//   * 20g 选路完全由几何决定: node->land_point != null && low >= roof 触发
//     "20g 种子 + bridge probe + 带 !open 过滤的 BFS"; 否则降级为 1g BFS.
//     与 SpinHook::config_is_20g 无关 (oracle search_tag 不读 Config).
//   * T piece (search_t) 走独立路径: cover_if 升级 ' '→'z'/'c' 的 BFS, 命中
//     land-point 时记录 last + is_last_rotate + is_ready (corners3_arr 位板,
//     由 SpinHook::on_init_rotations 在 search_t_native 入口构建).
//   * make_path 邻居集 = z c l r d (first_passing_kick = master rotate_*),
//     末段对 land_point.type != None 的命中做 last->rotate_*  -> land_point.node
//     单步反查, 与 oracle search_tag.cpp:39-159 一一对应. 不消费 disable_d 阶段,
//     不消费 wall-kick 链的额外分支.
//
// 共用:
//   * Context 复用 PathMark / BfsQueue / StateNodeLut mixin (不需要专属 mixin).
//   * SpinHook 必须是 TSpinHook 形态 (LandPoint 带 .node/.last/.is_check/
//     .is_last_rotate/.is_ready 字段).
//     当前调用面只有 ::m_tetris2::TSpinHook 一种实例.
//==========================================================================

#include "bb_bfs_engine.h"
#include "bb_state.h"
#include "movegen_context.h"
#include "movegen_hook.h"
#include "movegen_searcher.h"
#include "movegen_strategy.h"
#include "tetris_core.h"
#include "tetris_rule_spec.h"
#include "tetris_shape.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <type_traits>
#include <vector>

namespace m_tetris2
{
    namespace movegen
    {
        //=== Tag strategy 私有 mixin: SpinHook 依赖字段 ========================
        // 与 detail::path::ExtrasMixin 同形 (LandPoint cache + context/config 缓存).
        //commit (drop-search-state): hook_state_ / SpinHook::SearchState 已删除,
        //  TSpin 的 ready 改读 search_t_native 入口构建的 corners3_arr 位板.
        namespace detail
        {
            namespace tag
            {
                template<class SpinHook, class RuleSpec>
                struct ExtrasMixin
                {
                    using Config = typename SpinHook::Config;
                    using LandPoint = typename SpinHook::LandPoint;

                    Config const *config_ = nullptr;
                    std::vector<LandPoint> land_point_cache_{};
                    //commit B (PathMark 栈化): 旧 search_mark_ / t_mark_ /
                    //  make_path_mark_ 字段迁出 ctx, 改在各 native 入口栈分配.
                    //  ctx 不再持任何 PathMark 状态, 解锁后续并发 (多线程独立栈).
                };
            } // namespace tag
        } // namespace detail

        template<class SpinHook, class RuleSpec, class Policy = void>
        struct TagStrategy
        {
        private:
            using Helpers = bb::Helpers<RuleSpec>;
            using map_t = typename Helpers::map_t;
            using PathMark = typename Helpers::PathMark;
            using piece_info_t = typename Helpers::piece_info_t;

            static constexpr int kW = Helpers::kW;
            static constexpr int kH = Helpers::kH;
            static constexpr int kMaxR = Helpers::kMaxR;
            static constexpr int kPieceCount = Helpers::kPieceCount;

        public:
            using LandPoint = typename SpinHook::LandPoint;
            using Config = typename SpinHook::Config;

            using Context = MoveGenContext<RuleSpec,
                                           BfsQueueMixin<RuleSpec>,
                                           detail::tag::ExtrasMixin<SpinHook, RuleSpec>>;

            //=== init ==========================================================
            static void init(Context &ctx, Config const *config)
            {
                ctx.config_ = config;
            }

            //=== search ========================================================
            // 与 oracle/search_tag.cpp:161-313 一一对应:
            //   * !node->check(map) 直接返回空 (位板版用 usable_arr[entry.r] 过滤).
            //   * node->status.t == 'T' (本框架推广为 SpinHook::active_for_piece<T>):
            //     走 search_t_native; 否则 1g/20g 分支由几何条件 (node->land_point
            //     非空 && node->low >= map.roof) 决定 (与 oracle 完全等价 — 不读
            //     Config::is_20g, oracle search_tag 也不读).
            static std::vector<LandPoint> const *
            search(Context &ctx, TetrisMap const &map, bb::BBState const &spawn, std::size_t depth)
            {
                ctx.land_point_cache_.clear();
                (void)depth;
                map_t board = Helpers::build_board(map);
                bb::BBState const &spawn_state = spawn;
                char t = static_cast<char>(spawn_state.t);
                if (piece_info_t::index_of(t) < 0)
                {
                    assert(false && "TagStrategy::search: piece type outside RuleSpec");
                    return &ctx.land_point_cache_;
                }
                bool dispatched = false;
                [&]<std::size_t... Is>(std::index_sequence<Is...>)
                {
                    (((!dispatched && piece_info_t::type_at(Is) == t)
                          ? (run_piece_dispatch<piece_info_t::type_at(Is)>(ctx, board, spawn_state, map.roof), dispatched = true)
                          : false),
                     ...);
                }(std::make_index_sequence<kPieceCount>{});
                if (!dispatched)
                {
                    assert(false && "TagStrategy::search: piece type outside RuleSpec");
                }
                return &ctx.land_point_cache_;
            }

            //=== search_eval ===================================================
            // Phase 2 推式接口：与 PathStrategy::search_eval 同形，参见其注释。
            // EvalCallback 须可调用 on_land.operator()<T, R>(LandPoint const &)
            // 其中 T 为编译期 piece type，R 为编译期旋转态（均为编译期常量）。
            // TODO(multi-thread): 多线程路径在此并发分发 PendingTask。
            //==================================================================
            template<class EvalCallback>
            static void search_eval(Context &ctx, Map<RuleSpec::width, RuleSpec::height> const &board,
                                    bb::BBState const &spawn, std::size_t depth,
                                    EvalCallback &on_land)
            {
                (void)depth;
                int roof = Helpers::board_roof(board);
                char t = static_cast<char>(spawn.t);
                bool dispatched = false;
                [&]<std::size_t... Is>(std::index_sequence<Is...>)
                {
                    (((!dispatched && piece_info_t::type_at(Is) == t)
                          ? (run_piece_dispatch<piece_info_t::type_at(Is), EvalCallback>(
                                 ctx, board, spawn, roof, &on_land),
                             dispatched = true)
                          : false),
                     ...);
                }(std::make_index_sequence<kPieceCount>{});
                if (!dispatched)
                {
                    assert(false && "TagStrategy::search_eval: piece type outside RuleSpec");
                }
            }

            //=== make_path =====================================================
            // 与 oracle/search_tag.cpp:39-159 一一对应. 不读 SpinHook::config_is_20g
            // (oracle make_path 也不分 1g/20g).
            static std::vector<char>
            make_path(Context &ctx, bb::BBState const &spawn,
                      LandPoint const &land_point,
                      Map<RuleSpec::width, RuleSpec::height> const &board)
            {
                return make_path_native(ctx, spawn, land_point, board);
            }

        private:
            //==================================================================
            //=== run_piece_dispatch ============================================
            //==================================================================
            // active_for_piece<T> = true 走 search_t_native (T-spin 路径);
            // 否则按 oracle 几何判定决定 1g vs 20g.
            //
            //commit (strategy-deref): 旧版直接读 master `node->land_point !=
            //  nullptr && node->low >= map.roof`, 其中 node->low 是 master
            //  旧初始化阶段沿 spawn-row chain 批量填好的字段,
            //  本质上就是"该 piece 在 spawn rotation 下 cell 最低 y". 现在改用
            //  位板侧 helper `is_above_roof_bb(entry_state, map.roof)` 直接从
            //  BBState 算 piece-min-y 与 map.roof 比较, 与 oracle 行为对齐;
            //  不再 deref ctx 计算的 node 字段.
            template<char T, class EvalCallback = void>
            static void run_piece_dispatch(Context &ctx, map_t const &board, bb::BBState const &spawn,
                                           int roof, EvalCallback *on_land = nullptr)
            {
                if constexpr ([]() constexpr
                              {
                    if constexpr (requires { SpinHook::template active_for_piece<T, Policy>; })
                        return SpinHook::template active_for_piece<T, Policy>;
                    else
                        return SpinHook::template active_for_piece<T>; }())
                {
                    search_t_native<T>(ctx, board, spawn, on_land);
                }
                else
                {
                    if (Helpers::is_above_roof_bb(spawn, roof))
                    {
                        run_piece_20g_native<T, EvalCallback>(ctx, board, spawn, on_land);
                    }
                    else
                    {
                        run_piece_1g_native<T, EvalCallback>(ctx, board, spawn, on_land);
                    }
                }
            }

            //==================================================================
            //=== Tag-search 1g BFS (non-T) ====================================
            //==================================================================
            // 邻居顺序与 oracle/search_tag.cpp:262-310 严格一致: z c l r d.
            //
            // 旋转语义 (与 oracle node->rotate_clockwise/counterclockwise 严格等价):
            //   master 端 rotate_* 与 wall_kick_*[0] 共用 union; wall_kick_* 数组在
            //   prepare 阶段用空盘 (仅 bounds-check, 无 board check) 填入, 取第一个
            //   in-bounds 的 kick (pure rotation 在 bounds 时即为它). 运行时再对
            //   该单一 pointer 做 board check (node->rotate_*->check(map)).
            //   位板版无法用 first_passing_kick_bb (= 第一个 board-clear 的 kick),
            //   因为它会跳过 master 的 wall_kick_*[0] 选中位置 (board 失败) 而
            //   找到一个 master 不会试的更远 kick.
            //   等价做法: 取 cur 的 master node, 直接读 node->rotate_* (= "第一个
            //   in-bounds kick"), 然后用 usable_arr 做 board check; 失败 -> 不入队.
            // l/r/d 单步.
            struct TagSearch1gNeighbors
            {
                char piece_t;
                std::array<map_t, kMaxR> const *usable_arr;
                std::array<map_t, kMaxR> const *inbounds_arr;

                template<class Emit>
                void expand(bb::BBState const &cur, Emit emit)
                {
                    //commit (remove-context): rotate_* = wall_kick_*[0] = 空盘
                    //  inbounds 第一个命中的 kick 目标. 用 inbounds_arr 喂
                    //  first_passing_kick_bb, 与 master rotate_counterclockwise /
                    //  rotate_clockwise 出口完全等价; board check 由 dedup 走
                    //  usable_at_bb(usable_arr) 二段过滤, 与 oracle "rotate_* &&
                    //  rotate_*->check(map)" 一致.
                    if (auto rn = Helpers::first_passing_kick_bb(piece_t, bb::KickDir::Ccw, cur, *inbounds_arr))
                    {
                        if (Helpers::usable_at_bb(rn->r, rn->xb, rn->yb, *usable_arr))
                            emit(*rn, 'z');
                    }
                    if (auto rn = Helpers::first_passing_kick_bb(piece_t, bb::KickDir::Cw, cur, *inbounds_arr))
                    {
                        if (Helpers::usable_at_bb(rn->r, rn->xb, rn->yb, *usable_arr))
                            emit(*rn, 'c');
                    }
                    {
                        bb::BBState nL = cur;
                        nL.xb = static_cast<std::int8_t>(nL.xb - 1);
                        emit(nL, 'l');
                    }
                    {
                        bb::BBState nR = cur;
                        nR.xb = static_cast<std::int8_t>(nR.xb + 1);
                        emit(nR, 'r');
                    }
                    {
                        bb::BBState nd = cur;
                        nd.yb = static_cast<std::int8_t>(nd.yb - 1);
                        emit(nd, 'd');
                    }
                }
            };

            //=== Tag-search 1g visitor: pop 时检测 land-point ===================
            // land-point 谓词: !move_down->check(map), 等价 usable_at_bb(r, xb, yb-1)
            // 不成立. 与 oracle/search_tag.cpp:271-277 / 271 内部分支同形.
            // emitted_keys 走线性 cells_key 去重 (与 oracle node_mark_filtered_
            // 等价: cells_key 与 master IndexFilter 一一对应).
            //
            // Phase 2: EvalCallback 可选；非 void 时在 on_pop 展开运行时 cur.r →
            //   编译期 Rs，直接调 callback，跳过 push_back。
            // TODO(multi-thread): 多线程路径改为构造 PendingTask，参见 search_path.h。
            template<char T, class EvalCallback = void>
            struct TagSearch1gVisitor
            {
                Context *ctx;
                std::array<map_t, kMaxR> const *usable_arr;
                std::vector<bb::CellsKey> *emitted_keys;
                [[no_unique_address]] std::conditional_t<
                    std::is_void_v<EvalCallback>, std::monostate, EvalCallback *> on_land{};

                bool on_pop(bb::BBState const &cur)
                {
                    bool grounded = !Helpers::usable_at_bb(cur.r, cur.xb, cur.yb - 1, *usable_arr);
                    if (!grounded)
                        return true;
                    bb::CellsKey k = Helpers::cells_key_for_state(cur);
                    for (bb::CellsKey ek : *emitted_keys)
                        if (ek == k)
                            return true;
                    emitted_keys->push_back(k);
                    LandPoint node_ex;
                    node_ex.state = cur;
                    if constexpr (!std::is_void_v<EvalCallback>)
                    {
                        [&]<std::size_t... Rs>(std::index_sequence<Rs...>)
                        {
                            ((Rs == cur.r
                                  ? (on_land->template operator()<T, static_cast<std::uint8_t>(Rs)>(node_ex),
                                     true)
                                  : false) ||
                             ...);
                        }(std::make_index_sequence<Helpers::template rcount_v<T>()>{});
                    }
                    else
                    {
                        ctx->land_point_cache_.push_back(node_ex);
                    }
                    return true;
                }

                bool on_admit(bb::BBState const &, char, bb::EnqueueDecision)
                {
                    return true;
                }
            };

            //=== Tag-search 1g dedup: PathMarkBit (L1) 二态 + usable check =======
            // visitor 不读 prev/op, 因此选 1-bit-per-cell 的 PathMarkBit.
            //   mark 第一次成功 -> 进入 usable check;
            //   usable 通过 -> MarkAndEnqueue;
            //   usable 失败 -> MarkOnly (mark 已消费, 不再重访);
            //   mark 失败 -> Skip.
            // 与 oracle/search_tag.cpp:295-306 的 "mark + check" 二段判定等价.
            struct TagSearchDedup
            {
                typename Helpers::PathMarkBit *path_mark;
                std::array<map_t, kMaxR> const *usable_arr;

                bb::EnqueueDecision try_admit(bb::BBState const &s, bb::BBState const *, char)
                {
                    if (s.xb < 0 || s.xb >= kW || s.yb < 0 || s.yb >= kH || s.r >= kMaxR)
                        return bb::EnqueueDecision::Skip;
                    if (!path_mark->mark_bbox(static_cast<int>(s.r),
                                              static_cast<int>(s.xb),
                                              static_cast<int>(s.yb)))
                        return bb::EnqueueDecision::Skip;
                    if (!Helpers::usable_at_bb(s.r, s.xb, s.yb, *usable_arr))
                        return bb::EnqueueDecision::MarkOnly;
                    return bb::EnqueueDecision::MarkAndEnqueue;
                }
            };

            template<char T, class EvalCallback = void>
            static void run_piece_1g_native(Context &ctx, map_t const &board, bb::BBState const &entry,
                                            EvalCallback *on_land = nullptr)
            {
                std::array<map_t, kMaxR> usable_arr{};
                Helpers::build_usable_for_piece(T, board, usable_arr);
                std::array<map_t, kMaxR> inbounds_arr{};
                Helpers::build_inbounds_for_piece(T, inbounds_arr);
                //commit B: search_mark_ 栈分配; PathMarkBit 默认构造即空集.
                typename Helpers::PathMarkBit search_mark;
                std::vector<bb::CellsKey> emitted_keys;
                emitted_keys.reserve(32);
                TagSearch1gNeighbors neighbors{T, &usable_arr, &inbounds_arr};
                TagSearchDedup dedup{&search_mark, &usable_arr};
                TagSearch1gVisitor<T, EvalCallback> visitor{&ctx, &usable_arr, &emitted_keys};
                if constexpr (!std::is_void_v<EvalCallback>)
                    visitor.on_land = on_land;
                ctx.node_search_path_.clear();
                bb::run_bb_bfs(entry, neighbors, dedup, visitor, ctx.node_search_path_);
            }

            //==================================================================
            //=== Tag-search 20g BFS (non-T, land_point != null && low >= roof) =
            //==================================================================
            // 邻居顺序与 oracle/search_tag.cpp:227-258 一致: z c l r d.
            // 旋转走 master node->rotate_* (= wall_kick_*[0], 即"第一个 in-bounds
            // kick" 在 prepare 时用空盘填入), 然后用 usable_arr 做 board check.
            // 与 oracle 行 228-242 / 1g 路径用同样的语义 (避免 first_passing_kick_bb
            // 找到 master 永不会试的更远 kick).
            // l/r 加 !open 过滤 (与 oracle l/r 分支严格对应), z/c/d 不加 open 过滤
            // (对应 oracle 行 228-242, 254-257). open 等价用 open_bb_state
            struct TagSearch20gNeighbors
            {
                char piece_t;
                std::array<map_t, kMaxR> const *usable_arr;
                std::array<map_t, kMaxR> const *inbounds_arr;
                map_t const *board;
                Context *ctx;

                bool open_bb_state(bb::BBState const &s)
                {
                    return Helpers::open_bb_state(s, *board);
                }

                template<class Emit>
                void expand(bb::BBState const &cur, Emit emit)
                {
                    //commit (remove-context): 与 1g 路径同形, rotate_* 走
                    //  first_passing_kick_bb(inbounds_arr); 与 oracle (search_tag.cpp:
                    //  228-242) 一一对应.
                    if (auto rn = Helpers::first_passing_kick_bb(piece_t, bb::KickDir::Ccw, cur, *inbounds_arr))
                    {
                        if (Helpers::usable_at_bb(rn->r, rn->xb, rn->yb, *usable_arr))
                            emit(*rn, 'z');
                    }
                    if (auto rn = Helpers::first_passing_kick_bb(piece_t, bb::KickDir::Cw, cur, *inbounds_arr))
                    {
                        if (Helpers::usable_at_bb(rn->r, rn->xb, rn->yb, *usable_arr))
                            emit(*rn, 'c');
                    }
                    {
                        bb::BBState nL = cur;
                        nL.xb = static_cast<std::int8_t>(nL.xb - 1);
                        if (Helpers::usable_at_bb(nL.r, nL.xb, nL.yb, *usable_arr) && !open_bb_state(nL))
                            emit(nL, 'l');
                    }
                    {
                        bb::BBState nR = cur;
                        nR.xb = static_cast<std::int8_t>(nR.xb + 1);
                        if (Helpers::usable_at_bb(nR.r, nR.xb, nR.yb, *usable_arr) && !open_bb_state(nR))
                            emit(nR, 'r');
                    }
                    {
                        bb::BBState nd = cur;
                        nd.yb = static_cast<std::int8_t>(nd.yb - 1);
                        emit(nd, 'd');
                    }
                }
            };

            //=== Tag-search 20g visitor: pop 时检测 "!open && grounded" ==========
            // land-point 谓词 = !node->open(map) && (!move_down || !move_down->check(map)),
            // 与 oracle/search_tag.cpp:220-225 一致. emitted_keys 同 1g 路径线性 dedup.
            //
            // Phase 2: T 编译期参数用于 rcount_v<T> 展开；EvalCallback 非 void 时触发 callback。
            template<char T, class EvalCallback = void>
            struct TagSearch20gVisitor
            {
                Context *ctx;
                std::array<map_t, kMaxR> const *usable_arr;
                map_t const *board;
                std::vector<bb::CellsKey> *emitted_keys;
                [[no_unique_address]] std::conditional_t<
                    std::is_void_v<EvalCallback>, std::monostate, EvalCallback *> on_land{};

                bool on_pop(bb::BBState const &cur)
                {
                    bool grounded = !Helpers::usable_at_bb(cur.r, cur.xb, cur.yb - 1, *usable_arr);
                    if (!grounded)
                        return true;
                    if (Helpers::open_bb_state(cur, *board))
                        return true;
                    bb::CellsKey k = Helpers::cells_key_for_state(cur);
                    for (bb::CellsKey ek : *emitted_keys)
                        if (ek == k)
                            return true;
                    emitted_keys->push_back(k);
                    LandPoint node_ex;
                    node_ex.state = cur;
                    if constexpr (!std::is_void_v<EvalCallback>)
                    {
                        [&]<std::size_t... Rs>(std::index_sequence<Rs...>)
                        {
                            ((Rs == cur.r
                                  ? (on_land->template operator()<T, static_cast<std::uint8_t>(Rs)>(node_ex),
                                     true)
                                  : false) ||
                             ...);
                        }(std::make_index_sequence<Helpers::template rcount_v<T>()>{});
                    }
                    else
                    {
                        ctx->land_point_cache_.push_back(node_ex);
                    }
                    return true;
                }

                bool on_admit(bb::BBState const &, char, bb::EnqueueDecision)
                {
                    return true;
                }
            };

            //=== Tag-search 20g dedup: PathMarkBit (L1) 二态 + usable check ======
            // visitor 不读 prev/op, 选 1-bit-per-cell 档. l/r 的 !open 过滤
            // 由 expand 入口处理, 与 oracle 行 244-252 一致.
            struct TagSearch20gDedup
            {
                typename Helpers::PathMarkBit *path_mark;
                std::array<map_t, kMaxR> const *usable_arr;

                bb::EnqueueDecision try_admit(bb::BBState const &s, bb::BBState const *, char)
                {
                    if (s.xb < 0 || s.xb >= kW || s.yb < 0 || s.yb >= kH || s.r >= kMaxR)
                        return bb::EnqueueDecision::Skip;
                    if (!path_mark->mark_bbox(static_cast<int>(s.r),
                                              static_cast<int>(s.xb),
                                              static_cast<int>(s.yb)))
                        return bb::EnqueueDecision::Skip;
                    if (!Helpers::usable_at_bb(s.r, s.xb, s.yb, *usable_arr))
                        return bb::EnqueueDecision::MarkOnly;
                    return bb::EnqueueDecision::MarkAndEnqueue;
                }
            };

            template<char T, class EvalCallback = void>
            static void run_piece_20g_native(Context &ctx, map_t const &board, bb::BBState const &entry,
                                             EvalCallback *on_land = nullptr)
            {
                std::array<map_t, kMaxR> usable_arr{};
                Helpers::build_usable_for_piece(T, board, usable_arr);
                std::array<map_t, kMaxR> inbounds_arr{};
                Helpers::build_inbounds_for_piece(T, inbounds_arr);

                std::vector<bb::CellsKey> emitted_keys;
                emitted_keys.reserve(32);

                //commit B: search_mark_ 栈分配; PathMarkBit 默认构造即空集.
                typename Helpers::PathMarkBit search_mark;
                ctx.node_search_path_.clear();
                //  与 oracle/search_tag.cpp:177-214 同形 — spawn-row land_point 列表
                //  位板侧重建 (master 在旧初始化阶段建 piece-definition
                //  表, 现在每次 search 重建一份等价 BBState 列表), drop 用 board
                //  的 usable_arr 走 drop_bb_state. 与 master `(*cit)->drop(map)`
                //  一一等价.
                std::vector<bb::BBState> spawn_states;
                Helpers::enumerate_spawn_row_states(T, entry, inbounds_arr, spawn_states);

                std::vector<bb::BBState> drop_seeds;
                drop_seeds.reserve(spawn_states.size());
                for (bb::BBState const &spawn_state : spawn_states)
                {
                    auto sunk = Helpers::drop_bb_state(spawn_state, usable_arr);
                    if (!sunk)
                        continue;
                    drop_seeds.push_back(*sunk);
                    bb::CellsKey k = Helpers::cells_key_for_state(*sunk);
                    bool dup = false;
                    for (bb::CellsKey ek : emitted_keys)
                        if (ek == k)
                        {
                            dup = true;
                            break;
                        }
                    if (!dup)
                    {
                        emitted_keys.push_back(k);
                        LandPoint node_ex;
                        node_ex.state = *sunk;
                        // Phase 2: spawn-seed 落点同样展开运行时 r → 编译期 Rs。
                        if constexpr (!std::is_void_v<EvalCallback>)
                        {
                            [&]<std::size_t... Rs>(std::index_sequence<Rs...>)
                            {
                                ((Rs == sunk->r
                                      ? (on_land->template operator()<T, static_cast<std::uint8_t>(Rs)>(node_ex),
                                         true)
                                      : false) ||
                                 ...);
                            }(std::make_index_sequence<Helpers::template rcount_v<T>()>{});
                        }
                        else
                        {
                            ctx.land_point_cache_.push_back(node_ex);
                        }
                    }
                }
                //bridge probe + 入队. dedup 用 search_mark (PathMarkBit, 仅 1 bit/cell),
                //  种子先准入 (parent=null).
                TagSearch20gDedup dedup{&search_mark, &usable_arr};
                std::vector<bb::BBState> &queue = ctx.node_search_path_;
                queue.clear();
                for (bb::BBState const &cur : drop_seeds)
                {
                    auto admit = dedup.try_admit(cur, nullptr, '\0');
                    if (admit == bb::EnqueueDecision::MarkAndEnqueue)
                        queue.push_back(cur);
                }
                //bridge probe: 与 oracle/search_tag.cpp:191-211 一致 (相邻 spawn drop
                //  种子在同一 rotation 但 x 差 1 / y 差 > 1 时, 把"较高的那一侧 x 移 1
                //  再 down down"作为 bridge 种子). 位板版用 BBState 几何直推: 较高侧
                //  state.x 加减 1 后 yb -= 2, x 仍在 inbounds 范围内即入队.
                bb::BBState const *last_state = nullptr;
                for (bb::BBState const &cur : drop_seeds)
                {
                    if (last_state != nullptr)
                    {
                        if (last_state->r == cur.r &&
                            std::abs(static_cast<int>(last_state->xb) - static_cast<int>(cur.xb)) == 1 &&
                            std::abs(static_cast<int>(last_state->yb) - static_cast<int>(cur.yb)) > 1)
                        {
                            bb::BBState bridge;
                            if (last_state->yb > cur.yb)
                            {
                                bridge = *last_state;
                                bridge.xb = static_cast<std::int8_t>(
                                    bridge.xb + (last_state->xb > cur.xb ? -1 : +1));
                            }
                            else
                            {
                                bridge = cur;
                                bridge.xb = static_cast<std::int8_t>(
                                    bridge.xb + (cur.xb > last_state->xb ? -1 : +1));
                            }
                            bridge.yb = static_cast<std::int8_t>(bridge.yb - 2);
                            auto admit = dedup.try_admit(bridge, nullptr, '\0');
                            if (admit == bb::EnqueueDecision::MarkAndEnqueue)
                                queue.push_back(bridge);
                        }
                    }
                    last_state = &cur;
                }

                //=== BFS 主循环 (引擎手写: 已有种子, run_bb_bfs 无 multi-seed 入口) ===
                TagSearch20gNeighbors neighbors{T, &usable_arr, &inbounds_arr, &board, &ctx};
                TagSearch20gVisitor<T, EvalCallback> visitor{&ctx, &usable_arr, &board, &emitted_keys};
                if constexpr (!std::is_void_v<EvalCallback>)
                    visitor.on_land = on_land;
                std::size_t head = 0;
                while (head < queue.size())
                {
                    bb::BBState cur = queue[head++];
                    if (!visitor.on_pop(cur))
                        return;
                    bool stop = false;
                    auto emit = [&](bb::BBState const &child, char action,
                                    bb::BBState const *parent_override = nullptr) -> bb::EnqueueDecision
                    {
                        if (stop)
                            return bb::EnqueueDecision::Skip;
                        bb::BBState const *parent = parent_override ? parent_override : &cur;
                        bb::EnqueueDecision d = dedup.try_admit(child, parent, action);
                        if (d == bb::EnqueueDecision::Skip)
                            return d;
                        if (!visitor.on_admit(child, action, d))
                        {
                            stop = true;
                            return d;
                        }
                        if (d == bb::EnqueueDecision::MarkAndEnqueue)
                            queue.push_back(child);
                        return d;
                    };
                    neighbors.expand(cur, emit);
                    if (stop)
                        return;
                }
            }

            //==================================================================
            //=== Tag-search T (search_t) =====================================
            //==================================================================
            // 与 oracle/search_tag.cpp:315-378 严格对应.
            //  邻居顺序: d l r z c (与 oracle 行 336/341/346/351/359 一致).
            //  旋转用 cover_if 升级 ' '→'z'/'c': 状态首次访问写 op=' ', 之后若
            //  通过旋转邻居再访, 升级到 op='z'/'c' 并重入队 (允许重新探索).
            //  完整状态 (无可移动 d) 进入 "incomplete" 列表, 出 BFS 后批量 emit.
            //
            // T-spin 的 check 使用 TetrisMapSnap, 但 build_snap 内部把 'T'
            // 的 4 个 rotation 各预算一份 4xN 列表交集. 位板 usable_arr[r] 已
            // 经在 build_usable_for_piece 阶段对 (T, r) 全 cell 做了 board 碰撞
            // AND, 与 snap 的等价性见 commit 7a-2 注释 (cells_key_for + check_TR).
            //
            // T-spin 的 op/parent 由位板 PathMark 持有 (与 PathStrategy /
            //   SimulateStrategy 同款: ctx.path_mark_, 按 (r, xb, yb) 索引,
            //   版本化清理). 在 T 的单 piece BFS 中, (r, xb, yb) 与 master
            //   oracle node_mark_.set / cover_if 共享同样的寻址 + 三态语义,
            //   仅替换 mark 表的物理底层. 至此 TagStrategy 不再持有 master

            struct TagSearchTNeighbors
            {
                char piece_t;
                std::array<map_t, kMaxR> const *usable_arr;
                std::array<map_t, kMaxR> const *inbounds_arr;

                template<class Emit>
                void expand(bb::BBState const &cur, Emit emit)
                {
                    //d
                    {
                        bb::BBState nd = cur;
                        nd.yb = static_cast<std::int8_t>(nd.yb - 1);
                        emit(nd, ' ');
                    }
                    //l
                    {
                        bb::BBState nL = cur;
                        nL.xb = static_cast<std::int8_t>(nL.xb - 1);
                        emit(nL, ' ');
                    }
                    //r
                    {
                        bb::BBState nR = cur;
                        nR.xb = static_cast<std::int8_t>(nR.xb + 1);
                        emit(nR, ' ');
                    }
                    //z (走 first_passing_kick_bb(inbounds_arr) = master rotate_*);
                    //  与 oracle (search_tag.cpp:351-356) 一致: 先做 board check
                    //  (= usable_at_bb), 通过后才进入 cover_if 升级路径; 失败 -> 不
                    //  emit (避免在 dedup 中错误升级 op=' '→'z' 到 board-阻挡 state).
                    if (auto rn = Helpers::first_passing_kick_bb(piece_t, bb::KickDir::Ccw, cur, *inbounds_arr))
                    {
                        if (Helpers::usable_at_bb(rn->r, rn->xb, rn->yb, *usable_arr))
                            emit(*rn, 'z');
                    }
                    //c
                    if (auto rn = Helpers::first_passing_kick_bb(piece_t, bb::KickDir::Cw, cur, *inbounds_arr))
                    {
                        if (Helpers::usable_at_bb(rn->r, rn->xb, rn->yb, *usable_arr))
                            emit(*rn, 'c');
                    }
                }
            };

            // T-spin dedup: 与 oracle search_tag (search_tag.cpp:320/336/341/346/
            //   所有寻址按 (r, xb, yb) (= bbox 系), 与 PathStrategy /
            //   SimulateStrategy 选用同款 dedup 底层.
            //
            //   action == 0 (BFS engine 起点回调) -> mark_bbox(state) (= oracle line
            //     320 mark(entry) 等价: 仅占位 version, op '\0' 与默认 init 状态在
            //     cover_if(' ', ...) 检查中统一返回 false; 入口状态在 BFS 内不会被
            //     回访). 起点协议 prev = self (自指), build_path 反向回溯靠
            //     prev == cur 终止, 不再使用 0xFF 哨兵.
            //   action == ' ' -> set_bbox(state, parent, ' ') (与 oracle set 等价).
            //   action == 'z'/'c' -> cover_if_bbox(state, parent, ' ', action)
            //     (与 oracle cover_if 等价: 仅在当前 op == ' ' 时升级, 否则拒绝).
            //
            //   board 不可放置 (usable_arr 失败) -> 在 emit 处先 usable check 再 emit
            //     (z/c 分支已经做了); d/l/r 在 dedup 后端做 usable 过滤后回 MarkOnly,
            //     与 oracle "set 但 check(snap) 失败 -> 不 push 入队" 等价.
            struct TagSearchTDedup
            {
                typename Helpers::PathMark *path_mark;
                std::array<map_t, kMaxR> const *usable_arr;

                bb::EnqueueDecision try_admit(bb::BBState const &s, bb::BBState const *parent, char action)
                {
                    if (s.xb < 0 || s.xb >= kW || s.yb < 0 || s.yb >= kH || s.r >= kMaxR)
                        return bb::EnqueueDecision::Skip;
                    using PrevKey = typename Helpers::PathMark::PrevKey;
                    PrevKey pk;
                    if (parent == nullptr)
                    {
                        //起点协议: prev = self.
                        pk = PrevKey{static_cast<std::uint8_t>(s.r),
                                     static_cast<std::int8_t>(s.xb),
                                     static_cast<std::int8_t>(s.yb)};
                    }
                    else
                    {
                        pk = PrevKey{static_cast<std::uint8_t>(parent->r),
                                     static_cast<std::int8_t>(parent->xb),
                                     static_cast<std::int8_t>(parent->yb)};
                    }

                    bool admitted = false;
                    if (action == 0)
                    {
                        //入口 self-prev 初始化: 与 oracle search_tag.cpp:320 的
                        //  cover_if(entry, entry, '\0', ' ') 等价 — 占用 version
                        //  并写入 (prev = self, op = ' '), 让后续 rotate 回 entry
                        //  时 cover_if_bbox(self_prev, ' ', 'z'/'c') 升级路径
                        //  打通 (entry-grounded TSpin 漏判修复). prev = self
                        //  与 search_path / search_simulate 的 root 协议同形,
                        //  build_path 不会经过 t_mark_, 不影响 incomplete entry
                        //  解码的 self-prev guard.
                        admitted = path_mark->cover_if_bbox(static_cast<int>(s.r),
                                                            static_cast<int>(s.xb),
                                                            static_cast<int>(s.yb),
                                                            pk,
                                                            '\0',
                                                            ' ');
                    }
                    else if (action == ' ')
                    {
                        admitted = path_mark->set_bbox(static_cast<int>(s.r),
                                                       static_cast<int>(s.xb),
                                                       static_cast<int>(s.yb),
                                                       pk,
                                                       ' ');
                    }
                    else
                    {
                        admitted = path_mark->cover_if_bbox(static_cast<int>(s.r),
                                                            static_cast<int>(s.xb),
                                                            static_cast<int>(s.yb),
                                                            pk,
                                                            ' ',
                                                            action);
                    }
                    if (!admitted)
                        return bb::EnqueueDecision::Skip;
                    if (!Helpers::usable_at_bb(s.r, s.xb, s.yb, *usable_arr))
                        return bb::EnqueueDecision::MarkOnly;
                    return bb::EnqueueDecision::MarkAndEnqueue;
                }
            };

            template<char T, class EvalCallback = void>
            static void search_t_native(Context &ctx, map_t const &board, bb::BBState const &entry,
                                        EvalCallback *on_land = nullptr)
            {
                std::array<map_t, kMaxR> usable_arr{};
                Helpers::build_usable_for_piece(T, board, usable_arr);
                std::array<map_t, kMaxR> inbounds_arr{};
                Helpers::build_inbounds_for_piece(T, inbounds_arr);

                //commit (tag-bb-native): TSpinHook::on_init_rotations 在 board 上
                //  填一份 corners3_arr (per-rotation 的 "≥3 corner 占用" 位图).
                //  下文 incomplete 出循环 emit 阶段直接读 corners3_arr[r].get(xb, yb)
                //  作为 is_ready, 与 search_path.h Run20gVisitor 用 TSpinHook::
                //  on_emit 的 landings & corners3_arr 同款表. 替换原
                //  master-coord SpinHook::check_ready, 让 search_t_native 不再
                //  触碰 master 坐标系.
                typename SpinHook::template RotState<map_t, kMaxR> rot_state{};
                SpinHook::template on_init_rotations<RuleSpec, T, map_t, kMaxR>(board, rot_state);
                std::array<map_t, kMaxR> const &corners3_arr = rot_state.corners3_arr;

                //与 oracle search_tag.cpp:168 一致: search() 进入即 clear node_mark_.
                //  位板 t_mark 是 PathMark (L3). t_mark 仅由 search_t_native 读写, 而
                //  search_t_native 仅对 SpinHook::active_for_piece<T>=true 的 piece
                //  实例化, 故 t_mark 物理上单 piece 写入, 不需要 piece 分桶.
                //commit B: t_mark 栈分配; PathMark 默认构造即空表 (used_ 全 false).
                PathMark t_mark;
                std::vector<bb::CellsKey> emitted_keys;
                emitted_keys.reserve(32);
                std::vector<bb::BBState> incomplete;
                incomplete.reserve(32);

                struct Visitor
                {
                    std::array<map_t, kMaxR> const *usable_arr;
                    std::vector<bb::CellsKey> *emitted_keys;
                    std::vector<bb::BBState> *incomplete;

                    bool on_pop(bb::BBState const &cur)
                    {
                        bool grounded = !Helpers::usable_at_bb(cur.r, cur.xb, cur.yb - 1, *usable_arr);
                        if (!grounded)
                            return true;
                        bb::CellsKey k = Helpers::cells_key_for_state(cur);
                        for (bb::CellsKey ek : *emitted_keys)
                            if (ek == k)
                                return true;
                        emitted_keys->push_back(k);
                        incomplete->push_back(cur);
                        return true;
                    }

                    bool on_admit(bb::BBState const &, char, bb::EnqueueDecision)
                    {
                        return true;
                    }
                };

                TagSearchTNeighbors neighbors{T, &usable_arr, &inbounds_arr};
                TagSearchTDedup dedup{&t_mark, &usable_arr};
                Visitor visitor{&usable_arr, &emitted_keys, &incomplete};
                ctx.node_search_path_.clear();
                bb::run_bb_bfs(entry, neighbors, dedup, visitor, ctx.node_search_path_);

                for (bb::BBState const &sunk_state : incomplete)
                {
                    auto result = t_mark.get_bbox(static_cast<int>(sunk_state.r),
                                                  static_cast<int>(sunk_state.xb),
                                                  static_cast<int>(sunk_state.yb));
                    bb::BBState last_state{}; // zero-init: t==0 表示"无 parent"
                    char last_op = result.second;
                    {
                        //prev == cur (= sunk_state) 即起点 self prev (= oracle
                        //  search_t cover_if(entry, entry, '\0', ' ') 写入), last_state
                        //  保持零值 — 与 oracle node_ex.last 的语义一致 (oracle
                        //  写入 entry 自身, 但 last 字段后续被 SpinHook 用作"上一节点"
                        //  推断, 自指无意义, 保持零值更安全; is_last_rotate 仅
                        //  靠 last_op 判定, 二者解耦). 否则 prev 指向真实 parent.
                        bool is_self_prev =
                            static_cast<int>(result.first.r) == static_cast<int>(sunk_state.r) &&
                            static_cast<int>(result.first.xb) == static_cast<int>(sunk_state.xb) &&
                            static_cast<int>(result.first.yb) == static_cast<int>(sunk_state.yb);
                        if (!is_self_prev)
                        {
                            last_state.t = sunk_state.t;
                            last_state.r = result.first.r;
                            last_state.xb = result.first.xb;
                            last_state.yb = result.first.yb;
                        }
                    }
                    LandPoint node_ex;
                    node_ex.state = sunk_state;
                    node_ex.last = last_state;
                    node_ex.is_check = true;
                    node_ex.is_last_rotate = (last_op != ' ');
                    //commit (tag-bb-native): is_ready 不再走 master-coord 的
                    //  SpinHook::check_ready (block_data + x_diff/y_diff);
                    //  改读位板 corners3_arr[r].get(xb, yb) — 与 search_path.h
                    //  Run20gVisitor / TSpinHook::on_emit 用同一张表 (master
                    //  TetrisContext 不再被运行时访问). corners3_arr 在
                    //  search_t_native 入口由 TSpinHook::on_init_rotations
                    //  填好, 此处只读.
                    node_ex.is_ready = corners3_arr[sunk_state.r].get(
                        static_cast<int>(sunk_state.xb),
                        static_cast<int>(sunk_state.yb));
                    // Phase 2: T-spin 路径也在此展开运行时 sunk_state.r → 编译期 Rs，
                    //   触发 EvalCallback，跳过 push_back。
                    // TODO(multi-thread): 多线程路径改为构造 PendingTask，参见 search_path.h。
                    if constexpr (!std::is_void_v<EvalCallback>)
                    {
                        [&]<std::size_t... Rs>(std::index_sequence<Rs...>)
                        {
                            ((Rs == sunk_state.r
                                  ? (on_land->template operator()<T, static_cast<std::uint8_t>(Rs)>(node_ex),
                                     true)
                                  : false) ||
                             ...);
                        }(std::make_index_sequence<Helpers::template rcount_v<T>()>{});
                    }
                    else
                    {
                        ctx.land_point_cache_.push_back(node_ex);
                    }
                }
            }

            //==================================================================
            //=== make_path ===================================================
            //==================================================================
            // 邻居与 BFS dedup 与 1g search 同形 (z c l r d, single rotate, mark+
            // usable 三态). 命中谓词 = cells_key(child) == cells_key(land_point.node).
            // T-spin 末段 (land_point.type != None) 走 oracle 的 "递归 + suffix"
            // 模式: 先递归到 land_point.last (None 路径), 末尾 z/c 反查; 与
            // oracle/search_tag.cpp:62-79 一致.
            struct TagMakePathNeighbors
            {
                char piece_t;
                std::array<map_t, kMaxR> const *usable_arr;
                std::array<map_t, kMaxR> const *inbounds_arr;

                template<class Emit>
                void expand(bb::BBState const &cur, Emit emit)
                {
                    //commit (remove-context): rotate_* = first_passing_kick_bb(inbounds).
                    if (auto rn = Helpers::first_passing_kick_bb(piece_t, bb::KickDir::Ccw, cur, *inbounds_arr))
                    {
                        emit(*rn, 'z');
                    }
                    if (auto rn = Helpers::first_passing_kick_bb(piece_t, bb::KickDir::Cw, cur, *inbounds_arr))
                    {
                        emit(*rn, 'c');
                    }
                    {
                        bb::BBState nL = cur;
                        nL.xb = static_cast<std::int8_t>(nL.xb - 1);
                        emit(nL, 'l');
                    }
                    {
                        bb::BBState nR = cur;
                        nR.xb = static_cast<std::int8_t>(nR.xb + 1);
                        emit(nR, 'r');
                    }
                    {
                        bb::BBState nd = cur;
                        nd.yb = static_cast<std::int8_t>(nd.yb - 1);
                        emit(nd, 'd');
                    }
                }
            };

            //=== TagMakePathDedup: 与 oracle 的 z/c 分支 "set 二态 (无 usable check)"
            //=== 严格对应 (oracle/search_tag.cpp:91-119, z/c 用 set 后立刻判命中,
            //=== 不再要求子节点 check), 而 l/r/d 用 set+check 二段; 整体可统一为
            //=== set 二态: 命中判定 (cells_key) 在 visitor.on_admit 完成, 不需要
            //=== 在 dedup 处过滤 usable. 与 search_tag.cpp 行为完全等价 (即使写入
            //=== 不可放置的 mark, 该状态也不会被弹出展开邻居 — 但其 cells_key
            //=== 命中判定仍可生效, 这正是 oracle z/c 命中即返回的语义).
            //=== 注意 oracle z/c 命中是 "set 成功后判 child->index_filtered == idx 立即
            //=== build_path", 而 l/r/d 是 "set + check 后判命中". 我们用 on_admit 在
            //=== 准入时判命中, set 不带 check 即可统一两侧分支语义.
            struct TagMakePathDedup
            {
                PathMark *path_mark;
                std::array<map_t, kMaxR> const *usable_arr;

                bb::EnqueueDecision try_admit(bb::BBState const &s, bb::BBState const *parent, char action)
                {
                    if (s.xb < 0 || s.xb >= kW || s.yb < 0 || s.yb >= kH || s.r >= kMaxR)
                        return bb::EnqueueDecision::Skip;
                    typename PathMark::PrevKey pk;
                    if (parent == nullptr)
                        //起点协议: prev = self.
                        pk = typename PathMark::PrevKey{static_cast<std::uint8_t>(s.r),
                                                        static_cast<std::int8_t>(s.xb),
                                                        static_cast<std::int8_t>(s.yb)};
                    else
                        pk = typename PathMark::PrevKey{static_cast<std::uint8_t>(parent->r),
                                                        static_cast<std::int8_t>(parent->xb),
                                                        static_cast<std::int8_t>(parent->yb)};
                    if (!path_mark->set_bbox(static_cast<int>(s.r),
                                             static_cast<int>(s.xb),
                                             static_cast<int>(s.yb),
                                             pk,
                                             action))
                        return bb::EnqueueDecision::Skip;
                    //l/r/d 分支再做 usable 过滤; z/c 分支 oracle 不查 usable. 统一
                    //  用 usable_at_bb 在准入即过滤"不可放置入队", 与 oracle 在
                    //  l/r/d 分支的 set+check && 在 z/c 分支因 check(map) 已先判
                    //  导致只 set "可放置" 状态, 二者最终入队集合一致.
                    if (!Helpers::usable_at_bb(s.r, s.xb, s.yb, *usable_arr))
                        return bb::EnqueueDecision::MarkOnly;
                    return bb::EnqueueDecision::MarkAndEnqueue;
                }
            };

            struct TagMakePathVisitor
            {
                bb::CellsKey index_landpoint;
                bb::BBState found{};
                bool hit = false;

                bool on_pop(bb::BBState const &)
                {
                    return true;
                }

                bool on_admit(bb::BBState const &s, char, bb::EnqueueDecision d)
                {
                    //=== 与 oracle/search_tag.cpp:39-159 一致: 命中判定要求 "set && check"
                    //=== 同时成立 (z/c 是 check 在前 set 在后, l/r/d 是 set 在前 check 在后,
                    //=== 但都要二者都过). 这里 d == MarkAndEnqueue 表示 PathMark 写入成功
                    //=== (= oracle 的 set 成功) 且 usable 通过 (= oracle 的 check(map) 通过).
                    //=== MarkOnly 是 "set 成功但 check 失败", 不算 hit.
                    if (d != bb::EnqueueDecision::MarkAndEnqueue)
                        return true;
                    bb::CellsKey k = Helpers::cells_key_for_state(s);
                    if (k == index_landpoint)
                    {
                        found = s;
                        hit = true;
                        return false;
                    }
                    return true;
                }
            };

            // 内部 "None 路径" make_path: 不读 land_point.type, 仅按 cells_key
            // 命中目标 master node. 用作 T-spin 的 last 反查递归.
            //
            //commit (tag-bb-native): 入口 entry == target 短路改用 cells_key
            //  比较, 与 oracle `node->index_filtered == target->index_filtered`
            //  byte-equivalent (cells_key 与 master IndexFilter 一一对应),
            //  但 strategy 主体不再读 master node 的 index_filtered 字段.
            static std::vector<char>
            make_path_none(Context &ctx, bb::BBState const &spawn,
                           bb::BBState const &target,
                           Map<RuleSpec::width, RuleSpec::height> const &board)
            {
                if (target.t == 0)
                    return std::vector<char>();
                char piece_t = static_cast<char>(spawn.t);
                if (piece_info_t::index_of(piece_t) < 0)
                {
                    assert(false && "TagStrategy::make_path_none: piece type outside RuleSpec");
                    return std::vector<char>();
                }
                std::array<map_t, kMaxR> usable_arr{};
                Helpers::build_usable_for_piece(piece_t, board, usable_arr);
                std::array<map_t, kMaxR> inbounds_arr{};
                Helpers::build_inbounds_for_piece(piece_t, inbounds_arr);
                bb::BBState entry = spawn;
                bb::CellsKey index_landpoint = Helpers::cells_key_for_state(target);
                if (Helpers::cells_key_for_state(entry) == index_landpoint)
                {
                    return std::vector<char>();
                }
                PathMark make_path_mark;
                TagMakePathNeighbors neighbors{piece_t, &usable_arr, &inbounds_arr};
                TagMakePathDedup dedup{&make_path_mark, &usable_arr};
                TagMakePathVisitor visitor{index_landpoint, bb::BBState{}, false};
                ctx.node_search_path_.clear();
                bb::run_bb_bfs(entry, neighbors, dedup, visitor, ctx.node_search_path_);
                if (!visitor.hit)
                    return std::vector<char>();
                std::vector<char> path;
                int r = static_cast<int>(visitor.found.r);
                int xb = static_cast<int>(visitor.found.xb);
                int yb = static_cast<int>(visitor.found.yb);
                while (true)
                {
                    auto result = make_path_mark.get_bbox(r, xb, yb);
                    //prev == cur 即起点 (self prev 协议), 终止回溯.
                    if (static_cast<int>(result.first.r) == r &&
                        static_cast<int>(result.first.xb) == xb &&
                        static_cast<int>(result.first.yb) == yb)
                        break;
                    path.push_back(result.second);
                    r = static_cast<int>(result.first.r);
                    xb = static_cast<int>(result.first.xb);
                    yb = static_cast<int>(result.first.yb);
                }
                std::reverse(path.begin(), path.end());
                return path;
            }

            //=== make_path 入口 ================================================
            // SpinHook::lp_requires_last_rotate 区分 None / T-spin 路径; T-spin 路径
            // 末段递归 + 'z'/'c' 反查 (与 oracle search_tag.cpp:62-79 一致).
            //
            // 末段判定: 递归到 land_point.last 的路径若不以 'c'/'z' 结尾 (或
            // 路径为空), 则进入反查; 否则直接返回递归结果. 反查通过比较
            // last 旋转后的目标 cells_key 是否与 land_point.node 一致来选择
            // suffix 字符 — first_passing_kick_bb(inbounds) 的出口与 master
            // last->rotate_counterclockwise / rotate_clockwise 完全等价
            // (master 端 rotate_* = wall_kick_*[0] = 空盘 inbounds 第一个 kick).
            static std::vector<char>
            make_path_native(Context &ctx, bb::BBState const &spawn,
                             LandPoint const &land_point,
                             Map<RuleSpec::width, RuleSpec::height> const &board)
            {
                //commit (tag-bb-native): 入口短路改用 cells_key 比较, 与
                //  oracle `node->index_filtered == land_point->index_filtered`
                //  byte-equivalent.
                //commit C2 (N.4): 旧 `is_landpoint_none(lp)` 反转语义到
                //  `lp_requires_last_rotate(cfg, lp)` 之后, 该新 trait 内联了
                //  `cfg->last_rotate` 检查 — Tag 走 oracle/search_tag 路径不消费
                //  Config (默认 last_rotate=false), 但 Tag 仍依赖 lp.type != None
                //  来决定是否需要末段旋转后缀. 这里改用 get_last_state(lp) 作 proxy:
                //  TSpinHook (Tag 唯一消费者) 在 lp.type != None 时 emit_set 调
                //  find_last_rotate_pred + apply_emit_* 会写 lp.last; lp.type=None
                //  时 lp.last.t==0. 二者等价, 不依赖 ctx.config_.
                bb::BBState const *last_ptr = SpinHook::get_last_state(land_point);
                bool has_spin_landpoint = last_ptr != nullptr;
                if (!has_spin_landpoint &&
                    Helpers::cells_key_for_state(spawn) ==
                        Helpers::cells_key_for_state(land_point.state))
                {
                    return std::vector<char>();
                }
                std::vector<char> path = make_path_none(ctx, spawn, land_point.state, board);
                if (has_spin_landpoint &&
                    (path.empty() || (path.back() != 'c' && path.back() != 'z')))
                {
                    bb::BBState const &last_state = *last_ptr;
                    bb::BBState const &target_state = land_point.state;
                    path = make_path_none(ctx, spawn, last_state, board);
                    if (last_state.t != 0 && target_state.t != 0)
                    {
                        bb::CellsKey target_key = Helpers::cells_key_for_state(target_state);
                        std::array<map_t, kMaxR> inbounds_arr{};
                        Helpers::build_inbounds_for_piece(static_cast<char>(last_state.t), inbounds_arr);
                        if (auto ccw = Helpers::first_passing_kick_bb(static_cast<char>(last_state.t), bb::KickDir::Ccw, last_state, inbounds_arr);
                            ccw && Helpers::cells_key_for_state(*ccw) == target_key)
                        {
                            path.push_back('z');
                            return path;
                        }
                        if (auto cw = Helpers::first_passing_kick_bb(static_cast<char>(last_state.t), bb::KickDir::Cw, last_state, inbounds_arr);
                            cw && Helpers::cells_key_for_state(*cw) == target_key)
                        {
                            path.push_back('c');
                            return path;
                        }
                    }
                }
                return path;
            }
        };
    } // namespace movegen

    //=== 顶层 alias: 与 PathStrategy / SimulateStrategy 同形, 让上层可直接拼
    //    m_tetris2::TagStrategy<...> 而不必带 movegen:: 前缀.
    template<class SpinHook, class RuleSpec, class Policy = void>
    using TagStrategy = movegen::TagStrategy<SpinHook, RuleSpec, Policy>;
} // namespace m_tetris2

//=== Search tag: TetrisEngine 第三模板参数的"位板版"用法 ===================
// 与 oracle 的 search_tag::Search 同形 — TetrisEngine<Rule, AI, tag::Search>
// 单参数即可装配. TagStrategy 注释明确 "当前调用面只有 TSpinHook 一种实例",
// 因此 Default/Custom Config 解析都落到 TSpinHook; Config trait 仅保留
// enable 占位字段以与 path::SearchWith 形态一致.
//commit (seven-namespace): namespace tag 从 m_tetris2::tag 提到顶层 tag, 与
//  path / aspin / tspin / cautious / simple / simulate 形态统一.
namespace tag
{
    struct DefaultConfig
    {
    };

    template<class Config = DefaultConfig>
    struct SearchWith
    {
        template<class RuleType>
        using type = m_tetris2::movegen::Searcher<
            m_tetris2::TagStrategy,
            m_tetris2::TSpinHook,
            typename RuleType::rule_spec,
            typename m_tetris2::detail::policy_or<Config>::type>;

        template<class NewPolicy>
        struct PolicyOverrideConfig : Config
        {
            using PolicyType = NewPolicy;
        };

        template<class NewPolicy>
        using rebind_policy = SearchWith<PolicyOverrideConfig<NewPolicy>>;
    };

    using Search = SearchWith<>;
} // namespace tag

#endif // TETRIS_AI_RUNNER_SEARCH_TAG_H_
