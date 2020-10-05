
#pragma once

#include <algorithm>
#include <cassert>
#include <cstring>
#include <deque>
#include <functional>
#include <iterator>
#include <map>
#include <queue>
#include <string>
#include <vector>
#include <chrono>
#include <thread>
#include <condition_variable>

#include "chash_map.h"
#include "chash_set.h"

namespace m_tetris
{
    const int max_height = 40;
    const int max_wall_kick = 16;

    struct TetrisNode;
    struct TetrisWallKickOpertion;
    struct TetrisOpertion;
    struct TetrisMap;
    union TetrisBlockStatus;
    class TetrisContext;

    //游戏场景,下标从0开始,左下角为原点,最大支持[高度=40,宽度=32]
    struct TetrisMap
    {
        //行数据,具体用法看full函数吧...
        uint32_t row[max_height];
        //每一列的高度
        int32_t top[32];
        //场景宽
        int32_t width;
        //场景高
        int32_t height;
        //场景目前最大高度
        int32_t roof;
        //场景的方块数
        int32_t count;
        //判定[x,y]坐标是否有方块
        inline bool full(size_t x, size_t y) const
        {
            return (row[y] >> x) & 1;
        }
        TetrisMap()
        {
        }
        TetrisMap(int32_t w, int32_t h)
        {
            std::memset(this, 0, sizeof *this);
            width = w;
            height = h;
        }
        TetrisMap(TetrisMap const &other)
        {
            std::memcpy(this, &other, sizeof *this);
        }
        TetrisMap &operator = (TetrisMap const &other)
        {
            if (this != &other)
            {
                std::memcpy(this, &other, sizeof *this);
            }
            return *this;
        }
        bool operator == (TetrisMap const &other)
        {
            return std::memcmp(this, &other, sizeof *this) == 0;
        }
        bool operator != (TetrisMap const &other)
        {
            return std::memcmp(this, &other, sizeof *this) != 0;
        }
    };

    struct TetrisNodeBlockLocate
    {
        uint32_t count;
        struct
        {
            uint32_t x, y;
        } data[16];
        TetrisNodeBlockLocate()
        {
            count = 0;
        }
    };

    struct TetrisMapSnap
    {
        uint32_t row[4][max_height];
        TetrisMapSnap()
        {
            std::memset(row, 0, sizeof row);
        }
    };

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

    //踢墙表
    struct TetrisWallKickOpertion
    {
        struct WallKickNode
        {
            int8_t x, y;
        };
        uint32_t length;
        WallKickNode data[max_wall_kick];
    };

    //方块操作
    struct TetrisOpertion
    {
        //创建一个方块
        TetrisNode(*create)(size_t w, size_t h, TetrisOpertion const &op);
        //顺时针旋转(右旋)
        bool(*rotate_clockwise)(TetrisNode &node, TetrisContext const *context);
        //逆时针旋转(左旋)
        bool(*rotate_counterclockwise)(TetrisNode &node, TetrisContext const *context);
        //转动180°
        bool(*rotate_opposite)(TetrisNode &node, TetrisContext const *context);
        //顺时针旋转踢墙
        TetrisWallKickOpertion wall_kick_clockwise;
        //逆时针旋转踢墙
        TetrisWallKickOpertion wall_kick_counterclockwise;
        //转动180°踢墙
        TetrisWallKickOpertion wall_kick_opposite;
    };

    //指针网节点
    struct TetrisNode
    {
        //方块状态
        TetrisBlockStatus status;
        //方块操作函数
        TetrisOpertion op;
        //方块每行的数据
        uint32_t data[4];
        //方块每列的上沿高度
        int32_t top[4];
        //方块每列的下沿高度
        int32_t bottom[4];
        //方块在场景中的矩形位置
        int32_t row, height, col, width;
        //各种变形会触及到的最低高度
        int32_t low;

        //指针网索引
        //用于取代哈希表的hash

        size_t index;
        size_t index_filtered;

        //用于落点搜索优化
        std::vector<TetrisNode const *> const *land_point;

        //以下是指针网的数据
        //对应操作所造成的数据改变全都预置好,不需要再计算
        //如果为空,表示已经到达场景边界或者不支持该操作

        TetrisNode const *move_left;
        TetrisNode const *move_right;
        TetrisNode const *move_down;
        TetrisNode const *move_up;

        //踢墙序列,依次尝试
        //遇到nullptr,表示序列结束
        union
        {
            TetrisNode const *rotate_clockwise;
            TetrisNode const *wall_kick_clockwise[max_wall_kick];
        };
        union
        {
            TetrisNode const *rotate_counterclockwise;
            TetrisNode const *wall_kick_counterclockwise[max_wall_kick];
        };
        union
        {
            TetrisNode const *rotate_opposite;
            TetrisNode const *wall_kick_opposite[max_wall_kick];
        };

        TetrisNode const *move_down_multi[max_height];

        //检查当前块是否能够合并入场景
        bool check(TetrisMap const &map) const;
        //检查当前块是否能够合并入场景
        bool check(TetrisMapSnap const &snap) const;
        //构建场景快照
        void build_snap(TetrisMap const &map, TetrisContext const *context, TetrisMapSnap &snap) const;
        //检查当前块是否是露天的
        bool open(TetrisMap const &map) const;
        //当前块合并入场景,同时更新场景数据
        size_t attach(TetrisContext const *context, TetrisMap &map) const;
        //探测合并后消的最低行
        int clear_low(TetrisContext const *context, TetrisMap &map) const;
        //探测合并后消的最低行
        int clear_high(TetrisContext const *context, TetrisMap &map) const;
        //计算当前块软降位置
        TetrisNode const *drop(TetrisMap const &map) const;
    };

    //节点标记.广搜的时候使用
    class TetrisNodeMark
    {
    private:
        struct Mark
        {
            Mark() : version(0)
            {
            }
            size_t version;
            std::pair<TetrisNode const *, char> data;
        };
        size_t version_;
        std::vector<Mark> data_;

    public:
        void init(size_t size);
        void clear();
        std::pair<TetrisNode const *, char> get(size_t index);
        std::pair<TetrisNode const *, char> get(TetrisNode const *key);
        bool set(TetrisNode const *key, TetrisNode const *node, char op);
        bool cover_if(TetrisNode const *key, TetrisNode const *node, char ck, char op);
        bool mark(TetrisNode const *key);
    };

