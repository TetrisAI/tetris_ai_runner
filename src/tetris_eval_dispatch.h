#pragma once

//==========================================================================
// tetris_eval_dispatch.h — AI eval 接口编译期分发。
//
// 提供：
//   1. concept 探针（检测 AI::eval 需要哪种 SpinType 参数及 Map 类型）：
//
//        EvalWantsSpinT<AI,Spec,T,R,SpinT> — eval(TetrisNode<Det,SpinT>, TetrisMap<W,H>, TetrisMap<W,H>, int)
//                                            SpinT 通常为 AI::EvalSpinType
//        EvalWantsNoSpinMapT               — eval(TetrisNode<Det,monostate>,  TetrisMap<W,H>, TetrisMap<W,H>, int)
//
//   2. TetrisCallEval<AI, Spec>
//      供 Search 层 EvalCallback（for_each_move<T,R> 推式回调）直接调用：
//        - call_eval_typed<T,R>：T、R 均为编译期，构造 TetrisNode 并调 ai.eval()
//        - SpinType 由 AI::EvalSpinType 决定；无 EvalSpinType 则 fallback 至 monostate
//
// LandPoint 约定（三种形态）：
//   NoSpinHook 路径：TetrisLandPoint             — .state（State），无 .type
//
// 依赖：tetris_node.h, typed_dispatch.h, tetris_shape.h（rotation_count）
//
// Note: search_tspin.h / search_aspin.h are NOT included here to avoid a
// circular-include cycle:
//   search_tspin.h -> tetris_core.h -> tetris_eval_dispatch.h -> search_tspin.h (guard skip)
//==========================================================================

#include "tetris_node.h"
#include "typed_dispatch.h"
#include "tetris_shape.h"

#include <cstdint>
#include <type_traits>
#include <utility>
#include <variant> // std::monostate

namespace m_tetris2
{
    //----------------------------------------------------------------------
    // SearchRuleSpecOf<Search>
    //
    // Safe accessor: yields Search::rule_spec if it exists, void otherwise.
    // Used by TetrisCore::eval() so that Searches without a rule_spec
    // still compile through the generic eval path.
    //----------------------------------------------------------------------
    template<class Search, class = void>
    struct SearchRuleSpecOf
    {
        using type = void;
    };

    template<class Search>
    struct SearchRuleSpecOf<Search, std::void_t<typename Search::rule_spec>>
    {
        using type = typename Search::rule_spec;
    };

    //----------------------------------------------------------------------
    // SpinType — independent definitions
    //
    // These values match the spin enums used by search_tspin::Search and
    // search_aspin::Search. The enumerators are used in
    // TetrisNode<Details,SpinT>.spin and in concept probes below.
    //
    // NOTE: Do NOT use the search_*::Search::*SpinType aliases here; those
    // headers cannot be included without triggering the circular-include
    // cycle described above. Any change to the enumerator layout in
    // search_tspin.h / search_aspin.h must be mirrored here.
    //----------------------------------------------------------------------
    enum TSpinType : uint8_t
    {
        TSpinNone = 0,
        TSpin = 1,
        TSpinMini = 2
    };
    enum ASpinType : uint8_t
    {
        ASpinNone = 0,
        ASpin = 1
    };

    //----------------------------------------------------------------------
    // concept 探针
    //
    // 检测 AI::eval 接受哪种 SpinType 及 Map 类型。
    // 探针使用具体的 TetrisNodeDetails<Spec,T,R> 实例化 TetrisNode，
    // 通过普通参数推导调用 ai.eval(node, ...)，不依赖显式模板成员语法。
    //
    // 设计约定：
    //   AI 应声明 using EvalSpinType = <SpinT>（即 search_tspin/aspin 里的 enum）。
    //   TetrisCallEval 通过 AIEvalSpinTypeOf<AI>::type 安全提取该类型（无则 void），
    //   再用 EvalWantsSpinT<AI,Spec,T,R,SpinT> 探测；SpinT=void 时 concept
    //   自然不满足（TetrisNode<Det,void> 不合法），从而安全 fallback 到 monostate 路径。
    //   全程无 hard-error，不需要 requires { typename AI::EvalSpinType; } 短路保护。
    //----------------------------------------------------------------------

