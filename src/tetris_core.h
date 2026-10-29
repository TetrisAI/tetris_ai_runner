
#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <cstring>
#include <deque>
#include <functional>
#include <iterator>
#include <map>
#include <queue>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>
#include <chrono>
#include <thread>
#include <condition_variable>

#include "tetris_active_policy.h"

#include "chash_map.h"
#include "chash_set.h"
#include "tetris_rule_spec.h"

namespace m_tetris2
{
    const int max_height = 40;
    // TetrisMap<W,H> 支持 W<=64；此常量同时作为 TetrisMap::top / AI 栈数组等的容量上限。
    const int max_width = 64;
    const int max_wall_kick = 16;

    //row_t / WallKickList / OpLines / OpDesc / RuleSpec / kOpRotateNone 由
    //tetris_rule_spec.h 提供; 这里不再重复定义.

    union TetrisBlockStatus;

    //方块状态
    //t:OISZLJT字符
    //[x,y]坐标,y越大高度越大
    //r:旋转状态(0-3)
    union TetrisBlockStatus
    {
        struct
        {
            char t;
            int8_t x, y;
            uint8_t r;
        };
        uint32_t status;
        TetrisBlockStatus() = default;
        TetrisBlockStatus(TetrisBlockStatus const &) = default;
        TetrisBlockStatus(char _t, int8_t _x, int8_t _y, uint8_t _r) : t(_t), x(_x), y(_y), r(_r)
        {
        }
    };

    struct TetrisBlockStatusHash
    {
        size_t operator()(TetrisBlockStatus const &block) const
        {
            return block.status;
        };
    };

    struct TetrisBlockStatusEqual
    {
        bool operator()(TetrisBlockStatus const &left, TetrisBlockStatus const &right) const
        {
            return left.status == right.status;
        };
    };

    struct TetrisBlockStatusCompare
    {
        bool operator()(TetrisBlockStatus const &left, TetrisBlockStatus const &right) const
        {
            return left.status < right.status;
        };
    };

    struct AIEnv
    {
        char const *next;
        size_t length;
        char node;
        char hold;
        bool is_hold;
    };

    // One step of an order-sensitive 64-bit hash (splitmix64 finalizer). Transposition keys are compared by this hash
    // alone, so it must mix well.
    constexpr std::uint64_t hash_combine(std::uint64_t seed, std::uint64_t value) noexcept
    {
        std::uint64_t x = seed ^ (value + 0x9E3779B97F4A7C15ull);
        x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ull;
        x = (x ^ (x >> 27)) * 0x94D049BB133111EBull;
        return x ^ (x >> 31);
    }


    template<class Type>
    struct TetrisCallInit
    {
        template<class CallType, class T>
        struct CallInit
        {
            template<class... Params>
            CallInit(CallType &type, Params const &...params)
            {
            }
        };
        template<class CallType>
        struct CallInit<CallType, std::true_type>
        {
            template<class... Params>
            CallInit(CallType &type, Params const &...params)
            {
                type.init(params...);
            }
        };
        struct Fallback
        {
            int init;
        };
        struct Derived : Type, Fallback
        {
        };
        template<typename U, U>
        struct Check;
        template<typename U>
        static std::false_type func(Check<int Fallback::*, &U::init> *);
        template<typename U>
        static std::true_type func(...);

    public:
        template<class... Params>
        TetrisCallInit(Type &type, Params const &...params)
        {
            CallInit<Type, decltype(func<Derived>(nullptr))>(type, params...);
        }
    };

} // namespace m_tetris2

// tetris_eval_dispatch.h 必须在所有 namespace 块之外 include：它间接拉入
// search_tspin.h / search_aspin.h / tetris_node.h，这些头文件都开了自己的
// namespace 块并以完全限定名引用 m_tetris2:: 类型。若将 #include 放在
// namespace m_tetris2 {} 内部，内层 namespace 会变成 m_tetris2::search_tspin
// 等，导致编译错误。
//
// tetris_eval_dispatch.h 自身将所有声明包在 namespace m_tetris2 {} 里，
// 因此分发类型最终落在正确的 namespace 中。
#include "tetris_eval_dispatch.h"
#include "tetris_eval_get.h"
#include "movegen_state.h"

namespace m_tetris2
{

    //Stage 3:Rule 必须自带 rule_spec(RuleSpec<...> 别名),通过编译期常量做尺寸校验
    template<class Rule>
    struct TetrisRuleInit
    {
        static bool init(int w, int h)
        {
            return w == int(Rule::rule_spec::width) && h == int(Rule::rule_spec::height);
        }
    };

    template<class TetrisAI>
    struct TetrisAIHasRatio
    {
        struct Fallback
        {
            int ratio;
        };
        struct Derived : TetrisAI, Fallback
        {
        };
        template<typename U, U>
        struct Check;
        template<typename U>
        static std::false_type func(Check<int Fallback::*, &U::ratio> *);
        template<typename U>
        static std::true_type func(...);

    public:
        typedef decltype(func<Derived>(nullptr)) type;
    };

    template<class TetrisAI>
    struct TetrisAIHasIterate
    {
        struct Fallback
        {
            int iterate;
        };
        struct Derived : TetrisAI, Fallback
        {
        };
        template<typename U, U>
        struct Check;
        template<typename U>
        static std::false_type func(Check<int Fallback::*, &U::iterate> *);
        template<typename U>
        static std::true_type func(...);

    public:
        typedef decltype(func<Derived>(nullptr)) type;
    };

    template<class Type>
    struct TetrisHasConfig
    {
        struct Fallback
        {
            int Config;
        };
        struct Derived : Type, Fallback
        {
        };
        template<typename U, U>
        struct Check;
        template<typename U>
        static std::false_type func(Check<int Fallback::*, &U::Config> *);
        template<typename U>
        static std::true_type func(...);

    public:
        typedef decltype(func<Derived>(nullptr)) type;
    };

    template<class TreeContext, class TetrisRule, class TetrisAI, class TetrisSearch>
    struct LocalContextBuilder
    {
    private:
        template<class CallAI, class>
        struct AIConfig
        {
            class AIConfigHolder
            {
            public:
                typedef void AIConfigType;
                void const *ai_config() const
                {
                    return nullptr;
                }
                void set_ai_config(auto const &)
                {
                }
            };
        };
        template<class CallAI>
        struct AIConfig<CallAI, std::true_type>
        {
            class AIConfigHolder
            {
            public:
                typedef typename CallAI::Config AIConfigType;
                AIConfigType const *ai_config() const
                {
                    return &ai_config_;
                }
                void set_ai_config(AIConfigType const &c)
                {
                    ai_config_ = c;
                }

            private:
                AIConfigType ai_config_;
            };
        };
        template<class CallSearch, class>
        struct SearchConfig
        {
            class SearchConfigHolder
            {
            public:
                typedef void SearchConfigType;
                void const *search_config() const
                {
                    return nullptr;
                }
                void *search_config()
                {
                    return nullptr;
                }
            };
        };
        template<class CallSearch>
        struct SearchConfig<CallSearch, std::true_type>
        {
            class SearchConfigHolder
            {
            public:
                typedef typename CallSearch::Config SearchConfigType;
                SearchConfigType const *search_config() const
                {
                    return &status_config_;
                }
                SearchConfigType *search_config()
                {
                    return &status_config_;
                }

            private:
                SearchConfigType status_config_;
            };
        };

    public:
        template<class TreeNode>
        class LocalContext : public TreeContext, public AIConfig<TetrisAI, typename TetrisHasConfig<TetrisAI>::type>::AIConfigHolder, public SearchConfig<TetrisSearch, typename TetrisHasConfig<TetrisSearch>::type>::SearchConfigHolder
        {
        public:
            LocalContext(std::deque<TreeNode> *node_storage) : TreeContext(node_storage)
            {
            }
        };

    private:
        template<class TreeNode, class>
        struct CallInit
        {
            using SearchSpec = typename SearchRuleSpecOf<TetrisSearch>::type;

            static void call(TetrisAI &ai, LocalContext<TreeNode> const *local_context)
            {
                ai.template init<SearchSpec>(local_context->ai_config());
            }
            static void call(TetrisSearch &search, LocalContext<TreeNode> const *local_context)
            {
                TetrisCallInit<TetrisSearch>(search, local_context->search_config());
            }
        };
        template<class TreeNode>
        struct CallInit<TreeNode, void>
        {
            using SearchSpec = typename SearchRuleSpecOf<TetrisSearch>::type;

            static void call(TetrisAI &ai, LocalContext<TreeNode> const *local_context)
            {
                ai.template init<SearchSpec>();
            }
            static void call(TetrisSearch &search, LocalContext<TreeNode> const *local_context)
            {
                TetrisCallInit<TetrisSearch>{search};
            }
        };

    public:
        template<class TreeNode>
        static void init_ai(TetrisAI &ai, LocalContext<TreeNode> const *local_context)
        {
            CallInit<TreeNode, typename LocalContext<TreeNode>::AIConfigType>::call(ai, local_context);
        }
        template<class TreeNode>
        static void init_search(TetrisSearch &search, LocalContext<TreeNode> const *local_context)
        {
            CallInit<TreeNode, typename LocalContext<TreeNode>::SearchConfigType>::call(search, local_context);
        }
    };

    template<class TetrisAI, class TetrisSearch>
    struct TetrisCore
    {
    public:
        typedef typename TetrisSearch::LandPoint LandPoint;
        using Result = typename TetrisAI::Result;
        using Status = typename TetrisAI::Status;

    private:
        template<class TreeNode, class>
        struct TetrisGetRatio
        {
            static double get_ratio(TetrisAI &ai)
            {
                return ai.ratio();
            }
        };
        template<class TreeNode>
        struct TetrisGetRatio<TreeNode, std::false_type>
        {
            static double get_ratio(TetrisAI &ai)
            {
                return 0;
            }
        };
        template<class TreeNode, class>
        struct TetrisSelectIterate
        {
            static void iterate(TetrisAI &ai, Status const **status, size_t status_length, TreeNode *tree_node)
            {
                tree_node->status.set_vp(ai.iterate(status, status_length));
            }
        };
        template<class TreeNode>
        struct TetrisSelectIterate<TreeNode, std::false_type>
        {
            static void iterate(TetrisAI &ai, Status const **status, size_t status_length, TreeNode *tree_node)
            {
            }
        };
        // GetArgsInvoker：按 AI::GetArgs(type_list) 萃取每个标签，按需刷新 eval，调用 ai.get。
        // GetArgsInvoker：每个唯一 provider 计算一次（在去重后的 type_list 上按序展开 braced-init，
        // C++20 保证左→右求值），命中即 +1 ProviderCounters；重复标签在原声明位置复用同一值。
        // NextLength/NextNode/Hold/IsHold 为 O(1)，不构 fullEnv；仅 NextView/Env 用 next_c/parent->env。
        template<class TreeNode, bool EnableEnv>
        struct GetArgsInvoker
        {
            using Context = typename TreeNode::Context;
            Context *ctx;
            TreeNode *node;
            TreeNode *parent;