    //节点标记.过滤了位置相同的节点
    class TetrisNodeMarkFiltered
    {
    private:
        struct Mark
        {
            Mark() : version(0)
            {
            }
            size_t version;
            std::pair<TetrisNode const *, char> data;
        };
        size_t version_;
        std::vector<Mark> data_;

    public:
        void init(size_t size);
        void clear();
        std::pair<TetrisNode const *, char> get(size_t index);
        std::pair<TetrisNode const *, char> get(TetrisNode const *key);
        bool set(TetrisNode const *key, TetrisNode const *node, char op);
        bool cover_if(TetrisNode const *key, TetrisNode const *node, char ck, char op);
        bool mark(TetrisNode const *key);
    };

    template<class TetrisRule, class AI, class Search>
    struct TetrisContextBuilder;

    //上下文对象.场景大小改变了需要重新初始化上下文
    class TetrisContext
    {
        template<class TetrisRule, class AI, class Search>
        friend class TetrisEngine;
    private:
        TetrisContext()
        {
        }
        //指针网数据
        std::deque<TetrisNode> node_storage_;
        chash_map<TetrisBlockStatus, TetrisNode *, TetrisBlockStatusHash, TetrisBlockStatusEqual> node_index_;

        //方块偏移数据
        std::vector<TetrisNodeBlockLocate> node_block_;

        //规则信息
        std::map<std::pair<char, unsigned char>, TetrisOpertion> opertion_;
        std::map<char, TetrisBlockStatus(*)(TetrisContext const *)> generate_;

        //宽,高什么的...
        int32_t width_, height_;
        //满行
        uint32_t full_;

        //一些用于加速的数据...
        std::map<char, std::vector<TetrisNode const *>> place_cache_;
        size_t type_max_;
        TetrisNode const *generate_cache_[256];
        char index_to_type_[256];
        size_t type_to_index_[256];

    public:
        struct Env
        {
            char const *next;
            size_t length;
            char node;
            char hold;
            bool is_hold;
        };
        //初始化
        bool prepare(int32_t width, int32_t height);

        int32_t width() const;
        int32_t height() const;
        uint32_t full() const;
        size_t type_max() const;
        size_t node_max() const;
        size_t convert(char type) const;
        char convert(size_t index) const;
        TetrisOpertion get_opertion(char t, unsigned char r) const;
        TetrisNodeBlockLocate const *get_block(char t, unsigned char r) const;
        TetrisNode const *get(TetrisBlockStatus const &status) const;
        TetrisNode const *get(char t, int8_t x, int8_t y, uint8_t r) const;
        TetrisNode const *generate(char type) const;
        TetrisNode const *generate(size_t index) const;
        TetrisNode const *generate() const;
        bool create(TetrisBlockStatus const &status, TetrisNode &node) const;
    };

    template<class TetrisAI>
    struct TetrisAIInfo
    {
    private:
        template <typename T>
        struct function_traits_eval : public function_traits_eval<decltype(&T::eval)>
        {
        };
        template <typename ClassType, typename ReturnType, typename... Args>
        struct function_traits_eval<ReturnType(ClassType::*)(Args...) const>
        {
            typedef ReturnType result_type;
        };
        template <typename T>
        struct function_traits_get : public function_traits_get<decltype(&T::get)>
        {
        };
        template <typename ClassType, typename ReturnType, typename... Args>
        struct function_traits_get<ReturnType(ClassType::*)(Args...) const>
        {
            enum
            {
                arity = sizeof...(Args)
            };
            typedef ReturnType result_type;
        };
    public:
        typedef typename function_traits_eval<TetrisAI>::result_type Result;
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
            CallInit(CallType &type, Params const &... params)
            {
            }
        };
        template<class CallType>
        struct CallInit<CallType, std::true_type>
        {
            template<class... Params>
            CallInit(CallType &type, Params const &... params)
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
        template<typename U, U> struct Check;
        template<typename U> static std::false_type func(Check<int Fallback::*, &U::init> *);
        template<typename U> static std::true_type func(...);
    public:
        template<class... Params>
        TetrisCallInit(Type &type, Params const &... params)
        {
            CallInit<Type, decltype(func<Derived>(nullptr))>(type, params...);
        }
    };

    template<class AI, class Node>
    struct TetrisCallAI
    {
        template <typename T>
        struct eval_function_traits : public eval_function_traits<decltype(&AI::eval)>
        {
        };
        template <typename ClassType, typename ReturnType, typename... Args>
        struct eval_function_traits<ReturnType(ClassType::*)(Args...) const>
        {
            enum
            {
                arity = sizeof...(Args)
            };

            typedef ReturnType result_type;
            template<unsigned int i>
            struct arg
            {
                typedef typename std::tuple_element<i, std::tuple<Args...>>::type type;
            };
        };
        typedef typename eval_function_traits<AI>::template arg<0u>::type EvalOtherNode;
        typedef typename eval_function_traits<AI>::result_type eval_result_type;

        template <typename T>
        struct get_function_traits : public get_function_traits<decltype(&AI::get)>
        {
        };
        template <typename ClassType, typename ReturnType, typename... Args>
        struct get_function_traits<ReturnType(ClassType::*)(Args...) const>
        {
            enum
            {
                arity = sizeof...(Args)
            };

            typedef ReturnType result_type;
            template<unsigned int i>
            struct arg
            {
                typedef typename std::tuple_element<i, std::tuple<Args...>>::type type;
            };
        };
        typedef typename get_function_traits<AI>::template arg<0u>::type GetOtherNode;
        typedef typename get_function_traits<AI>::result_type get_result_type;