    // 安全提取 AI::EvalSpinType：有则取，无则 void（供 EvalWantsSpinT 使用）
    template<class AI, class = void>
    struct AIEvalSpinTypeOf
    {
        using type = void;
    };
    template<class AI>
    struct AIEvalSpinTypeOf<AI, std::void_t<typename AI::EvalSpinType>>
    {
        using type = typename AI::EvalSpinType;
    };

    // 泛化 SpinT 路径：eval(TetrisNode<Details, SpinT>, TetrisMap<W,H>, TetrisMap<W,H>, int)
    // SpinT 由 AIEvalSpinTypeOf<AI>::type 提供（AI::EvalSpinType 的真实类型，
    // 与 eval 签名严格一致），或 void（AI 无 EvalSpinType 时，concept 自然不满足）。
    // SpinT=void 时 !std::is_void_v<SpinT> 约束在 concept 内短路，不会实例化
    // TetrisNode<Det,void>（TetrisNode<Det,void> 的 spin 字段是 void，ill-formed）。
    template<class AI, class Spec, char T, uint8_t R, class SpinT>
    concept EvalWantsSpinT = !std::is_void_v<SpinT> && requires(
                                                           AI const &ai,
                                                           TetrisNode<TetrisNodeDetails<Spec, T, R>, SpinT> node,
                                                           TetrisMap<Spec::width, Spec::height> const &m,
                                                           int clear)
    {
        {ai.eval(node, m, m, clear)};
    };

    // NoSpin + MapT 路径：eval(TetrisNode<Details, monostate>, TetrisMap<W,H>, TetrisMap<W,H>, int)
    template<class AI, class Spec, char T, uint8_t R>
    concept EvalWantsNoSpinMapT = requires(
        AI const &ai,
        TetrisNode<TetrisNodeDetails<Spec, T, R>, std::monostate> node,
        TetrisMap<Spec::width, Spec::height> const &m,
        int clear)
    {
        {ai.eval(node, m, m, clear)};
    };