            template<class Tag> void bump(Tag) const
            {
                if (!ctx->provider_counters.enabled) return;
                if constexpr (std::is_same_v<Tag, args::Depth>) ++ctx->provider_counters.depth;
                else if constexpr (std::is_same_v<Tag, args::Clear>) ++ctx->provider_counters.clear;
                else if constexpr (std::is_same_v<Tag, args::MapBefore>) ++ctx->provider_counters.map_before;
                else if constexpr (std::is_same_v<Tag, args::MapAfter>) ++ctx->provider_counters.map_after;
                else if constexpr (std::is_same_v<Tag, args::NextView>) ++ctx->provider_counters.next_view;
                else if constexpr (std::is_same_v<Tag, args::NextLength>) ++ctx->provider_counters.next_length;
                else if constexpr (std::is_same_v<Tag, args::NextNode>) ++ctx->provider_counters.next_node;
                else if constexpr (std::is_same_v<Tag, args::Hold>) ++ctx->provider_counters.hold;
                else if constexpr (std::is_same_v<Tag, args::IsHold>) ++ctx->provider_counters.is_hold;
                else if constexpr (std::is_same_v<Tag, args::Env>) ++ctx->provider_counters.env;
                else if constexpr (detail::is_parent_tag<Tag>::value) ++ctx->provider_counters.parent;
                else if constexpr (detail::is_landing_tag<Tag>::value) ++ctx->provider_counters.landing;
                else if constexpr (detail::is_eval_tag<Tag>::value) ++ctx->provider_counters.eval;
            }

            // 与原 TetrisTreeNode::env() 完全一致：queue_begin 以 parent(this) 的 next/hold 为准，
            // child->is_hold && parent.hold==' ' 时 next 前进一位（hold swap 消耗队列头）。
            // 结果缓存一次，NextView/NextLength/Env 共享，不为 C2 构 Env。
            using NextIt = typename std::vector<typename TreeNode::next_t>::const_iterator;
            mutable bool queue_ready_ = false;
            mutable NextIt queue_begin_{};
            mutable std::size_t queue_len_ = 0;
            void ensure_queue() const
            {
                if (queue_ready_) return;
                auto base = parent->next;
                queue_begin_ = (node->is_hold && parent->hold == ' ') ? std::next(base) : base;
                queue_len_ = static_cast<std::size_t>(std::distance(queue_begin_, ctx->next.cend()));
                queue_ready_ = true;
            }
            std::size_t queue_length() const
            {
                ensure_queue();
                return queue_len_;
            }

            template<class E>
            E const &operator()(args::Eval<E>) const
            {
                static_assert(std::is_same_v<E, Result>,
                              "args::Eval<E> 的 E 必须等于引擎 Result 类型");
                if (node->eval_slot.eval_gen != ctx->eval_cache.generation() ||
                    node->eval_slot.eval_era != ctx->eval_era)
                {
                    node->eval_slot.result = ctx->eval_cache.lookup(*ctx->ai, node->map);
                    node->eval_slot.eval_gen = ctx->eval_cache.generation();
                    node->eval_slot.eval_era = ctx->eval_era;
                }
                return node->eval_slot.result;
            }
            template<class S>
            S const &operator()(args::Parent<S>) const
            {
                return parent->status.get_raw();
            }
            std::size_t operator()(args::Depth) const { return parent->level; }
            int operator()(args::Clear) const { return node->clear; }
            typename TreeNode::map_t const &operator()(args::MapBefore) const { return parent->map; }
            typename TreeNode::map_t const &operator()(args::MapAfter) const { return node->map; }
            template<class L>
            L const &operator()(args::Landing<L>) const { return node->landing; }
            char const *operator()(args::NextView) const
            {
                if constexpr (!EnableEnv) return nullptr;   // 虚拟 run：旧空 env 语义
                else
                {
                    ensure_queue();
                    if (queue_len_ == 0) return nullptr;
                    return ctx->next_c.data() + (ctx->next_c.size() - queue_len_);
                }
            }
            std::size_t operator()(args::NextLength) const
            {
                if constexpr (!EnableEnv) return 0;          // 旧空 env length=0
                else return queue_length();
            }
            char operator()(args::NextNode) const
            {
                if constexpr (!EnableEnv) return ' ';        // 旧空 env node=' '
                else return node->identity.state.t;
            }
            char operator()(args::Hold) const
            {
                if constexpr (!EnableEnv) return ' ';       // 旧空 env hold=' '
                else return node->is_hold ? parent->node : parent->hold;
            }
            bool operator()(args::IsHold) const
            {
                if constexpr (!EnableEnv) return false;     // 旧空 env is_hold=false
                else return node->is_hold;
            }
            AIEnv operator()(args::Env) const
            {
                if constexpr (!EnableEnv) return AIEnv{nullptr, 0, ' ', ' ', false};  // 旧 run_virtual 空 env
                else return parent->template env<true>(ctx, node);
            }
        };

        template<class TreeNode, bool EnableEnv, class... Us>
        static auto make_storage(GetArgsInvoker<TreeNode, EnableEnv> &p, type_list<Us...>)
        {
            if (p.ctx->provider_counters.enabled)
            {
                if constexpr (EnableEnv) ++p.ctx->provider_counters.real_get;
                else ++p.ctx->provider_counters.virtual_get;
            }
            (p.bump(Us{}), ...); // 每个唯一标签计数一次（fold 左→右）
            return std::tuple<decltype(p(Us{}))...>{ p(Us{})... };
        }
        template<class TreeNode, bool EnableEnv, class... Ps>
        static Status invoke_get(GetArgsInvoker<TreeNode, EnableEnv> &p, type_list<Ps...>)
        {
            using Args = type_list<Ps...>;
            using Unique = unique_types_t<Args>;
            // 固定顺序：braced-init 左→右，每个唯一 provider 计算一次 + 计数一次。
            auto storage = make_storage<TreeNode, EnableEnv>(p, Unique{});
            // 原声明位置展开：从 storage 取该标签对应唯一槽位（重复标签复用同一值）。
            auto got = (*p.ctx->ai).get(std::get<index_of_v<Ps, Unique>>(storage)...);
            static_assert(std::is_same_v<decltype(got), Status const &> || std::is_same_v<decltype(got), Status>,
                          "AI::get 必须返回 Status（const& 或值）");
            return got;
        }

    public:
        template<class TreeNode>
        using EnableNextC = std::bool_constant<GetTraits<TetrisAI>::kWantEnv || GetTraits<TetrisAI>::kWantNextView>;

        template<class TreeNode>
        static double get_ratio(TetrisAI &ai)
        {
            return TetrisGetRatio<TreeNode, typename TetrisAIHasRatio<TetrisAI>::type>::get_ratio(ai);
        }
        template<bool EnableEnv, class TreeNode>
        static void get(typename TreeNode::Context *context, TreeNode *node, TreeNode *parent)
        {
            GetArgsInvoker<TreeNode, EnableEnv> p{context, node, parent};
            node->status.set(invoke_get<TreeNode, EnableEnv>(p, typename TetrisAI::GetArgs{}));
        }
        template<class TreeNode>
        static void iterate(TetrisAI &ai, Status const **status, size_t status_length, TreeNode *tree_node)
        {
            TetrisSelectIterate<TreeNode, typename TetrisAIHasIterate<TetrisAI>::type>::iterate(ai, status, status_length, tree_node);
        }
    };

    template<class Status, class TetrisAI, class TetrisSearch>
    struct TetrisTreeNode
    {
        typedef TetrisCore<TetrisAI, TetrisSearch> Core;
        using LandPoint = typename Core::LandPoint;
        using _SearchSpec = typename SearchRuleSpecOf<TetrisSearch>::type;
        using map_t = m_tetris2::TetrisMap<_SearchSpec::width, _SearchSpec::height>;
        struct Context
        {
        public:
            struct ValueHeapCompare
            {
                bool operator()(TetrisTreeNode *left, TetrisTreeNode *right) const
                {
                    return left->status.get() < right->status.get();
                }
            };
            template<class, class>
            struct TetrisNext
            {
                TetrisNext(char _node) : node(_node), vp()
                {
                }
                void set_vp(bool _vp)
                {
                    vp = _vp;
                }
                bool get_vp()
                {
                    return vp;
                }
                bool operator==(TetrisNext const &other) const
                {
                    return node == other.node && vp == other.vp;
                }
                operator char() const
                {
                    return node;
                }
                char node;
                bool vp;
            };
            template<class Unuse>
            struct TetrisNext<Unuse, std::false_type>
            {
                TetrisNext(char _node) : node(_node)
                {
                }
                void set_vp(bool _vp)
                {
                }
                bool get_vp()
                {
                    return false;
                }
                bool operator==(TetrisNext const &other) const
                {
                    return node == other.node;
                }
                operator char() const
                {
                    return node;
                }
                char node;
            };
            typedef TetrisNext<TetrisAI, typename TetrisAIHasIterate<TetrisAI>::type> next_t;

        public:
            Context(std::deque<TetrisTreeNode> *_node_storage) : version(), is_complete(), is_open_hold(), max_length(), node_storage(_node_storage), free_list(nullptr), free_count(0), current_t('\0'), width(), total(), avg()
            {
            }

        public:
            typedef std::priority_queue<TetrisTreeNode *, std::vector<TetrisTreeNode *>, ValueHeapCompare> value_heap_t;
            typedef chash_map<TetrisBlockStatus, TetrisTreeNode *, TetrisBlockStatusHash, TetrisBlockStatusEqual> children_map_t;
            typedef chash_set<TetrisBlockStatus, TetrisBlockStatusHash, TetrisBlockStatusEqual> identity_set_t;
            size_t version;
            TetrisAI *ai;
            TetrisSearch *search;
            std::vector<value_heap_t> sort;
            std::vector<value_heap_t> wait;
            children_map_t old;
            identity_set_t uniq;
            bool is_complete;
            bool is_open_hold;
            bool is_virtual;
            bool unused_bool;
            size_t max_length;
            size_t width;
            size_t scored = 0;
            std::deque<TetrisTreeNode> *node_storage;
            TetrisTreeNode *free_list;
            size_t free_count;
            // A discarded subtree root, whose sibling link may still lead into the live tree, or with `siblings` a
            // whole children list.
            struct Discarded
            {
                TetrisTreeNode *node;
                bool siblings;
            };
            std::vector<Discarded> discarded;
            std::vector<Status const *> iterate_cache;
            char current_t;
            m_tetris2::State root_start{};
            std::vector<next_t> next;
            std::vector<char> next_c;
            std::vector<double> width_cache;
            double total;
            double avg;
            EvalCache<map_t, typename Core::Result> eval_cache;
        std::uint64_t eval_era = 0; // 缓存溢出时 era++，全树 eval holder 失效
            ProviderCounters provider_counters;