        template<class CallAI, class A, class B>
        struct CallEval
        {
            template<class Return, class TetrisNodeEx, class... Params>
            static Return eval(CallAI const &ai, TetrisNode const *node, Params const &... params)
            {
                typename std::remove_reference<EvalOtherNode>::type node_ex(node);
                return ai.eval(node_ex, params...);
            }
        };
        template<class CallAI, class T>
        struct CallEval<CallAI, T, T>
        {
            template<class Return, class TetrisNodeEx, class... Params>
            static Return eval(CallAI const &ai, TetrisNodeEx &node, Params const &... params)
            {
                return ai.eval(node, params...);
            }
        };
        template<class CallAI, class A, class B>
        struct CallGet
        {
            template<class Return, class TetrisNodeEx, class... Params>
            static Return get(CallAI const &ai, TetrisNode const *node, Params const &... params)
            {
                typename std::remove_reference<EvalOtherNode>::type node_ex(node);
                return ai.get(node_ex, params...);
            }
        };
        template<class CallAI, class T>
        struct CallGet<CallAI, T, T>
        {
            template<class Return, class TetrisNodeEx, class... Params>
            static Return get(CallAI const &ai, TetrisNodeEx &node, Params const &... params)
            {
                return ai.get(node, params...);
            }
        };
    public:
        template<class... Params>
        static auto eval(AI const &ai, Node &node, Params const &... params)->eval_result_type
        {
            typedef typename std::remove_reference<typename std::remove_const<Node>::type>::type NodeLeft;
            typedef typename std::remove_reference<typename std::remove_const<EvalOtherNode>::type>::type NodeRight;
            return CallEval<AI, NodeLeft, NodeRight>::template eval<eval_result_type, Node>(ai, node, params...);
        }
        template<class... Params>
        static auto get(AI const &ai, Node &node, Params const &... params)->get_result_type
        {
            typedef typename std::remove_reference<typename std::remove_const<Node>::type>::type NodeLeft;
            typedef typename std::remove_reference<typename std::remove_const<GetOtherNode>::type>::type NodeRight;
            return CallGet<AI, NodeLeft, NodeRight>::template get<get_result_type, Node>(ai, node, params...);
        }
    };

    template<class Rule>
    struct TetrisRuleInit
    {
        template<class CallRule, class T>
        struct RuleInit
        {
            static bool init(int w, int h)
            {
                return true;
            }
        };
        template<class CallRule>
        struct RuleInit<CallRule, std::true_type>
        {
            static bool init(int w, int h)
            {
                return CallRule::init(w, h);
            }
        };
        struct Fallback
        {
            int init;
        };
        struct Derived : Rule, Fallback
        {
        };
        template<typename U, U> struct Check;
        template<typename U> static std::false_type func(Check<int Fallback::*, &U::init> *);
        template<typename U> static std::true_type func(...);
    public:
        static bool init(int w, int h)
        {
            return RuleInit<Rule, decltype(func<Derived>(nullptr))>::init(w, h);
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
        template<typename U, U> struct Check;
        template<typename U> static std::false_type func(Check<int Fallback::*, &U::ratio> *);
        template<typename U> static std::true_type func(...);
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
        template<typename U, U> struct Check;
        template<typename U> static std::false_type func(Check<int Fallback::*, &U::iterate> *);
        template<typename U> static std::true_type func(...);
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
        template<typename U, U> struct Check;
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
            static void call(TetrisAI &ai, LocalContext<TreeNode> const *local_context, TetrisContext const *shared_context)
            {
                TetrisCallInit<TetrisAI>(ai, shared_context, local_context->ai_config());
            }
            static void call(TetrisSearch &search, LocalContext<TreeNode> const *local_context, TetrisContext const *shared_context)
            {
                TetrisCallInit<TetrisSearch>(search, shared_context, local_context->search_config());
            }
        };
        template<class TreeNode>
        struct CallInit<TreeNode, void>
        {
            static void call(TetrisAI &ai, LocalContext<TreeNode> const *local_context, TetrisContext const *shared_context)
            {
                TetrisCallInit<TetrisAI>(ai, shared_context);
            }
            static void call(TetrisSearch &search, LocalContext<TreeNode> const *local_context, TetrisContext const *shared_context)
            {
                TetrisCallInit<TetrisSearch>(search, shared_context);
            }
        };
    public:
        template<class TreeNode>
        static void init_ai(TetrisAI &ai, LocalContext<TreeNode> const *local_context, TetrisContext const *shared_context)
        {
            CallInit<TreeNode, typename LocalContext<TreeNode>::AIConfigType>::call(ai, local_context, shared_context);
        }
        template<class TreeNode>
        static void init_search(TetrisSearch &search, LocalContext<TreeNode> const *local_context, TetrisContext const *shared_context)
        {
            CallInit<TreeNode, typename LocalContext<TreeNode>::SearchConfigType>::call(search, local_context, shared_context);
        }
    };

    template<class TetrisAI, class TetrisSearch>
    struct TetrisCore
    {
    private:
        template <typename TemplateElement>
        struct element_traits
        {
            typedef void Element;
        };
        template <typename TemplateElement>
        struct element_traits<std::vector<TemplateElement> const *>
        {
            typedef TemplateElement Element;
        };

