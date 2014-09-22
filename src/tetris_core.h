
#pragma once

#include <unordered_map>
#include <vector>
#include <map>
#include <algorithm>
#include <iterator>
#include <functional>
#include <cassert>

namespace m_tetris
{
    const int max_height = 40;
    const int max_wall_kick = 4;

    struct TetrisNode;
    struct TetrisWallKickOpertion;
    struct TetrisOpertion;
    struct TetrisMap;
    union TetrisBlockStatus;
    class TetrisContext;

    //��Ϸ����,�±��0��ʼ,���½�Ϊԭ��,���֧��[�߶�=40,���=32]
    struct TetrisMap
    {
        //������,�����÷���full������...
        int row[max_height];
        //ÿһ�еĸ߶�
        int top[32];
        //������
        int width;
        //������
        int height;
        //����Ŀǰ���߶�
        int roof;
        //�����ķ�����
        int count;
        //�ж�[x,y]�����Ƿ��з���
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

    //����״̬
    //t:OISZLJT�ַ�
    //[x,y]����,yԽ��߶�Խ��
    //r:��ת״̬(0-3)
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

    //��ǽ��
    struct TetrisWallKickOpertion
    {
        struct WallKickNode
        {
            short int x, y;
        };
        size_t length;
        WallKickNode data[max_wall_kick];
    };

    //�������
    struct TetrisOpertion
    {
        //����һ������
        TetrisNode(*create)(int w, int h, TetrisOpertion const &op);
        //˳ʱ����ת(����)
        bool(*rotate_clockwise)(TetrisNode &node, TetrisContext const *context);
        //��ʱ����ת(����)
        bool(*rotate_counterclockwise)(TetrisNode &node, TetrisContext const *context);
        //ת��180��
        bool(*rotate_opposite)(TetrisNode &node, TetrisContext const *context);
        //����
        bool(*move_left)(TetrisNode &node, TetrisContext const *context);
        //����
        bool(*move_right)(TetrisNode &node, TetrisContext const *context);
        //����
        bool(*move_down)(TetrisNode &node, TetrisContext const *context);
        //˳ʱ����ת��ǽ
        TetrisWallKickOpertion wall_kick_clockwise;
        //��ʱ����ת��ǽ
        TetrisWallKickOpertion wall_kick_counterclockwise;
        //ת��180����ǽ
        TetrisWallKickOpertion wall_kick_opposite;
    };

    //ָ�����ڵ�
    struct TetrisNode
    {
        //����״̬
        TetrisBlockStatus status;
        //�����������
        TetrisOpertion op;
        //����ÿ�е�����
        int data[4];
        //����ÿ�е����ظ߶�
        int top[4];
        //����ÿ�е����ظ߶�
        int bottom[4];
        //�����ڳ����еľ���λ��
        char row, height, col, width;
        //���ֱ��λᴥ��������͸߶�
        int low;

        //ָ��������
        //����ȡ����ϣ���hash

        size_t index;
        size_t index_filtered;

        //������������Ż�
        std::vector<TetrisNode const *> const *land_point;

        //������ָ����������
        //��Ӧ��������ɵ����ݸı�ȫ��Ԥ�ú�,����Ҫ�ټ���
        //���Ϊ��,��ʾ�Ѿ����ﳡ���߽���߲�֧�ָò���

        TetrisNode const *rotate_clockwise;
        TetrisNode const *rotate_counterclockwise;
        TetrisNode const *rotate_opposite;
        TetrisNode const *move_left;
        TetrisNode const *move_right;
        TetrisNode const *move_down;
        TetrisNode const *move_down_multi[max_height];

        //��ǽ����,���γ���
        //����nullptr,��ʾ���н���

        TetrisNode const *wall_kick_clockwise[max_wall_kick];
        TetrisNode const *wall_kick_counterclockwise[max_wall_kick];
        TetrisNode const *wall_kick_opposite[max_wall_kick];

        //������...�����Ҫ����ô?
        TetrisContext const *context;

        //��鵱ǰ���Ƿ��ܹ��ϲ��볡��
        bool check(TetrisMap const &map) const;
        //��鵱ǰ���Ƿ���¶���
        bool open(TetrisMap const &map) const;
        //��ǰ��ϲ��볡��,ͬʱ���³�������
        size_t attach(TetrisMap &map) const;
        //̽��ϲ������������
        int clear_low(TetrisMap &map) const;
        //̽��ϲ������������
        int clear_high(TetrisMap &map) const;
        //���㵱ǰ����λ��
        TetrisNode const *drop(TetrisMap const &map) const;
    };

