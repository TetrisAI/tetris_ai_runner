// search_aspin.h — 定义 A-spin 搜索使用的公开类型，并提供顶层 aspin::Search 装配入口。
// movegen_hook.h 先消费 search_aspin::Search 的类型定义，search_path.h 就绪后再补全顶层搜索标签。
#ifndef TETRIS_AI_RUNNER_SEARCH_ASPIN_PHASE1_
#define TETRIS_AI_RUNNER_SEARCH_ASPIN_PHASE1_

#include "tetris_core.h"
#include "movegen_state.h"

#include <cstddef>
#include <cstring>
#include <vector>

namespace search_aspin
{
    struct Search
    {
        enum ASpinType
        {
            None,
            ASpin
        };
        struct Config
        {
            bool allow_rotate_move = false;
            bool allow_180 = true;
            bool allow_d = true;
            bool allow_D = true;
            bool allow_LR = true;
            bool is_20g = false;
        };
        struct TetrisNodeWithASpinType
        {
            TetrisNodeWithASpinType()
            {
                std::memset(this, 0, sizeof(*this));
            }
            m_tetris2::bb::State state{};  // 落点位板坐标 (t/r/xb/yb)
            ASpinType type;

            bool operator==(TetrisNodeWithASpinType const &other) const
            {
                return state.t == other.state.t && state.r == other.state.r &&
                       state.xb == other.state.xb && state.yb == other.state.yb &&
                       type == other.type;
            }
            bool operator==(std::nullptr_t) const
            {
                return state.t == 0;
            }
            bool operator!=(std::nullptr_t) const
            {
                return state.t != 0;
            }
        };
    };
} // namespace search_aspin

#endif // TETRIS_AI_RUNNER_SEARCH_ASPIN_PHASE1_

// aspin::Search 依赖 search_path.h 的 PathStrategy 等定义，通过双阶段头机制在 search_path.h 包含完后展开。
#if !defined(TETRIS_AI_RUNNER_SEARCH_ASPIN_PHASE2_) && defined(TETRIS_AI_RUNNER_SEARCH_PATH_TAGS_READY_)
#define TETRIS_AI_RUNNER_SEARCH_ASPIN_PHASE2_

namespace aspin
{
    struct DefaultConfig
    {
        using HookType = m_tetris2::ASpinHook;
        //字段值与 src/search_aspin.h Config / oracle/search_aspin.h
        //  search_aspin_oracle::Search::Config 默认值同步 (运行时实际生效的
        //  仍是 SearchConfigHolder 里实例化的 ASpinHook::Config).
        static constexpr bool allow_rotate_move = false;
        static constexpr bool allow_180 = true;
        static constexpr bool allow_d = true;
        static constexpr bool allow_D = true;
        static constexpr bool allow_LR = true;
        static constexpr bool is_20g = false;
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
} // namespace aspin

#endif
