#pragma once

//==========================================================================
// tetris_eval_dispatch.h — Config-aware eval(map) 编译期契约探针。
//
// 新契约（见 artifacts/API_FREEZE_SPEC_v1.md）：
//   - AI::eval(MapT const&) -> Result，唯一显式参数 = 落块消行后盘面。
//   - 不再有 eval(node, after, src, clear) 四参形态，不提供兼容 shim。
//   - spin 枚举独立定义，与 search_tspin/aspin/amini 镜像。
//
// 本头不做 get 分发（get 由 tetris_core.h 的 GetArgs invoker 按 AI::GetArgs 萃取）。
//==========================================================================

#include "tetris_node.h"
#include "tetris_shape.h"

#include <cstdint>
#include <type_traits>
#include <utility>
#include <variant> // std::monostate

namespace m_tetris2
{
    //----------------------------------------------------------------------
    // SearchRuleSpecOf<Search>：安全取 Search::rule_spec，缺省 void。
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
    // SpinType — independent definitions（与 search_tspin/aspin/amini 镜像，
    // 不可改用 search_*::Search::*SpinType，避免循环包含）。
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
    enum AMiniType : uint8_t
    {
        AMiniNone = 0,
        AMiniMini = 1,
        AMiniFull = 2
    };

    //----------------------------------------------------------------------
    // AIEvalSpinTypeOf<AI>：安全提取 AI::EvalSpinType，无则 void。
    //----------------------------------------------------------------------
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

    //----------------------------------------------------------------------
    // AIEvalsMap<AI, MapT>：AI::eval(MapT const&) 可调用且返回 AI::Result。
    //----------------------------------------------------------------------
    template<class AI, class MapT, class = void>
    struct EvalResultOf
    {
        using type = void;
    };
    template<class AI, class MapT>
    struct EvalResultOf<AI, MapT, std::void_t<decltype(std::declval<AI const&>().eval(std::declval<MapT const&>()))>>
    {
        using type = decltype(std::declval<AI const&>().eval(std::declval<MapT const&>()));
    };

    template<class AI, class MapT>
    concept AIEvalsMap = std::is_same_v<typename EvalResultOf<AI, MapT>::type, typename AI::Result>;

    //----------------------------------------------------------------------
    // AIHasGetArgs<AI>：AI 声明 using GetArgs = type_list<...>。
    //----------------------------------------------------------------------
    template<class AI, class = void>
    struct AIHasGetArgs : std::false_type
    {
    };
    template<class AI>
    struct AIHasGetArgs<AI, std::void_t<typename AI::GetArgs>> : std::true_type
    {
    };
} // namespace m_tetris2