    //----------------------------------------------------------------------
    // TetrisCallEval<AI, Spec>
    //
    // Search 层 EvalCallback（for_each_move<T,R> 推式回调）的编译期分发桥。
    //
    // 使用方式（search_eval EvalCallback 内）：
    //   TetrisCallEval<TetrisAI, RuleSpec>::call_eval_typed<T, R>(
    //       ai, lp, after, src_map, clear);
    //
    // 模板参数：
    //   AI   — AI 类型（持有 eval 模板成员函数，参数推导形式）
    //   Spec — RuleSpec（提供 rotation_count 等信息）
    //
    // 注：T、R 均由 Search 层在调用点静态传入，编译器可完全内联，零运行时 dispatch。
    //----------------------------------------------------------------------
    template<class AI, class Spec>
    struct TetrisCallEval
    {
    public:
        //------------------------------------------------------------------
        // call_eval_typed<T, R>：T 和 R 均为编译期，构造 TetrisNode 并调 eval。
        //
        // 供 Search 层 EvalCallback（for_each_move<T,R> 回调）直接调用，
        // T、R 在调用点已静态确定，编译器可完全内联，零运行时 dispatch。
        //------------------------------------------------------------------
        template<char T, uint8_t R, class LP>
        static auto call_eval_typed(AI const &ai, LP const &lp,
                                    TetrisMap<Spec::width, Spec::height> const &after,
                                    TetrisMap<Spec::width, Spec::height> const &src_map,
                                    size_t clear)
        {
            if constexpr (!std::is_same_v<Spec, void> && !shape::has_op<Spec, T, R>)
            {
                static_assert(!(!std::is_same_v<Spec, void> && !shape::has_op<Spec, T, R>),
                              "call_eval_typed invoked with (T,R) not in Spec::ops. "
                              "All AIs must use TetrisMap<W,H>-based eval signatures.");
            }
            else
            {
                using Det = TetrisNodeDetails<Spec, T, R>;

                // ── 从 LandPoint 提取位板坐标 ──────────────────────────────────
                // State 持有 (t, r, xb, yb)：
                //   xb = bbox 左列（= TetrisNode.x = master status.x - origin.x）
                //   yb = bbox 底行 y-up（= node.row）
                // TetrisNode.y = status.y = yb + kOriginY（kOriginY 为编译期常量）
                uint8_t spin_u8 = 0u;
                int8_t node_x = 0;
                int8_t node_y = 0;
                if constexpr (std::is_pointer_v<std::remove_cvref_t<LP>>)
                {
                    node_x = static_cast<int8_t>(lp->status.x);
                    node_y = static_cast<int8_t>(lp->status.y);
                }
                else
                {
                    // 均持有 .state（State{t,r,xb,yb}）
                    node_x = static_cast<int8_t>(lp.state.xb);
                    // TetrisNode.y = yb + kOriginY，在编译期使用 Det::kOriginY 计算
                    node_y = static_cast<int8_t>(
                        static_cast<int>(lp.state.yb) +
                        TetrisNodeBase<Det>::kOriginY);
                }
                if constexpr (requires { lp.type; })
                {
                    spin_u8 = static_cast<uint8_t>(lp.type);
                }

                if constexpr (EvalWantsSpinT<AI, Spec, T, R, typename AIEvalSpinTypeOf<AI>::type>)
                {
                    using SpinT = typename AIEvalSpinTypeOf<AI>::type;
                    TetrisNode<Det, SpinT> node;
                    node.x = node_x;
                    node.y = node_y;
                    node.spin = static_cast<SpinT>(spin_u8);
                    return ai.eval(node, after, src_map, (int)clear);
                }
                else if constexpr (EvalWantsNoSpinMapT<AI, Spec, T, R>)
                {
                    TetrisNode<Det, std::monostate> node;
                    node.x = node_x;
                    node.y = node_y;
                    return ai.eval(node, after, src_map, (int)clear);
                }
                else
                {
                    static_assert(EvalWantsNoSpinMapT<AI, Spec, T, R>,
                                  "AI must implement one of: "
                                  "eval(TetrisNode<Det, EvalSpinType>, TetrisMap<W,H>, TetrisMap<W,H>, int) [requires using EvalSpinType = ...], or "
                                  "eval(TetrisNode<Det,monostate>, TetrisMap<W,H>, TetrisMap<W,H>, int).");
                }
            } // end else (has_op)
        }
    };

    //==========================================================================
    // TetrisCallGet<AI>  — get() 侧调用桥。
    //
    // get() 侧调用分发器，与 TetrisCallEval 保持对称命名。
    //
    // 职责：将框架持有的 LandPoint（可能带 spin 信息的
    //       包装结构体）转换为 AI::get() 所期望的第一参数类型，然后透传其余参数。
    //
    // 转换规则（编译期 if constexpr，零运行时代价）：
    //   ① ai.get(params...) 直接有效（AI::get 不接受 lp，第一参数为 eval_result）
    //      → 跳过 lp，直接透传 params
    //   ② ai.get(lp, ...) 直接有效（第一参数接受 const ref 或 by-value）→ 直接传
    //   ③ ai.get(non_const_lp, ...) 有效（第一参数要求 non-const ref，同型）
    //      → 拷贝 lp 后传 non-const ref（LP 为 POD，编译器可内联消除）
    //   ④ ①②③ 均无效 → AI::get 第一参数为裸指针，直接取 lp 或 lp.node 透传

