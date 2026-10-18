
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


    template<class TetrisAI>
    struct TetrisAIInfo
    {
    private:
        template<typename T>
        struct function_traits_get : public function_traits_get<decltype(&T::get)>
        {
        };
        template<typename ClassType, typename ReturnType, typename... Args>
        struct function_traits_get<ReturnType (ClassType::*)(Args...) const>
        {
            enum
            {
                arity = sizeof...(Args)
            };
            typedef ReturnType result_type;
        };

    public:
        // eval 是模板成员；每个 AI 类必须在内部声明 Result 类型别名。
        typedef typename TetrisAI::Result Result;
        typedef typename function_traits_get<TetrisAI>::result_type Status;
        enum
        {
            arity = function_traits_get<TetrisAI>::arity
        };
    };

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
                void *ai_config()
                {
                    return nullptr;
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
                AIConfigType *ai_config()
                {
                    return &ai_config_;
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
        typedef typename TetrisAIInfo<TetrisAI>::Result Result;
        typedef typename TetrisAIInfo<TetrisAI>::Status Status;

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
        template<class TreeNode, bool EnableEnv, size_t>
        struct TetrisSelectGet
        {
            typedef std::true_type enable_next_c;
            static void get(typename TreeNode::Context *context, TreeNode *node, TreeNode *parent)
            {
                // arity >= 5: AI::get(node, result, depth, status, env)
                node->status.set(TetrisCallGet<TetrisAI>::get(*context->ai, node->identity, node->result, parent->level, parent->status.get_raw(), parent->template env<EnableEnv>(context, node)));
            }
        };
        template<class TreeNode, bool EnableEnv>
        struct TetrisSelectGet<TreeNode, EnableEnv, 4>
        {
            typedef std::true_type enable_next_c;
            static void get(typename TreeNode::Context *context, TreeNode *node, TreeNode *parent)
            {
                node->status.set(TetrisCallGet<TetrisAI>::get(*context->ai,
                                                          node->identity,
                                                          node->result,
                                                          parent->level,
                                                          parent->status.get_raw(),
                                                          parent->template env<EnableEnv>(context, node)));
            }
        };
        template<class TreeNode, bool EnableEnv>
        struct TetrisSelectGet<TreeNode, EnableEnv, 3>
        {
            typedef std::false_type enable_next_c;
            static void get(typename TreeNode::Context *context, TreeNode *node, TreeNode *parent)
            {
                node->status.set(TetrisCallGet<TetrisAI>::get(*context->ai,
                                                          node->identity,
                                                          node->result,
                                                          parent->level,
                                                          parent->status.get_raw()));
            }
        };
        template<class TreeNode, bool EnableEnv>
        struct TetrisSelectGet<TreeNode, EnableEnv, 2>
        {
            typedef std::false_type enable_next_c;
            static void get(typename TreeNode::Context *context, TreeNode *node, TreeNode *parent)
            {
                node->status.set(TetrisCallGet<TetrisAI>::get(*context->ai,
                                                          node->identity,
                                                          node->result,
                                                          parent->level));
            }
        };

    public:
        template<class TreeNode>
        using EnableNextC = typename TetrisSelectGet<TreeNode, false, TetrisAIInfo<TetrisAI>::arity>::enable_next_c;

        template<class TreeNode>
        static double get_ratio(TetrisAI &ai)
        {
            return TetrisGetRatio<TreeNode, typename TetrisAIHasRatio<TetrisAI>::type>::get_ratio(ai);
        }
        template<bool EnableEnv, class TreeNode>
        static void get(typename TreeNode::Context *context, TreeNode *node, TreeNode *parent)
        {
            TetrisSelectGet<TreeNode, EnableEnv, TetrisAIInfo<TetrisAI>::arity>::get(context, node, parent);
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
        // Opt-in: Status::transposition_key() hashes the status fields that shape later placements.
        static constexpr bool kTranspositions = requires(Status const &value) { value.transposition_key(); };
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
            std::vector<chash_set<std::uint64_t>> expanded_keys;
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
            char flag[2];
            TetrisNodeFlag()
            {
                clear();
            }
            bool empty()
            {
                return flag[0] == '\0';
            }
            bool check(char node1)
            {
                assert(node1 != '\0');
                return flag[0] == node1 && flag[1] == '\0';
            }
            bool check(char node1, char node2)
            {
                assert(node1 != '\0');
                assert(node2 != '\0');
                return flag[0] == node1 && flag[1] == node2;
            }
            void set(char node1)
            {
                assert(node1 != '\0');
                flag[0] = node1;
                flag[1] = '\0';
            }
            void set(char node1, char node2)
            {
                assert(node1 != '\0');
                assert(node2 != '\0');
                flag[0] = node1;
                flag[1] = node2;
            }
            void clear()
            {
                flag[0] = '\0';
                flag[1] = '\0';
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
        using _SearchSpec = typename SearchRuleSpecOf<TetrisSearch>::type;
        using map_t = m_tetris2::TetrisMap<_SearchSpec::width, _SearchSpec::height>;
        map_t map;
        typename Core::LandPoint identity;
        typename Core::Result result;
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

        TetrisTreeNode *update_root(Context *context, map_t const &_map)
        {
            if (map == _map)
            {
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
            }
            else
            {
                new_root->parent = nullptr;
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
        void update_version(Context *context)
        {
            ++context->version;
            context->total += context->width;
            context->avg = context->total / context->version;
            context->width = 0;
            context->scored = 0;
            context->wait.clear();
            context->sort.clear();
            context->wait.resize(context->max_length + 1);
            context->sort.resize(context->max_length + 1);
            if constexpr (kTranspositions)
            {
                context->expanded_keys.resize(context->max_length + 1);
                for (auto &keys : context->expanded_keys)
                {
                    keys.clear();
                }
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
        // Children of the active piece depend on its start (reachability and spin class), so a new start drops them.
        bool restart(Context *context, bool searched_as_root, TetrisBlockStatus const &active)
        {
            using SearchSpec = typename SearchRuleSpecOf<TetrisSearch>::type;
            auto start = m_tetris2::Helpers<SearchSpec>::state_from_status(active.t, static_cast<std::uint8_t>(active.r), active.x, active.y);
            bool changed = !m_tetris2::StateEqual{}(start, searched_as_root ? context->root_start : spawn_state(active.t));
            context->root_start = start;
            if (changed)
            {
                for (auto it = children; it != nullptr; it = it->children_next)
                {
                    context->dealloc(it);
                }
                children = nullptr;
                node_flag.clear();
                is_dead = false;
            }
            return changed;
        }
        TetrisTreeNode *update(Context *context, map_t const &_map, Status const &status, TetrisBlockStatus const &active, char const *_next, size_t _next_length)
        {
            char _t = active.t;
            TetrisTreeNode *root = update_root(context, _map);
            bool restarted = root->restart(context, root == this, active);
            std::vector<next_t> next = process_next(_next, _next_length, _t);
            if (restarted || root != this || (context->current_t == '\0' || _t != root->node) || context->is_open_hold || next != context->next)
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
            else if (context->current_t != _t)
            {
                context->is_complete = false;
                ++context->version;
                context->current_t = _t;
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
            if (restarted || root != this || (context->current_t == '\0' || _t != root->node) || !context->is_open_hold || next != context->next || _hold != root->hold || !!_hold_lock != root->is_hold_lock)
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
            else if (context->current_t != _t)
            {
                context->is_complete = false;
                ++context->version;
                context->current_t = _t;
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
            using TetrisCallEvalT = TetrisCallEval<TetrisAI, Spec>;
            TetrisTreeNode *child = context->alloc(this);
            child->map = map;
            child->identity = lp;
            // 纯位板 attach：直接从 State 写方块、消行、更新 top/roof.
            // 不再需要 TetrisNode* 或 TetrisContext::get() 反查.
            size_t clear = m_tetris2::Helpers<Spec>::attach_to_map(lp.state, child->map);
            auto after_board = m_tetris2::build_board_for_search<Spec>(child->map);
            child->result = TetrisCallEvalT::template call_eval_typed<T, R>(
                *context->ai, child->identity, after_board, src_board, clear);
            return child;
        }

        void search(Context *context, char search_t, bool is_hold)
        {
            using SearchSpec = typename SearchRuleSpecOf<TetrisSearch>::type;
            auto board = m_tetris2::build_board_for_search<SearchSpec>(map);
            auto spawn = start_state(context, search_t);
            if (node_flag.empty())
            {
                node_flag.set(search_t);
                auto fresh_cb = [&]<char T, std::uint8_t R>(LandPoint const &land_point_node)
                {
                    TetrisTreeNode *child = this->template alloc_eval_typed<T, R>(context, map, board, land_point_node);
                    child->is_hold = is_hold;
                    child->children_next = children;
                    children = child;
                };
                context->search->search_eval(board, spawn, level, fresh_cb);
            }
            else if (!node_flag.check(search_t))
            {
                node_flag.set(search_t);
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
                    if (find != old.end())
                    {
                        child = find->second;
                        old.erase(find);
                    }
                    else
                    {
                        child = this->template alloc_eval_typed<T, R>(context, map, board, land_point_node);
                    }
                    child->is_hold = is_hold;
                    child->children_next = children;
                    children = child;
                };
                context->search->search_eval(board, spawn, level, rescan_cb);
                for (auto &pair : old)
                {
                    context->dealloc(pair.second);
                }
                old.clear();
            }
        }
        void search(Context *context, char search_t, char hold_t)
        {
            using SearchSpec = typename SearchRuleSpecOf<TetrisSearch>::type;
            auto board = m_tetris2::build_board_for_search<SearchSpec>(map);
            auto search_spawn = start_state(context, search_t);
            auto hold_spawn = hold_t != '\0' ? start_state(context, hold_t) : m_tetris2::State{};
            if (search_t == hold_t)
            {
                return search(context, search_t, false);
            }
            else
            {
                if (node_flag.empty())
                {
                    node_flag.set(search_t, hold_t);
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
                else if (!node_flag.check(search_t, hold_t))
                {
                    if (node_flag.check(hold_t, search_t))
                    {
                        node_flag.set(search_t, hold_t);
                        // 用 search_eval + probe 判断 search_t 是否有任何落点，
                        // 避免向 search() 传 State（其签名接收 TetrisBlockStatus）。
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
                            for (auto it = children; it != nullptr; it = it->children_next)
                            {
                                context->dealloc(it);
                            }
                            children = nullptr;
                        }
                    }
                    else
                    {
                        node_flag.set(search_t, hold_t);
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
                        context->search->search_eval(board, search_spawn, level, rescan_main_cb);
                        // hold_node 落点。
                        auto rescan_hold_cb = [&]<char T, std::uint8_t R>(LandPoint const &land_point_node)
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
                            child->is_hold = true;
                            child->children_next = children;
                            children = child;
                        };
                        context->search->search_eval(board, hold_spawn, level, rescan_hold_cb);
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
        // Nodes of one depth with equal keys reach the same board, hold, queue position and AI state, hence the same
        // future. Keys are compared by hash alone.
        std::uint64_t transposition_key(Context const *context) const
        {
            auto const queue = is_hold && parent->hold == ' ' ? std::next(parent->next) : parent->next;
            std::uint64_t key = status.get_raw().transposition_key();
            for (auto lane : map.data.data)
            {
                key = hash_combine(key, lane);
            }
            key = hash_combine(key, static_cast<unsigned char>(is_hold ? parent->node : parent->hold));
            return hash_combine(key, static_cast<std::uint64_t>(std::distance(context->next.cbegin(), queue)));
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
                    if constexpr (kTranspositions)
                    {
                        // Only the first node of a position popped at this depth is expanded; later copies are dropped,
                        // even better-valued ones a later widening produces.
                        if (!context->expanded_keys[next_length + 1].insert(child->transposition_key(context)).second)
                        {
                            return;
                        }
                    }
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
            if (prepared_ && width_ == width && height_ == height)
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
            return 0;
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
            return (local_context_.node_storage->size() - local_context_.free_count) * sizeof(TreeNode);
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
        auto ai_config()
        {
            return local_context_.ai_config();
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
        // Positions scored (AI::get calls) since the beam last restarted.
        size_t scored() const
        {
            return local_context_.scored;
        }
        // The beam has expanded every node it holds; widening adds nothing more.
        bool complete() const
        {
            return local_context_.is_complete;
        }
        bool run()
        {
            return widen<false>();
        }
        bool run_hold()
        {
            return widen<true>();
        }
        //run!
        RunResult run(map_t const &map, TetrisBlockStatus const &status, char const *next, size_t next_length, time_t limit = 100)
        {
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
        //带hold的run!
        RunResult run_hold(map_t const &map, TetrisBlockStatus const &status, char hold, bool hold_free, char const *next, size_t next_length, time_t limit = 100)
        {
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
        ValueHolder<typename std::decay<decltype(*Engine().ai_config())>::type, void> ai_config_;
        ValueHolder<typename std::decay<decltype(*Engine().search_config())>::type, void> search_config_;
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
            ai_config_.assign(engine_.ai_config());
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
        auto ai_config()
        {
            return ai_config_.get();
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
