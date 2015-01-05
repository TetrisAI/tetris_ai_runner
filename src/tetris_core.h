
#pragma once

#include <unordered_map>
#include <vector>
#include <map>
#include <algorithm>
#include <iterator>
#include <functional>
#include <cassert>
#include "rb_tree.h"

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
        int row[max_height];
        //每一列的高度
        int top[32];
        //场景宽
        int width;
        //场景高
        int height;
        //场景目前最大高度
        int roof;
        //场景的方块数
        int count;
        //判定[x,y]坐标是否有方块
        inline bool full(int x, int y) const
        {
            return (row[y] >> x) & 1;
        }
        TetrisMap()
        {
        }
        TetrisMap(int w, int h)
        {
            memset(this, 0, sizeof *this);
            width = w;
            height = h;
        }
        TetrisMap(TetrisMap const &other)
        {
            memcpy(this, &other, sizeof *this);
        }
        TetrisMap &operator = (TetrisMap const &other)
        {
            if(this != &other)
            {
                memcpy(this, &other, sizeof *this);
            }
            return *this;
        }
        bool operator == (TetrisMap const &other)
        {
            return std::memcmp(this, &other, sizeof *this) == 0;
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
            unsigned char t;
            char x, y;
            unsigned char r;
        };
        size_t status;
    };

    struct TetrisBlockStatusHash
    {
        size_t operator()(TetrisBlockStatus const &block) const
        {
            return std::hash<size_t>()(block.status);
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
            short int x, y;
        };
        size_t length;
        WallKickNode data[max_wall_kick];
    };

    //方块操作
    struct TetrisOpertion
    {
        //创建一个方块
        TetrisNode(*create)(int w, int h, TetrisOpertion const &op);
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
        int data[4];
        //方块每列的上沿高度
        int top[4];
        //方块每列的下沿高度
        int bottom[4];
        //方块在场景中的矩形位置
        char row, height, col, width;
        //各种变形会触及到的最低高度
        int low;

        //指针网索引
        //用于取代哈希表的hash

        size_t index;
        size_t index_filtered;

        //用于落点搜索优化
        std::vector<TetrisNode const *> const *land_point;

        //以下是指针网的数据
        //对应操作所造成的数据改变全都预置好,不需要再计算
        //如果为空,表示已经到达场景边界或者不支持该操作

        TetrisNode const *rotate_clockwise;
        TetrisNode const *rotate_counterclockwise;
        TetrisNode const *rotate_opposite;
        TetrisNode const *move_left;
        TetrisNode const *move_right;
        TetrisNode const *move_down;
        TetrisNode const *move_up;
        TetrisNode const *move_down_multi[max_height];

        //踢墙序列,依次尝试
        //遇到nullptr,表示序列结束

        TetrisNode const *wall_kick_clockwise[max_wall_kick];
        TetrisNode const *wall_kick_counterclockwise[max_wall_kick];
        TetrisNode const *wall_kick_opposite[max_wall_kick];

        //上下文...这个需要解释么?
        TetrisContext const *context;

        //检查当前块是否能够合并入场景
        bool check(TetrisMap const &map) const;
        //检查当前块是否是露天的
        bool open(TetrisMap const &map) const;
        //当前块合并入场景,同时更新场景数据
        size_t attach(TetrisMap &map) const;
        //探测合并后消的最低行
        int clear_low(TetrisMap &map) const;
        //探测合并后消的最低行
        int clear_high(TetrisMap &map) const;
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
        bool cover(TetrisNode const *key, TetrisNode const *node, char op);
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
        bool cover(TetrisNode const *key, TetrisNode const *node, char op);
        bool mark(TetrisNode const *key);
    };

    template<class TetrisRuleSet, class AI, class LandPointSearch>
    struct TetrisContextBuilder;

    //上下文对象.场景大小改变了需要重新初始化上下文
    class TetrisContext
    {
        template<class TetrisRuleSet, class AI, class LandPointSearch>
        friend struct TetrisContextBuilder;
    private:
        TetrisContext()
        {
        }
        //指针网数据
        std::unordered_map<TetrisBlockStatus, TetrisNode, TetrisBlockStatusHash, TetrisBlockStatusEqual> node_cache_;

        //规则信息

        std::map<std::pair<unsigned char, unsigned char>, TetrisOpertion> opertion_;
        std::map<unsigned char, TetrisBlockStatus(*)(TetrisContext const *)> generate_;

        //宽,高什么的...
        int width_, height_;
        //满行
        int full_;

        //一些用于加速的数据...
        std::map<unsigned char, std::vector<TetrisNode const *>> place_cache_;
        size_t type_max_;
        TetrisNode const *generate_cache_[256];
        unsigned char index_to_type_[256];
        size_t type_to_index_[256];

    public:
        enum PrepareResult : int
        {
            fail = 0, ok = 1, rebuild = 2,
        };
        //准备好上下文,返回fail表示上下错误
        PrepareResult prepare(int width, int height);

        int width() const;
        int height() const;
        int full() const;
        size_t type_max() const;
        size_t node_max() const;
        TetrisOpertion get_opertion(unsigned char t, unsigned char r) const;
        TetrisNode const *get(TetrisBlockStatus const &status) const;
        TetrisNode const *get(unsigned char t, char x, char y, unsigned char r) const;
        TetrisNode const *generate(unsigned char type) const;
        TetrisNode const *generate(size_t index) const;
        TetrisNode const *generate() const;
        bool TetrisContext::create(TetrisBlockStatus const &status, TetrisNode &node) const;
    };

    template<class Type>
    struct TetrisCallInit
    {
        template<class T>
        struct CallInit
        {
            template<class... Params>
            CallInit(Type &type, Params const &... params)
            {
            }
        };
        template<>
        struct CallInit<std::true_type>
        {
            template<class... Params>
            CallInit(Type &type, Params const &... params)
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
        template<typename U>
        static std::false_type func(Check<int Fallback::*, &U::init> *);
        template<typename U>
        static std::true_type func(...);
    public:
        template<class... Params>
        TetrisCallInit(Type &type, Params const &... params)
        {
            CallInit<decltype(func<Derived>(nullptr))>(type, params...);
        }
    };

    template<class AI, class Node>
    struct TetrisCallEval
    {
        template <typename T>
        struct function_traits : public function_traits<decltype(&T::eval)>
        {
        };

        template <typename ClassType, typename ReturnType, typename... Args>
        struct function_traits<ReturnType(ClassType::*)(Args...) const>
        {
            enum
            {
                arity = sizeof...(Args)
            };

            typedef ReturnType result_type;

            template <size_t i>
            struct arg
            {
                typedef typename std::tuple_element<i, std::tuple<Args...>>::type type;
            };
        };
        template<class A, class B>
        struct CallEval
        {
            template<class Return, class TetrisNodeEx, class... Params>
            static Return eval(AI const &ai, TetrisNode const *node, Params const &... params)
            {
                std::remove_reference<function_traits<AI>::arg<0>::type>::type node_ex(node);
                return ai.eval(node_ex, params...);
            }
        };
        template<class T>
        struct CallEval<T, T>
        {
            template<class Return, class TetrisNodeEx, class... Params>
            static Return eval(AI const &ai, TetrisNodeEx &node, Params const &... params)
            {
                return ai.eval(node, params...);
            }
        };
    public:
        template<class... Params>
        static auto eval(AI const &ai, Node &node, Params const &... params)->typename function_traits<AI>::result_type
        {
            return CallEval<Node &, function_traits<AI>::arg<0>::type &>::eval<function_traits<AI>::result_type, Node>(ai, node, params...);
        }
    };

    template<class Type>
    struct TetrisRuleInit
    {
        template<class T>
        struct RuleInit
        {
            static bool init(Type &type, int w, int h)
            {
                return true;
            }
        };
        template<>
        struct RuleInit<std::true_type>
        {
            static bool init(Type &type, int w, int h)
            {
                return type.init(w, h);
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
        template<typename U>
        static std::false_type func(Check<int Fallback::*, &U::init> *);
        template<typename U>
        static std::true_type func(...);
    public:
        static bool init(Type &type, int w, int h)
        {
            return RuleInit<decltype(func<Derived>(nullptr))>::init(type, w, h);
        }
    };

    template<class TetrisAI>
    struct TetrisAIHasIterated
    {
        struct Fallback
        {
            int iterated;
        };
        struct Derived : TetrisAI, Fallback
        {
        };
        template<typename U, U> struct Check;
        template<typename U>
        static std::false_type func(Check<int Fallback::*, &U::iterated> *);
        template<typename U>
        static std::true_type func(...);
    public:
        typedef decltype(func<Derived>(nullptr)) type;
    };

    template<class TetrisRuleSet, class AI, class LandPointSearch>
    struct TetrisContextBuilder
    {
    private:
        template<class AI, class = typename std::enable_if<(sizeof(AI::Param) > 0)>::type>
        static std::true_type has_param(AI *);
        template<class AI, class = void>
        static std::false_type has_param(...);

        template<class LandPointSearch, class = typename std::enable_if<(sizeof(LandPointSearch::Status) > 0)>::type>
        static std::true_type has_status(LandPointSearch *);
        template<class LandPointSearch, class = void>
        static std::false_type has_status(...);

        template<class AI, class T>
        struct AIParam
        {
            class AIParamHolder
            {
            public:
                typedef void Param;
                void const *get_param() const
                {
                    return nullptr;
                }
                void *get_param()
                {
                    return nullptr;
                }
            };
        };
        template<class AI>
        struct AIParam<AI, std::true_type>
        {
            class AIParamHolder
            {
            public:
                typedef typename AI::Param Param;
                Param const *get_param() const
                {
                    return &param_;
                }
                Param *get_param()
                {
                    return &param_;
                }
            private:
                Param param_;
            };
        };

        template<class LandPointSearch, class T>
        struct LandPointSearchStatus
        {
            class LandPointSearchStatusHolder
            {
            public:
                typedef void Status;
                void const *get_status() const
                {
                    return nullptr;
                }
                void *get_status()
                {
                    return nullptr;
                }
            };
        };
        template<class LandPointSearch>
        struct LandPointSearchStatus<LandPointSearch, std::true_type>
        {
            class LandPointSearchStatusHolder
            {
            public:
                typedef typename LandPointSearch::Status Status;
                Status const *get_status() const
                {
                    return &status_;
                }
                Status *get_status()
                {
                    return &status_;
                }
            private:
                Status status_;
            };
        };
    public:
        class TetrisContextEx : public TetrisContext, public AIParam<AI, decltype(has_param<AI>(nullptr))>::AIParamHolder, public LandPointSearchStatus<LandPointSearch, decltype(has_status<LandPointSearch>(nullptr))>::LandPointSearchStatusHolder
        {
        };
        static TetrisContextEx *build_context()
        {
            TetrisContextEx *context = new TetrisContextEx();
            context->opertion_ = TetrisRuleSet::get_opertion();
            context->generate_ = TetrisRuleSet::get_generate();
            return context;
        }
    private:
        template<class TetrisAI, class = typename std::enable_if<std::is_same<void, TetrisContextEx::Param>::value>::type>
        static void call_init_ai(TetrisAI &ai, TetrisContextEx const *context, void *)
        {
            TetrisCallInit<TetrisAI>(ai, context);
        }
        template<class TetrisAI, class = void>
        static void call_init_ai(TetrisAI &ai, TetrisContextEx const *context, ...)
        {
            TetrisCallInit<TetrisAI>(ai, context, context->get_param());
        }
        template<class TetrisLandPointSearch, class = typename std::enable_if<std::is_same<void, TetrisContextEx::Status>::value>::type>
        static void call_init_land_point_search(TetrisLandPointSearch &land_point_search, TetrisContextEx const *context, void *)
        {
            TetrisCallInit<TetrisLandPointSearch>(land_point_search, context);
        }
        template<class TetrisLandPointSearch, class = void>
        static void call_init_land_point_search(TetrisLandPointSearch &land_point_search, TetrisContextEx const *context, ...)
        {
            TetrisCallInit<TetrisLandPointSearch>(land_point_search, context, context->get_status());
        }
    public:
        template<class TetrisAI, class = void>
        static void init_ai(TetrisAI &ai, TetrisContextEx const *context)
        {
            call_init_ai(ai, context, nullptr);
        }
        template<class TetrisLandPointSearch, class = void>
        static void init_land_point_search(TetrisLandPointSearch &land_point_search, TetrisContextEx const *context)
        {
            call_init_land_point_search(land_point_search, context, nullptr);
        }
    };

    template<class TetrisAI, class TetrisLandPointSearchEngine>
    struct TetrisCore
    {
    private:
        template <typename Element>
        struct element_traits
        {
            typedef void Element;
        };
        template <typename Element>
        struct element_traits<std::vector<Element> const *>
        {
            typedef Element Element;
        };
        template <typename T>
        struct function_traits : public function_traits<decltype(&T::eval)>
        {
        };

        template <typename ClassType, typename ReturnType, typename... Args>
        struct function_traits<ReturnType(ClassType::*)(Args...) const>
        {
            enum
            {
                arity = sizeof...(Args)
            };

            typedef ReturnType result_type;

            template <size_t i>
            struct arg
            {
                typedef typename std::tuple_element<i, std::tuple<Args...>>::type type;
            };
        };
    public:
        typedef typename element_traits<decltype(TetrisLandPointSearchEngine().search(TetrisMap(), nullptr))>::Element LandPoint;
        typedef typename function_traits<TetrisAI>::result_type Eval;
        typedef decltype(TetrisAI().get(nullptr, 0)) FinalEval;
        typedef std::pair<LandPoint, FinalEval> Result;

        template<class TreeNode>
        void eval_node(TetrisAI &ai, TetrisMap &map, LandPoint &node, TreeNode *tree_node)
        {
            TetrisMap &new_map = tree_node->map;
            new_map = map;
            size_t clear = node->attach(new_map);
            tree_node->identity = node;
            tree_node->eval = TetrisCallEval<TetrisAI, LandPoint>::eval(ai, tree_node->identity, new_map, map, clear);
        }
        FinalEval run(TetrisAI &ai, Eval &eval, std::vector<Eval> &history)
        {
            history.push_back(eval);
            FinalEval result = ai.get(history.data(), history.size());
            history.pop_back();
            return result;
        }
        Result run(TetrisAI &ai, TetrisLandPointSearchEngine &search, TetrisMap const &map, LandPoint &node)
        {
            auto const *land_point = search.search(map, node);
            FinalEval final_eval = ai.bad();
            LandPoint best_node = node;
            for(auto cit = land_point->begin(); cit != land_point->end(); ++cit)
            {
                auto node_it = *cit;
                TetrisMap copy = map;
                size_t clear = node_it->attach(copy);
                Eval eval = TetrisCallEval<TetrisAI, decltype(node_it)>::eval(ai, node_it, copy, map, clear);
                FinalEval new_eval = ai.get(&eval, 1);
                if(new_eval > final_eval)
                {
                    final_eval = new_eval;
                    best_node = node_it;
                }
            }
            return std::make_pair(best_node, final_eval);
        }
    };

    template<class FinalEval, class Eval, class TetrisAI, class TetrisLandPointSearchEngine>
    struct TetrisTreeNode
    {
        typedef TetrisCore<TetrisAI, TetrisLandPointSearchEngine> Core;
        struct Context
        {
        public:
            struct RBTreeInterface
            {
                typedef decltype(TetrisTreeNode::final_eval) key_t;
                static key_t const &get_key(TetrisTreeNode *node)
                {
                    return node->final_eval;
                }
                static TetrisTreeNode *get_parent(TetrisTreeNode *node)
                {
                    return node->rb_parent;
                }
                static void set_parent(TetrisTreeNode *node, TetrisTreeNode *parent)
                {
                    node->rb_parent = parent;
                }
                static TetrisTreeNode *get_left(TetrisTreeNode *node)
                {
                    return node->rb_left;
                }
                static void set_left(TetrisTreeNode *node, TetrisTreeNode *left)
                {
                    node->rb_left = left;
                }
                static TetrisTreeNode *get_right(TetrisTreeNode *node)
                {
                    return node->rb_right;
                }
                static void set_right(TetrisTreeNode *node, TetrisTreeNode *right)
                {
                    node->rb_right = right;
                }
                static bool is_black(TetrisTreeNode *node)
                {
                    return node->is_black;
                }
                static void set_black(TetrisTreeNode *node, bool black)
                {
                    node->is_black = black;
                }
                static bool predicate(key_t const &left, key_t const &right)
                {
                    return left > right;
                }
            };
        public:
            Context() : version(), is_complete(), is_open_hold(), width(), total(), avg()
            {
            }
            void release()
            {
                for(auto node : tree_cache_)
                {
                    delete node;
                }
            }
        public:
            typedef zzz::rb_tree<TetrisTreeNode, RBTreeInterface> DeepthTree;
            size_t version;
            TetrisContext const *context;
            TetrisAI *ai;
            TetrisLandPointSearchEngine *search;
            std::vector<DeepthTree> deepth;
            DeepthTree current;
            std::vector<Eval> history;
            bool is_complete;
            bool is_open_hold;
            size_t width;
            std::vector<TetrisTreeNode *> tree_cache_;
            double total;
            double avg;
        public:
            TetrisTreeNode *alloc(TetrisTreeNode *parent)
            {
                TetrisTreeNode *node;
                if(!tree_cache_.empty())
                {
                    node = tree_cache_.back();
                    tree_cache_.pop_back();
                    node->version = version - 1;
                }
                else
                {
                    node = new TetrisTreeNode(this);
                }
                node->parent = parent;
                return node;
            }
            void dealloc(TetrisTreeNode *node)
            {
                for(auto child : node->children)
                {
                    dealloc(child);
                }
                node->children.clear();
                node->children_source[0] = nullptr;
                node->children_source[1] = nullptr;
                node->hold = ' ';
                node->flag = 0;
                node->next.clear();
                tree_cache_.push_back(node);
            }
        };
        struct RBTreeInterface
        {
            typedef TetrisBlockStatus key_t;
            static TetrisBlockStatus const &get_key(TetrisTreeNode *node)
            {
                return node->identity->status;
            }
            static TetrisTreeNode *get_parent(TetrisTreeNode *node)
            {
                return node->rb_parent;
            }
            static void set_parent(TetrisTreeNode *node, TetrisTreeNode *parent)
            {
                node->rb_parent = parent;
            }
            static TetrisTreeNode *get_left(TetrisTreeNode *node)
            {
                return node->rb_left;
            }
            static void set_left(TetrisTreeNode *node, TetrisTreeNode *left)
            {
                node->rb_left = left;
            }
            static TetrisTreeNode *get_right(TetrisTreeNode *node)
            {
                return node->rb_right;
            }
            static void set_right(TetrisTreeNode *node, TetrisTreeNode *right)
            {
                node->rb_right = right;
            }
            static bool is_black(TetrisTreeNode *node)
            {
                return node->is_black;
            }
            static void set_black(TetrisTreeNode *node, bool black)
            {
                node->is_black = black;
            }
            static bool predicate(key_t const &left, key_t const &right)
            {
                return TetrisBlockStatusCompare()(left, right);
            }
        };
        typedef zzz::rb_tree<TetrisTreeNode, RBTreeInterface> children_sort_t;
        TetrisTreeNode(Context *_context) : context(_context), version(context->version - 1), parent(), identity(), hold(' '), level(), flag(), node()
        {
            children_source[0] = 0;
            children_source[1] = 0;
        }
        Context *context;
        size_t version;
        TetrisMap map;
        typename Core::LandPoint identity;
        Eval eval;
        FinalEval final_eval;
        TetrisTreeNode *parent;
        std::vector<TetrisTreeNode *> children;
        TetrisNode const *children_source[2];
        TetrisTreeNode *rb_parent, *rb_left, *rb_right;
        TetrisNode const *node;
        union
        {
            struct
            {
                unsigned char hold;
                unsigned char level;
                unsigned char flag;
            };
            struct
            {
                unsigned char : 8;
                unsigned char : 8;
                unsigned char is_dead : 1;
                unsigned char is_hold : 1;
                unsigned char is_hold_lock : 1;
                unsigned char is_black : 1;
            };
        };
        std::vector<unsigned char> next;
        void (TetrisTreeNode::*search_ptr)(bool);

        TetrisTreeNode *update_root(TetrisMap const &_map)
        {
            if(map == _map)
            {
                return this;
            }
            TetrisTreeNode *new_root = nullptr;
            for(auto it = children.begin(); it != children.end(); ++it)
            {
                auto &child = *it;
                if(child->map == _map)
                {
                    new_root = child;
                    new_root->parent = nullptr;
                    children.erase(it);
                    break;
                }
            }
            if(new_root == nullptr)
            {
                new_root = context->alloc(nullptr);
                new_root->map = _map;
            }
            context->dealloc(this);
            context->is_complete = false;
            return new_root;
        }
        TetrisTreeNode *update(TetrisMap const &_map, TetrisNode const *_node, unsigned char const *_next, size_t _next_length)
        {
            TetrisTreeNode *root = update_root(_map);
            if(root != this || context->is_open_hold || _node != root->node || _next_length != root->next.size() || std::memcmp(_next, root->next.data(), _next_length) != 0)
            {
                ++context->version;
                context->total += context->width;
                context->avg = context->total / context->version;
                context->width = 0;
            }
            context->is_open_hold = false;
            root->node = _node;
            root->next.assign(_next, _next + _next_length);
            return root;
        }
        TetrisTreeNode *update(TetrisMap const &_map, TetrisNode const *_node, unsigned char _hold, bool _hold_lock, unsigned char const *_next, size_t _next_length)
        {
            TetrisTreeNode *root = update_root(_map);
            if(root != this || !context->is_open_hold || _node != root->node || _hold != root->hold || !!_hold_lock != root->is_hold_lock || _next_length != root->next.size() || std::memcmp(_next, root->next.data(), _next_length) != 0)
            {
                ++context->version;
                context->total += context->width;
                context->avg = context->total / context->version;
                context->width = 0;
            }
            context->is_open_hold = true;
            root->node = _node;
            root->hold = _hold;
            root->is_hold_lock = _hold_lock;
            root->next.assign(_next, _next + _next_length);
            return root;
        }
        void search(bool hold_control)
        {
            if(children_source[0] == nullptr)
            {
                children_source[0] = node;
                for(auto land_point_node : *context->search->search(map, node))
                {
                    TetrisTreeNode *child = context->alloc(this);
                    Core().eval_node(*context->ai, map, land_point_node, child);
                    child->is_hold = hold_control;
                    children.push_back(child);
                }
            }
            else if(children_source[0] != node || children_source[1] != nullptr)
            {
                children_source[0] = node;
                children_source[1] = nullptr;
                children_sort_t old;
                old.insert(children.begin(), children.end());
                children.clear();
                for(auto land_point_node : *context->search->search(map, node))
                {
                    TetrisTreeNode *child;
                    auto find = old.find(land_point_node->status);
                    if(find != old.end())
                    {
                        child = &*find;
                        old.erase(find);
                    }
                    else
                    {
                        child = context->alloc(this);
                        Core().eval_node(*context->ai, map, land_point_node, child);
                    }
                    child->is_hold = hold_control;
                    children.push_back(child);
                }
                for(auto &child : old)
                {
                    context->dealloc(&child);
                }
            }
        }
        void search_hold(bool hold_control)
        {
            if(is_hold_lock)
            {
                search(hold_control);
                return;
            }
            if(hold == ' ' || next.empty())
            {
                assert(!is_hold_lock);
                TetrisNode const *node_save = node;
                unsigned char hold_save = hold;
                auto next_save = next;
                if(hold == ' ')
                {
                    hold = node->status.t;
                    node = context->context->generate(next.front());
                    next.erase(next.begin());
                    search_hold(true);
                }
                else
                {
                    next.push_back(hold);
                    hold = ' ';
                    is_hold_lock = true;
                    search(true);
                }
                node = node_save;
                hold = hold_save;
                next.swap(next_save);
                is_hold_lock = false;
                return;
            }
            if(node->status.t == hold)
            {
                TetrisNode const *hold_node = context->context->generate(hold);
                if(children_source[0] == nullptr)
                {
                    children_source[0] = node;
                    children_source[1] = hold_node;
                    for(auto land_point_node : *context->search->search(map, node))
                    {
                        TetrisTreeNode *child = context->alloc(this);
                        Core().eval_node(*context->ai, map, land_point_node, child);
                        child->is_hold = hold_control;
                        children.push_back(child);
                    }
                    children_sort_t sort;
                    sort.insert(children.begin(), children.end());
                    for(auto land_point_node : *context->search->search(map, hold_node))
                    {
                        if(sort.find(land_point_node->status) != sort.end())
                        {
                            continue;
                        }
                        TetrisTreeNode *child = context->alloc(this);
                        Core().eval_node(*context->ai, map, land_point_node, child);
                        child->is_hold = !hold_control;
                        children.push_back(child);
                    }
                }
                else if((children_source[0] != node || children_source[1] != hold_node) && (children_source[0] != hold_node || children_source[1] != node))
                {
                    children_source[0] = node;
                    children_source[1] = hold_node;
                    children_sort_t old;
                    children_sort_t sort;
                    old.insert(children.begin(), children.end());
                    children.clear();
                    for(auto land_point_node : *context->search->search(map, node))
                    {
                        TetrisTreeNode *child;
                        auto find = old.find(land_point_node->status);
                        if(find != old.end())
                        {
                            child = &*find;
                            old.erase(find);
                        }
                        else
                        {
                            child = context->alloc(this);
                            Core().eval_node(*context->ai, map, land_point_node, child);
                        }
                        child->is_hold = hold_control;
                        children.push_back(child);
                        sort.insert(child);
                    }
                    for(auto land_point_node : *context->search->search(map, hold_node))
                    {
                        if(sort.find(land_point_node->status) != sort.end())
                        {
                            continue;
                        }
                        TetrisTreeNode *child;
                        auto find = old.find(land_point_node->status);
                        if(find != old.end())
                        {
                            child = &*find;
                            old.erase(find);
                        }
                        else
                        {
                            child = context->alloc(this);
                            Core().eval_node(*context->ai, map, land_point_node, child);
                        }
                        child->is_hold = !hold_control;
                        children.push_back(child);
                    }
                    for(auto &child : old)
                    {
                        context->dealloc(&child);
                    }
                }
            }
            else if(node->status.t != hold)
            {
                TetrisNode const *hold_node = context->context->generate(hold);
                if(children_source[0] == nullptr)
                {
                    children_source[0] = node;
                    children_source[1] = hold_node;
                    for(auto land_point_node : *context->search->search(map, node))
                    {
                        TetrisTreeNode *child = context->alloc(this);
                        Core().eval_node(*context->ai, map, land_point_node, child);
                        child->is_hold = hold_control;
                        children.push_back(child);
                    }
                    for(auto land_point_node : *context->search->search(map, hold_node))
                    {
                        TetrisTreeNode *child = context->alloc(this);
                        Core().eval_node(*context->ai, map, land_point_node, child);
                        child->is_hold = !hold_control;
                        children.push_back(child);
                    }
                }
                else if((children_source[0] != node || children_source[1] != hold_node) && (children_source[0] != hold_node || children_source[1] != node))
                {
                    children_source[0] = node;
                    children_source[1] = hold_node;
                    children_sort_t old;
                    old.insert(children.begin(), children.end());
                    children.clear();
                    for(auto land_point_node : *context->search->search(map, node))
                    {
                        TetrisTreeNode *child;
                        auto find = old.find(land_point_node->status);
                        if(find != old.end())
                        {
                            child = &*find;
                            old.erase(find);
                        }
                        else
                        {
                            child = context->alloc(this);
                            Core().eval_node(*context->ai, map, land_point_node, child);
                        }
                        child->is_hold = hold_control;
                        children.push_back(child);
                    }
                    for(auto land_point_node : *context->search->search(map, hold_node))
                    {
                        TetrisTreeNode *child;
                        auto find = old.find(land_point_node->status);
                        if(find != old.end())
                        {
                            child = &*find;
                            old.erase(find);
                        }
                        else
                        {
                            child = context->alloc(this);
                            Core().eval_node(*context->ai, map, land_point_node, child);
                        }
                        child->is_hold = !hold_control;
                        children.push_back(child);
                    }
                    for(auto &child : old)
                    {
                        context->dealloc(&child);
                    }
                }
            }
        }
        void update_info()
        {
            if(parent == nullptr)
            {
                level = 0;
                if(context->is_open_hold)
                {
                    search_ptr = &TetrisTreeNode::search_hold;
                }
                else
                {
                    search_ptr = &TetrisTreeNode::search;
                }
                return;
            }
            level = parent->level + 1;
            if(!parent->next.empty())
            {
                node = context->context->generate(parent->next.front());
                next.assign(parent->next.begin() + 1, parent->next.end());
            }
            else
            {
                assert(context->is_open_hold);
                assert(is_hold);
                assert(next.empty());
                node = context->context->generate(parent->hold);
            }
            if(context->is_open_hold)
            {
                if(is_hold)
                {
                    hold = parent->node->status.t;
                }
                else
                {
                    hold = parent->hold;
                }
                search_ptr = &TetrisTreeNode::search_hold;
            }
            else
            {
                search_ptr = &TetrisTreeNode::search;
            }
        }
        bool build_children()
        {
            if(version == context->version || is_dead)
            {
                return false;
            }
            version = context->version;
            update_info();
            (this->*search_ptr)(false);
            if(children.empty())
            {
                is_dead = true;
                return false;
            }
            auto &ai = context->ai;
            std::vector<Eval> &history = context->history;
            size_t deepth = level;
            history.resize(deepth);
            TetrisTreeNode *node_eval = this;
            while(deepth-- > 0)
            {
                history[deepth] = node_eval->eval;
                node_eval = node_eval->parent;
            }
            for(auto child : children)
            {
                child->final_eval = Core().run(*context->ai, child->eval, history);
            }
            return true;
        }
        bool run(int incr)
        {
            assert(parent == nullptr);
            assert(!next.empty());
            size_t next_length = next.size();
            if(context->is_open_hold && hold != ' ')
            {
                ++next_length;
            }
            size_t next_length_max = next_length;
            context->deepth.resize(next_length);
            if(context->is_complete)
            {
                return false;
            }
            if(context->width == 0)
            {
                for(auto &level : context->deepth)
                {
                    level.clear();
                }
                build_children();
                auto level = &context->current;
                level->clear();
                level->insert(children.begin(), children.end());
            }
            size_t prune_hold = (context->width += incr);
            size_t prune_hold_max = prune_hold * 36 / 10;
            bool complete = true;
            auto level = &context->current;
            while(next_length-- > 0)
            {
                size_t level_prune_hold = prune_hold_max * next_length / next_length_max + prune_hold;
                auto next_level = &context->deepth[next_length];
                if(level_prune_hold <= level->size())
                {
                    complete = false;
                }
                for(auto it = level->begin(); level_prune_hold != 0 && it != level->end(); ++it)
                {
                    TetrisTreeNode *child = &*it;
                    --level_prune_hold;
                    if(child->build_children())
                    {
                        next_level->insert(child->children.begin(), child->children.end());
                    }
                }
                level = next_level;
            }
            if(complete)
            {
                context->is_complete = true;
                return true;
            }
            return false;
        }
        std::pair<TetrisTreeNode const *, FinalEval> get_best()
        {
            TetrisTreeNode *best = nullptr;
            for(auto &level : context->deepth)
            {
                if(!level.empty())
                {
                    best = &*level.begin();
                    break;
                }
            }
            if(best == nullptr)
            {
                return std::make_pair(nullptr, context->ai->bad());
            }
            while(best->parent->parent != nullptr)
            {
                best = best->parent;
            }
            return std::make_pair(best, best->final_eval);
        }
    };

    template<class TetrisRuleSet, class TetrisAI, class TetrisLandPointSearchEngine>
    class TetrisEngine
    {
    private:
        typedef TetrisCore<TetrisAI, TetrisLandPointSearchEngine> Core;
        typedef TetrisTreeNode<typename Core::FinalEval, typename Core::Eval, TetrisAI, TetrisLandPointSearchEngine> TreeNode;
        typedef TetrisContextBuilder<TetrisRuleSet, TetrisAI, TetrisLandPointSearchEngine> ContextBuilder;
        typedef typename Core::LandPoint LandPoint;
        typename ContextBuilder::TetrisContextEx *context_;
        TetrisAI ai_;
        TetrisLandPointSearchEngine search_;
        typename TreeNode::Context tree_context_;
        TreeNode *tree_root_;

    public:
        struct RunResult
        {
            RunResult(typename Core::FinalEval const &_eval) : target(), eval(_eval), change_hold()
            {
            }
            RunResult(typename Core::FinalEval const &_eval, bool _change_hold) : target(), eval(_eval), change_hold(_change_hold)
            {
            }
            RunResult(std::pair<TreeNode const *, typename Core::FinalEval> const &_result) : target(_result.first ? _result.first->identity : nullptr), eval(_result.second), change_hold()
            {
            }
            RunResult(std::pair<LandPoint, typename Core::FinalEval> const &_result) : target(_result.first), eval(_result.second), change_hold()
            {
            }
            RunResult(std::pair<TreeNode const *, typename Core::FinalEval> const &_result, bool _change_hold) : target(_result.first ? _result.first->identity : nullptr), eval(_result.second), change_hold(_change_hold)
            {
            }
            RunResult(std::pair<LandPoint, typename Core::FinalEval> const &_result, bool _change_hold) : target(_result.first), eval(_result.second), change_hold(_change_hold)
            {
            }
            LandPoint target;
            typename Core::FinalEval eval;
            bool change_hold;
        };

    public:
        TetrisEngine() : context_(ContextBuilder::build_context()), ai_(), tree_root_(new TreeNode(&tree_context_))
        {
            tree_context_.context = context_;
            tree_context_.ai = &ai_;
            tree_context_.search = &search_;
        }
        ~TetrisEngine()
        {
            tree_context_.dealloc(tree_root_);
            tree_context_.release();
            delete context_;
        }
        //从状态获取当前块
        TetrisNode const *get(TetrisBlockStatus const &status) const
        {
            return context_->get(status);
        }
        //上下文对象...用来做什么呢= =?
        TetrisContext const *context() const
        {
            return context_;
        }
        //AI名称
        std::string ai_name() const
        {
            return ai_.ai_name();
        }
        auto param()->decltype(context_->get_param())
        {
            return context_->get_param();
        }
        auto param() const->decltype(context_->get_param())
        {
            return context_->get_param();
        }
        auto status()->decltype(context_->get_status())
        {
            return context_->get_status();
        }
        auto status() const->decltype(context_->get_status())
        {
            return context_->get_status();
        }
        //准备好上下文
        bool prepare(int width, int height)
        {
            if(!TetrisRuleInit<TetrisRuleSet>::init(TetrisRuleSet(), width, height))
            {
                return false;
            }
            TetrisContext::PrepareResult result = context_->prepare(width, height);
            if(result == TetrisContext::rebuild)
            {
                ContextBuilder::init_ai(ai_, context_);
                ContextBuilder::init_land_point_search(search_, context_);
                return true;
            }
            else if(result == TetrisContext::fail)
            {
                return false;
            }
            return true;
        }
        //update!强制刷新上下文
        void update()
        {
            ++tree_context_.version;
            tree_context_.total += tree_context_.width;
            tree_context_.avg = tree_context_.total / tree_context_.version;
            tree_context_.width = 0;
        }
        //run!
        RunResult run(TetrisMap const &map, TetrisNode const *node, unsigned char const *next, size_t next_length, time_t limit = 100)
        {
            if(node == nullptr || !node->check(map))
            {
                return RunResult(ai_.bad());
            }
            if(next_length == 0)
            {
                Core::LandPoint node_ex(node);
                return RunResult(Core().run(ai_, search_, map, node_ex));
            }
            else
            {
                time_t now = clock(), end = now + limit;
                tree_root_ = tree_root_->update(map, node, next, next_length);
                do
                {
                    if(tree_root_->run(static_cast<int>(std::max<time_t>(1, end - now) / next_length)))
                    {
                        break;
                    }
                } while((now = clock()) < end);
                return RunResult(tree_root_->get_best());
            }
        }
        //带hold的run!
        RunResult run_hold(TetrisMap const &map, TetrisNode const *node, unsigned char hold, bool hold_free, unsigned char const *next, size_t next_length, time_t limit = 100)
        {
            if(node == nullptr || !node->check(map))
            {
                return RunResult(ai_.bad());
            }
            if(next_length == 0)
            {
                if(hold_free)
                {
                    if(hold == ' ')
                    {
                        return RunResult(ai_.bad(), true);
                    }
                    else
                    {
                        Core::LandPoint node_ex(node), hold_ex(context_->generate(hold));
                        auto node_result = Core().run(ai_, search_, map, node_ex);
                        auto hold_result = Core().run(ai_, search_, map, hold_ex);
                        if(hold_result.second > node_result.second)
                        {
                            return RunResult(hold_result, true);
                        }
                        else
                        {
                            return RunResult(node_result, false);
                        }
                    }
                }
                else
                {
                    Core::LandPoint node_ex(node);
                    return RunResult(Core().run(ai_, search_, map, node_ex));
                }
            }
            time_t now = clock(), end = now + limit;
            tree_root_ = tree_root_->update(map, node, hold, !hold_free, next, next_length);
            do
            {
                if(tree_root_->run(static_cast<int>(std::max<time_t>(1, end - now) / next_length)))
                {
                    break;
                }
            } while((now = clock()) < end);
            auto best = tree_root_->get_best();
            return RunResult(best, best.first == nullptr ? false : best.first->is_hold);
        }
        //根据run的结果得到一个操作路径
        std::vector<char> make_path(TetrisNode const *node, LandPoint const &land_point, TetrisMap const &map, bool cut_drop = true)
        {
            auto path = search_.make_path(node, land_point, map);
            if(cut_drop)
            {
                while(!path.empty() && (path.back() == 'd' || path.back() == 'D'))
                {
                    path.pop_back();
                }
            }
            return path;
        }
        //根据run的结果得到一组按键状态
        std::vector<char> make_status(TetrisNode const *node, LandPoint const &land_point, TetrisMap const &map)
        {
            return search_.make_status(node, land_point, map);
        }
    };

}

namespace m_tetris_rule_tools
{
    using namespace m_tetris;


    //创建一个节点(只支持4x4矩阵,这里包含了矩阵收缩)
    TetrisNode create_node(int w, int h, unsigned char T, char X, char Y, unsigned char R, int line1, int line2, int line3, int line4, TetrisOpertion const &op);

    //创建一个节点(只支持4x4矩阵,这里包含了矩阵收缩)
    template<unsigned char T, char X, char Y, unsigned char R, int line1, int line2, int line3, int line4>
    TetrisNode create_node(int w, int h, TetrisOpertion const &op)
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