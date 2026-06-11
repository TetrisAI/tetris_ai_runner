// search_tspin.h — 提供 T-spin 搜索所需的类型定义、Hook 装配接口与顶层 tspin::Search 适配器。
// 顶层 tspin::Search 与其他位板 search 标签同形，可直接用于 TetrisEngine 装配。
#ifndef TETRIS_AI_RUNNER_SEARCH_TSPIN_PHASE1_
#define TETRIS_AI_RUNNER_SEARCH_TSPIN_PHASE1_

#include "tetris_core.h"
#include "movegen_state.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace search_tspin
{
    // 类型定义命名空间。TSpinType、Config、TetrisNodeWithTSpinType 供全库直接引用。
    class Search
    {
    public:
        enum TSpinType
        {
            None,
            TSpin,
            TSpinMini
        };
        struct Config
        {
            bool allow_rotate_move = false;
            bool allow_180 = true;
            bool allow_d = true;
            bool allow_D = true;
            bool allow_LR = true;
            bool is_20g = false;
            bool last_rotate = false;
        };
        struct TetrisNodeWithTSpinType
        {
            TetrisNodeWithTSpinType()
            {
                std::memset(this, 0, sizeof(*this));
            }
            m_tetris2::bb::State state{};  // 落点位板坐标 (t/r/xb/yb)
            m_tetris2::bb::State last{};   // last-rotate 前驱位置（无前驱时 t==0）
            TSpinType type;
            union
            {
                struct
                {
                    bool is_check;
                    bool is_last_rotate;
                    bool is_ready;
                    bool is_mini_ready;
                };
                uint32_t flags;
            };
            // 兼容接口：以 state.t==0 作为"无效/nullptr"哨兵
            bool operator==(std::nullptr_t) const
            {
                return state.t == 0;
            }
            bool operator!=(std::nullptr_t) const
            {
                return state.t != 0;
            }
            bool operator==(TetrisNodeWithTSpinType const &other) const
            {
                return state.t == other.state.t && state.r == other.state.r &&
                       state.xb == other.state.xb && state.yb == other.state.yb &&
                       type == other.type && flags == other.flags;
            }
        };
    };
} // namespace search_tspin

#endif // TETRIS_AI_RUNNER_SEARCH_TSPIN_PHASE1_

// tspin::Search 需要 search_path.h 的 PathStrategy 等定义，通过双阶段头机制在 search_path.h 包含完后展开。
#if !defined(TETRIS_AI_RUNNER_SEARCH_TSPIN_PHASE2_) && defined(TETRIS_AI_RUNNER_SEARCH_PATH_TAGS_READY_)
#define TETRIS_AI_RUNNER_SEARCH_TSPIN_PHASE2_

namespace tspin
{
    struct DefaultConfig
    {
        using HookType = m_tetris2::TSpinHook;
        // 字段语义与 search_tspin::Search::Config 保持一致。
        static constexpr bool allow_rotate_move = false;
        static constexpr bool allow_180 = true;
        static constexpr bool allow_d = true;
        static constexpr bool allow_D = true;
        static constexpr bool allow_LR = true;
        static constexpr bool is_20g = false;
        static constexpr bool last_rotate = false;
    };

    template<class Config = DefaultConfig>
    struct SearchWith
    {
        template<class RuleType>
        using type = m_tetris2::movegen::Searcher<
            m_tetris2::PathStrategy,
            typename path::detail::merge<DefaultConfig, Config>::HookType,
            typename RuleType::rule_spec,
            typename path::detail::merge<DefaultConfig, Config>::PolicyType>;

        template<class NewPolicy>
        struct PolicyOverrideConfig : Config { using PolicyType = NewPolicy; };

        template<class NewPolicy>
        using rebind_policy = SearchWith<PolicyOverrideConfig<NewPolicy>>;
    };

    using Search = SearchWith<>;
} // namespace tspin

#endif