        public:
            TetrisTreeNode *alloc(TetrisTreeNode *parent)
            {
                TetrisTreeNode *node;
                if (free_list == nullptr)
                {
                    reclaim(1);
                }
                if (free_list != nullptr)
                {
                    node = free_list;
                    free_list = free_list->parent;
                    node->version = version - 1;
                    --free_count;
                }
                else
                {
                    node_storage->emplace_back();
                    node = &node_storage->back();
                }
                node->parent = parent;
                return node;
            }
            // O(1): freeing a whole subtree at once costs ~0.1 us per node, tens of ms for a widened tree, so its
            // nodes move to the free list one by one, as alloc needs them or through reclaim.
            void dealloc(TetrisTreeNode *node)
            {
                discarded.push_back({node, false});
            }
            // Moves up to `count` discarded nodes to the free list, touching each once; true while some remain.
            bool reclaim(size_t count)
            {
                for (; count > 0 && !discarded.empty(); --count)
                {
                    auto const [node, siblings] = discarded.back();
                    discarded.pop_back();
                    if (siblings && node->children_next != nullptr)
                    {
                        discarded.push_back({node->children_next, true});
                    }
                    if (node->children != nullptr)
                    {
                        discarded.push_back({node->children, true});
                    }
                    node->children = nullptr;
                    node->node_flag.clear();
                    node->flag = 0;
                    node->node = ' ';
                    node->hold = ' ';
                    node->level = 1;
                    node->parent = free_list;
                    free_list = node;
                    ++free_count;
                }
                return !discarded.empty();
            }
        };
        struct TetrisNodeFlag
        {
            static constexpr char kVirtualSearchFlag = '~';

            enum class Kind : std::uint8_t
            {
                Empty,
                Virtual,
                SingleStart,
                StartPair,
            };

            Kind kind = Kind::Empty;
            m_tetris2::State start[2]{};

            bool empty() const
            {
                return kind == Kind::Empty;
            }
            bool check(char virtual_flag) const
            {
                assert(virtual_flag == '~');
                return kind == Kind::Virtual;
            }
            bool check(m_tetris2::State const &start1) const
            {
                return kind == Kind::SingleStart && m_tetris2::StateEqual{}(start[0], start1);
            }
            bool check(m_tetris2::State const &start1, m_tetris2::State const &start2) const
            {
                return kind == Kind::StartPair && m_tetris2::StateEqual{}(start[0], start1) &&
                       m_tetris2::StateEqual{}(start[1], start2);
            }
            void set(char virtual_flag)
            {
                assert(virtual_flag == '~');
                kind = Kind::Virtual;
            }
            void set(m_tetris2::State const &start1)
            {
                assert(start1.t != 0);
                start[0] = start1;
                kind = Kind::SingleStart;
            }
            void set(m_tetris2::State const &start1, m_tetris2::State const &start2)
            {
                assert(start1.t != 0);
                assert(start2.t != 0);
                start[0] = start1;
                start[1] = start2;
                kind = Kind::StartPair;
            }
            void clear()
            {
                kind = Kind::Empty;
            }
        };
        template<class, class>
        struct TreeNodeStatus
        {
            Status status_raw;
            Status status;
            Status const &get() const
            {
                return status;
            }
            Status const &get_raw() const
            {
                return status_raw;
            }
            void set(Status const &_status)
            {
                status = _status;
                status_raw = _status;
            }
            void set_vp(Status const &_status)
            {
                status = _status;
            }
        };
        template<class Unuse>
        struct TreeNodeStatus<Unuse, std::false_type>
        {
            Status status;
            Status const &get() const
            {
                return status;
            }
            Status const &get_raw() const
            {
                return status;
            }
            void set(Status const &_status)
            {
                status = _status;
            }
            void set_vp(Status const &_status)
            {
                status = _status;
            }
        };
        typedef typename Context::next_t next_t;
        TetrisTreeNode() : node(' '), hold(' '), level(1), flag(), version(-1), identity(), parent(), children()
        {
        }
        union
        {
            struct
            {
                uint8_t flag;
                char node;
                char hold;
                uint8_t level;
            };
            struct
            {
                uint8_t is_dead : 1;
                uint8_t is_hold : 1;
                uint8_t is_hold_lock : 1;
                uint8_t is_virtual : 1;
            };
        };
        size_t version;
        map_t map;
        typename Core::LandPoint identity;
        [[no_unique_address]] std::conditional_t<GetTraits<TetrisAI>::kWantEval,
            EvalSlot<typename Core::Result>, std::monostate> eval_slot;
        [[no_unique_address]] std::conditional_t<GetTraits<TetrisAI>::kWantClear, int, std::monostate> clear;
        [[no_unique_address]] std::conditional_t<GetTraits<TetrisAI>::kWantLanding,
            GetLanding<typename LandingSpinOf<TetrisAI>::type>, std::monostate> landing;
        TreeNodeStatus<TetrisAI, typename TetrisAIHasIterate<TetrisAI>::type> status;
        TetrisTreeNode *parent;
        TetrisTreeNode *children;
        TetrisTreeNode *children_next;
        TetrisNodeFlag node_flag;
        typename std::vector<next_t>::const_iterator next;

        // lp_key: 提取 children_map_t / identity_set_t 的 TetrisBlockStatus key。
        //   LandPoint (TetrisLandPoint / TetrisNodeWith*SpinType): 用 {t, xb, yb, r}
        //   构造伪 TetrisBlockStatus。yb 为底行（非真实 status.y 顶行），但在
        //   同一 LandPoint 类型的同一个 map 内，唯一性与真实 status 等价。
        //   amini emits the same state once per spin class, so the class goes into r's spare high bits
        //   (rotation counts are < 32).
        template<class LP>
        static TetrisBlockStatus lp_key(LP const &lp) noexcept
        {
            std::uint8_t r = lp.state.r;
            if constexpr (requires { lp.type; })
                r = static_cast<std::uint8_t>(r | (static_cast<std::uint8_t>(lp.type) << 5));
            return TetrisBlockStatus{static_cast<char>(lp.state.t),
                                     static_cast<int8_t>(lp.state.xb),
                                     static_cast<int8_t>(lp.state.yb),
                                     r};
        }
        // lp_type: 提取 piece-type char（用于 convert / is_hold 比较）。
        template<class LP>
        static char lp_type(LP const &lp) noexcept
        {
            return static_cast<char>(lp.state.t);
        }
        // lp_is_valid: 检测 identity 是否有效（非空 / 非哨兵）。
        template<class LP>
        static bool lp_is_valid(LP const &lp) noexcept
        {
            return lp.state.t != 0;
        }