    //�ڵ���.���ѵ�ʱ��ʹ��
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
        bool mark(TetrisNode const *key);
    };

    //�ڵ���.������λ����ͬ�Ľڵ�
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
        bool mark(TetrisNode const *key);
    };

    template<class TetrisRuleSet, class AIParam>
    struct TetrisContextBuilder;

    //�����Ķ���.������С�ı�����Ҫ���³�ʼ��������
    class TetrisContext
    {
        template<class TetrisRuleSet, class AIParam>
        friend struct TetrisContextBuilder;
    private:
        TetrisContext()
        {
        }
        //ָ��������
        std::unordered_map<TetrisBlockStatus, TetrisNode, TetrisBlockStatusHash, TetrisBlockStatusEqual> node_cache_;

        //������Ϣ

        std::map<std::pair<unsigned char, unsigned char>, TetrisOpertion> init_opertion_;
        std::map<unsigned char, TetrisBlockStatus(*)(TetrisContext const *)> init_generate_;
        std::map<unsigned char, TetrisBlockStatus(*)(TetrisContext const *)> game_generate_;

        //��,��ʲô��...
        int width_, height_;
        //����
        int full_;

        //һЩ���ڼ��ٵ�����...
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
        //׼����������,����fail��ʾ���´���
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
    
    //�������۲���...
    template<class... types>
    struct EvalParam;
    template<class LandPointEval>
    struct EvalParam<LandPointEval>
    {
        EvalParam()
        {
        }
        EvalParam(TetrisNode const *_node, size_t _clear, TetrisMap const &_map, LandPointEval const &_eval) : node(_node), clear(_clear), map(&_map), eval(_eval)
        {
        }
        TetrisNode const *node;
        size_t clear;
        TetrisMap const *map;
        LandPointEval eval;
    };
    template<>
    struct EvalParam<>
    {
        EvalParam()
        {
        }
        EvalParam(TetrisNode const *_node, size_t _clear, TetrisMap const &_map) : node(_node), clear(_clear), map(&_map)
        {
        }
        TetrisNode const *node;
        size_t clear;
        TetrisMap const *map;
    };

    //��֦����...
    template<class MapEval>
    struct PruneParam
    {
        PruneParam(MapEval const &_eval, TetrisNode const *_land_point) : eval(_eval), land_point(_land_point)
        {
        }
        MapEval eval;
        TetrisNode const *land_point;
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
    struct TetrisAIHasLandPointEval
    {
        struct Fallback
        {
            int eval_land_point;
        };
        struct Derived : TetrisAI, Fallback
        {
        };
        template<typename U, U> struct Check;
        template<typename U>
        static std::false_type func(Check<int Fallback::*, &U::eval_land_point> *);
        template<typename U>
        static std::true_type func(...);
    public:
        typedef decltype(func<Derived>(nullptr)) type;
    };

    template<class TetrisAI>
    struct TetrisAIHasGetVirtualValue
    {
        struct Fallback
        {
            int get_virtual_eval;
        };
        struct Derived : TetrisAI, Fallback
        {
        };
        template<typename U, U> struct Check;
        template<typename U>
        static std::false_type func(Check<int Fallback::*, &U::get_virtual_eval> *);
        template<typename U>
        static std::true_type func(...);
    public:
        typedef decltype(func<Derived>(nullptr)) type;
    };

    template<class TetrisAI>
    struct TetrisAIHasPruneMap
    {
        struct Fallback
        {
            int prune_map;
        };
        struct Derived : TetrisAI, Fallback
        {
        };
        template<typename U, U> struct Check;
        template<typename U>
        static std::false_type func(Check<int Fallback::*, &U::prune_map> *);
        template<typename U>
        static std::true_type func(...);
    public:
        typedef decltype(func<Derived>(nullptr)) type;
    };

    struct TetrisAIEmptyParam
    {
    };
    template<class TetrisRuleSet, class AIParam>
    struct TetrisContextBuilder
    {
        class AIParamHolder
        {
        private:
            AIParam param_;
        public:
            AIParam const *get_param() const
            {
                return &param_;
            }
            AIParam *get_param()
            {
                return &param_;
            }
        };
        class TetrisContextWithParam : public TetrisContext, public AIParamHolder
        {
        };
        static TetrisContextWithParam *build_context()
        {
            TetrisContextWithParam *context = new TetrisContextWithParam();
            context->init_opertion_ = TetrisRuleSet::get_init_opertion();
            context->init_generate_ = TetrisRuleSet::get_init_generate();
            context->game_generate_ = TetrisRuleSet::get_game_generate();
            return context;
        }
        template<class TetrisAI>
        static void init_ai(TetrisAI &ai, TetrisContextWithParam const *context)
        {
            TetrisCallInit<TetrisAI>(ai, context, context->get_param());
        }
    };
    template<class TetrisRuleSet>
    struct TetrisContextBuilder<TetrisRuleSet, TetrisAIEmptyParam>
    {
        class AIParamHolder
        {
        public:
            void const *get_param() const
            {
                return nullptr;
            }
            void *get_param()
            {
                return nullptr;
            }
        };
        class TetrisContextWithParam : public TetrisContext, public AIParamHolder
        {
        };
        static TetrisContextWithParam *build_context()
        {
            TetrisContextWithParam *context = new TetrisContextWithParam();
            context->init_opertion_ = TetrisRuleSet::get_init_opertion();
            context->init_generate_ = TetrisRuleSet::get_init_generate();
            context->game_generate_ = TetrisRuleSet::get_game_generate();
            return context;
        }
        template<class TetrisAI>
        static void init_ai(TetrisAI &ai, TetrisContextWithParam const *context)
        {
            TetrisCallInit<TetrisAI>(ai, context);
        }
    };

    //���������
    template<class TetrisAI, class TetrisLandPointSearchEngine, class HasLandPointEval>
    struct TetrisCore
    {
    public:
        typedef decltype(TetrisAI().eval_land_point(nullptr, TetrisMap(), TetrisMap(), 0)) LandPointEval;
        typedef decltype(TetrisAI().eval_map(TetrisMap(), nullptr, 0)) MapEval;
        typedef EvalParam<LandPointEval> EvalParam;
        typedef std::pair<TetrisNode const *, MapEval> Result;

        void build_param(TetrisAI &ai, TetrisMap &map, TetrisMap &src_map, TetrisNode const *node, EvalParam &param)
        {
            map = src_map;
            param.map = &map;
            param.clear = node->attach(map);
            param.node = node;
            param.eval = ai.eval_land_point(param.node, map, src_map, param.clear);
        }
        MapEval run(TetrisAI &ai, TetrisMap const &map, TetrisMap &src_map, EvalParam &param, std::vector<EvalParam> &history)
        {
            param.eval = ai.eval_land_point(param.node, map, src_map, param.clear);
            history.push_back(param);
            MapEval eval = ai.eval_map(map, history.data(), history.size());
            history.pop_back();
            return eval;
        }
        Result run(TetrisAI &ai, TetrisLandPointSearchEngine &search, TetrisMap const &map, TetrisNode const *node)
        {
            std::vector<TetrisNode const *> const *land_point = search.search(map, node);
            MapEval eval = ai.eval_map_bad();
            TetrisNode const *best_node = node;
            EvalParam param;
            for(auto cit = land_point->begin(); cit != land_point->end(); ++cit)
            {
                node = *cit;
                TetrisMap copy = map;
                size_t clear = node->attach(copy);
                param = EvalParam(node, clear, map, ai.eval_land_point(node, copy, map, clear));
                MapEval new_eval = ai.eval_map(copy, &param, 1);
                if(new_eval > eval)
                {
                    eval = new_eval;
                    best_node = node;
                }
            }
            return std::make_pair(best_node, eval);
        }
    };

    //���������
    template<class TetrisAI, class TetrisLandPointSearchEngine>
    struct TetrisCore<TetrisAI, TetrisLandPointSearchEngine, std::false_type>
    {
    public:
        typedef decltype(TetrisAI().eval_map(TetrisMap(), nullptr, 0)) MapEval;
        typedef EvalParam<> EvalParam;
        typedef std::pair<TetrisNode const *, MapEval> Result;

        void build_param(TetrisAI &ai, TetrisMap &map, TetrisMap &src_map, TetrisNode const *node, EvalParam &param)
        {
            map = src_map;
            param.map = &map;
            param.clear = node->attach(map);
            param.node = node;
        }
        MapEval run(TetrisAI &ai, TetrisMap const &map, TetrisMap &src_map, EvalParam &param, std::vector<EvalParam> &history)
        {
            history.push_back(param);
            MapEval eval = ai.eval_map(map, history.data(), history.size());
            history.pop_back();
            return eval;
        }
        Result run(TetrisAI &ai, TetrisLandPointSearchEngine &search, TetrisMap const &map, TetrisNode const *node)
        {
            std::vector<TetrisNode const *> const *land_point = search.search(map, node);
            MapEval eval = ai.eval_map_bad();
            TetrisNode const *best_node = node;
            EvalParam param;
            for(auto cit = land_point->begin(); cit != land_point->end(); ++cit)
            {
                node = *cit;
                TetrisMap copy = map;
                param = EvalParam(node, node->attach(copy), map);
                MapEval new_eval = ai.eval_map(copy, &param, 1);
                if(new_eval > eval)
                {
                    eval = new_eval;
                    best_node = node;
                }
            }
            return std::make_pair(best_node, eval);
        }
    };
    
    template<class MapEval, class EvalParam, class TetrisAI, class TetrisLandPointSearchEngine>
    struct TetrisTreeNode
    {
        typedef TetrisCore<TetrisAI, TetrisLandPointSearchEngine, typename TetrisAIHasLandPointEval<TetrisAI>::type> Core;
        struct Context
        {
            Context() : version(), is_complete(), is_open_hold(), width(), total(), avg()
            {
            }
            size_t version;
            TetrisContext const *context;
            TetrisAI *ai;
            TetrisLandPointSearchEngine *search;
            std::vector<std::vector<TetrisTreeNode *>> deepth;
            std::vector<std::vector<TetrisTreeNode *>> best;
            std::vector<TetrisTreeNode *> temp_level;
            std::vector<EvalParam> history;
            bool is_complete;
            bool is_open_hold;
            size_t width;
            std::function<TetrisTreeNode *(TetrisTreeNode *)> alloc;
            std::function<void (TetrisTreeNode *)> dealloc;
            double total;
            double avg;
        };
        struct ChildrenSortByEval
        {
            bool operator ()(TetrisTreeNode const *const &left, TetrisTreeNode const *const &right)
            {
                return left->eval > right->eval;
            }
        };
        struct ChildrenSortByStatus
        {
            bool operator ()(TetrisTreeNode const *const &left, TetrisTreeNode const *const &right)
            {
                return TetrisBlockStatusCompare()(left->param.node->status, right->param.node->status);
            }
            bool operator ()(TetrisBlockStatus const &left, TetrisTreeNode const *const &right)
            {
                return TetrisBlockStatusCompare()(left, right->param.node->status);
            }
            bool operator ()(TetrisTreeNode const *const &left, TetrisBlockStatus const &right)
            {
                return TetrisBlockStatusCompare()(left->param.node->status, right);
            }
        };
        TetrisTreeNode(Context *_context) : context(_context), version(context->version - 1), eval(context->ai->eval_map_bad()), parent(), param(), hold(' '), level(), flag()
        {
        }
        Context *context;
        size_t version;
        TetrisMap map;
        EvalParam param;
        MapEval eval;
        TetrisTreeNode *parent;
        std::vector<TetrisTreeNode *> children;
        std::vector<TetrisTreeNode *> children_old;
        std::vector<TetrisNode const *> land_point;
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
        TetrisTreeNode *update(TetrisMap const &_map, TetrisNode const *_node, unsigned char *_next, size_t _next_length)
        {
            TetrisTreeNode *root = update_root(_map);
            if(root != this || context->is_open_hold || _node != root->node || _next_length != root->next.size() || std::memcmp(_next, root->next.data(), _next_length) != 0)
            {
                ++context->version;
                context->total += context->width;
                context->avg = context->total / context->version;
                context->width = 0;
                for(auto &level : context->deepth)
                {
                    level.clear();
                }
                for(auto &level : context->best)
                {
                    level.clear();
                }
            }
            context->is_open_hold = false;
            root->node = _node;
            root->next.assign(_next, _next + _next_length);
            return root;
        }
        TetrisTreeNode *update(TetrisMap const &_map, TetrisNode const *_node, unsigned char _hold, bool _hold_lock, unsigned char *_next, size_t _next_length)
        {
            TetrisTreeNode *root = update_root(_map);
            if(root != this || !context->is_open_hold || _node != root->node || _hold != root->hold || !!_hold_lock != root->is_hold_lock || _next_length != root->next.size() || std::memcmp(_next, root->next.data(), _next_length) != 0)
            {
                ++context->version;
                context->total += context->width;
                context->avg = context->total / context->version;
                context->width = 0;
                for(auto &level : context->deepth)
                {
                    level.clear();
                }
                for(auto &level : context->best)
                {
                    level.clear();
                }
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
            if(land_point.empty())
            {
                land_point.push_back(node);
                for(auto land_point_node : *context->search->search(map, node))
                {
                    TetrisTreeNode *child = context->alloc(this);
                    Core().build_param(*context->ai, child->map, map, land_point_node, child->param);
                    child->is_hold = hold_control;
                    children.push_back(child);
                }
            }
            else if(land_point.size() != 1 || land_point.front() != node)
            {
                land_point.clear();
                land_point.push_back(node);
                children_old.swap(children);
                std::sort(children_old.begin(), children_old.end(), ChildrenSortByStatus());
                for(auto land_point_node : *context->search->search(map, node))
                {
                    TetrisTreeNode *child;
                    auto find = std::lower_bound(children_old.begin(), children_old.end(), land_point_node->status, ChildrenSortByStatus());
                    if(find != children_old.end() && (*find)->param.node == land_point_node)
                    {
                        child = *find;
                        children_old.erase(find);
                    }
                    else
                    {
                        child = context->alloc(this);
                        Core().build_param(*context->ai, child->map, map, land_point_node, child->param);
                    }
                    child->is_hold = hold_control;
                    children.push_back(child);
                }
                for(auto child : children_old)
                {
                    context->dealloc(child);
                }
                children_old.clear();
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
                hold = hold_save;
                next.swap(next_save);
                is_hold_lock = false;
                return;
            }
            if(node->status.t == hold)
            {
                TetrisNode const *hold_node = context->context->generate(hold);
                if(land_point.empty())
                {
                    land_point.push_back(node);
                    land_point.push_back(hold_node);
                    for(auto land_point_node : *context->search->search(map, node))
                    {
                        TetrisTreeNode *child = context->alloc(this);
                        Core().build_param(*context->ai, child->map, map, land_point_node, child->param);
                        child->is_hold = hold_control;
                        children.push_back(child);
                    }
                    std::sort(children.begin(), children.end(), ChildrenSortByStatus());
                    for(auto land_point_node : *context->search->search(map, hold_node))
                    {
                        auto find = std::lower_bound(children.begin(), children.end(), land_point_node->status, ChildrenSortByStatus());
                        if(find != children.end() && (*find)->param.node == land_point_node)
                        {
                            continue;
                        }
                        TetrisTreeNode *child = context->alloc(this);
                        Core().build_param(*context->ai, child->map, map, land_point_node, child->param);
                        child->is_hold = !hold_control;
                        children.push_back(child);
                    }
                }
                else if(land_point.size() != 2 || ((land_point[0] != node || land_point[1] != hold_node) && (land_point[0] != hold_node || land_point[1] != node)))
                {
                    land_point.clear();
                    land_point.push_back(node);
                    land_point.push_back(hold_node);
                    children_old.swap(children);
                    std::sort(children_old.begin(), children_old.end(), ChildrenSortByStatus());
                    for(auto land_point_node : *context->search->search(map, node))
                    {
                        TetrisTreeNode *child;
                        auto find = std::lower_bound(children_old.begin(), children_old.end(), land_point_node->status, ChildrenSortByStatus());
                        if(find != children_old.end() && (*find)->param.node == land_point_node)
                        {
                            child = *find;
                            children_old.erase(find);
                        }
                        else
                        {
                            child = context->alloc(this);
                            Core().build_param(*context->ai, child->map, map, land_point_node, child->param);
                        }
                        child->is_hold = hold_control;
                        children.push_back(child);
                    }
                    for(auto land_point_node : *context->search->search(map, hold_node))
                    {
                        auto find = std::lower_bound(children.begin(), children.end(), land_point_node->status, ChildrenSortByStatus());
                        if(find != children.end() && (*find)->param.node == land_point_node)
                        {
                            continue;
                        }
                        TetrisTreeNode *child;
                        find = std::lower_bound(children_old.begin(), children_old.end(), land_point_node->status, ChildrenSortByStatus());
                        if(find != children_old.end() && (*find)->param.node == land_point_node)
                        {
                            child = *find;
                            children_old.erase(find);
                        }
                        else
                        {
                            child = context->alloc(this);
                            Core().build_param(*context->ai, child->map, map, land_point_node, child->param);
                        }
                        child->is_hold = !hold_control;
                        children.push_back(child);
                    }
                    for(auto child : children_old)
                    {
                        context->dealloc(child);
                    }
                    children_old.clear();
                }
            }
            else if(node->status.t != hold)
            {
                TetrisNode const *hold_node = context->context->generate(hold);
                if(land_point.empty())
                {
                    land_point.push_back(node);
                    land_point.push_back(hold_node);
                    for(auto land_point_node : *context->search->search(map, node))
                    {
                        TetrisTreeNode *child = context->alloc(this);
                        Core().build_param(*context->ai, child->map, map, land_point_node, child->param);
                        child->is_hold = hold_control;
                        children.push_back(child);
                    }
                    for(auto land_point_node : *context->search->search(map, hold_node))
                    {
                        TetrisTreeNode *child = context->alloc(this);
                        Core().build_param(*context->ai, child->map, map, land_point_node, child->param);
                        child->is_hold = !hold_control;
                        children.push_back(child);
                    }
                }
                else if(land_point.size() != 2 || ((land_point[0] != node || land_point[1] != hold_node) && (land_point[0] != hold_node || land_point[1] != node)))
                {
                    land_point.clear();
                    land_point.push_back(node);
                    land_point.push_back(hold_node);
                    std::vector<TetrisTreeNode *> children_old;
                    children_old.swap(children);
                    std::sort(children_old.begin(), children_old.end(), ChildrenSortByStatus());
                    for(auto land_point_node : *context->search->search(map, node))
                    {
                        TetrisTreeNode *child;
                        auto find = std::lower_bound(children_old.begin(), children_old.end(), land_point_node->status, ChildrenSortByStatus());
                        if(find != children_old.end() && (*find)->param.node == land_point_node)
                        {
                            child = *find;
                            children_old.erase(find);
                        }
                        else
                        {
                            child = context->alloc(this);
                            Core().build_param(*context->ai, child->map, map, land_point_node, child->param);
                        }
                        child->is_hold = hold_control;
                        children.push_back(child);
                    }
                    for(auto land_point_node : *context->search->search(map, hold_node))
                    {
                        TetrisTreeNode *child;
                        auto find = std::lower_bound(children_old.begin(), children_old.end(), land_point_node->status, ChildrenSortByStatus());
                        if(find != children_old.end() && (*find)->param.node == land_point_node)
                        {
                            child = *find;
                            children_old.erase(find);
                        }
                        else
                        {
                            child = context->alloc(this);
                            Core().build_param(*context->ai, child->map, map, land_point_node, child->param);
                        }
                        child->is_hold = !hold_control;
                        children.push_back(child);
                    }
                    for(auto child : children_old)
                    {
                        context->dealloc(child);
                    }
                    children_old.clear();
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
            if(!node->check(map))
            {
                is_dead = true;
                return false;
            }
            (this->*search_ptr)(false);
            auto &ai = context->ai;
            std::vector<EvalParam> &history = context->history;
            size_t deepth = level;
            history.resize(deepth);
            TetrisTreeNode *node_param = this;
            while(deepth-- > 0)
            {
                history[deepth] = node_param->param;
                node_param = node_param->parent;
            }
            for(auto child : children)
            {
                child->eval = Core().run(*context->ai, child->map, map, child->param, history);
            }
            std::sort(children.begin(), children.end(), ChildrenSortByEval());
            return true;
        }
        bool run()
        {
            class TiltIterator
            {
            private:
                size_t size_;
                size_t current_;
            public:
                TiltIterator(size_t size) : size_(size), current_(1)
                {
                    assert(size < (1 << 31));
                }
                bool end()
                {
                    return current_ > size_;
                }
                void next()
                {
                    current_ <<= 1;
                    if(current_ > size_)
                    {
                        current_ >>= 1;
                        while(!(current_ & 1))
                        {
                            current_ >>= 1;
                        }
                        current_ += 2;
                    }
                }
                size_t value()
                {
                    return current_ - 1;
                }
            };
            assert(parent == nullptr);
            assert(!next.empty());
            size_t next_length = next.size();
            if(context->is_open_hold && hold != ' ')
            {
                ++next_length;
            }
            size_t next_length_max = next_length;
            context->deepth.resize(next_length);
            context->best.resize(next_length);
            if(context->is_complete)
            {
                return false;
            }
            size_t prune_hold = ++context->width;
            size_t prune_hold_max = prune_hold * 36 / 10;
            bool complete = true;
            build_children();
            std::vector<TetrisTreeNode *> *level = &children, &temp_level = context->temp_level;
            while(next_length-- > 0)
            {
                size_t level_prune_hold = prune_hold_max * next_length / next_length_max + prune_hold;
                std::vector<TetrisTreeNode *> *next_level = &context->deepth[next_length];
                std::vector<TetrisTreeNode *> *best = &context->best[next_length];
                if(level_prune_hold <= level->size())
                {
                    complete = false;
                }
                for(TiltIterator it(level->size()); level_prune_hold != 0 && !it.end(); it.next())
                {
                    TetrisTreeNode *child = (*level)[it.value()];
                    if(!child->is_dead)
                    {
                        --level_prune_hold;
                    }
                    if(!child->build_children() || std::binary_search(best->begin(), best->end(), child))
                    {
                        continue;
                    }
                    best->insert(std::lower_bound(best->begin(), best->end(), child), child);
                    std::vector<TetrisTreeNode *> &child_children = child->children;
                    temp_level.resize(next_level->size() + child_children.size());
                    std::merge(next_level->begin(), next_level->end(), child_children.begin(), child_children.end(), temp_level.begin(), ChildrenSortByEval());
                    next_level->swap(temp_level);
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
        std::pair<TetrisTreeNode const *, MapEval> get_best()
        {
            TetrisTreeNode *node = nullptr;
            for(auto &level : context->deepth)
            {
                if(!level.empty())
                {
                    node = level.front();
                    break;
                }
            }
            if(node == nullptr)
            {
                return std::make_pair(nullptr, context->ai->eval_map_bad());
            }
            while(node->parent->parent != nullptr)
            {
                node = node->parent;
            }
            return std::make_pair(node, node->eval);
        }
    };

    template<class TetrisRuleSet, class TetrisAI, class TetrisLandPointSearchEngine, class TetrisAIParam = TetrisAIEmptyParam>
    class TetrisEngine
    {
    private:
        typedef TetrisCore<TetrisAI, TetrisLandPointSearchEngine, typename TetrisAIHasLandPointEval<TetrisAI>::type> Core;
        typedef TetrisTreeNode<typename Core::MapEval, typename Core::EvalParam, TetrisAI, TetrisLandPointSearchEngine> TreeNode;
        typename TetrisContextBuilder<TetrisRuleSet, TetrisAIParam>::TetrisContextWithParam *context_;
        TetrisAI ai_;
        TetrisLandPointSearchEngine search_;
        typename TreeNode::Context tree_context_;
        TreeNode *tree_root_;
        std::vector<TreeNode *> tree_cache_;

    private:
        TreeNode *alloc(TreeNode *parent)
        {
            TreeNode *node;
            if(!tree_cache_.empty())
            {
                node = tree_cache_.back();
                tree_cache_.pop_back();
                node->version = tree_context_.version - 1;
            }
            else
            {
                node = new TreeNode(&tree_context_);
            }
            node->parent = parent;
            return node;
        }
        void dealloc(TreeNode *node)
        {
            node->eval = ai_.eval_map_bad();
            for(auto child : node->children)
            {
                dealloc(child);
            }
            node->children.clear();
            node->land_point.clear();
            node->hold = ' ';
            node->flag = 0;
            node->next.clear();
            tree_cache_.push_back(node);
        }

    public:
        struct RunResult
        {
            RunResult(typename Core::MapEval const &_eval) : target(), eval(_eval), change_hold()
            {
            }
            RunResult(typename Core::MapEval const &_eval, bool _change_hold) : target(), eval(_eval), change_hold(_change_hold)
            {
            }
            RunResult(std::pair<TreeNode const *, typename Core::MapEval> const &_result) : target(_result.first ? _result.first->param.node : nullptr), eval(_result.second), change_hold()
            {
            }
            RunResult(std::pair<TetrisNode const *, typename Core::MapEval> const &_result) : target(_result.first), eval(_result.second), change_hold()
            {
            }
            RunResult(std::pair<TreeNode const *, typename Core::MapEval> const &_result, bool _change_hold) : target(_result.first ? _result.first->param.node : nullptr), eval(_result.second), change_hold(_change_hold)
            {
            }
            RunResult(std::pair<TetrisNode const *, typename Core::MapEval> const &_result, bool _change_hold) : target(_result.first), eval(_result.second), change_hold(_change_hold)
            {
            }
            TetrisNode const *target;
            typename Core::MapEval eval;
            bool change_hold;
        };

    public:
        TetrisEngine() : context_(TetrisContextBuilder<TetrisRuleSet, TetrisAIParam>::build_context()), ai_(), tree_root_(new TreeNode(&tree_context_))
        {
            tree_context_.context = context_;
            tree_context_.ai = &ai_;
            tree_context_.search = &search_;
            tree_context_.alloc = std::bind(&TetrisEngine::alloc, this, std::placeholders::_1);
            tree_context_.dealloc = std::bind(&TetrisEngine::dealloc, this, std::placeholders::_1);
        }
        ~TetrisEngine()
        {
            dealloc(tree_root_);
            for(auto node : tree_cache_)
            {
                delete node;
            }
            delete context_;
        }
        //��״̬��ȡ��ǰ��
        TetrisNode const *get(TetrisBlockStatus const &status) const
        {
            return context_->get(status);
        }
        //�����Ķ���...������ʲô��= =?
        TetrisContext const *context() const
        {
            return context_;
        }
        //AI����
        std::string ai_name() const
        {
            return ai_.ai_name();
        }
        TetrisAIParam *param()
        {
            return context_->get_param();
        }
        //׼����������
        bool prepare(int width, int height)
        {
            if(!TetrisRuleInit<TetrisRuleSet>::init(TetrisRuleSet(), width, height))
            {
                return false;
            }
            TetrisContext::PrepareResult result = context_->prepare(width, height);
            if(result == TetrisContext::rebuild)
            {
                TetrisContextBuilder<TetrisRuleSet, TetrisAIParam>::init_ai(ai_, context_);
                TetrisCallInit<TetrisLandPointSearchEngine>(search_, context_);
                return true;
            }
            else if(result == TetrisContext::fail)
            {
                return false;
            }
            return true;
        }
        //run!
        RunResult run(TetrisMap const &map, TetrisNode const *node, unsigned char *next, size_t next_length, time_t limit = 100)
        {
            if(node == nullptr || !node->check(map))
            {
                return RunResult(ai_.eval_map_bad());
            }
            if(next_length == 0)
            {
                return RunResult(Core().run(ai_, search_, map, node));
            }
            else
            {
                time_t end = clock() + limit;
                tree_root_ = tree_root_->update(map, node, next, next_length);
                do
                {
                    if(tree_root_->run())
                    {
                        break;
                    }
                } while(clock() < end);
                return RunResult(tree_root_->get_best());
            }
        }
        //��hold��run!
        RunResult run_hold(TetrisMap const &map, TetrisNode const *node, unsigned char hold, bool hold_free, unsigned char *next, size_t next_length, time_t limit = 100)
        {
            if(node == nullptr || !node->check(map))
            {
                return RunResult(ai_.eval_map_bad());
            }
            if(next_length == 0)
            {
                if(hold_free)
                {
                    if(hold == ' ')
                    {
                        return RunResult(ai_.eval_map_bad(), true);
                    }
                    else
                    {
                        auto node_result = Core().run(ai_, search_, map, node);
                        auto hold_result = Core().run(ai_, search_, map, context_->generate(hold));
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
                    return RunResult(Core().run(ai_, search_, map, node));
                }
            }
            time_t end = clock() + limit;
            tree_root_ = tree_root_->update(map, node, hold, !hold_free, next, next_length);
            do
            {
                if(tree_root_->run())
                {
                    break;
                }
            } while(clock() < end);
            auto best = tree_root_->get_best();
            return RunResult(best, best.first == nullptr ? false : best.first->is_hold);
        }
        //����run�Ľ���õ�һ������·��
        std::vector<char> path(TetrisNode const *node, TetrisNode const *land_point, TetrisMap const &map)
        {
            return search_.make_path(node, land_point, map);
        }
    };

}

namespace m_tetris_rule_tools
{
    using namespace m_tetris;


    //����һ���ڵ�(ֻ֧��4x4����,��������˾�������)
    TetrisNode create_node(int w, int h, unsigned char T, char X, char Y, unsigned char R, int line1, int line2, int line3, int line4, TetrisOpertion const &op);

    //����һ���ڵ�(ֻ֧��4x4����,��������˾�������)
    template<unsigned char T, char X, char Y, unsigned char R, int line1, int line2, int line3, int line4>
    TetrisNode create_node(int w, int h, TetrisOpertion const &op)
    {
        static_assert(X < 0 || X >= 4 || Y < 0 || Y >= 4 || (line1 || line2 || line3 || line3), "data error");
        return create_node(w, h, T, X, Y, R, line1, line2, line3, line4, op);
    }

    //һ��ͨ�õ���ת
    bool rotate_default(TetrisNode &node, unsigned char R, TetrisContext const *context);

    //һ��ͨ�õ���תģ��
    template<unsigned char R>
    bool rotate_template(TetrisNode &node, TetrisContext const *context)
    {
        return rotate_default(node, R, context);
    }

    //����,����,����,����...

    bool move_left(TetrisNode &node, TetrisContext const *context);
    bool move_right(TetrisNode &node, TetrisContext const *context);
    bool move_up(TetrisNode &node, TetrisContext const *context);
    bool move_down(TetrisNode &node, TetrisContext const *context);
}