    //
    // 注：不需要反射 AI::get 的第一参数类型，探针直接在 LP 上操作，
    //     无 incomplete-type 或 circular-include 问题。
    //
    // 用法（TetrisSelectGet 各特化内）：
    //   TetrisCallGet<TetrisAI>::get(ai, lp, result, depth, ...)
    //==========================================================================
    template<class AI>
    struct TetrisCallGet
    {
    private:
        template<class LP>
        static auto call0(AI const &ai, LP const &lp)
        {
            if constexpr (requires { ai.get(); })
            {
                return ai.get();
            }
            else if constexpr (requires { ai.get(lp); })
            {
                return ai.get(lp);
            }
            else if constexpr (requires(LP & lp_mut) { ai.get(lp_mut); })
            {
                LP lp_copy = lp;
                return ai.get(lp_copy);
            }
            else
            {
                static_assert(sizeof(LP) == 0,
                              "TetrisCallGet: no compatible AI::get signature for current LandPoint + params.");
            }
        }

        template<class LP, class P0>
        static auto call1(AI const &ai, LP const &lp, P0 const &p0)
        {
            if constexpr (requires { ai.get(p0); })
            {
                return ai.get(p0);
            }
            else if constexpr (requires { ai.get(lp, p0); })
            {
                return ai.get(lp, p0);
            }
            else if constexpr (requires(LP & lp_mut) { ai.get(lp_mut, p0); })
            {
                LP lp_copy = lp;
                return ai.get(lp_copy, p0);
            }
            else
            {
                return call0(ai, lp);
            }
        }

        template<class LP, class P0, class P1>
        static auto call2(AI const &ai, LP const &lp, P0 const &p0, P1 const &p1)
        {
            if constexpr (requires { ai.get(p0, p1); })
            {
                return ai.get(p0, p1);
            }
            else if constexpr (requires { ai.get(lp, p0, p1); })
            {
                return ai.get(lp, p0, p1);
            }
            else if constexpr (requires(LP & lp_mut) { ai.get(lp_mut, p0, p1); })
            {
                LP lp_copy = lp;
                return ai.get(lp_copy, p0, p1);
            }
            else
            {
                return call1(ai, lp, p0);
            }
        }

        template<class LP, class P0, class P1, class P2>
        static auto call3(AI const &ai, LP const &lp, P0 const &p0, P1 const &p1, P2 const &p2)
        {
            if constexpr (requires { ai.get(p0, p1, p2); })
            {
                return ai.get(p0, p1, p2);
            }
            else if constexpr (requires { ai.get(lp, p0, p1, p2); })
            {
                return ai.get(lp, p0, p1, p2);
            }
            else if constexpr (requires(LP & lp_mut) { ai.get(lp_mut, p0, p1, p2); })
            {
                LP lp_copy = lp;
                return ai.get(lp_copy, p0, p1, p2);
            }
            else
            {
                return call2(ai, lp, p0, p1);
            }
        }

        template<class LP, class P0, class P1, class P2, class P3>
        static auto call4(AI const &ai, LP const &lp, P0 const &p0, P1 const &p1, P2 const &p2, P3 const &p3)
        {
            if constexpr (requires { ai.get(p0, p1, p2, p3); })
            {
                return ai.get(p0, p1, p2, p3);
            }
            else if constexpr (requires { ai.get(lp, p0, p1, p2, p3); })
            {
                return ai.get(lp, p0, p1, p2, p3);
            }
            else if constexpr (requires(LP & lp_mut) { ai.get(lp_mut, p0, p1, p2, p3); })
            {
                LP lp_copy = lp;
                return ai.get(lp_copy, p0, p1, p2, p3);
            }
            else
            {
                return call3(ai, lp, p0, p1, p2);
            }
        }

    public:
        template<class LP, class... Params>
        static auto get(AI const &ai, LP const &lp, Params const &...params)
        {
            static_assert(sizeof...(Params) <= 4,
                          "TetrisCallGet currently supports up to 4 trailing params.");
            if constexpr (sizeof...(Params) == 0)
            {
                return call0(ai, lp);
            }
            else if constexpr (sizeof...(Params) == 1)
            {
                return call1(ai, lp, params...);
            }
            else if constexpr (sizeof...(Params) == 2)
            {
                return call2(ai, lp, params...);
            }
            else if constexpr (sizeof...(Params) == 3)
            {
                return call3(ai, lp, params...);
            }
            else
            {
                return call4(ai, lp, params...);
            }
        }
    };

} // namespace m_tetris2