        // root 的 Status 由 host 直写、永不经 args::Eval 取 eval。回收/换盘会把带 foreign-map eval 副本的
        // 旧节点提升为 root；早返路径 slot 纪元也可能过期。此时把整个 eval holder 重置为未求值默认
        // （result=R{} + eval_gen=0 + eval_era=0），使旧 map 结果不得被当作 valid 读取；绝不为清洁 slot
        // 强行 eval 空/原始盘面（会触发部分 AI 空盘 UB，且多算一次 eval 破坏 provider==cache 计数）。
        // 真正需要该副本的 consumer（GetArgs args::Eval）在 operator() 内按完整 map+当前 Config resolve。
        // cached map 与 epoch 仍有效时保留旧副本。
        static void invalidate_root_eval_if_stale(Context *context, TetrisTreeNode *root)
        {
            if constexpr (GetTraits<TetrisAI>::kWantEval)
            {
                if (root->eval_slot.eval_gen != context->eval_cache.generation() ||
                    root->eval_slot.eval_era != context->eval_era)
                {
                    root->eval_slot = {}; // 未求值 holder；consumer 按需 resolve
                }
            }
        }
        TetrisTreeNode *update_root(Context *context, map_t const &_map)
        {
            if (map == _map)
            {
                invalidate_root_eval_if_stale(context, this);
                return this;
            }
            TetrisTreeNode *new_root = nullptr;
            for (TetrisTreeNode *it = children, *last = nullptr; it != nullptr; last = it, it = it->children_next)
            {
                if (it->map == _map)
                {
                    new_root = it;
                    (last == nullptr ? children : last->children_next) = it->children_next;
                    break;
                }
            }
            if (new_root == nullptr)
            {
                new_root = context->alloc(nullptr);
                new_root->map = _map;
                // 回收节点带 foreign-map 副本：重置为未求值 holder，不 eval 原始盘面。
                if constexpr (GetTraits<TetrisAI>::kWantEval)
                    new_root->eval_slot = {};
            }
            else
            {
                new_root->parent = nullptr;
                invalidate_root_eval_if_stale(context, new_root);
            }
            context->dealloc(this);
            return new_root;
        }
        // Keeps only the child placed at `target`; false, tree unchanged, if there is none. The beam must restart
        // afterwards: its queues may still hold the dropped nodes.
        bool keep_only(Context *context, LandPoint const &target)
        {
            auto const key = lp_key(target).status;
            TetrisTreeNode *kept = children;
            while (kept != nullptr && lp_key(kept->identity).status != key)
            {
                kept = kept->children_next;
            }
            if (kept == nullptr)
            {
                return false;
            }
            for (TetrisTreeNode *it = children, *sibling; it != nullptr; it = sibling)
            {
                sibling = it->children_next;
                if (it != kept)
                {
                    context->dealloc(it);
                }
            }
            kept->children_next = nullptr;
            children = kept;
            return true;
        }
        void reset_frontier(Context *context)
        {
            context->total += context->width;
            context->avg = context->total / context->version;
            context->width = 0;
            context->scored = 0;
            context->wait.clear();
            context->sort.clear();
            context->wait.resize(context->max_length + 1);
            context->sort.resize(context->max_length + 1);
        }
        void update_version(Context *context)
        {
            ++context->version;
            reset_frontier(context);
        }
        typename Context::identity_set_t direct_child_identities() const
        {
            typename Context::identity_set_t result;
            for (auto child = children; child != nullptr; child = child->children_next)
            {
                result.insert(lp_key(child->identity));
            }
            return result;
        }
        bool direct_children_equal(typename Context::identity_set_t const &before) const
        {
            size_t count = 0;
            for (auto child = children; child != nullptr; child = child->children_next)
            {
                ++count;
                if (before.find(lp_key(child->identity)) == before.end())
                {
                    return false;
                }
            }
            return count == before.size();
        }
        void score_new_root_child(Context *context, TetrisTreeNode *child, bool reused)
        {
            if (parent == nullptr && !reused)
            {
                Core::template get<true>(context, child, this);
                ++context->scored;
            }
        }
        // A root rescan may unlink subtrees still referenced by the beam heaps. Clear those heaps before dealloc
        // marks any removed root child as discarded; priority_queue cannot erase arbitrary stale pointers.
        void reset_root_frontier_if_children_changed(Context *context,
                                                     typename Context::identity_set_t const &before,
                                                     bool &already_reset)
        {
            if (parent == nullptr && !already_reset && !direct_children_equal(before))
            {
                context->is_complete = false;
                reset_frontier(context);
                already_reset = true;
            }
        }
        static std::vector<next_t> process_next(char const *_next, size_t _next_length, char _t)
        {
            std::vector<next_t> next;
            next.push_back(_t);
            size_t length = 0;
            for (size_t i = 0; i < _next_length; ++i)
            {
                if (_next[i] == '?')
                {
                    next[length].set_vp(true);
                }
                else
                {
                    ++length;
                    next.push_back(_next[i]);
                }
            }
            return next;
        }
        static m_tetris2::State spawn_state(char t)
        {
            using SearchSpec = typename SearchRuleSpecOf<TetrisSearch>::type;
            auto sp = SearchSpec::spawn(t, SearchSpec::width, SearchSpec::height);
            return m_tetris2::Helpers<SearchSpec>::state_from_status(t, 0, sp.first, sp.second);
        }
        // Only the root's active piece starts from the host's position; held and preview pieces enter at spawn.
        m_tetris2::State start_state(Context const *context, char t) const
        {
            return parent == nullptr && t == context->current_t ? context->root_start : spawn_state(t);
        }
        // Align with master: a new start publishes the host position to context->root_start;
        // children stay so the search tree keeps accumulating depth, and update() rotates version instead of dropping children.
        bool restart(Context *context, bool searched_as_root, TetrisBlockStatus const &active)
        {
            using SearchSpec = typename SearchRuleSpecOf<TetrisSearch>::type;
            auto start = m_tetris2::Helpers<SearchSpec>::state_from_status(active.t, static_cast<std::uint8_t>(active.r), active.x, active.y);
            bool changed = !m_tetris2::StateEqual{}(start, searched_as_root ? context->root_start : spawn_state(active.t));
            context->root_start = start;
            return changed;
        }
        TetrisTreeNode *update(Context *context, map_t const &_map, Status const &status, TetrisBlockStatus const &active, char const *_next, size_t _next_length)
        {
            char _t = active.t;
            TetrisTreeNode *root = update_root(context, _map);
            bool restarted = root->restart(context, root == this, active);
            std::vector<next_t> next = process_next(_next, _next_length, _t);
            if (root != this || (context->current_t == '\0' || _t != root->node) || context->is_open_hold || next != context->next)
            {
                context->is_complete = false;
                context->max_length = next.size() - 1;
                update_version(context);
                context->is_open_hold = false;
                context->current_t = _t;
                context->next = next;
                if (Core::template EnableNextC<TetrisTreeNode>::value)
                {
                    context->next_c.assign(next.begin(), next.end());
                }
                root->node = _t;
                root->next = std::next(context->next.begin());
            }
            else if (context->current_t != _t || restarted)
            {
                context->is_complete = false;
                context->current_t = _t;
                root->status.set(status);
                root->template search_children<false>(context);
            }
            root->status.set(status);
            context->width_cache.clear();
            return root;
        }
        TetrisTreeNode *update(Context *context, map_t const &_map, Status const &status, TetrisBlockStatus const &active, char _hold, bool _hold_lock, char const *_next, size_t _next_length)
        {
            char _t = active.t;
            TetrisTreeNode *root = update_root(context, _map);
            bool restarted = root->restart(context, root == this, active);
            std::vector<next_t> next = process_next(_next, _next_length, _t);
            if (root != this || (context->current_t == '\0' || _t != root->node) || !context->is_open_hold || next != context->next || _hold != root->hold || !!_hold_lock != root->is_hold_lock)
            {
                context->is_complete = false;
                context->max_length = next.size() - 1;
                if (_hold != ' ' && (_next_length > 1 || !_hold_lock))
                {
                    ++context->max_length;
                }
                update_version(context);
                context->is_open_hold = true;
                context->current_t = _t;
                context->next = next;
                if (Core::template EnableNextC<TetrisTreeNode>::value)
                {
                    context->next_c.assign(next.begin(), next.end());
                }
                root->node = _t;
                root->hold = _hold;
                root->is_hold = false;
                root->is_hold_lock = _hold_lock;
                root->next = std::next(context->next.begin());
            }
            else if (context->current_t != _t || restarted)
            {
                context->is_complete = false;
                context->current_t = _t;
                root->status.set(status);
                root->template search_children<true>(context);
            }
            root->status.set(status);
            context->width_cache.clear();
            return root;
        }
        // alloc_eval_typed<T,R>: 分配子节点并直接以编译期 <T,R> 调用 eval。
        template<char T, std::uint8_t R, class SrcBoard>
        TetrisTreeNode *alloc_eval_typed(Context *context, map_t &map,
                                         SrcBoard const &src_board,
                                         LandPoint const &lp)
        {
            using Spec = typename SearchRuleSpecOf<TetrisSearch>::type;
            TetrisTreeNode *child = context->alloc(this);
            child->map = map;
            child->identity = lp;
            // 纯位板 attach：直接从 State 写方块、消行。clear 仅在 AI 请求时落节点。
            size_t clear = m_tetris2::Helpers<Spec>::attach_to_map(lp.state, child->map);
            // eval(map) 经直接映射缓存：仅当 AI 请求 Eval；hash 定位 + 全 raw map 相等，miss 当场 eval。
            // enemy/farter 无 Eval，此处 0 次 eval。
            if constexpr (GetTraits<TetrisAI>::kWantEval)
            {
                child->eval_slot.result = context->eval_cache.lookup(*context->ai, child->map);
                child->eval_slot.eval_gen = context->eval_cache.generation();
                child->eval_slot.eval_era = context->eval_era;
            }
            if constexpr (GetTraits<TetrisAI>::kWantClear)
            {
                child->clear = static_cast<int>(clear);
            }
            if constexpr (GetTraits<TetrisAI>::kWantLanding)
            {
                using Base = TetrisNodeBase<TetrisNodeDetails<Spec, T, R>>;
                auto &g = child->landing;
                g.piece = static_cast<char>(lp.state.t);
                g.rotation = static_cast<std::uint8_t>(lp.state.r);
                g.xb = static_cast<std::int8_t>(lp.state.xb);
                g.yb = static_cast<std::int8_t>(lp.state.yb);
                g.w = static_cast<std::int8_t>(Base::kWidth);
                g.h = static_cast<std::int8_t>(Base::kHeight);
                g.y_master_plus1 = static_cast<int>(lp.state.yb) + Base::kOriginY + 1;
                for (int i = 0; i < Base::kHeight && i < GetLanding<typename LandingSpinOf<TetrisAI>::type>::kMaxLines; ++i)
                {
                    g.lines[i] = Base::kLines[i];
                }
                // 仅在 spin 字段可真接收 lp.type 时赋值；GetLanding<monostate> 时 g.spin 为
                // monostate，赋值 ill-formed，须跳过以保留旧行为（不强制 spin 类型转换）。
                if constexpr (requires { g.spin = lp.type; })
                {
                    g.spin = lp.type;
                }
            }
            return child;
        }