    public:
        typedef typename element_traits<decltype(TetrisSearch().search(TetrisMap(), nullptr, 0))>::Element LandPoint;
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
                node->status.set(TetrisCallAI<TetrisAI, LandPoint>::get(*context->ai, node->identity, node->result, parent->level, parent->status.get_raw(), parent->template env<EnableEnv>(context, node)));
            }
        };
        template<class TreeNode, bool EnableEnv>
        struct TetrisSelectGet<TreeNode, EnableEnv, 4>
        {
            typedef std::false_type enable_next_c;
            static void get(typename TreeNode::Context *context, TreeNode *node, TreeNode *parent)
            {
                node->status.set(TetrisCallAI<TetrisAI, LandPoint>::get(*context->ai, node->identity, node->result, parent->level, parent->status.get_raw()));
            }
        };
        template<class TreeNode, bool EnableEnv>
        struct TetrisSelectGet<TreeNode, EnableEnv, 3>
        {
            typedef std::false_type enable_next_c;
            static void get(typename TreeNode::Context *context, TreeNode *node, TreeNode *parent)
            {
                node->status.set(TetrisCallAI<TetrisAI, LandPoint>::get(*context->ai, node->identity, node->result, parent->level));
            }
        };
        template<class TreeNode, bool EnableEnv>
        struct TetrisSelectGet<TreeNode, EnableEnv, 2>
        {
            typedef std::false_type enable_next_c;
            static void get(typename TreeNode::Context *context, TreeNode *node, TreeNode *parent)
            {
                node->status.set(TetrisCallAI<TetrisAI, LandPoint>::get(*context->ai, node->identity, node->result));
            }
        };
    public:
        template<class TreeNode>
        using EnableNextC = typename TetrisSelectGet<TreeNode, false, TetrisAIInfo<TetrisAI>::arity>::enable_next_c;

        template<class TreeNode>
        static void eval(typename TreeNode::Context *context, TetrisMap &map, LandPoint &node, TreeNode *tree_node)
        {
            TetrisMap &new_map = tree_node->map;
            new_map = map;
            tree_node->identity = node;
            size_t clear = node->attach(context->engine, new_map);
            tree_node->result = TetrisCallAI<TetrisAI, LandPoint>::eval(*context->ai, tree_node->identity, new_map, map, clear);
        }
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
                bool operator == (TetrisNext const &other) const
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
                bool operator == (TetrisNext const &other) const
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
            Context(std::deque<TetrisTreeNode> *_node_storage) : version(), is_complete(), is_open_hold(), node_storage(_node_storage), free_list(nullptr), width(), total(), avg()
            {
            }
        public:
            typedef std::priority_queue<TetrisTreeNode *, std::vector<TetrisTreeNode *>, ValueHeapCompare> value_heap_t;
            typedef chash_map<TetrisBlockStatus, TetrisTreeNode *, TetrisBlockStatusHash, TetrisBlockStatusEqual> children_map_t;
            typedef chash_set<TetrisBlockStatus, TetrisBlockStatusHash, TetrisBlockStatusEqual> identity_set_t;
            size_t version;
            TetrisContext const *engine;
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
            std::deque<TetrisTreeNode> *node_storage;
            TetrisTreeNode* free_list;
            std::vector<Status const *> iterate_cache;
            TetrisNode virtual_flag;
            TetrisNode const *current;
            std::vector<next_t> next;
            std::vector<char> next_c;
            std::vector<double> width_cache;
            double total;
            double avg;
        public:
            TetrisTreeNode *alloc(TetrisTreeNode *parent)
            {
                TetrisTreeNode *node;
                if (free_list != nullptr)
                {
                    node = free_list;
                    free_list = free_list->parent;
                    node->version = version - 1;
                }
                else
                {
                    node_storage->emplace_back();
                    node = &node_storage->back();
                }
                node->parent = parent;
                return node;
            }
            void dealloc(TetrisTreeNode *node)
            {
                for (auto it = node->children; it != nullptr; it = it->children_next)
                {
                    dealloc(it);
                }
                node->children = nullptr;
                node->node_flag.clear();
                node->node = ' ';
                node->hold = ' ';
                node->level = 1;
                node->flag = 0;
                node->parent = free_list;
                free_list = node;
            }
        };
        struct TetrisNodeFlag
        {
            TetrisNode const *flag[2];
            TetrisNodeFlag()
            {
                clear();
            }
            bool empty()
            {
                return flag[0] == nullptr;
            }
            bool check(TetrisNode const *node1)
            {
                assert(node1 != nullptr);
                return flag[0] == node1 && flag[1] == nullptr;
            }
            bool check(TetrisNode const *node1, TetrisNode const *node2)
            {
                assert(node1 != nullptr);
                assert(node2 != nullptr);
                return flag[0] == node1 && flag[1] == node2;
            }
            void set(TetrisNode const *node1)
            {
                assert(node1 != nullptr);
                flag[0] = node1;
                flag[1] = nullptr;
            }
            void set(TetrisNode const *node1, TetrisNode const *node2)
            {
                assert(node1 != nullptr);
                assert(node2 != nullptr);
                flag[0] = node1;
                flag[1] = node2;
            }
            void clear()
            {
                flag[0] = nullptr;
                flag[1] = nullptr;
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
                char node;
                char hold;
                uint8_t level;
                uint8_t flag;
            };
            struct
            {
                char : 8;
                char : 8;
                uint8_t : 8;
                          uint8_t is_dead : 1;
                          uint8_t is_hold : 1;
                          uint8_t is_hold_lock : 1;
                          uint8_t is_virtual : 1;
            };
        };
        size_t version;
        TetrisMap map;
        typename Core::LandPoint identity;
        typename Core::Result result;
        TreeNodeStatus<TetrisAI, typename TetrisAIHasIterate<TetrisAI>::type> status;
        TetrisTreeNode *parent;
        TetrisTreeNode *children;
        TetrisTreeNode *children_next;
        TetrisNodeFlag node_flag;
        typename std::vector<next_t>::const_iterator next;

        TetrisTreeNode *update_root(Context *context, TetrisMap const &_map)
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
        void update_version(Context *context)
        {
            ++context->version;
            context->total += context->width;
            context->avg = context->total / context->version;
            context->width = 0;
            context->wait.clear();
            context->sort.clear();
            context->wait.resize(context->max_length + 1);
            context->sort.resize(context->max_length + 1);
        }
        static std::vector<next_t> process_next(char const *_next, size_t _next_length, TetrisNode const *_node)
        {
            std::vector<next_t> next;
            next.push_back(_node->status.t);
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
        TetrisTreeNode *update(Context *context, TetrisMap const &_map, Status const &status, TetrisNode const *_node, char const *_next, size_t _next_length)
        {
            TetrisTreeNode *root = update_root(context, _map);
            std::vector<next_t> next = process_next(_next, _next_length, _node);
            if (root != this || (context->current == nullptr || _node->status.t != root->node) || context->is_open_hold || next != context->next)
            {
                context->is_complete = false;
                context->max_length = next.size() - 1;
                update_version(context);
                context->is_open_hold = false;
                context->current = _node;
                context->next = next;
                if (Core::template EnableNextC<TetrisTreeNode>::value)
                {
                    context->next_c.assign(next.begin(), next.end());
                }
                root->node = _node->status.t;
                root->next = std::next(context->next.begin());
            }
            else if (context->current != _node)
            {
                context->is_complete = false;
                ++context->version;
                context->current = _node;
            }
            root->status.set(status);
            context->width_cache.clear();
            return root;
        }
        TetrisTreeNode *update(Context *context, TetrisMap const &_map, Status const &status, TetrisNode const *_node, char _hold, bool _hold_lock, char const *_next, size_t _next_length)
        {
            TetrisTreeNode *root = update_root(context, _map);
            std::vector<next_t> next = process_next(_next, _next_length, _node);
            if (root != this || (context->current == nullptr || _node->status.t != root->node) || !context->is_open_hold || next != context->next || _hold != root->hold || !!_hold_lock != root->is_hold_lock)
            {
                context->is_complete = false;
                context->max_length = next.size() - 1;
                if (_hold != ' ' && (_next_length > 1 || !_hold_lock))
                {
                    ++context->max_length;
                }
                update_version(context);
                context->is_open_hold = true;
                context->current = _node;
                context->next = next;
                if (Core::template EnableNextC<TetrisTreeNode>::value)
                {
                    context->next_c.assign(next.begin(), next.end());
                }
                root->node = _node->status.t;
                root->hold = _hold;
                root->is_hold = false;
                root->is_hold_lock = _hold_lock;
                root->next = std::next(context->next.begin());
            }
            else if (context->current != _node)
            {
                context->is_complete = false;
                ++context->version;
                context->current = _node;
            }
            root->status.set(status);
            context->width_cache.clear();
            return root;
        }
        void search(Context *context, TetrisNode const *search_node, bool is_hold)
        {
            if (node_flag.empty())
            {
                node_flag.set(search_node);
                for (auto land_point_node : *context->search->search(map, search_node, level))
                {
                    TetrisTreeNode *child = context->alloc(this);
                    Core::eval(context, map, land_point_node, child);
                    child->is_hold = is_hold;
                    child->children_next = children;
                    children = child;
                }
            }
            else if (!node_flag.check(search_node))
            {
                node_flag.set(search_node);
                auto &old = context->old;
                for (auto it = children; it != nullptr; it = it->children_next)
                {
                    old.emplace(it->identity->status, it);
                }
                children = nullptr;
                for (auto land_point_node : *context->search->search(map, search_node, level))
                {
                    TetrisTreeNode *child;
                    auto find = old.find(land_point_node->status);
                    if (find != old.end())
                    {
                        child = find->second;
                        old.erase(find);
                    }
                    else
                    {
                        child = context->alloc(this);
                        Core::eval(context, map, land_point_node, child);
                    }
                    child->is_hold = is_hold;
                    child->children_next = children;
                    children = child;
                }
                for (auto &pair : old)
                {
                    context->dealloc(pair.second);
                }
                old.clear();
            }
        }
        void search(Context *context, TetrisNode const *search_node, TetrisNode const *hold_node)
        {
            if (search_node == hold_node)
            {
                return search(context, search_node, false);
            }
            if (search_node->status.t == hold_node->status.t)
            {
                if (node_flag.empty())
                {
                    node_flag.set(search_node, hold_node);
                    auto &uniq = context->uniq;
                    for (auto land_point_node : *context->search->search(map, search_node, level))
                    {
                        TetrisTreeNode *child = context->alloc(this);
                        Core::eval(context, map, land_point_node, child);
                        child->is_hold = false;
                        child->children_next = children;
                        children = child;
                        uniq.insert(child->identity->status);
                    }
                    if (children != nullptr)
                    {
                        for (auto land_point_node : *context->search->search(map, hold_node, level))
                        {
                            if (uniq.find(land_point_node->status) != uniq.end())
                            {
                                continue;
                            }
                            TetrisTreeNode *child = context->alloc(this);
                            Core::eval(context, map, land_point_node, child);
                            child->is_hold = true;
                            child->children_next = children;
                            children = child;
                        }
                    }
                    uniq.clear();
                }
                else if (!node_flag.check(search_node, hold_node))
                {
                    auto &old = context->old;
                    for (auto it = children; it != nullptr; it = it->children_next)
                    {
                        old.emplace(it->identity->status, it);
                    }
                    children = nullptr;
                    if (node_flag.check(hold_node, search_node))
                    {
                        node_flag.set(search_node, hold_node);
                        for (auto land_point_node : *context->search->search(map, search_node, level))
                        {
                            auto find = old.find(land_point_node->status);
                            assert(find != old.end());
                            TetrisTreeNode *child = find->second;
                            old.erase(find);
                            child->is_hold = false;
                            child->children_next = children;
                            children = child;
                        }
                        if (children != nullptr)
                        {
                            for (auto &pair : old)
                            {
                                auto child = pair.second;
                                child->is_hold = true;
                                child->children_next = children;
                                children = child;
                            }
                        }
                        else
                        {
                            for (auto &pair : old)
                            {
                                context->dealloc(pair.second);
                            }
                        }
                    }
                    else
                    {
                        node_flag.set(search_node, hold_node);
                        auto &uniq = context->uniq;
                        for (auto land_point_node : *context->search->search(map, search_node, level))
                        {
                            TetrisTreeNode *child;
                            auto find = old.find(land_point_node->status);
                            if (find != old.end())
                            {
                                child = find->second;
                                old.erase(find);
                            }
                            else
                            {
                                child = context->alloc(this);
                                Core::eval(context, map, land_point_node, child);
                            }
                            child->is_hold = false;
                            child->children_next = children;
                            children = child;
                            uniq.insert(child->identity->status);
                        }
                        if (children != nullptr)
                        {
                            for (auto land_point_node : *context->search->search(map, hold_node, level))
                            {
                                if (uniq.find(land_point_node->status) != uniq.end())
                                {
                                    continue;
                                }
                                TetrisTreeNode *child;
                                auto find = old.find(land_point_node->status);
                                if (find != old.end())
                                {
                                    child = find->second;
                                    old.erase(find);
                                }
                                else
                                {
                                    child = context->alloc(this);
                                    Core::eval(context, map, land_point_node, child);
                                }
                                child->is_hold = true;
                                child->children_next = children;
                                children = child;
                            }
                        }
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
                    node_flag.set(search_node, hold_node);
                    for (auto land_point_node : *context->search->search(map, search_node, level))
                    {
                        TetrisTreeNode *child = context->alloc(this);
                        Core::eval(context, map, land_point_node, child);
                        child->is_hold = false;
                        child->children_next = children;
                        children = child;
                    }
                    if (children != nullptr)
                    {
                        for (auto land_point_node : *context->search->search(map, hold_node, level))
                        {
                            TetrisTreeNode *child = context->alloc(this);
                            Core::eval(context, map, land_point_node, child);
                            child->is_hold = true;
                            child->children_next = children;
                            children = child;
                        }
                    }
                }
                else if (!node_flag.check(search_node, hold_node))
                {
                    if (node_flag.check(hold_node, search_node))
                    {
                        node_flag.set(search_node, hold_node);
                        if (!context->search->search(map, search_node, level)->empty())
                        {
                            for (auto it = children; it != nullptr; it = it->children_next)
                            {
                                it->is_hold = it->identity->status.t == hold_node->status.t;
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
                        node_flag.set(search_node, hold_node);
                        auto &old = context->old;
                        for (auto it = children; it != nullptr; it = it->children_next)
                        {
                            old.emplace(it->identity->status, it);
                        }
                        children = nullptr;
                        for (auto land_point_node : *context->search->search(map, search_node, level))
                        {
                            TetrisTreeNode *child;
                            auto find = old.find(land_point_node->status);
                            if (find != old.end())
                            {
                                child = find->second;
                                old.erase(find);
                            }
                            else
                            {
                                child = context->alloc(this);
                                Core::eval(context, map, land_point_node, child);
                            }
                            child->is_hold = false;
                            child->children_next = children;
                            children = child;
                        }
                        for (auto land_point_node : *context->search->search(map, hold_node, level))
                        {
                            TetrisTreeNode *child;
                            auto find = old.find(land_point_node->status);
                            if (find != old.end())
                            {
                                child = find->second;
                                old.erase(find);
                            }
                            else
                            {
                                child = context->alloc(this);
                                Core::eval(context, map, land_point_node, child);
                            }
                            child->is_hold = true;
                            child->children_next = children;
                            children = child;
                        }
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
            if (node_flag.empty())
            {
                node_flag.set(&context->virtual_flag);
                size_t max = context->engine->type_max();
                for (size_t i = 0; i < max; ++i)
                {
                    for (auto land_point_node : *context->search->search(map, context->engine->generate(i), level))
                    {
                        TetrisTreeNode *child = context->alloc(this);
                        Core::eval(context, map, land_point_node, child);
                        child->is_hold = false;
                        child->children_next = children;
                        children = child;
                    }
                }
            }
            else if (node_flag.check(&context->virtual_flag))
            {
                node_flag.set(&context->virtual_flag);
                auto  &old = context->old;
                for (auto it = children; it != nullptr; it = it->children_next)
                {
                    old.emplace(it->identity->status, it);
                }
                children = nullptr;
                size_t max = context->engine->type_max();
                for (size_t i = 0; i < max; ++i)
                {
                    for (auto land_point_node : *context->search->search(map, context->engine->generate(i), level))
                    {
                        TetrisTreeNode *child;
                        auto find = old.find(land_point_node->status);
                        if (find != old.end())
                        {
                            child = find->second;
                            old.erase(find);
                        }
                        else
                        {
                            child = context->alloc(this);
                            Core::eval(context, map, land_point_node, child);
                        }
                        child->is_hold = false;
                        child->children_next = children;
                        children = child;
                    }
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
            search(context);
            auto *engine = context->engine;
            auto &iterate_cache = context->iterate_cache;
            iterate_cache.clear();
            iterate_cache.resize(context->engine->type_max(), nullptr);
            for (auto it = children; it != nullptr; it = it->children_next)
            {
                Core::template get<false>(context, it, this);
                auto &status = iterate_cache[engine->convert(it->identity->status.t)];
                if (status == nullptr || *status < it->status.get())
                {
                    status = &it->status.get();
                }
            }
            Core::iterate(*context->ai, iterate_cache.data(), iterate_cache.size(), this);
        }
        template<bool EnableEnv>
        TetrisContext::Env env(Context *context, TetrisTreeNode const *tree_node)
        {
            if (EnableEnv)
            {
                TetrisContext::Env result =
                {
                    nullptr, 0, tree_node->identity->status.t, tree_node->is_hold ? node : hold
                };
                result.length = std::distance(next, context->next.cend());
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
                return
                {
                    nullptr, 0, ' ', ' ', false
                };
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
            }
            if (TetrisAIHasIterate<TetrisAI>::type::value && context->next[level].get_vp())
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
                assert(context->current->status.t == node);
                level = 0;
                if (EnableHold)
                {
                    if (hold == ' ')
                    {
                        if (is_hold_lock || next == context->next.end())
                        {
                            search(context, context->current, false);
                        }
                        else
                        {
                            search(context, context->current, context->engine->generate(next->node));
                        }
                    }
                    else
                    {
                        if (is_hold_lock)
                        {
                            search(context, context->current, false);
                        }
                        else
                        {
                            search(context, context->current, context->engine->generate(hold));
                        }
                    }
                }
                else
                {
                    search(context, context->current, false);
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
                        search(context, context->engine->generate(node), false);
                    }
                    else
                    {
                        search(context, context->engine->generate(node), context->engine->generate(next->node));
                    }
                }
                else
                {
                    if (node == ' ')
                    {
                        search(context, context->engine->generate(hold), true);
                    }
                    else
                    {
                        search(context, context->engine->generate(node), context->engine->generate(hold));
                    }
                }
            }
            else
            {
                assert(parent->next != context->next.end());
                node = parent->next->node;
                next = std::next(parent->next);
                search(context, context->engine->generate(node), false);
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
            double ratio = Core::template get_ratio<TetrisTreeNode>(*context->ai);
            while (next_length > context->width_cache.size())
            {
                context->width_cache.emplace_back(std::pow(context->width_cache.size() + 2, ratio));
            }
            double div_ratio = *std::max_element(context->width_cache.begin(), context->width_cache.end()) / 2;
            while (next_length-- > 0)
            {
                size_t level_prune_hold = std::max<size_t>(1, size_t(context->width_cache[next_length] * context->width / div_ratio));
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

    template<class TetrisRule, class TetrisAI, class TetrisSearch>
    class TetrisEngine
    {
    public:
        typedef TetrisCore<TetrisAI, TetrisSearch> Core;
        typedef TetrisTreeNode<typename Core::Status, TetrisAI, TetrisSearch> TreeNode;
        typedef LocalContextBuilder<typename TreeNode::Context, TetrisRule, TetrisAI, TetrisSearch> ContextBuilder;
        typedef typename Core::LandPoint LandPoint;

    private:
        std::deque<TreeNode> node_storage_;
        std::shared_ptr<TetrisContext> shared_context_;
        typename ContextBuilder::template LocalContext<TreeNode> local_context_;
        TreeNode *root_;
        TetrisAI ai_;
        TetrisSearch search_;
        typename Core::Status status_;
        uint64_t memory_limit_;

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
            RunResult(std::pair<TreeNode const *, Status const *> const &_result) : target(_result.first ? _result.first->identity : nullptr), status(*_result.second), change_hold()
            {
            }
            RunResult(std::pair<TreeNode const *, Status const *> const &_result, bool _change_hold) : target(_result.first ? _result.first->identity : nullptr), status(*_result.second), change_hold(_change_hold)
            {
            }
            LandPoint target;
            Status status;
            bool change_hold;
        };

    public:
        TetrisEngine() : shared_context_(), local_context_(&node_storage_), ai_(), root_(nullptr), status_(), memory_limit_(128ull << 20)
        {
            node_storage_.emplace_back();
            root_ = &node_storage_.back();
        }
        TetrisEngine(std::shared_ptr<TetrisContext> context) : shared_context_(context), local_context_(&node_storage_), ai_(), root_(nullptr), status_(), memory_limit_(128ull << 20)
        {
            node_storage_.emplace_back();
            root_ = &node_storage_.back();
            local_context_.engine = shared_context_.get();
            local_context_.ai = &ai_;
            local_context_.search = &search_;
            if (TetrisRuleInit<TetrisRule>::init(shared_context_->width(), shared_context_->height()))
            {
                ContextBuilder::init_ai(ai_, &local_context_, shared_context_.get());
                ContextBuilder::init_search(search_, &local_context_, shared_context_.get());
            }
            else
            {
                shared_context_.reset();
            }
        }
        bool prepare(int width, int height)
        {
            if (shared_context_ != nullptr && shared_context_->width() == width && shared_context_->height() == height)
            {
                return true;
            }
            shared_context_.reset(new TetrisContext());
            shared_context_->opertion_ = TetrisRule::get_opertion();
            shared_context_->generate_ = TetrisRule::get_generate();
            if (!shared_context_->prepare(width, height))
            {
                shared_context_.reset();
                return false;
            }
            local_context_.engine = shared_context_.get();
            local_context_.ai = &ai_;
            local_context_.search = &search_;
            if (TetrisRuleInit<TetrisRule>::init(width, height))
            {
                ContextBuilder::init_ai(ai_, &local_context_, shared_context_.get());
                ContextBuilder::init_search(search_, &local_context_, shared_context_.get());
            }
            else
            {
                shared_context_.reset();
                return false;
            }
            return true;
        }
        //从状态获取当前块
        TetrisNode const *get(TetrisBlockStatus const &status) const
        {
            return shared_context_->get(status);
        }
        //上下文对象...
        std::shared_ptr<TetrisContext> const &context() const
        {
            return shared_context_;
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
            return shared_context_->node_storage_.size() * sizeof(TetrisNode) + local_context_.node_storage->size() * sizeof(TreeNode);
        }
        //AI名称
        std::string ai_name() const
        {
            return ai_.ai_name();
        }
        auto ai_config() const->decltype(local_context_.ai_config())
        {
            return local_context_.ai_config();
        }
        auto ai_config()->decltype(local_context_.ai_config())
        {
            return local_context_.ai_config();
        }
        auto search_config() const->decltype(local_context_.search_config())
        {
            return local_context_.search_config();
        }
        auto search_config()->decltype(local_context_.search_config())
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
            ++local_context_.version;
            local_context_.total += local_context_.width;
            local_context_.avg = local_context_.total / local_context_.version;
            local_context_.width = 0;
            local_context_.wait.clear();
            local_context_.sort.clear();
            local_context_.wait.resize(local_context_.max_length + 1);
            local_context_.sort.resize(local_context_.max_length + 1);
        }
        bool run()
        {
            if (root_->identity != nullptr && memory_usage() < memory_limit_)
            {
                return root_->template run<false>(&local_context_);
            }
            return true;
        }
        bool run_hold()
        {
            if (root_->identity != nullptr && memory_usage() < memory_limit_)
            {
                return root_->template run<true>(&local_context_);
            }
            return true;
        }
        //run!
        RunResult run(TetrisMap const &map, TetrisNode const *node, char const *next, size_t next_length, time_t limit = 100)
        {
            using namespace std::chrono;
            if (shared_context_ == nullptr || node == nullptr || !node->check(map))
            {
                return RunResult();
            }
            auto now = high_resolution_clock::now(), end = now + std::chrono::milliseconds(limit);
            root_ = root_->update(&local_context_, map, status_, node, next, next_length);
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
        RunResult run_hold(TetrisMap const &map, TetrisNode const *node, char hold, bool hold_free, char const *next, size_t next_length, time_t limit = 100)
        {
            using namespace std::chrono;
            if (shared_context_ == nullptr || node == nullptr || !node->check(map))
            {
                return RunResult();
            }
            auto now = high_resolution_clock::now(), end = now + std::chrono::milliseconds(limit);
            root_ = root_->update(&local_context_, map, status_, node, hold, !hold_free, next, next_length);
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
                return best.first != nullptr ? RunResult(best, best.first == nullptr ? false : best.first->is_hold) : RunResult(false);
            }
        }
        //根据run的结果得到一个操作路径
        std::vector<char> make_path(TetrisNode const *node, LandPoint const &land_point, TetrisMap const &map, bool cut_drop = true)
        {
            auto path = search_.make_path(node, land_point, map);
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
        void search(TetrisNode const *node, TetrisMap const &map, container_t &result)
        {
            auto const *land_point = search_.search(map, node);
            result.assign(land_point->begin(), land_point->end());
        }
    };

    template<class TetrisRule, class TetrisAI, class TetrisSearch>
    class TetrisThreadEngine
    {
    private:
        typedef TetrisEngine<TetrisRule, TetrisAI, TetrisSearch> Engine;

        template<class T, class U> struct ValueHolder
        {
            T value;
            T const *get() const
            {
                return &value;
            }
            T *get() {
                return &value;
            }
            void assign(T *ptr) const
            {
                *ptr = value;
            }
        };
        template<class U> struct ValueHolder<void, U>
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
        bool backgrond_;
        bool running_;

        struct PauseBackground {
            TetrisThreadEngine* self;

            PauseBackground(TetrisThreadEngine* _self) : self(_self)
            {
                self->backgrond_ = false;
                self->mutex_.lock();
            }

            ~PauseBackground()
            {
                self->backgrond_ = true;
                self->mutex_.unlock();
            }
        };

        void work_thread_func()
        {
            std::unique_lock<std::mutex> lock(mutex_);
            while (running_) {
                if (cv_.wait_for(lock, std::chrono::seconds(1), [&] {
                    return !running_ || (backgrond_ && std::chrono::high_resolution_clock::now() < stop_);
                }))
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
            if (!running_)
            {
                running_ = true;
                worker_ = std::move(std::thread(&TetrisThreadEngine::work_thread_func, this));
            }
            with_hold_ = with_hold;
            stop_ = std::chrono::high_resolution_clock::now() + std::chrono::seconds(10);
            cv_.notify_all();
        }

    public:
        typedef typename Engine::Status Status;
        typedef typename Engine::RunResult RunResult;

    public:
        TetrisThreadEngine() : engine_(), with_hold_(false), backgrond_(false), running_(false)
        {
        }
        TetrisThreadEngine(std::shared_ptr<TetrisContext> context) : engine_(context), with_hold_(false), backgrond_(false), running_(false)
        {
        }
        ~TetrisThreadEngine()
        {
            if (running_) {
                running_ = false;
                cv_.notify_all();
                worker_.join();
            }
        }
        bool prepare(int width, int height)
        {
            PauseBackground pause(this);
            return engine_.prepare(width, height);
        }
        //从状态获取当前块
        TetrisNode const *get(TetrisBlockStatus const &status) const
        {
            return engine_.get(status);
        }
        //上下文对象...
        std::shared_ptr<TetrisContext> const &context() const
        {
            return engine_.context();
        }
        uint64_t memory_limit() const
        {
            return engine_.memory_limit();
        }
        void memory_limit(uint64_t value)
        {
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
        auto ai_config() const->decltype(engine_.ai_config())
        {
            return ai_config_.get();
        }
        auto ai_config()->decltype(engine_.ai_config())
        {
            return ai_config_.get();
        }
        auto search_config() const->decltype(engine_.search_config())
        {
            return search_config_.get();
        }
        auto search_config()->decltype(engine_.search_config())
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
            engine_.update();
        }
        //run!
        RunResult run(TetrisMap const &map, TetrisNode const *node, char const *next, size_t next_length, time_t limit = 100)
        {
            PauseBackground pause(this);
            ai_config_.assign(engine_.ai_config());
            search_config_.assign(engine_.search_config());
            *engine_.status() = status_;
            auto run_result = engine_.run(map, node, next, next_length, limit);
            start_work(false);
            return run_result;
        }
        //带hold的run!
        RunResult run_hold(TetrisMap const &map, TetrisNode const *node, char hold, bool hold_free, char const *next, size_t next_length, time_t limit = 100)
        {
            PauseBackground pause(this);
            ai_config_.assign(engine_.ai_config());
            search_config_.assign(engine_.search_config());
            *engine_.status() = status_;
            auto run_result = engine_.run_hold(map, node, hold, hold_free, next, next_length, limit);
            start_work(true);
            return run_result;
        }
        //根据run的结果得到一个操作路径
        std::vector<char> make_path(TetrisNode const *node, typename Engine::LandPoint const &land_point, TetrisMap const &map, bool cut_drop = true)
        {
            PauseBackground pause(this);
            return engine_.make_path(node, land_point, map, cut_drop);
        }
        //单块评价
        template<class container_t>
        void search(TetrisNode const *node, TetrisMap const &map, container_t &result)
        {
            PauseBackground pause(this);
            engine_.search(node, map, result);
        }
    };


    inline bool TetrisNode::check(TetrisMap const &map) const
    {
        switch (height)
        {
        default:
            assert(0);
        case 4:
            return ((map.row[row] & data[0]) | (map.row[row + 1] & data[1]) | (map.row[row + 2] & data[2]) | (map.row[row + 3] & data[3])) == 0;
        case 3:
            return ((map.row[row] & data[0]) | (map.row[row + 1] & data[1]) | (map.row[row + 2] & data[2])) == 0;
        case 2:
            return ((map.row[row] & data[0]) | (map.row[row + 1] & data[1])) == 0;
        case 1:
            return ((map.row[row] & data[0])) == 0;
        }
    }

    inline bool TetrisNode::check(TetrisMapSnap const &snap) const
    {
        return ((snap.row[status.r][row] >> col) & 1) == 0;
    }


    inline bool TetrisNode::open(TetrisMap const &map) const
    {
        switch (width)
        {
        default:
            assert(0);
        case 4:
            return ((bottom[0] < map.top[col]) & (bottom[1] < map.top[col + 1]) & (bottom[2] < map.top[col + 2]) & (bottom[3] < map.top[col + 3])) == 0;
        case 3:
            return ((bottom[0] < map.top[col]) & (bottom[1] < map.top[col + 1]) & (bottom[2] < map.top[col + 2])) == 0;
        case 2:
            return ((bottom[0] < map.top[col]) & (bottom[1] < map.top[col + 1])) == 0;
        case 1:
            return ((bottom[0] < map.top[col])) == 0;
        }
    }
}

namespace m_tetris_rule_tools
{
    using namespace m_tetris;


    //创建一个节点(只支持4x4矩阵,这里包含了矩阵收缩)
    TetrisNode create_node(size_t w, size_t h, char T, int8_t X, int8_t Y, uint8_t R, uint32_t line1, uint32_t line2, uint32_t line3, uint32_t line4, TetrisOpertion const &op);

    //创建一个节点(只支持4x4矩阵,这里包含了矩阵收缩)
    template<char T, int8_t X, int8_t Y, uint8_t R, uint32_t line1, uint32_t line2, uint32_t line3, uint32_t line4>
    TetrisNode create_node(size_t w, size_t h, TetrisOpertion const &op)
    {
        static_assert(X < 0 || X >= 4 || Y < 0 || Y >= 4 || (line1 || line2 || line3 || line3), "data error");
        return create_node(w, h, T, X, Y, R, line1, line2, line3, line4, op);
    }

    //一个通用的旋转
    bool rotate_default(TetrisNode &node, unsigned char R, TetrisContext const *context);

    //一个通用的旋转模板
    template<unsigned char R>
    bool rotate_template(TetrisNode &node, TetrisContext const *context)
    {
        return rotate_default(node, R, context);
    }

    //左移,右移,上移,下移...

    bool move_left(TetrisNode &node, TetrisContext const *context);
    bool move_right(TetrisNode &node, TetrisContext const *context);
    bool move_up(TetrisNode &node, TetrisContext const *context);
    bool move_down(TetrisNode &node, TetrisContext const *context);
}
