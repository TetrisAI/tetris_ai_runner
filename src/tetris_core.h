
#pragma once

#include <algorithm>
#include <array>
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

#include "chash_map.h"
#include "chash_set.h"
#include "tetris_rule_spec.h"

namespace m_tetris2
{
    const int max_height = 40;
    //C-A2: legacy bridge layer 解锁 W>32. 新引擎 (Map<W,H>) 已支持 W<=64;
    //  此常量同时作为 TetrisMap::top / AI 栈数组等的容量上限.
    const int max_width = 64;
    const int max_wall_kick = 16;

    //row_t / WallKickList / OpLines / OpDesc / RuleSpec / kOpRotateNone 由
    //tetris_rule_spec.h 提供; 这里不再重复定义.

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
        row_t row[max_height];
        //每一列的高度
        int32_t top[max_width];
        //场景宽
        int32_t width;
        //场景高
        int32_t height;
        //场景目前最大高度
        int32_t roof;
        //场景的方块数
        int32_t count;
        //
        row_t line;
        //判定[x,y]坐标是否有方块
        inline bool full(size_t x, size_t y) const
        {
            return ((row[y] >> x) & 1) == 0;
        }
        TetrisMap()
        {
        }
        TetrisMap(int32_t w, int32_t h)
        {
            std::memset(this, 0, sizeof *this);
            width = w;
            height = h;
            line = width >= int(sizeof(row_t) * 8) ? row_t(~row_t(0)) : row_t((row_t(1) << width) - row_t(1));
            clear();
        }
        TetrisMap(TetrisMap const &other)
        {
            std::memcpy(this, &other, sizeof *this);
        }
        TetrisMap &operator=(TetrisMap const &other)
        {
            if (this != &other)
            {
                std::memcpy(this, &other, sizeof *this);
            }
            return *this;
        }
        bool operator==(TetrisMap const &other)
        {
            return std::memcmp(this, &other, sizeof *this) == 0;
        }
        bool operator!=(TetrisMap const &other)
        {
            return std::memcmp(this, &other, sizeof *this) != 0;
        }
        row_t empty_line() const
        {
            return line;
        }
        void clear()
        {
            for (int y = 0; y < height; ++y)
            {
                row[y] = line;
            }
            std::memset(top, 0, sizeof top);
            roof = 0;
            count = 0;
        }
        void prepare_internal()
        {
            roof = 0;
            count = 0;
            for (int my = 0; my < height; ++my)
            {
                for (int mx = 0; mx < width; ++mx)
                {
                    if (full(mx, my))
                    {
                        top[mx] = roof = my + 1;
                        ++count;
                    }
                }
            }
        }
        void prepare()
        {
            roof = 0;
            count = 0;
            for (int my = 0; my < height; ++my)
            {
                row[my] = ~row[my] & empty_line();
                for (int mx = 0; mx < width; ++mx)
                {
                    if (full(mx, my))
                    {
                        top[mx] = roof = my + 1;
                        ++count;
                    }
                }
            }
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
        row_t row[4][max_height];
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
        TetrisNode (*create)(size_t w, size_t h, TetrisOpertion const &op);
        //顺时针旋转(右旋)
        bool (*rotate_clockwise)(TetrisNode &node, TetrisContext const *context);
        //逆时针旋转(左旋)
        bool (*rotate_counterclockwise)(TetrisNode &node, TetrisContext const *context);
        //转动180°
        bool (*rotate_opposite)(TetrisNode &node, TetrisContext const *context);
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
        //方块每行的数据
        row_t data[4];
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
        TetrisNode const *move_up;
        union
        {
            struct
            {
                TetrisNode const *self;
                TetrisNode const *move_down;
            };
            TetrisNode const *move_down_multi[max_height];
        };

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
    template<bool Filtered>
    class TetrisNodeMarkTemplate
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

    using TetrisNodeMark = TetrisNodeMarkTemplate<false>;
    using TetrisNodeMarkFiltered = TetrisNodeMarkTemplate<true>;

    template<class TetrisRule, class AI, class Search>
    struct TetrisContextBuilder;

    struct AIEnv
    {
        char const *next;
        size_t length;
        char node;
        char hold;
        bool is_hold;
    };

    //上下文对象.场景大小改变了需要重新初始化上下文
    class TetrisContext
    {
        template<class TetrisRule, class AI, class Search>
        friend class TetrisEngine;

    private:
        TetrisContext() : opertion_(), width_(0), height_(0), row_mask_(0), type_max_(0), index_to_type_(), type_to_index_(), spawn_x_(), spawn_y_()
        {
        }

        //规则信息
        std::map<std::pair<char, unsigned char>, TetrisOpertion> opertion_;

        //宽,高什么的...
        int32_t width_, height_;
        //满行
        row_t row_mask_;

        size_t type_max_;
        char index_to_type_[256];
        size_t type_to_index_[256];
        int8_t spawn_x_[256];
        int8_t spawn_y_[256];

    public:
        using Env = AIEnv;

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
        row_t row_mask() const
        {
            return row_mask_;
        }
        TetrisNode const *generate(char type) const;
        size_t type_max() const;
        size_t convert(char type) const;
        char convert(size_t index) const;
        //用于替代 TetrisNode::op 字段,build 期与运行期热路径都改走这个查询接口
        TetrisOpertion const &get_opertion(char t, unsigned char r) const;
        TetrisNodeBlockLocate const *get_block(char t, unsigned char r) const;
    };

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
        // Commit 4 Phase 2: eval is now a template member; Result must be
        // declared as a nested type alias inside each AI class.
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

// Commit 4 Phase 2: bb_eval_bridge.h must be included OUTSIDE any open
// namespace block.  It pulls in search_tspin.h / search_aspin.h / bb_node.h
// which all open their own namespace blocks (search_tspin / search_aspin /
// etc.) and reference m_tetris2:: types with full qualification.  Placing the
// #include inside namespace m_tetris2 {} would turn those inner namespace
// openings into m_tetris2::search_tspin etc. with m_tetris2:: references
// resolving to m_tetris2::m_tetris2::, causing compile errors.
//
// bb_eval_bridge.h itself wraps all its declarations in namespace m_tetris2 {}
// so the bridge types end up in the correct namespace regardless.
#include "bb_eval_bridge.h"
#include "bb_state.h"

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

    //flatten_rulespec 的前置声明:正式定义在文件末尾(因为依赖 m_tetris2_rule_tools).
    //TetrisEngine 模板里要在尚未看到完整定义时引用它,所以这里前置一个签名相同的声明.
    template<class Rule>
    std::map<std::pair<char, uint8_t>, TetrisOpertion> flatten_rulespec();

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

            static void call(TetrisAI &ai, LocalContext<TreeNode> const *local_context, TetrisContext const * /*shared_context*/)
            {
                ai.template init<SearchSpec>(local_context->ai_config());
            }
            static void call(TetrisSearch &search, LocalContext<TreeNode> const *local_context, TetrisContext const * /*shared_context*/)
            {
                TetrisCallInit<TetrisSearch>(search, local_context->search_config());
            }
        };
        template<class TreeNode>
        struct CallInit<TreeNode, void>
        {
            using SearchSpec = typename SearchRuleSpecOf<TetrisSearch>::type;