        void search(Context *context, char search_t, bool is_hold)
        {
            auto const root_children_before = parent == nullptr ? direct_child_identities() : typename Context::identity_set_t{};
            bool root_frontier_reset = false;
            using SearchSpec = typename SearchRuleSpecOf<TetrisSearch>::type;
            auto board = m_tetris2::build_board_for_search<SearchSpec>(map);
            auto spawn = start_state(context, search_t);
            if (node_flag.empty())
            {
                node_flag.set(spawn);
                auto fresh_cb = [&]<char T, std::uint8_t R>(LandPoint const &land_point_node)
                {
                    TetrisTreeNode *child = this->template alloc_eval_typed<T, R>(context, map, board, land_point_node);
                    child->is_hold = is_hold;
                    child->children_next = children;
                    children = child;
                };
                context->search->search_eval(board, spawn, level, fresh_cb);
            }
            else if (!node_flag.check(spawn))
            {
                node_flag.set(spawn);
                auto &old = context->old;
                for (auto it = children; it != nullptr; it = it->children_next)
                {
                    old.emplace(lp_key(it->identity), it);
                }
                children = nullptr;
                auto rescan_cb = [&]<char T, std::uint8_t R>(LandPoint const &land_point_node)
                {
                    TetrisTreeNode *child;
                    auto find = old.find(lp_key(land_point_node));
                    bool const reused = find != old.end();
                    if (reused)
                    {
                        child = find->second;
                        old.erase(find);
                    }
                    else
                    {
                        child = this->template alloc_eval_typed<T, R>(context, map, board, land_point_node);
                    }
                    child->is_hold = is_hold;
                    score_new_root_child(context, child, reused);
                    child->children_next = children;
                    children = child;
                };
                context->search->search_eval(board, spawn, level, rescan_cb);
                reset_root_frontier_if_children_changed(context, root_children_before, root_frontier_reset);
                for (auto &pair : old)
                {
                    context->dealloc(pair.second);
                }
                old.clear();
            }
        }
        void search(Context *context, char search_t, char hold_t)
        {
            auto const root_children_before = parent == nullptr ? direct_child_identities() : typename Context::identity_set_t{};
            bool root_frontier_reset = false;
            using SearchSpec = typename SearchRuleSpecOf<TetrisSearch>::type;
            auto board = m_tetris2::build_board_for_search<SearchSpec>(map);
            auto search_spawn = start_state(context, search_t);
            auto hold_spawn = hold_t != '\0' ? start_state(context, hold_t) : m_tetris2::State{};
            if (m_tetris2::StateEqual{}(search_spawn, hold_spawn))
            {
                return search(context, search_t, false);
            }
            if (search_t == hold_t)
            {
                if (node_flag.empty())
                {
                    node_flag.set(search_spawn, hold_spawn);
                    auto &uniq = context->uniq;
                    auto fresh_main_cb = [&]<char T, std::uint8_t R>(LandPoint const &land_point_node)
                    {
                        TetrisTreeNode *child = this->template alloc_eval_typed<T, R>(context, map, board, land_point_node);
                        child->is_hold = false;
                        child->children_next = children;
                        children = child;
                        uniq.insert(lp_key(child->identity));
                    };
                    context->search->search_eval(board, search_spawn, level, fresh_main_cb);
                    if (children != nullptr)
                    {
                        auto fresh_hold_cb = [&]<char T, std::uint8_t R>(LandPoint const &land_point_node)
                        {
                            if (uniq.find(lp_key(land_point_node)) != uniq.end())
                            {
                                return;
                            }
                            TetrisTreeNode *child = this->template alloc_eval_typed<T, R>(context, map, board, land_point_node);
                            child->is_hold = true;
                            child->children_next = children;
                            children = child;
                        };
                        context->search->search_eval(board, hold_spawn, level, fresh_hold_cb);
                    }
                    uniq.clear();
                }
                else if (!node_flag.check(search_spawn, hold_spawn))
                {
                    auto &old = context->old;
                    for (auto it = children; it != nullptr; it = it->children_next)
                    {
                        old.emplace(lp_key(it->identity), it);
                    }
                    children = nullptr;
                    if (node_flag.check(hold_spawn, search_spawn))
                    {
                        node_flag.set(search_spawn, hold_spawn);
                        auto reversed_main_cb = [&]<char T, std::uint8_t R>(LandPoint const &land_point_node)
                        {
                            auto find = old.find(lp_key(land_point_node));
                            assert(find != old.end());
                            TetrisTreeNode *child = find->second;
                            old.erase(find);
                            child->is_hold = false;
                            child->children_next = children;
                            children = child;
                        };
                        context->search->search_eval(board, search_spawn, level, reversed_main_cb);
                        if (children != nullptr)
                        {
                            for (auto &pair : old)
                            {
                                TetrisTreeNode *child = pair.second;
                                child->is_hold = true;
                                child->children_next = children;
                                children = child;
                            }
                        }
                        else
                        {
                            reset_root_frontier_if_children_changed(context, root_children_before, root_frontier_reset);
                            for (auto &pair : old)
                            {
                                context->dealloc(pair.second);
                            }
                        }
                    }
                    else
                    {
                        node_flag.set(search_spawn, hold_spawn);
                        auto &uniq = context->uniq;
                        auto rescan_main_cb = [&]<char T, std::uint8_t R>(LandPoint const &land_point_node)
                        {
                            TetrisTreeNode *child;
                            auto find = old.find(lp_key(land_point_node));
                            bool const reused = find != old.end();
                            if (reused)
                            {
                                child = find->second;
                                old.erase(find);
                            }
                            else
                            {
                                child = this->template alloc_eval_typed<T, R>(context, map, board, land_point_node);
                            }
                            child->is_hold = false;
                            score_new_root_child(context, child, reused);
                            child->children_next = children;
                            children = child;
                            uniq.insert(lp_key(child->identity));
                        };
                        context->search->search_eval(board, search_spawn, level, rescan_main_cb);
                        if (children != nullptr)
                        {
                            auto rescan_hold_cb = [&]<char T, std::uint8_t R>(LandPoint const &land_point_node)
                            {
                                if (uniq.find(lp_key(land_point_node)) != uniq.end())
                                {
                                    return;
                                }
                                TetrisTreeNode *child;
                                auto find = old.find(lp_key(land_point_node));
                                bool const reused = find != old.end();
                                if (reused)
                                {
                                    child = find->second;
                                    old.erase(find);
                                }
                                else
                                {
                                    child = this->template alloc_eval_typed<T, R>(context, map, board, land_point_node);
                                }
                                child->is_hold = true;
                                score_new_root_child(context, child, reused);
                                child->children_next = children;
                                children = child;
                            };
                            context->search->search_eval(board, hold_spawn, level, rescan_hold_cb);
                        }
                        reset_root_frontier_if_children_changed(context, root_children_before, root_frontier_reset);
                        for (auto &pair : old)
                        {
                            context->dealloc(pair.second);
                        }
                        uniq.clear();
                    }
                    old.clear();
                }
            }
            else
            {
                if (node_flag.empty())
                {
                    node_flag.set(search_spawn, hold_spawn);
                    auto fresh_main_cb = [&]<char T, std::uint8_t R>(LandPoint const &land_point_node)
                    {
                        TetrisTreeNode *child = this->template alloc_eval_typed<T, R>(context, map, board, land_point_node);
                        child->is_hold = false;
                        child->children_next = children;
                        children = child;
                    };
                    context->search->search_eval(board, search_spawn, level, fresh_main_cb);
                    if (children != nullptr)
                    {
                        auto fresh_hold_cb = [&]<char T, std::uint8_t R>(LandPoint const &land_point_node)
                        {
                            TetrisTreeNode *child = this->template alloc_eval_typed<T, R>(context, map, board, land_point_node);
                            child->is_hold = true;
                            child->children_next = children;
                            children = child;
                        };
                        context->search->search_eval(board, hold_spawn, level, fresh_hold_cb);
                    }
                }
                else if (!node_flag.check(search_spawn, hold_spawn))
                {
                    if (node_flag.check(hold_spawn, search_spawn))
                    {
                        node_flag.set(search_spawn, hold_spawn);
                        bool search_has_any = false;
                        auto probe_cb = [&]<char T, std::uint8_t R>(LandPoint const &) { search_has_any = true; };
                        context->search->search_eval(board, search_spawn, level, probe_cb);
                        if (search_has_any)
                        {
                            for (auto it = children; it != nullptr; it = it->children_next)
                            {
                                it->is_hold = lp_type(it->identity) == hold_t;
                            }
                        }
                        else
                        {
                            auto removed = children;
                            children = nullptr;
                            reset_root_frontier_if_children_changed(context, root_children_before, root_frontier_reset);
                            for (auto *it = removed, *next_removed = removed; it != nullptr; it = next_removed)
                            {
                                next_removed = it->children_next;
                                context->dealloc(it);
                            }
                        }
                    }
                    else
                    {
                        node_flag.set(search_spawn, hold_spawn);
                        auto &old = context->old;
                        for (auto it = children; it != nullptr; it = it->children_next)
                        {
                            old.emplace(lp_key(it->identity), it);
                        }
                        children = nullptr;
                        auto rescan_main_cb = [&]<char T, std::uint8_t R>(LandPoint const &land_point_node)
                        {
                            TetrisTreeNode *child;
                            auto find = old.find(lp_key(land_point_node));
                            bool const reused = find != old.end();
                            if (reused)
                            {
                                child = find->second;
                                old.erase(find);
                            }
                            else
                            {
                                child = this->template alloc_eval_typed<T, R>(context, map, board, land_point_node);
                            }
                            child->is_hold = false;
                            score_new_root_child(context, child, reused);
                            child->children_next = children;
                            children = child;
                        };
                        context->search->search_eval(board, search_spawn, level, rescan_main_cb);
                        auto rescan_hold_cb = [&]<char T, std::uint8_t R>(LandPoint const &land_point_node)
                        {
                            TetrisTreeNode *child;
                            auto find = old.find(lp_key(land_point_node));
                            bool const reused = find != old.end();
                            if (reused)
                            {
                                child = find->second;
                                old.erase(find);
                            }
                            else
                            {
                                child = this->template alloc_eval_typed<T, R>(context, map, board, land_point_node);
                            }
                            child->is_hold = true;
                            score_new_root_child(context, child, reused);
                            child->children_next = children;
                            children = child;
                        };
                        context->search->search_eval(board, hold_spawn, level, rescan_hold_cb);
                        reset_root_frontier_if_children_changed(context, root_children_before, root_frontier_reset);
                        for (auto &pair : old)
                        {
                            context->dealloc(pair.second);
                        }
                        old.clear();
                    }
                }
            }
        }
        void search(Context *context)
        {
            using SearchSpec = typename SearchRuleSpecOf<TetrisSearch>::type;
            using SearchHelpers = m_tetris2::Helpers<SearchSpec>;
            using piece_info_t = typename SearchHelpers::piece_info_t;
            auto board = m_tetris2::build_board_for_search<SearchSpec>(map);
            if (node_flag.empty())
            {
                node_flag.set(TetrisNodeFlag::kVirtualSearchFlag);
                for (int i = 0; i < SearchHelpers::kPieceCount; ++i)
                {
                    auto fresh_cb = [&]<char T, std::uint8_t R>(LandPoint const &land_point_node)
                    {
                        TetrisTreeNode *child = this->template alloc_eval_typed<T, R>(context, map, board, land_point_node);
                        child->is_hold = false;
                        child->children_next = children;
                        children = child;
                    };
                    char piece_t = piece_info_t::type_at(static_cast<std::size_t>(i));
                    auto sp = SearchSpec::spawn(piece_t, SearchSpec::width, SearchSpec::height);
                    auto spawn = SearchHelpers::state_from_status(piece_t, 0, sp.first, sp.second);
                    context->search->search_eval(board, spawn, level, fresh_cb);
                }
            }
            else if (node_flag.check(TetrisNodeFlag::kVirtualSearchFlag))
            {
                node_flag.set(TetrisNodeFlag::kVirtualSearchFlag);
                auto &old = context->old;
                for (auto it = children; it != nullptr; it = it->children_next)
                {
                    old.emplace(lp_key(it->identity), it);
                }
                children = nullptr;
                for (int i = 0; i < SearchHelpers::kPieceCount; ++i)
                {
                    auto rescan_cb = [&]<char T, std::uint8_t R>(LandPoint const &land_point_node)
                    {
                        TetrisTreeNode *child;
                        auto find = old.find(lp_key(land_point_node));
                        if (find != old.end())
                        {
                            child = find->second;
                            old.erase(find);
                        }
                        else
                        {
                            child = this->template alloc_eval_typed<T, R>(context, map, board, land_point_node);
                        }
                        child->is_hold = false;
                        child->children_next = children;
                        children = child;
                    };
                    char piece_t = piece_info_t::type_at(static_cast<std::size_t>(i));
                    auto sp = SearchSpec::spawn(piece_t, SearchSpec::width, SearchSpec::height);
                    auto spawn = SearchHelpers::state_from_status(piece_t, 0, sp.first, sp.second);
                    context->search->search_eval(board, spawn, level, rescan_cb);
                }
                for (auto &pair : old)
                {
                    context->dealloc(pair.second);
                }
                old.clear();
            }
        }
        void run_virtual(Context *context)
        {
            using SearchSpec = typename SearchRuleSpecOf<TetrisSearch>::type;
            using SearchHelpers = m_tetris2::Helpers<SearchSpec>;
            using piece_info_t = typename SearchHelpers::piece_info_t;
            search(context);
            auto &iterate_cache = context->iterate_cache;
            iterate_cache.clear();
            iterate_cache.resize(static_cast<std::size_t>(SearchHelpers::kPieceCount), nullptr);
            for (auto it = children; it != nullptr; it = it->children_next)
            {
                Core::template get<false>(context, it, this);
                int idx = piece_info_t::index_of(lp_type(it->identity));
                auto &status = iterate_cache[static_cast<std::size_t>(idx)];
                if (status == nullptr || *status < it->status.get())
                {
                    status = &it->status.get();
                }
            }
            Core::iterate(*context->ai, iterate_cache.data(), iterate_cache.size(), this);
        }
        template<bool EnableEnv>
        AIEnv env(Context *context, TetrisTreeNode const *tree_node)
        {
            if (EnableEnv)
            {
                AIEnv result =
                    {
                        nullptr, 0, lp_type(tree_node->identity), tree_node->is_hold ? node : hold};
                auto queue_begin = tree_node->is_hold && hold == ' ' ? std::next(next) : next;
                result.length = std::distance(queue_begin, context->next.cend());
                if (result.length == 0)
                {
                    result.next = nullptr;
                }
                else
                {
                    result.next = context->next_c.data() + (context->next_c.size() - result.length);
                }
                result.is_hold = tree_node->is_hold;
                return result;
            }
            else
            {
                return {
                    nullptr, 0, ' ', ' ', false};
            }
        }
        template<bool EnableHold>
        TetrisTreeNode *build_children(Context *context)
        {
            if (version == context->version || is_dead)
            {
                return children;
            }
            version = context->version;
            search_children<EnableHold>(context);
            if (children == nullptr)
            {
                is_dead = true;
                return children;
            }
            for (auto it = children; it != nullptr; it = it->children_next)
            {
                Core::template get<true>(context, it, this);
                ++context->scored;
            }
            if (TetrisAIHasIterate<TetrisAI>::type::value && level < context->next.size() && context->next[level].get_vp())
            {
                for (auto it = children; it != nullptr; it = it->children_next)
                {
                    it->level = level + 1;
                    it->run_virtual(context);
                }
            }
            return children;
        }
        template<bool EnableHold>
        void search_children(Context *context)
        {
            if (parent == nullptr)
            {
                assert(context->current_t == node);
                level = 0;
                if (EnableHold)
                {
                    if (hold == ' ')
                    {
                        if (is_hold_lock || next == context->next.end())
                        {
                            search(context, context->current_t, false);
                        }
                        else
                        {
                            search(context, context->current_t, next->node);
                        }
                    }
                    else
                    {
                        if (is_hold_lock)
                        {
                            search(context, context->current_t, false);
                        }
                        else
                        {
                            search(context, context->current_t, hold);
                        }
                    }
                }
                else
                {
                    search(context, context->current_t, false);
                }
                return;
            }
            level = parent->level + 1;
            if (EnableHold)
            {
                if (is_hold && parent->hold == ' ')
                {
                    assert(parent->next != context->next.end());
                    next = std::next(parent->next);
                }
                else
                {
                    next = parent->next;
                }
                if (next == context->next.end())
                {
                    node = ' ';
                }
                else
                {
                    node = next->node;
                    next = std::next(next);
                }
                hold = is_hold ? parent->node : parent->hold;
                assert(node != ' ' || hold != ' ');
                if (hold == ' ')
                {
                    if (next == context->next.end())
                    {
                        search(context, node, false);
                    }
                    else
                    {
                        search(context, node, next->node);
                    }
                }
                else
                {
                    if (node == ' ')
                    {
                        search(context, hold, true);
                    }
                    else
                    {
                        search(context, node, hold);
                    }
                }
            }
            else
            {
                assert(parent->next != context->next.end());
                node = parent->next->node;
                next = std::next(parent->next);
                search(context, node, false);
            }
        }
        template<bool EnableHold>
        bool run(Context *context)
        {
            if (context->is_complete)
            {
                return true;
            }
            assert(parent == nullptr);
            if (context->width == 0)
            {
                auto &wait = context->wait.back();
                for (auto it = build_children<EnableHold>(context); it != nullptr; it = it->children_next)
                {
                    wait.push(it);
                }
                context->width = 2;
            }
            else
            {
                context->width += 1;
            }
            bool complete = true;
            size_t next_length = context->max_length;
            double div_ratio = 1;
            if (next_length > 0)
            {
                double ratio = Core::template get_ratio<TetrisTreeNode>(*context->ai);
                while (next_length > context->width_cache.size())
                {
                    context->width_cache.emplace_back(std::pow(context->width_cache.size() + 2, ratio));
                }
                div_ratio = 2 / *std::max_element(context->width_cache.begin(), context->width_cache.end());
            }
            while (next_length-- > 0)
            {
                size_t level_prune_hold = std::max<size_t>(1, size_t(context->width_cache[next_length] * context->width * div_ratio));
                auto wait = &context->wait[next_length + 1];
                if (wait->empty())
                {
                    continue;
                }
                else
                {
                    complete = false;
                }
                auto sort = &context->sort[next_length + 1];
                auto next = &context->wait[next_length];
                auto push_one = [&]
                {
                    TetrisTreeNode *child = wait->top();
                    wait->pop();
                    sort->push(child);
                    for (auto it = child->build_children<EnableHold>(context); it != nullptr; it = it->children_next)
                    {
                        next->push(it);
                    }
                };
                if (wait->empty())
                {
                    // do nothing
                }
                else if (sort->size() >= level_prune_hold)
                {
                    if (sort->top()->status.get() < wait->top()->status.get())
                    {
                        push_one();
                    }
                }
                else
                {
                    do
                    {
                        push_one();
                    } while (sort->size() < level_prune_hold && !wait->empty());
                }
            }
            if (complete)
            {
                context->is_complete = true;
                return true;
            }
            return false;
        }
        std::pair<TetrisTreeNode const *, Status const *> get_best(Context *context)
        {
            TetrisTreeNode *best = nullptr;
            for (size_t i = 0; i < context->wait.size() && i < context->sort.size(); ++i)
            {
                auto wait_best = (context->wait.size() <= i || context->wait[i].empty()) ? nullptr : context->wait[i].top();
                auto sort_best = (context->sort.size() <= i || context->sort[i].empty()) ? nullptr : context->sort[i].top();
                if (wait_best == nullptr)
                {
                    if (sort_best == nullptr)
                    {
                        continue;
                    }
                    else
                    {
                        best = sort_best;
                    }
                }
                else
                {
                    if (sort_best == nullptr)
                    {
                        best = wait_best;
                    }
                    else
                    {
                        best = sort_best->status.get() < wait_best->status.get() ? wait_best : sort_best;
                    }
                }
                break;
            }
            if (best == nullptr)
            {
                return std::make_pair(nullptr, nullptr);
            }
            auto status = &best->status.get_raw();
            while (best->parent->parent != nullptr)
            {
                best = best->parent;
            }
            return std::make_pair(best, status);
        }
    };

    //=== SearchTag resolution =================================================
    // 把 TetrisEngine 第三模板参数从"已实例化的 search 类"扩展成"search tag" —
    //   tag 暴露 `template<class RuleType> using type = <concrete search>`,
    //   ResolveSearch 在 tag 上探测 ::template type<RuleType> 是否存在,
    //     存在: 走 tag 路径, 解析成 tag::template type<RuleType>;
    //     不存在: 当作旧式直接传入 search 类型, 原样穿透.
    //   兼容旧调用面 (QQTetrisSearch / 直写 movegen::Searcher<...> 三层嵌套).
    namespace detail
    {
        template<class T, class RuleType, class = void>
        struct SearchTagResolve
        {
            using type = T;
        };
        template<class T, class RuleType>
        struct SearchTagResolve<T, RuleType,
                                std::void_t<typename T::template type<RuleType>>>
        {
            using type = typename T::template type<RuleType>;
        };

        //----------------------------------------------------------------------
        // rebind_search_policy<Tag, Policy>
        //
        //   若 Tag 暴露 template<class P> using rebind_policy = ...,
        //   则把 Policy 注入, 返回 Tag::rebind_policy<Policy>.
        //   否则 (旧式直接传 Searcher / 无 rebind_policy 的 tag) 原样穿透.
        //
        //   供 TetrisEngine / TetrisThreadEngine 用于从 SearchTag 自动注入
        //   DeduceSpinPolicy<AI>.
        //----------------------------------------------------------------------
        template<class Tag, class Policy, class = void>
        struct rebind_search_policy
        {
            using type = Tag;
        };
        template<class Tag, class Policy>
        struct rebind_search_policy<Tag, Policy,
                                    std::void_t<typename Tag::template rebind_policy<Policy>>>
        {
            using type = typename Tag::template rebind_policy<Policy>;
        };

        template<class Tag, class Policy>
        using rebind_search_policy_t = typename rebind_search_policy<Tag, Policy>::type;
    } // namespace detail

    template<class TetrisRule, class TetrisAI, class TetrisSearchTag>
    class TetrisEngine
    {
    public:
        using ResolvedSearchTag = detail::rebind_search_policy_t<TetrisSearchTag, DeduceSpinPolicy<TetrisAI>>;
        typedef typename detail::SearchTagResolve<ResolvedSearchTag, TetrisRule>::type TetrisSearch;
        typedef TetrisCore<TetrisAI, TetrisSearch> Core;
        typedef TetrisTreeNode<typename Core::Status, TetrisAI, TetrisSearch> TreeNode;
        typedef LocalContextBuilder<typename TreeNode::Context, TetrisRule, TetrisAI, TetrisSearch> ContextBuilder;
        typedef typename Core::LandPoint LandPoint;
        using map_t = typename TreeNode::map_t;

    private:
        std::deque<TreeNode> tree_storage_;
        int32_t width_ = 0;
        int32_t height_ = 0;
        size_t type_max_ = 0;
        std::array<char, 256> index_to_type_{};
        std::array<size_t, 256> type_to_index_{};
        bool prepared_ = false;
        typename ContextBuilder::template LocalContext<TreeNode> local_context_;
        TreeNode *root_;
        TetrisAI ai_;
        TetrisSearch search_;
        typename Core::Status status_;
        uint64_t memory_limit_;

        // Nodes freed per background step; a foreground call waits for at most one step, about 0.4 ms of freeing.
        static constexpr size_t kReclaimBatch = 4096;

        // One step of the argument-free runs: free discarded nodes first, then widen the beam of the last searched
        // position (there is none before the first search).
        template<bool EnableHold>
        bool widen()
        {
            if (local_context_.reclaim(kReclaimBatch))
            {
                return false;
            }
            if (local_context_.current_t != '\0' && memory_usage() < memory_limit_)
            {
                return root_->template run<EnableHold>(&local_context_);
            }
            return true;
        }