            static void call(TetrisAI &ai, LocalContext<TreeNode> const *local_context, TetrisContext const * /*shared_context*/)
            {
                ai.template init<SearchSpec>();
            }
            static void call(TetrisSearch &search, LocalContext<TreeNode> const *local_context, TetrisContext const * /*shared_context*/)
            {
                TetrisCallInit<TetrisSearch>{search};
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
                node->status.set(BBCallGet<TetrisAI>::get(*context->ai, node->identity, node->result, parent->level, parent->status.get_raw(), parent->template env<EnableEnv>(context, node)));
            }
        };
        template<class TreeNode, bool EnableEnv>
        struct TetrisSelectGet<TreeNode, EnableEnv, 4>
        {
            typedef std::false_type enable_next_c;
            static void get(typename TreeNode::Context *context, TreeNode *node, TreeNode *parent)
            {
                node->status.set(BBCallGet<TetrisAI>::get(*context->ai,
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
                node->status.set(BBCallGet<TetrisAI>::get(*context->ai,
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
                node->status.set(BBCallGet<TetrisAI>::get(*context->ai,
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
            Context(std::deque<TetrisTreeNode> *_node_storage) : version(), is_complete(), is_open_hold(), node_storage(_node_storage), free_list(nullptr), free_count(0), current_t('\0'), width(), total(), avg()
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
            TetrisTreeNode *free_list;
            size_t free_count;
            std::vector<Status const *> iterate_cache;
            char current_t;
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
            void dealloc(TetrisTreeNode *node)
            {
                for (auto it = node->children; it != nullptr; it = it->children_next)
                {
                    dealloc(it);
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
        TetrisMap map;
        typename Core::LandPoint identity;
        typename Core::Result result;
        TreeNodeStatus<TetrisAI, typename TetrisAIHasIterate<TetrisAI>::type> status;
        TetrisTreeNode *parent;
        TetrisTreeNode *children;
        TetrisTreeNode *children_next;
        TetrisNodeFlag node_flag;
        typename std::vector<next_t>::const_iterator next;

        // lp_key: 提取 children_map_t / identity_set_t 的 TetrisBlockStatus key。
        //   LandPoint (BBLandPoint / TetrisNodeWith*SpinType): 用 {t, xb, yb, r}
        //   构造伪 TetrisBlockStatus。yb 为底行（非真实 status.y 顶行），但在
        //   同一 LandPoint 类型的同一个 map 内，唯一性与真实 status 等价。
        template<class LP>
        static TetrisBlockStatus lp_key(LP const &lp) noexcept
        {
            return TetrisBlockStatus{static_cast<char>(lp.state.t),
                                     static_cast<int8_t>(lp.state.xb),
                                     static_cast<int8_t>(lp.state.yb),
                                     lp.state.r};
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
        TetrisTreeNode *update(Context *context, TetrisMap const &_map, Status const &status, char _t, char const *_next, size_t _next_length)
        {
            TetrisTreeNode *root = update_root(context, _map);
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
        TetrisTreeNode *update(Context *context, TetrisMap const &_map, Status const &status, char _t, char _hold, bool _hold_lock, char const *_next, size_t _next_length)
        {
            TetrisTreeNode *root = update_root(context, _map);
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
        // alloc_eval_typed<T,R>: 分配子节点，直接以编译期 <T,R> 调 BBCallEval::call_eval_typed，
        // 完全绕过 BBCallEval::eval() 里的 for_each_typed_r 运行时循环。
        // 这是 Phase 2 推式 eval 的核心：Search 层回调携带编译期 <T,R>，在此直接实例化。
        // TODO(multi-thread): 多线程路径在此构造 PendingTask 而非同步 eval，详见 plan doc。
        template<char T, std::uint8_t R, class SrcBoard>
        TetrisTreeNode *alloc_eval_typed(Context *context, TetrisMap &map,
                                         SrcBoard const &src_board,
                                         LandPoint const &lp)
        {
            using Spec = typename SearchRuleSpecOf<TetrisSearch>::type;
            using BBCallEvalT = BBCallEval<TetrisAI, Spec>;
            TetrisTreeNode *child = context->alloc(this);
            child->map = map;
            child->identity = lp;
            // 纯位板 attach：直接从 BBState 写方块、消行、更新 top/roof.
            // 不再需要 TetrisNode* 或 TetrisContext::get() 反查.
            size_t clear = m_tetris2::bb::Helpers<Spec>::attach_to_map(lp.state, child->map);
            auto after_board = m_tetris2::bb::build_board_for_search<Spec>(child->map);
            child->result = BBCallEvalT::template call_eval_typed<T, R>(
                *context->ai, child->identity, after_board, src_board, clear);
            return child;
        }

        void search(Context *context, char search_t, bool is_hold)
        {
            using SearchSpec = typename SearchRuleSpecOf<TetrisSearch>::type;
            using SearchHelpers = m_tetris2::bb::Helpers<SearchSpec>;
            auto sp = SearchSpec::spawn(search_t, SearchSpec::width, SearchSpec::height);
            auto board = m_tetris2::bb::build_board_for_search<SearchSpec>(map);
            auto spawn = SearchHelpers::state_from_status(search_t, 0, sp.first, sp.second);
            if (node_flag.empty())
            {
                node_flag.set(search_t);
                // Phase 2: 推式 search_eval，消除外层 for 循环拉取。
                // Fresh 场景：每个落点直接 alloc + eval，无需查字典。
                auto fresh_cb = [&]<char T, std::uint8_t R>(LandPoint const &land_point_node)
                {
                    TetrisTreeNode *child = alloc_eval_typed<T, R>(context, map, board, land_point_node);
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
                // Phase 2: Rescan 场景：先查字典复用，未命中则 alloc + eval。
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
                        child = alloc_eval_typed<T, R>(context, map, board, land_point_node);
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
            using SearchHelpers = m_tetris2::bb::Helpers<SearchSpec>;
            auto s_sp = SearchSpec::spawn(search_t, SearchSpec::width, SearchSpec::height);
            auto h_sp = SearchSpec::spawn(hold_t, SearchSpec::width, SearchSpec::height);
            auto board = m_tetris2::bb::build_board_for_search<SearchSpec>(map);
            auto search_spawn = SearchHelpers::state_from_status(search_t, 0, s_sp.first, s_sp.second);
            auto hold_spawn = hold_t != '\0' ? SearchHelpers::state_from_status(hold_t, 0, h_sp.first, h_sp.second) : m_tetris2::bb::BBState{};
            if (search_t == hold_t)
            {
                return search(context, search_t, false);
            }
            else
            {
                if (node_flag.empty())
                {
                    node_flag.set(search_t, hold_t);
                    // Phase 2: Fresh (diff-type hold) — search_node 落点。
                    auto fresh_main_cb = [&]<char T, std::uint8_t R>(LandPoint const &land_point_node)
                    {
                        TetrisTreeNode *child = alloc_eval_typed<T, R>(context, map, board, land_point_node);
                        child->is_hold = false;
                        child->children_next = children;
                        children = child;
                    };
                    context->search->search_eval(board, search_spawn, level, fresh_main_cb);
                    if (children != nullptr)
                    {
                        auto fresh_hold_cb = [&]<char T, std::uint8_t R>(LandPoint const &land_point_node)
                        {
                            TetrisTreeNode *child = alloc_eval_typed<T, R>(context, map, board, land_point_node);
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
                        // 避免向 search() 传 BBState（其签名接收 TetrisBlockStatus）。
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
                        // Phase 2: Rescan (diff-type hold) — search_node 落点。
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
                                child = alloc_eval_typed<T, R>(context, map, board, land_point_node);
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
                                child = alloc_eval_typed<T, R>(context, map, board, land_point_node);
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
            using SearchHelpers = m_tetris2::bb::Helpers<SearchSpec>;
            using piece_info_t = typename SearchHelpers::piece_info_t;
            auto board = m_tetris2::bb::build_board_for_search<SearchSpec>(map);
            if (node_flag.empty())
            {
                node_flag.set(TetrisNodeFlag::kVirtualSearchFlag);
                for (int i = 0; i < SearchHelpers::kPieceCount; ++i)
                {
                    // Phase 2: 推式 search_eval。
                    auto fresh_cb = [&]<char T, std::uint8_t R>(LandPoint const &land_point_node)
                    {
                        TetrisTreeNode *child = alloc_eval_typed<T, R>(context, map, board, land_point_node);
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
                    // Phase 2: 推式 search_eval。
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
                            child = alloc_eval_typed<T, R>(context, map, board, land_point_node);
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
            using SearchHelpers = m_tetris2::bb::Helpers<SearchSpec>;
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
        TetrisContext::Env env(Context *context, TetrisTreeNode const *tree_node)
        {
            if (EnableEnv)
            {
                TetrisContext::Env result =
                    {
                        nullptr, 0, lp_type(tree_node->identity), tree_node->is_hold ? node : hold};
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
        //   供 TetrisEngine2 用于从 SearchTag 自动注入 DeduceSpinPolicy<AI>.
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
        typedef typename detail::SearchTagResolve<TetrisSearchTag, TetrisRule>::type TetrisSearch;
        typedef TetrisCore<TetrisAI, TetrisSearch> Core;
        typedef TetrisTreeNode<typename Core::Status, TetrisAI, TetrisSearch> TreeNode;
        typedef LocalContextBuilder<typename TreeNode::Context, TetrisRule, TetrisAI, TetrisSearch> ContextBuilder;
        typedef typename Core::LandPoint LandPoint;

    private:
        std::deque<TreeNode> tree_storage_;
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
        TetrisEngine() : shared_context_(), local_context_(&tree_storage_), ai_(), root_(nullptr), status_(), memory_limit_(128ull << 20)
        {
            tree_storage_.emplace_back();
            root_ = &tree_storage_.back();
        }
        TetrisEngine(std::shared_ptr<TetrisContext> context) : shared_context_(context), local_context_(&tree_storage_), ai_(), root_(nullptr), status_(), memory_limit_(128ull << 20)
        {
            tree_storage_.emplace_back();
            root_ = &tree_storage_.back();
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
            shared_context_->opertion_ = m_tetris2::flatten_rulespec<typename TetrisRule::rule_spec>();
            {
                auto &ctx = *shared_context_;
                ctx.width_ = width;
                ctx.height_ = height;
                ctx.row_mask_ = width >= int(sizeof(m_tetris2::row_t) * 8)
                                    ? m_tetris2::row_t(~m_tetris2::row_t(0))
                                    : m_tetris2::row_t((m_tetris2::row_t(1) << width) - m_tetris2::row_t(1));
                ctx.type_max_ = 0;
                std::fill(std::begin(ctx.index_to_type_), std::end(ctx.index_to_type_), '\0');
                std::fill(std::begin(ctx.type_to_index_), std::end(ctx.type_to_index_), size_t(0));
                std::fill(std::begin(ctx.spawn_x_), std::end(ctx.spawn_x_), int8_t(0));
                std::fill(std::begin(ctx.spawn_y_), std::end(ctx.spawn_y_), int8_t(0));
                bool seen[256] = {false};
                for (auto const &entry : ctx.opertion_)
                {
                    char t = entry.first.first;
                    int upper_index = int(::toupper(t)) + 128;
                    if (seen[upper_index])
                    {
                        continue;
                    }
                    seen[upper_index] = true;
                    ctx.index_to_type_[ctx.type_max_] = ::toupper(t);
                    ctx.type_to_index_[int(::tolower(t)) + 128] = ctx.type_max_;
                    ctx.type_to_index_[int(::toupper(t)) + 128] = ctx.type_max_;
                    auto spawn = TetrisRule::rule_spec::spawn(::toupper(t), width, height);
                    ctx.spawn_x_[int(::tolower(t)) + 128] = static_cast<int8_t>(spawn.first);
                    ctx.spawn_x_[int(::toupper(t)) + 128] = static_cast<int8_t>(spawn.first);
                    ctx.spawn_y_[int(::tolower(t)) + 128] = static_cast<int8_t>(spawn.second);
                    ctx.spawn_y_[int(::toupper(t)) + 128] = static_cast<int8_t>(spawn.second);
                    ++ctx.type_max_;
                }
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
        size_t type_max() const
        {
            return shared_context_->type_max();
        }
        size_t convert(char type) const
        {
            return shared_context_->convert(type);
        }
        char convert(size_t index) const
        {
            return shared_context_->convert(index);
        }
        int32_t width() const
        {
            return shared_context_->width();
        }
        int32_t height() const
        {
            return shared_context_->height();
        }
        uint32_t full() const
        {
            return shared_context_->full();
        }
        template<class LP,
                 std::enable_if_t < !std::is_pointer_v<std::remove_cvref_t<LP>> &&
                     requires(LP const &x)
        {
            x.state;
        }
        ,
            int > = 0 >
                    size_t attach(LP const &lp, TetrisMap &map) const
        {
            using Spec = typename SearchRuleSpecOf<TetrisSearch>::type;
            return m_tetris2::bb::Helpers<Spec>::attach_to_map(lp.state, map);
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
            if (TreeNode::lp_is_valid(root_->identity) && memory_usage() < memory_limit_)
            {
                return root_->template run<false>(&local_context_);
            }
            return true;
        }
        bool run_hold()
        {
            if (TreeNode::lp_is_valid(root_->identity) && memory_usage() < memory_limit_)
            {
                return root_->template run<true>(&local_context_);
            }
            return true;
        }
        //run!
        RunResult run(TetrisMap const &map, TetrisBlockStatus const &status, char const *next, size_t next_length, time_t limit = 100)
        {
            using namespace std::chrono;
            auto now = high_resolution_clock::now(), end = now + std::chrono::milliseconds(limit);
            root_ = root_->update(&local_context_, map, status_, status.t, next, next_length);
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
        RunResult run_hold(TetrisMap const &map, TetrisBlockStatus const &status, char hold, bool hold_free, char const *next, size_t next_length, time_t limit = 100)
        {
            using namespace std::chrono;
            auto now = high_resolution_clock::now(), end = now + std::chrono::milliseconds(limit);
            root_ = root_->update(&local_context_, map, status_, status.t, hold, !hold_free, next, next_length);
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
        //根据run的结果得到一个操作路径
        std::vector<char> make_path(TetrisBlockStatus const &status, LandPoint const &land_point, TetrisMap const &map, bool cut_drop = true)
        {
            using Spec = typename SearchRuleSpecOf<TetrisSearch>::type;
            using SpecHelpers = m_tetris2::bb::Helpers<Spec>;
            auto board = m_tetris2::bb::build_board_for_search<Spec>(map);
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
        void search(TetrisBlockStatus const &status, TetrisMap const &map, container_t &result)
        {
            using Spec = typename SearchRuleSpecOf<TetrisSearch>::type;
            using SpecHelpers = m_tetris2::bb::Helpers<Spec>;
            auto board = m_tetris2::bb::build_board_for_search<Spec>(map);
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

    // TetrisEngine2 已移至 src/tetris_engine2.h.
    // 需要使用时 include src/tetris_engine2.h (该文件已 include movegen_hook.h,
    // 因此 DeduceSpinPolicy 在定义点可见).

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
        bool backgrond_;
        bool running_;

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
            if (running_)
            {
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
                    size_t attach(LP const &lp, TetrisMap &map) const
        {
            return engine_.attach(lp, map);
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
            engine_.update();
        }
        //run!
        RunResult run(TetrisMap const &map, TetrisBlockStatus const &status, char const *next, size_t next_length, time_t limit = 100)
        {
            PauseBackground pause(this);
            ai_config_.assign(engine_.ai_config());
            search_config_.assign(engine_.search_config());
            *engine_.status() = status_;
            auto run_result = engine_.run(map, status, next, next_length, limit);
            start_work(false);
            return run_result;
        }
        //带hold的run!
        RunResult run_hold(TetrisMap const &map, TetrisBlockStatus const &status, char hold, bool hold_free, char const *next, size_t next_length, time_t limit = 100)
        {
            PauseBackground pause(this);
            ai_config_.assign(engine_.ai_config());
            search_config_.assign(engine_.search_config());
            *engine_.status() = status_;
            auto run_result = engine_.run_hold(map, status, hold, hold_free, next, next_length, limit);
            start_work(true);
            return run_result;
        }
        //根据run的结果得到一个操作路径
        std::vector<char> make_path(TetrisBlockStatus const &status, typename Engine::LandPoint const &land_point, TetrisMap const &map, bool cut_drop = true)
        {
            PauseBackground pause(this);
            return engine_.make_path(status, land_point, map, cut_drop);
        }
        //单块评价
        template<class container_t>
        void search(TetrisBlockStatus const &status, TetrisMap const &map, container_t &result)
        {
            PauseBackground pause(this);
            engine_.search(status, map, result);
        }
    };

    inline bool TetrisNode::check(TetrisMap const &map) const
    {
        row_t l = 0;
        switch (height)
        {
        default:
            assert(0);
        case 4:
            l |= ~map.row[row + 3] & data[3];
        case 3:
            l |= ~map.row[row + 2] & data[2];
        case 2:
            l |= ~map.row[row + 1] & data[1];
        case 1:
            l |= ~map.row[row + 0] & data[0];
        }
        return l == 0;
    }

    inline bool TetrisNode::check(TetrisMapSnap const &snap) const
    {
        return (snap.row[status.r][row] >> col) & 1;
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

    //==========================================================================
    // 编译期规则原语 (WallKickList / OpLines / OpDesc / RuleSpec / kOpRotateNone)
    // 已抽到 tetris_rule_spec.h. 这里仅保留运行时桥接层.
    //==========================================================================

    //==========================================================================
    // RuleSpec -> 运行时 std::map<{t,r}, TetrisOpertion> 的桥接层
    //
    // 让运行时桥接层继续复用原 init / get_opertion 接口,
    // 内部完全用 OpDesc / OpLines / WallKickList 描述方块,行为与之前等价.
    //==========================================================================

    namespace detail
    {
        //把编译期 OpDesc 翻译为一次 m_tetris2_rule_tools::create_node 调用
        //之所以拆出来是因为 create_node 的实现位于 m_tetris2_rule_tools 命名空间,
        //这里只声明,定义放在 tetris_core.cpp 的对应位置(避免循环包含).
        template<class Op>
        TetrisNode op_create_bridge(size_t w, size_t h, TetrisOpertion const &op);

        //把编译期 WallKickList 拷贝到运行时 TetrisWallKickOpertion
        template<class Wk>
        TetrisWallKickOpertion to_wallkick();

        //旋转目标 R != kOpRotateNone 时返回 rotate_template<R>,否则 nullptr.
        //通过偏特化避开 C++14 静态 constexpr 成员的 ODR-use 麻烦.
        using RotateFn = bool (*)(TetrisNode &, TetrisContext const *);
        template<uint8_t R, bool IsNone = (R == kOpRotateNone)>
        struct RotateSelector
        {
            static RotateFn get();
        };
        template<uint8_t R>
        struct RotateSelector<R, true>
        {
            static RotateFn get()
            {
                return nullptr;
            }
        };

        //把单个 OpDesc 平展成 TetrisOpertion
        template<class Op>
        TetrisOpertion to_opertion()
        {
            TetrisOpertion result = {};
            result.create = &op_create_bridge<Op>;
            result.rotate_clockwise = RotateSelector<Op::target_cw>::get();
            result.rotate_counterclockwise = RotateSelector<Op::target_ccw>::get();
            result.rotate_opposite = RotateSelector<Op::target_opp>::get();
            result.wall_kick_clockwise = to_wallkick<typename Op::wk_cw>();
            result.wall_kick_counterclockwise = to_wallkick<typename Op::wk_ccw>();
            result.wall_kick_opposite = to_wallkick<typename Op::wk_opp>();
            return result;
        }

        //tuple 展开:把 RuleSpec::ops 里每个 OpDesc 插入 std::map
        template<class Map, class Tuple, size_t I, size_t Size>
        struct OpFlattener
        {
            static void run(Map &m)
            {
                using Op = typename std::tuple_element<I, Tuple>::type;
                m.insert(std::make_pair(std::make_pair(Op::type, Op::rotation), to_opertion<Op>()));
                OpFlattener<Map, Tuple, I + 1, Size>::run(m);
            }
        };
        template<class Map, class Tuple, size_t Size>
        struct OpFlattener<Map, Tuple, Size, Size>
        {
            static void run(Map &) {}
        };
    }

    //把 RuleSpec 平展为 std::map<{t, r}, TetrisOpertion>
    template<class Rule>
    std::map<std::pair<char, uint8_t>, TetrisOpertion> flatten_rulespec()
    {
        std::map<std::pair<char, uint8_t>, TetrisOpertion> info;
        using Tup = typename Rule::ops;
        detail::OpFlattener<decltype(info), Tup, 0, std::tuple_size<Tup>::value>::run(info);
        return info;
    }
}

namespace m_tetris2_rule_tools
{
    using namespace m_tetris2;

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

//==========================================================================
// RuleSpec 桥接层模板定义
// 这些模板的实现依赖 m_tetris2_rule_tools::create_node / rotate_default,
// 所以放在 m_tetris2_rule_tools 声明之后.
//==========================================================================
namespace m_tetris2
{
    namespace detail
    {
        //把编译期的 OpLines<L0,L1,L2,L3> 与 OpDesc 信息翻译为一次 create_node 调用.
        //当前桥接层固定按 4 行矩阵调用(对应 m_tetris2_rule_tools::create_node 的接口).
        //RuleSpec::N != 4 的规则尚未接入桥接层,会在后续阶段(N 完全可变)放开.
        //
        //Op::spawn_x / Op::spawn_y: piece-local 初始化锚点, 不是游戏出生位置;
        //  此处把它直接交给 create_node 用于建立每个 piece 的 base 节点.
        //  游戏出生位置由 RuleSpec::spawn(PT, W, H) 提供, 与本桥接层无关.
        template<class Op>
        TetrisNode op_create_bridge(size_t w, size_t h, TetrisOpertion const &op)
        {
            using Lines = typename Op::lines;
            static_assert(Lines::size == 4, "Bridge layer currently only supports N==4 rules");
            return m_tetris2_rule_tools::create_node(
                w, h, Op::type, Op::spawn_x, Op::spawn_y, Op::rotation,
                Lines::data[0], Lines::data[1], Lines::data[2], Lines::data[3], op);
        }

        template<class Wk>
        TetrisWallKickOpertion to_wallkick()
        {
            TetrisWallKickOpertion result = {};
            result.length = static_cast<uint32_t>(Wk::length);
            for (size_t i = 0; i < Wk::length; ++i)
            {
                result.data[i].x = Wk::data[i * 2 + 0];
                result.data[i].y = Wk::data[i * 2 + 1];
            }
            return result;
        }

        template<uint8_t R, bool IsNone>
        RotateFn RotateSelector<R, IsNone>::get()
        {
            return &m_tetris2_rule_tools::rotate_template<R>;
        }
    }
}