    public:
        typedef typename Core::Status Status;
        struct RunResult
        {
            typedef typename Core::Status Status;
            RunResult() : target(), status(), change_hold()
            {
            }
            RunResult(bool _change_hold) : target(), status(), change_hold(_change_hold)
            {
            }
            RunResult(std::pair<TreeNode const *, Status const *> const &_result) : target(_result.first ? _result.first->identity : LandPoint{}), status(*_result.second), change_hold()
            {
            }
            RunResult(std::pair<TreeNode const *, Status const *> const &_result, bool _change_hold) : target(_result.first ? _result.first->identity : LandPoint{}), status(*_result.second), change_hold(_change_hold)
            {
            }
            LandPoint target;
            Status status;
            bool change_hold;
        };

    public:
        TetrisEngine() : local_context_(&tree_storage_), ai_(), root_(nullptr), status_(), memory_limit_(128ull << 20)
        {
            tree_storage_.emplace_back();
            root_ = &tree_storage_.back();
            local_context_.ai = &ai_;
            local_context_.search = &search_;
        }
        bool prepare(int width, int height)
        {
            if (prepared_ && local_context_.eval_cache.prepared() && width_ == width && height_ == height)
            {
                return true;
            }
            width_ = width;
            height_ = height;
            type_max_ = 0;
            prepared_ = false;
            std::fill(index_to_type_.begin(), index_to_type_.end(), '\0');
            std::fill(type_to_index_.begin(), type_to_index_.end(), size_t(0));
            bool seen[256] = {false};
            using Ops = typename TetrisRule::rule_spec::ops;
            std::apply([&](auto... op_tag)
            {
                (([&]
                {
                    using Op = std::decay_t<decltype(op_tag)>;
                    char t = Op::type;
                    int upper_index = int(::toupper(t)) + 128;
                    if (!seen[upper_index])
                    {
                        seen[upper_index] = true;
                        char t_upper = ::toupper(t);
                        int lo = int(::tolower(t_upper)) + 128;
                        int hi = int(::toupper(t_upper)) + 128;
                        index_to_type_[type_max_] = t_upper;
                        type_to_index_[lo] = type_max_;
                        type_to_index_[hi] = type_max_;
                        ++type_max_;
                    }
                }()), ...);
            }, Ops{});
            local_context_.ai = &ai_;
            local_context_.search = &search_;
            // 容量非法（0/溢出/超最高幂）时 cache.prepare 返回 false：不半初始化，直接失败返回。
            if (!local_context_.eval_cache.prepare(local_context_.eval_cache.options().bucket_count))
            {
                prepared_ = false;
                return false;
            }
            if (TetrisRuleInit<TetrisRule>::init(width, height))
            {
                ContextBuilder::init_ai(ai_, &local_context_);
                ContextBuilder::init_search(search_, &local_context_);
                prepared_ = true;
                return true;
            }
            width_ = 0;
            height_ = 0;
            type_max_ = 0;
            std::fill(index_to_type_.begin(), index_to_type_.end(), '\0');
            std::fill(type_to_index_.begin(), type_to_index_.end(), size_t(0));
            return false;
        }
        bool prepared() const { return prepared_ && local_context_.eval_cache.prepared(); }
        size_t type_max() const
        {
            return type_max_;
        }
        size_t convert(char type) const
        {
            return type_to_index_[int(type) + 128];
        }
        char convert(size_t index) const
        {
            return index_to_type_[index];
        }
        int32_t width() const
        {
            return width_;
        }
        int32_t height() const
        {
            return height_;
        }
        uint32_t full() const
        {
            return static_cast<uint32_t>(map_t::row_full);
        }
        template<class LP,
                 std::enable_if_t < !std::is_pointer_v<std::remove_cvref_t<LP>> &&
                     requires(LP const &x)
        {
            x.state;
        }
        ,
            int > = 0 >
                    size_t attach(LP const &lp, map_t &map) const
        {
            using Spec = typename SearchRuleSpecOf<TetrisSearch>::type;
            return m_tetris2::Helpers<Spec>::attach_to_map(lp.state, map);
        }
        uint64_t memory_limit() const
        {
            return memory_limit_;
        }
        void memory_limit(uint64_t value)
        {
            value = std::max<uint64_t>(32ull << 20, value);
            value = std::min<uint64_t>(2ull << 30, value);
            memory_limit_ = value;
        }
        uint64_t memory_usage() const
        {
            // 节点堆 + eval cache 真实占用字节（cache total_bytes = bucket_count * sizeof(Entry)）。
            return (local_context_.node_storage->size() - local_context_.free_count) * sizeof(TreeNode)
                 + local_context_.eval_cache.stats().total_bytes;
        }
        //AI名称
        std::string ai_name() const
        {
            return ai_.ai_name();
        }
        auto ai_config() const
        {
            return local_context_.ai_config();
        }
        // 只读 ai_config()；可变写入统一走 set_ai_config / notify_ai_config_changed。
        EvalCache<map_t, typename Core::Result> const &eval_cache() const
        {
            return local_context_.eval_cache;
        }
        EvalCache<map_t, typename Core::Result> &eval_cache()
        {
            return local_context_.eval_cache;
        }
        typename EvalCache<map_t, typename Core::Result>::Stats eval_cache_stats() const
        {
            return local_context_.eval_cache.stats();
        }
        size_t eval_generation() const
        {
            return local_context_.eval_cache.generation();
        }
        void set_ai_config(auto const &next)
        {
            local_context_.set_ai_config(next);
            on_ai_config_changed();
        }
        void notify_ai_config_changed()
        {
            on_ai_config_changed();
        }
        ProviderCounters &provider_counters()
        {
            return local_context_.provider_counters;
        }
        auto search_config() const
        {
            return local_context_.search_config();
        }
        auto search_config()
        {
            return local_context_.search_config();
        }
        Status const *status() const
        {
            return &status_;
        }
        Status *status()
        {
            return &status_;
        }
        TetrisAI *ai()
        {
            return &ai_;
        }
        //update!强制刷新上下文
        void update()
        {
            local_context_.is_complete = false;
            root_->update_version(&local_context_);
        }
        // 显式 Config 变更：旧树每个节点都持有一份 eval(map) 副本（eval_slot.result）和由旧 Config 推出的
        // raw Status / iterate 结果。bump cache generation 只在某节点下次被 get 时才刷新它；当轮 beam 未重访到的
        // 节点副本保持旧 Config 量级，之后仍参与选子（reused 根 children 与未重访子树均如此）。因此主动丢弃整棵已搜
        // 索子树、只留 root shell，下轮 run 用新 Config 全新重建。host 输入（盘面、Status、next）不受影响，由下次
        // update() 重新套用。frontier(wait/sort) 与 version 仍由 update_version 清空/失效。
        void discard_search_tree()
        {
            for (TreeNode *child = root_->children; child != nullptr; child = child->children_next)
            {
                local_context_.dealloc(child);
            }
            *root_ = TreeNode{};
            root_->parent = nullptr;
        }
        // Config 变更：bump eval generation（旧桶靠 written_gen 不等自动 stale）+ 丢弃旧搜索树 +
        // update_version 清空 frontier/失效 version，下轮 run 自顶向下全新重建。
        void on_ai_config_changed()
        {
            bool overflowed = local_context_.eval_cache.on_config_changed();
            // 缓存溢出（generation 环绕）：旧树节点 eval_gen 可能误等于新纪元，era++ 使全树 eval holder 失效。
            if (overflowed) ++local_context_.eval_era;
            local_context_.is_complete = false;
            discard_search_tree();
            root_->update_version(&local_context_);
        }
        // Positions scored (AI::get calls) since the beam last restarted.
        size_t scored() const
        {
            return local_context_.scored;
        }
        // Current iterative beam width; exposed for deterministic frontier-reset regression tests and diagnostics.
        size_t search_width() const
        {
            return local_context_.width;
        }
        size_t search_version() const
        {
            return local_context_.version;
        }
        std::vector<std::pair<std::uint32_t, size_t>> root_child_versions() const
        {
            std::vector<std::pair<std::uint32_t, size_t>> result;
            for (auto child = root_->children; child != nullptr; child = child->children_next)
            {
                result.emplace_back(TreeNode::lp_key(child->identity).status, child->version);
            }
            return result;
        }
        // The beam has expanded every node it holds; widening adds nothing more.
        bool complete() const
        {
            return local_context_.is_complete;
        }
        bool run()
        {
            if (!prepared_ || !local_context_.eval_cache.prepared())
                return false;   // 未成功 prepare：不 widen 旧 root（graceful，不 abort）
            return widen<false>();
        }
        bool run_hold()
        {
            if (!prepared_ || !local_context_.eval_cache.prepared())
                return false;
            return widen<true>();
        }
        //run!
        RunResult run(map_t const &map, TetrisBlockStatus const &status, char const *next, size_t next_length, time_t limit = 100)
        {
            if (!prepared_)
            {
                // 未 prepare（或 prepare 失败/cache 非法）：不允许在旧树上运行，运行期清晰失败。
                std::fputs("tetris engine: run() before successful prepare()\n", stderr);
                std::abort();
            }
            using namespace std::chrono;
            auto const &bb_map = map;
            auto now = high_resolution_clock::now(), end = now + std::chrono::milliseconds(limit);
            root_ = root_->update(&local_context_, bb_map, status_, status, next, next_length);
            do
            {
                if (root_->template run<false>(&local_context_))
                {
                    break;
                }
            } while ((now = high_resolution_clock::now()) < end);
            auto best = root_->get_best(&local_context_);
            return best.first != nullptr ? RunResult(root_->get_best(&local_context_)) : RunResult(false);
        }
        // 只读快照：返回当前 root 已展开树的 best（含 target identity + status + is_hold），
        // 不 widen、不更新 root、不计分、不改 queue。供测试多次 run() 后取 final best。
        RunResult current_result()
        {
            if (!prepared_)
                return RunResult(false);
            auto best = root_->get_best(&local_context_);
            return best.first != nullptr
                ? RunResult(std::make_pair(best.first, best.second), best.first->is_hold)
                : RunResult(false);
        }
        // 只读 typed visitor：对当前 root 可达树递归调用 f(node, parent)（root parent==nullptr）。
        // 不构满 Context、不计分/不更新 root/不改 queue，0 side effect；正常 driver 不调用。
        // node 为 const TreeNode&，lambda auto 可访问 public map/identity/eval_slot/status/level/
        // parent/children/is_virtual 等。只遍历 children→children_next 链表（可达 root 子树）。
        template<class F>
        void visit_tree(F&& f) const
        {
            if (root_)
                visit_node(*root_, nullptr, static_cast<F&&>(f));
        }
    private:
        template<class F>
        static void visit_node(TreeNode const &node, TreeNode const *parent, F &&f)
        {
            std::forward<F>(f)(node, parent);
            for (TreeNode const *c = node.children; c != nullptr; c = c->children_next)
                visit_node(*c, &node, std::forward<F>(f));
        }
    public:
        //带hold的run!
        RunResult run_hold(map_t const &map, TetrisBlockStatus const &status, char hold, bool hold_free, char const *next, size_t next_length, time_t limit = 100)
        {
            if (!prepared_)
            {
                std::fputs("tetris engine: run_hold() before successful prepare()\n", stderr);
                std::abort();
            }
            using namespace std::chrono;
            auto const &bb_map = map;
            auto now = high_resolution_clock::now(), end = now + std::chrono::milliseconds(limit);
            root_ = root_->update(&local_context_, bb_map, status_, status, hold, !hold_free, next, next_length);
            do
            {
                if (root_->template run<true>(&local_context_))
                {
                    break;
                }
            } while ((now = high_resolution_clock::now()) < end);
            if (root_->hold == ' ' && local_context_.next.size() == 1 && !root_->is_hold_lock)
            {
                return RunResult(true);
            }
            else
            {
                auto best = root_->get_best(&local_context_);
                return best.first != nullptr ? RunResult(best, best.first != nullptr && best.first->is_hold) : RunResult(false);
            }
        }
        // The host plays `result`: the other root moves are dropped, so further widening (the background thinking of
        // TetrisThreadEngine) explores only the position after it, which the next search reuses if the board matches.
        // A later search of the same position can only return `result`.
        void commit(RunResult const &result)
        {
            if (TreeNode::lp_is_valid(result.target) && root_->keep_only(&local_context_, result.target))
            {
                local_context_.is_complete = false;
                root_->update_version(&local_context_);
            }
        }
        //根据run的结果得到一个操作路径
        std::vector<char> make_path(TetrisBlockStatus const &status, LandPoint const &land_point, map_t const &map, bool cut_drop = true)
        {
            using Spec = typename SearchRuleSpecOf<TetrisSearch>::type;
            using SpecHelpers = m_tetris2::Helpers<Spec>;
            auto const &board = map;
            auto spawn = SpecHelpers::state_from_status(
                status.t,
                static_cast<std::uint8_t>(status.r),
                status.x,
                status.y);
            auto path = search_.make_path(spawn, land_point, board);
            if (cut_drop)
            {
                while (!path.empty() && (path.back() == 'd' || path.back() == 'D'))
                {
                    path.pop_back();
                }
            }
            return path;
        }
        //单块评价
        template<class container_t>
        void search(TetrisBlockStatus const &status, map_t const &map, container_t &result)
        {
            using Spec = typename SearchRuleSpecOf<TetrisSearch>::type;
            using SpecHelpers = m_tetris2::Helpers<Spec>;
            auto const &board = map;
            std::array<typename SpecHelpers::map_t, SpecHelpers::kMaxR> usable_arr{};
            SpecHelpers::build_usable_for_piece(status.t, board, usable_arr);
            if (!SpecHelpers::check_T(status.t, status.x, status.y, static_cast<std::uint8_t>(status.r), usable_arr))
            {
                result.clear();
                return;
            }
            auto const *land_point = search_.search(map, status, 0);
            result.assign(land_point->begin(), land_point->end());
        }
    };

    template<class TetrisRule, class TetrisAI, class TetrisSearch>
    class TetrisThreadEngine
    {
    private:
        typedef TetrisEngine<TetrisRule, TetrisAI, TetrisSearch> Engine;

        template<class T, class U>
        struct ValueHolder
        {
            T value;
            T const *get() const
            {
                return &value;
            }
            T *get()
            {
                return &value;
            }
            void assign(T *ptr) const
            {
                *ptr = value;
            }
        };
        template<class U>
        struct ValueHolder<void, U>
        {
            void const *get() const
            {
                return nullptr;
            }
            void *get()
            {
                return nullptr;
            }
            void assign(void *) const
            {
            }
        };

        Engine engine_;
        // ai_config()/search_config() 返回 ConfigType const* 或 void const*；remove_pointer 解引用，
        // 无 Config AI 得 void → ValueHolder<void> 空操作（不构 Config、setter 仅 actual requires cfg）。
        ValueHolder<std::remove_cv_t<std::remove_pointer_t<decltype(std::declval<Engine const &>().ai_config())>>, void> ai_config_;
        ValueHolder<std::remove_cv_t<std::remove_pointer_t<decltype(std::declval<Engine const &>().search_config())>>, void> search_config_;
        typename Engine::Status status_;

        std::mutex mutex_;
        std::thread worker_;
        std::chrono::high_resolution_clock::time_point stop_;
        std::condition_variable cv_;
        bool with_hold_;
        // Cleared without the mutex: that is how a foreground call gets the worker to release it.
        std::atomic<bool> backgrond_;
        bool running_;
        bool update_pending_ = false;
        size_t config_set_count_ = 0;

        struct PauseBackground
        {
            TetrisThreadEngine *self;

            PauseBackground(TetrisThreadEngine *_self) : self(_self)
            {
                self->backgrond_ = false;
                self->mutex_.lock();
            }

            ~PauseBackground()
            {
                self->backgrond_ = true;
                self->mutex_.unlock();
                // The worker may have gone back to waiting while paused; without a wake-up it idles until its timeout.
                self->cv_.notify_all();
            }
        };

        void work_thread_func()
        {
            std::unique_lock<std::mutex> lock(mutex_);
            while (running_)
            {
                if (cv_.wait_for(lock, std::chrono::seconds(1), [&]
                                 { return !running_ || (backgrond_ && std::chrono::high_resolution_clock::now() < stop_); }))
                {
                    if (!engine_.prepared())
                    {
                        backgrond_ = false;   // prepare 失败/未完成：后台不 resume 旧 root widen
                        continue;
                    }
                    if (with_hold_ ? engine_.run_hold() : engine_.run())
                    {
                        backgrond_ = false;
                    }
                }
            }
        }

        void start_work(bool with_hold)
        {
            with_hold_ = with_hold;
            if (!running_)
            {
                running_ = true;
                worker_ = std::move(std::thread(&TetrisThreadEngine::work_thread_func, this));
            }
            stop_ = std::chrono::high_resolution_clock::now() + std::chrono::seconds(10);
            cv_.notify_all();
        }

        // Hands the host's status and configs to the engine. A pending update() restarts the beam only here, after the
        // copy: restarted earlier, the worker could rebuild it with the old ones, and a run on the same position keeps
        // those nodes.
        void apply_host_state()
        {
            search_config_.assign(engine_.search_config());
            *engine_.status() = status_;
            if (std::exchange(update_pending_, false))
            {
                engine_.update();
            }
        }

    public:
        typedef typename Engine::Status Status;
        typedef typename Engine::RunResult RunResult;
        using map_t = typename Engine::map_t;

    public:
        TetrisThreadEngine() : engine_(), with_hold_(false), backgrond_(false), running_(false)
        {
        }
        ~TetrisThreadEngine()
        {
            if (running_)
            {
                {
                    PauseBackground pause(this);
                    running_ = false;
                }
                worker_.join();
            }
        }
        bool prepare(int width, int height)
        {
            PauseBackground pause(this);
            return engine_.prepare(width, height);
        }
        size_t type_max() const
        {
            return engine_.type_max();
        }
        size_t convert(char type) const
        {
            return engine_.convert(type);
        }
        char convert(size_t index) const
        {
            return engine_.convert(index);
        }
        int32_t width() const
        {
            return engine_.width();
        }
        int32_t height() const
        {
            return engine_.height();
        }
        uint32_t full() const
        {
            return engine_.full();
        }
        template<class LP,
                 std::enable_if_t < !std::is_pointer_v<std::remove_cvref_t<LP>> &&
                     requires(LP const &x)
        {
            x.state;
        }
        ,
            int > = 0 >
                    size_t attach(LP const &lp, map_t &map) const
        {
            return engine_.attach(lp, map);
        }
        uint64_t memory_limit() const
        {
            return engine_.memory_limit();
        }
        void memory_limit(uint64_t value)
        {
            PauseBackground pause(this);
            engine_.memory_limit(value);
        }
        uint64_t memory_usage() const
        {
            return engine_.memory_usage();
        }
        //AI名称
        std::string ai_name() const
        {
            return engine_.ai_name();
        }
        auto ai_config() const
        {
            return ai_config_.get();
        }
        void set_ai_config(auto const &next)
        {
            PauseBackground pause(this);
            ai_config_.value = next;
            engine_.set_ai_config(next);
            ++config_set_count_;
        }
        void notify_ai_config_changed()
        {
            PauseBackground pause(this);
            engine_.notify_ai_config_changed();
            ++config_set_count_;
        }
        size_t config_set_count() const
        {
            return config_set_count_;
        }
        auto search_config() const
        {
            return search_config_.get();
        }
        auto search_config()
        {
            return search_config_.get();
        }
        typename Engine::Status const *status() const
        {
            return &status_;
        }
        typename Engine::Status *status()
        {
            return &status_;
        }
        TetrisAI *ai()
        {
            return engine_.ai();
        }
        //update!强制刷新上下文
        void update()
        {
            PauseBackground pause(this);
            update_pending_ = true;
        }
        //run!
        RunResult run(typename Engine::map_t const &map, TetrisBlockStatus const &status, char const *next, size_t next_length, time_t limit = 100)
        {
            PauseBackground pause(this);
            apply_host_state();
            auto run_result = engine_.run(map, status, next, next_length, limit);
            start_work(false);
            return run_result;
        }
        //带hold的run!
        RunResult run_hold(typename Engine::map_t const &map, TetrisBlockStatus const &status, char hold, bool hold_free, char const *next, size_t next_length, time_t limit = 100)
        {
            PauseBackground pause(this);
            apply_host_state();
            auto run_result = engine_.run_hold(map, status, hold, hold_free, next, next_length, limit);
            start_work(true);
            return run_result;
        }
        // TetrisEngine::commit; background thinking then continues on the position after `result`.
        void commit(RunResult const &result)
        {
            PauseBackground pause(this);
            engine_.commit(result);
        }
        // 只读当前 best 快照（PauseBackground 廉价返回，不 widen/计分/改 queue）。
        RunResult current_result()
        {
            PauseBackground pause(this);
            return engine_.current_result();
        }
        //根据run的结果得到一个操作路径
        std::vector<char> make_path(TetrisBlockStatus const &status, typename Engine::LandPoint const &land_point, typename Engine::map_t const &map, bool cut_drop = true)
        {
            PauseBackground pause(this);
            return engine_.make_path(status, land_point, map, cut_drop);
        }
        //单块评价
        template<class container_t>
        void search(TetrisBlockStatus const &status, typename Engine::map_t const &map, container_t &result)
        {
            PauseBackground pause(this);
            engine_.search(status, map, result);
        }
    };

#include "tetris_active_policy.h"
}
