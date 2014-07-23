
#pragma once

#include <unordered_map>
#include <vector>
#include <map>
#include <algorithm>
#include <functional>

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
        TetrisNode(*create)(int w, int h, TetrisOpertion op);
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
        EvalParam(TetrisNode const *_node, size_t _clear, TetrisMap const &_map, LandPointEval const &_eval) : node(_node), clear(_clear), map(_map), eval(_eval)
        {
        }
        TetrisNode const *node;
        size_t clear;
        TetrisMap const &map;
        LandPointEval const &eval;
    };
    template<>
    struct EvalParam<>
    {
        EvalParam(TetrisNode const *_node, size_t _clear, TetrisMap const &_map) : node(_node), clear(_clear), map(_map)
        {
        }
        TetrisNode const *node;
        size_t clear;
        TetrisMap const &map;
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

    //֮����һ��Ϊ�����ܸ����������...
    template<class TetrisAI, class TetrisLandPointSearchEngine, size_t NextLength, class HasLandPointEval, class HasGetVirtualValue, class HasPruneMap>
    class TetrisCore;

    template<class TetrisAI, class TetrisLandPointSearchEngine, size_t NextLength, class HasLandPointEval, class HasGetVirtualValue, class HasPruneMap>
    struct TetrisCoreSelect
    {
        typedef TetrisCore<TetrisAI, TetrisLandPointSearchEngine, NextLength, HasLandPointEval, HasGetVirtualValue, HasPruneMap> type;
    };
    template<class TetrisAI, class TetrisLandPointSearchEngine, class HasLandPointEval, class HasGetVirtualValue, class HasPruneMap>
    struct TetrisCoreSelect<TetrisAI, TetrisLandPointSearchEngine, 0, HasLandPointEval, HasGetVirtualValue, HasPruneMap>
    {
        typedef TetrisCore<TetrisAI, TetrisLandPointSearchEngine, 0, HasLandPointEval, std::nullptr_t, std::nullptr_t> type;
    };

    template<class CallRunHold>
    struct TetrisCallRun
    {
        template<class Result, class NextCore, class EvalParam>
        static Result run(NextCore &next_core, TetrisMap const &map, TetrisNode const *node, std::vector<EvalParam> &history, unsigned char *next)
        {
            auto result = next_core.run_hold(map, node, history, next);
            return result.second.second > result.first.second ? result.second : result.first;
        }
    };
    template<>
    struct TetrisCallRun<std::false_type>
    {
        template<class Result, class NextCore, class EvalParam>
        static Result run(NextCore &next_core, TetrisMap const &map, TetrisNode const *node, std::vector<EvalParam> &history, unsigned char *next)
        {
            return next_core.run(map, node, history, next);
        }
    };

    //��next
    //���������
    //��vp����
    //�޼�֦
    template<class TetrisAI, class TetrisLandPointSearchEngine, size_t NextLength>
    class TetrisCore<TetrisAI, TetrisLandPointSearchEngine, NextLength, std::false_type, std::false_type, std::false_type>
    {
    public:
        typedef decltype(TetrisAI().eval_map(TetrisMap(), nullptr, 0)) MapEval;
        typedef EvalParam<> EvalParam;
        typedef std::pair<TetrisNode const *, MapEval> Result;
        typedef std::function<Result(TetrisMap const &, TetrisNode const *, std::vector<EvalParam> &history, unsigned char *)> CallAI;
        typedef std::function<std::pair<Result, Result>(TetrisMap const &, TetrisNode const *, std::vector<EvalParam> &history, unsigned char *)> CallHoldAI;
        TetrisCore(TetrisContext const *context, TetrisAI &ai, std::vector<CallAI> &call_ai, std::vector<CallHoldAI> &call_hold_ai) : context_(context), ai_(ai), next_(context, ai, call_ai, call_hold_ai)
        {
            call_ai.push_back(std::bind(&TetrisCore::run, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));
            call_hold_ai.push_back(std::bind(&TetrisCore::run_hold, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));
        }
        void init(TetrisContext const *context)
        {
            TetrisCallInit<TetrisLandPointSearchEngine>(search_, context);
            next_.init(context);
        }
        Result run(TetrisMap const &map, TetrisNode const *node, std::vector<EvalParam> &history, unsigned char *next)
        {
            return run_proc_<std::false_type>(map, node, history, next);
        }
        std::pair<Result, Result> run_hold(TetrisMap const &map, TetrisNode const *node, std::vector<EvalParam> &history, unsigned char *next)
        {
            auto this_result = run_proc_<std::true_type>(map, node, history, next);
            unsigned char hold_buffer[NextLength + 1];
            if(next[NextLength] == ' ')
            {
                memcpy(hold_buffer, next + 1, NextLength - 1);
                hold_buffer[NextLength - 1] = node->status.t;
                auto hold_result = next_.run_hold(map, context_->generate(*next), history, hold_buffer).first;
                return std::make_pair(this_result, hold_result);
            }
            else
            {
                memcpy(hold_buffer, next, NextLength);
                hold_buffer[NextLength] = node->status.t;
                auto hold_result = run_proc_<std::true_type>(map, context_->generate(next[NextLength]), history, hold_buffer);
                return std::make_pair(this_result, hold_result);
            }
        }
    private:
        template<class CallRun>
        Result run_proc_(TetrisMap const &map, TetrisNode const *node, std::vector<EvalParam> &history, unsigned char *next)
        {
            std::vector<TetrisNode const *> const *land_point = search_.search(map, node);
            TetrisMap copy;
            MapEval eval = ai_.eval_map_bad();
            TetrisNode const *best_node = node;
            for(auto cit = land_point->begin(); cit != land_point->end(); ++cit)
            {
                node = *cit;
                copy = map;
                history.push_back(EvalParam(node, node->attach(copy), map));
                MapEval new_eval;
                if(*next == ' ')
                {
                    TetrisNode const *next_node = context_->generate(size_t(0));
                    new_eval = next_node->check(copy) ? TetrisCallRun<CallRun>::run<Result>(next_, copy, next_node, history, next + 1).second : ai_.eval_map_bad();
                    for(size_t i = 1; i < context_->type_max(); ++i)
                    {
                        next_node = context_->generate(i);
                        new_eval += next_node->check(copy) ? TetrisCallRun<CallRun>::run<Result>(next_, copy, next_node, history, next + 1).second : ai_.eval_map_bad();
                    }
                    new_eval /= int(context_->type_max());
                }
                else
                {
                    TetrisNode const *next_node = context_->generate(*next);
                    new_eval = next_node->check(copy) ? TetrisCallRun<CallRun>::run<Result>(next_, copy, next_node, history, next + 1).second : ai_.eval_map_bad();
                }
                history.pop_back();
                if(new_eval > eval)
                {
                    eval = new_eval;
                    best_node = node;
                }
            }
            return std::make_pair(best_node, eval);
        }
    private:
        TetrisLandPointSearchEngine search_;
        TetrisContext const *context_;
        TetrisAI &ai_;
        typename TetrisCoreSelect<TetrisAI, TetrisLandPointSearchEngine, NextLength - 1, std::false_type, std::false_type, std::false_type>::type next_;
    };

    //��next
    //���������
    //��vp����
    //�м�֦
    template<class TetrisAI, class TetrisLandPointSearchEngine, size_t NextLength>
    class TetrisCore<TetrisAI, TetrisLandPointSearchEngine, NextLength, std::false_type, std::false_type, std::true_type>
    {
    public:
        typedef decltype(TetrisAI().eval_map(TetrisMap(), nullptr, 0)) MapEval;
        typedef EvalParam<> EvalParam;
        typedef std::pair<TetrisNode const *, MapEval> Result;
        typedef std::function<Result(TetrisMap const &, TetrisNode const *, std::vector<EvalParam> &history, unsigned char *)> CallAI;
        typedef std::function<std::pair<Result, Result>(TetrisMap const &, TetrisNode const *, std::vector<EvalParam> &history, unsigned char *)> CallHoldAI;
        TetrisCore(TetrisContext const *context, TetrisAI &ai, std::vector<CallAI> &call_ai, std::vector<CallHoldAI> &call_hold_ai) : context_(context), ai_(ai), next_(context, ai, call_ai, call_hold_ai)
        {
            call_ai.push_back(std::bind(&TetrisCore::run, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));
            call_hold_ai.push_back(std::bind(&TetrisCore::run_hold, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));
        }
        void init(TetrisContext const *context)
        {
            TetrisCallInit<TetrisLandPointSearchEngine>(search_, context);
            next_.init(context);
        }
        Result run(TetrisMap const &map, TetrisNode const *node, std::vector<EvalParam> &history, unsigned char *next)
        {
            return run_proc_<std::false_type>(map, node, history, next);
        }
        std::pair<Result, Result> run_hold(TetrisMap const &map, TetrisNode const *node, std::vector<EvalParam> &history, unsigned char *next)
        {
            auto this_result = run_proc_<std::true_type>(map, node, history, next);
            unsigned char hold_buffer[NextLength + 1];
            if(next[NextLength] == ' ')
            {
                memcpy(hold_buffer, next + 1, NextLength - 1);
                hold_buffer[NextLength - 1] = node->status.t;
                auto hold_result = next_.run_hold(map, context_->generate(*next), history, hold_buffer).first;
                return std::make_pair(this_result, hold_result);
            }
            else
            {
                memcpy(hold_buffer, next, NextLength);
                hold_buffer[NextLength] = node->status.t;
                auto hold_result = run_proc_<std::true_type>(map, context_->generate(next[NextLength]), history, hold_buffer);
                return std::make_pair(this_result, hold_result);
            }
        }
    private:
        template<class CallRun>
        Result run_proc_(TetrisMap const &map, TetrisNode const *node, std::vector<EvalParam> &history, unsigned char *next)
        {
            std::vector<TetrisNode const *> const *land_point = search_.search(map, node);
            prune_param_.clear();
            TetrisMap copy;
            for(auto cit = land_point->begin(); cit != land_point->end(); ++cit)
            {
                node = *cit;
                copy = map;
                history.push_back(EvalParam(node, node->attach(copy), map));
                prune_param_.push_back(PruneParam<MapEval>(ai_.eval_map(copy, history.data(), history.size()), node));
                history.pop_back();
            }
            after_pruning_.resize(prune_param_.size());
            after_pruning_.resize(ai_.prune_map(prune_param_.data(), prune_param_.size(), after_pruning_.data(), NextLength));
            MapEval eval = ai_.eval_map_bad();
            TetrisNode const *best_node = node;
            for(auto cit = after_pruning_.begin(); cit != after_pruning_.end(); ++cit)
            {
                node = *cit;
                copy = map;
                history.push_back(EvalParam(node, node->attach(copy), map));
                MapEval new_eval;
                if(*next == ' ')
                {
                    TetrisNode const *next_node = context_->generate(size_t(0));
                    new_eval = next_node->check(copy) ? TetrisCallRun<CallRun>::run<Result>(next_, copy, next_node, history, next + 1).second : ai_.eval_map_bad();
                    for(size_t i = 1; i < context_->type_max(); ++i)
                    {
                        next_node = context_->generate(i);
                        new_eval += next_node->check(copy) ? TetrisCallRun<CallRun>::run<Result>(next_, copy, next_node, history, next + 1).second : ai_.eval_map_bad();
                    }
                    new_eval /= int(context_->type_max());
                }
                else
                {
                    TetrisNode const *next_node = context_->generate(*next);
                    new_eval = next_node->check(copy) ? TetrisCallRun<CallRun>::run<Result>(next_, copy, next_node, history, next + 1).second : ai_.eval_map_bad();
                }
                history.pop_back();
                if(new_eval > eval)
                {
                    eval = new_eval;
                    best_node = node;
                }
            }
            return std::make_pair(best_node, eval);
        }
    private:
        TetrisLandPointSearchEngine search_;
        TetrisContext const *context_;
        TetrisAI &ai_;
        std::vector<PruneParam<MapEval>> prune_param_;
        std::vector<TetrisNode const *> after_pruning_;
        typename TetrisCoreSelect<TetrisAI, TetrisLandPointSearchEngine, NextLength - 1, std::false_type, std::false_type, std::true_type>::type next_;
    };

    //��next
    //���������
    template<class TetrisAI, class TetrisLandPointSearchEngine>
    class TetrisCore<TetrisAI, TetrisLandPointSearchEngine, 0, std::false_type, std::nullptr_t, std::nullptr_t>
    {
    public:
        typedef decltype(TetrisAI().eval_map(TetrisMap(), nullptr, 0)) MapEval;
        typedef EvalParam<> EvalParam;
        typedef std::pair<TetrisNode const *, MapEval> Result;
        typedef std::function<Result(TetrisMap const &, TetrisNode const *, std::vector<EvalParam> &history, unsigned char *)> CallAI;
        typedef std::function<std::pair<Result, Result>(TetrisMap const &, TetrisNode const *, std::vector<EvalParam> &history, unsigned char *)> CallHoldAI;
        TetrisCore(TetrisContext const *context, TetrisAI &ai, std::vector<CallAI> &call_ai, std::vector<CallHoldAI> &call_hold_ai) : context_(context), ai_(ai)
        {
            call_ai.push_back(std::bind(&TetrisCore::run, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));
            call_hold_ai.push_back(std::bind(&TetrisCore::run_hold, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));
        }
        void init(TetrisContext const *context)
        {
            TetrisCallInit<TetrisLandPointSearchEngine>(search_, context);
        }
        Result run(TetrisMap const &map, TetrisNode const *node, std::vector<EvalParam> &history, unsigned char *next)
        {
            return run_proc_(map, node, history, next);
        }
        std::pair<Result, Result> run_hold(TetrisMap const &map, TetrisNode const *node, std::vector<EvalParam> &history, unsigned char *next)
        {
            auto this_result = run_proc_(map, node, history, next);
            if(*next == ' ')
            {
                return std::make_pair(this_result, this_result);
            }
            else
            {
                auto hold_result = run_proc_(map, context_->generate(*next), history, next);
                return std::make_pair(this_result, hold_result);
            }
        }
    private:
        Result run_proc_(TetrisMap const &map, TetrisNode const *node, std::vector<EvalParam> &history, unsigned char *)
        {
            std::vector<TetrisNode const *> const *land_point = search_.search(map, node);
            MapEval eval = ai_.eval_map_bad();
            TetrisNode const *best_node = node;
            for(auto cit = land_point->begin(); cit != land_point->end(); ++cit)
            {
                node = *cit;
                TetrisMap copy = map;
                history.push_back(EvalParam(node, node->attach(copy), map));
                MapEval new_eval = ai_.eval_map(copy, history.data(), history.size());
                history.pop_back();
                if(new_eval > eval)
                {
                    eval = new_eval;
                    best_node = node;
                }
            }
            return std::make_pair(best_node, eval);
        }
    private:
        TetrisLandPointSearchEngine search_;
        TetrisContext const *context_;
        TetrisAI &ai_;
    };

    //��next
    //���������
    //��vp����
    //�޼�֦
    template<class TetrisAI, class TetrisLandPointSearchEngine, size_t NextLength>
    class TetrisCore<TetrisAI, TetrisLandPointSearchEngine, NextLength, std::true_type, std::false_type, std::false_type>
    {
    public:
        typedef decltype(TetrisAI().eval_land_point(nullptr, TetrisMap(), 0)) LandPointEval;
        typedef decltype(TetrisAI().eval_map(TetrisMap(), nullptr, 0)) MapEval;
        typedef EvalParam<LandPointEval> EvalParam;
        typedef std::pair<TetrisNode const *, MapEval> Result;
        typedef std::function<Result(TetrisMap const &, TetrisNode const *, std::vector<EvalParam> &history, unsigned char *)> CallAI;
        typedef std::function<std::pair<Result, Result>(TetrisMap const &, TetrisNode const *, std::vector<EvalParam> &history, unsigned char *)> CallHoldAI;
        TetrisCore(TetrisContext const *context, TetrisAI &ai, std::vector<CallAI> &call_ai, std::vector<CallHoldAI> &call_hold_ai) : context_(context), ai_(ai), next_(context, ai, call_ai, call_hold_ai)
        {
            call_ai.push_back(std::bind(&TetrisCore::run, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));
            call_hold_ai.push_back(std::bind(&TetrisCore::run_hold, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));
        }
        void init(TetrisContext const *context)
        {
            TetrisCallInit<TetrisLandPointSearchEngine>(search_, context);
            next_.init(context);
        }
        Result run(TetrisMap const &map, TetrisNode const *node, std::vector<EvalParam> &history, unsigned char *next)
        {
            return run_proc_<std::false_type>(map, node, history, next);
        }
        std::pair<Result, Result> run_hold(TetrisMap const &map, TetrisNode const *node, std::vector<EvalParam> &history, unsigned char *next)
        {
            auto this_result = run_proc_<std::true_type>(map, node, history, next);
            unsigned char hold_buffer[NextLength + 1];
            if(next[NextLength] == ' ')
            {
                memcpy(hold_buffer, next + 1, NextLength - 1);
                hold_buffer[NextLength - 1] = node->status.t;
                auto hold_result = next_.run_hold(map, context_->generate(*next), history, hold_buffer).first;
                return std::make_pair(this_result, hold_result);
            }
            else
            {
                memcpy(hold_buffer, next, NextLength);
                hold_buffer[NextLength] = node->status.t;
                auto hold_result = run_proc_<std::true_type>(map, context_->generate(next[NextLength]), history, hold_buffer);
                return std::make_pair(this_result, hold_result);
            }
        }
    private:
        template<class CallRun>
        Result run_proc_(TetrisMap const &map, TetrisNode const *node, std::vector<EvalParam> &history, unsigned char *next)
        {
            std::vector<TetrisNode const *> const *land_point = search_.search(map, node);
            MapEval eval = ai_.eval_map_bad();
            TetrisNode const *best_node = node;
            for(auto cit = land_point->begin(); cit != land_point->end(); ++cit)
            {
                node = *cit;
                TetrisMap copy = map;
                size_t clear = node->attach(copy);
                history.push_back(EvalParam(node, clear, map, ai_.eval_land_point(node, copy, clear)));
                MapEval new_eval;
                if(*next == ' ')
                {
                    TetrisNode const *next_node = context_->generate(size_t(0));
                    new_eval = next_node->check(copy) ? TetrisCallRun<CallRun>::run<Result>(next_, copy, next_node, history, next + 1).second : ai_.eval_map_bad();
                    for(size_t i = 1; i < context_->type_max(); ++i)
                    {
                        next_node = context_->generate(i);
                        new_eval += next_node->check(copy) ? TetrisCallRun<CallRun>::run<Result>(next_, copy, next_node, history, next + 1).second : ai_.eval_map_bad();
                    }
                    new_eval /= int(context_->type_max());
                }
                else
                {
                    TetrisNode const *next_node = context_->generate(*next);
                    new_eval = next_node->check(copy) ? TetrisCallRun<CallRun>::run<Result>(next_, copy, next_node, history, next + 1).second : ai_.eval_map_bad();
                }
                history.pop_back();
                if(new_eval > eval)
                {
                    eval = new_eval;
                    best_node = node;
                }
            }
            return std::make_pair(best_node, eval);
        }
    private:
        TetrisLandPointSearchEngine search_;
        TetrisContext const *context_;
        TetrisAI &ai_;
        typename TetrisCoreSelect<TetrisAI, TetrisLandPointSearchEngine, NextLength - 1, std::true_type, std::false_type, std::false_type>::type next_;
    };

    //��next
    //���������
    //��vp����
    //�м�֦
    template<class TetrisAI, class TetrisLandPointSearchEngine, size_t NextLength>
    class TetrisCore<TetrisAI, TetrisLandPointSearchEngine, NextLength, std::true_type, std::false_type, std::true_type>
    {
    public:
        typedef decltype(TetrisAI().eval_land_point(nullptr, TetrisMap(), 0)) LandPointEval;
        typedef decltype(TetrisAI().eval_map(TetrisMap(), nullptr, 0)) MapEval;
        typedef EvalParam<LandPointEval> EvalParam;
        typedef std::pair<TetrisNode const *, MapEval> Result;
        typedef std::function<Result(TetrisMap const &, TetrisNode const *, std::vector<EvalParam> &history, unsigned char *)> CallAI;
        typedef std::function<std::pair<Result, Result>(TetrisMap const &, TetrisNode const *, std::vector<EvalParam> &history, unsigned char *)> CallHoldAI;
        TetrisCore(TetrisContext const *context, TetrisAI &ai, std::vector<CallAI> &call_ai, std::vector<CallHoldAI> &call_hold_ai) : context_(context), ai_(ai), next_(context, ai, call_ai, call_hold_ai)
        {
            call_ai.push_back(std::bind(&TetrisCore::run, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));
            call_hold_ai.push_back(std::bind(&TetrisCore::run_hold, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));
        }
        void init(TetrisContext const *context)
        {
            TetrisCallInit<TetrisLandPointSearchEngine>(search_, context);
            next_.init(context);
        }
        Result run(TetrisMap const &map, TetrisNode const *node, std::vector<EvalParam> &history, unsigned char *next)
        {
            return run_proc_<std::false_type>(map, node, history, next);
        }
        std::pair<Result, Result> run_hold(TetrisMap const &map, TetrisNode const *node, std::vector<EvalParam> &history, unsigned char *next)
        {
            auto this_result = run_proc_<std::true_type>(map, node, history, next);
            unsigned char hold_buffer[NextLength + 1];
            if(next[NextLength] == ' ')
            {
                memcpy(hold_buffer, next + 1, NextLength - 1);
                hold_buffer[NextLength - 1] = node->status.t;
                auto hold_result = next_.run_hold(map, context_->generate(*next), history, hold_buffer).first;
                return std::make_pair(this_result, hold_result);
            }
            else
            {
                memcpy(hold_buffer, next, NextLength);
                hold_buffer[NextLength] = node->status.t;
                auto hold_result = run_proc_<std::true_type>(map, context_->generate(next[NextLength]), history, hold_buffer);
                return std::make_pair(this_result, hold_result);
            }
        }
    private:
        template<class CallRun>
        Result run_proc_(TetrisMap const &map, TetrisNode const *node, std::vector<EvalParam> &history, unsigned char *next)
        {
            std::vector<TetrisNode const *> const *land_point = search_.search(map, node);
            prune_param_.clear();
            MapEval eval = ai_.eval_map_bad();
            for(auto cit = land_point->begin(); cit != land_point->end(); ++cit)
            {
                node = *cit;
                TetrisMap copy = map;
                size_t clear = node->attach(copy);
                history.push_back(EvalParam(node, clear, map, ai_.eval_land_point(node, copy, clear)));
                prune_param_.push_back(PruneParam<MapEval>(ai_.eval_map(copy, history.data(), history.size()), node));
                history.pop_back();
            }
            after_pruning_.resize(prune_param_.size());
            after_pruning_.resize(ai_.prune_map(prune_param_.data(), prune_param_.size(), after_pruning_.data(), NextLength));
            TetrisNode const *best_node = node;
            for(auto cit = after_pruning_.begin(); cit != after_pruning_.end(); ++cit)
            {
                node = *cit;
                TetrisMap copy = map;
                size_t clear = node->attach(copy);
                history.push_back(EvalParam(node, clear, map, ai_.eval_land_point(node, copy, clear)));
                MapEval new_eval;
                if(*next == ' ')
                {
                    TetrisNode const *next_node = context_->generate(size_t(0));
                    new_eval = next_node->check(copy) ? TetrisCallRun<CallRun>::run<Result>(next_, copy, next_node, history, next + 1).second : ai_.eval_map_bad();
                    for(size_t i = 1; i < context_->type_max(); ++i)
                    {
                        next_node = context_->generate(i);
                        new_eval += next_node->check(copy) ? TetrisCallRun<CallRun>::run<Result>(next_, copy, next_node, history, next + 1).second : ai_.eval_map_bad();
                    }
                    new_eval /= int(context_->type_max());
                }
                else
                {
                    TetrisNode const *next_node = context_->generate(*next);
                    new_eval = next_node->check(copy) ? TetrisCallRun<CallRun>::run<Result>(next_, copy, next_node, history, next + 1).second : ai_.eval_map_bad();
                }
                history.pop_back();
                if(new_eval > eval)
                {
                    eval = new_eval;
                    best_node = node;
                }
            }
            return std::make_pair(best_node, eval);
        }
    private:
        TetrisLandPointSearchEngine search_;
        TetrisContext const *context_;
        TetrisAI &ai_;
        std::vector<PruneParam<MapEval>> prune_param_;
        std::vector<TetrisNode const *> after_pruning_;
        typename TetrisCoreSelect<TetrisAI, TetrisLandPointSearchEngine, NextLength - 1, std::true_type, std::false_type, std::true_type>::type next_;
    };

    //��next
    //���������
    template<class TetrisAI, class TetrisLandPointSearchEngine>
    class TetrisCore<TetrisAI, TetrisLandPointSearchEngine, 0, std::true_type, std::nullptr_t, std::nullptr_t>
    {
    public:
        typedef decltype(TetrisAI().eval_land_point(nullptr, TetrisMap(), 0)) LandPointEval;
        typedef decltype(TetrisAI().eval_map(TetrisMap(), nullptr, 0)) MapEval;
        typedef EvalParam<LandPointEval> EvalParam;
        typedef std::pair<TetrisNode const *, MapEval> Result;
        typedef std::function<Result(TetrisMap const &, TetrisNode const *, std::vector<EvalParam> &history, unsigned char *)> CallAI;
        typedef std::function<std::pair<Result, Result>(TetrisMap const &, TetrisNode const *, std::vector<EvalParam> &history, unsigned char *)> CallHoldAI;
        TetrisCore(TetrisContext const *context, TetrisAI &ai, std::vector<CallAI> &call_ai, std::vector<CallHoldAI> &call_hold_ai) : context_(context), ai_(ai)
        {
            call_ai.push_back(std::bind(&TetrisCore::run, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));
            call_hold_ai.push_back(std::bind(&TetrisCore::run_hold, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));
        }
        void init(TetrisContext const *context)
        {
            TetrisCallInit<TetrisLandPointSearchEngine>(search_, context);
        }
        Result run(TetrisMap const &map, TetrisNode const *node, std::vector<EvalParam> &history, unsigned char *next)
        {
            return run_proc_(map, node, history, next);
        }
        std::pair<Result, Result> run_hold(TetrisMap const &map, TetrisNode const *node, std::vector<EvalParam> &history, unsigned char *next)
        {
            auto this_result = run_proc_(map, node, history, next);
            if(*next == ' ')
            {
                return std::make_pair(this_result, this_result);
            }
            else
            {
                auto hold_result = run_proc_(map, context_->generate(*next), history, next);
                return std::make_pair(this_result, hold_result);
            }
        }
    private:
        Result run_proc_(TetrisMap const &map, TetrisNode const *node, std::vector<EvalParam> &history, unsigned char *)
        {
            std::vector<TetrisNode const *> const *land_point = search_.search(map, node);
            MapEval eval = ai_.eval_map_bad();
            TetrisNode const *best_node = node;
            for(auto cit = land_point->begin(); cit != land_point->end(); ++cit)
            {
                node = *cit;
                TetrisMap copy = map;
                size_t clear = node->attach(copy);
                history.push_back(EvalParam(node, clear, map, ai_.eval_land_point(node, copy, clear)));
                MapEval new_eval = ai_.eval_map(copy, history.data(), history.size());
                history.pop_back();
                if(new_eval > eval)
                {
                    eval = new_eval;
                    best_node = node;
                }
            }
            return std::make_pair(best_node, eval);
        }
    private:
        TetrisLandPointSearchEngine search_;
        TetrisContext const *context_;
        TetrisAI &ai_;
    };

    //��next
    //���������
    //��vp����
    //�޼�֦
    template<class TetrisAI, class TetrisLandPointSearchEngine, size_t NextLength>
    class TetrisCore<TetrisAI, TetrisLandPointSearchEngine, NextLength, std::false_type, std::true_type, std::false_type>
    {
    public:
        typedef decltype(TetrisAI().eval_map(TetrisMap(), nullptr, 0)) MapEval;
        typedef EvalParam<> EvalParam;
        typedef std::pair<TetrisNode const *, MapEval> Result;
        typedef std::function<Result(TetrisMap const &, TetrisNode const *, std::vector<EvalParam> &history, unsigned char *)> CallAI;
        typedef std::function<std::pair<Result, Result>(TetrisMap const &, TetrisNode const *, std::vector<EvalParam> &history, unsigned char *)> CallHoldAI;
        TetrisCore(TetrisContext const *context, TetrisAI &ai, std::vector<CallAI> &call_ai, std::vector<CallHoldAI> &call_hold_ai) : context_(context), ai_(ai), next_(context, ai, call_ai, call_hold_ai)
        {
            call_ai.push_back(std::bind(&TetrisCore::run, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));
            call_hold_ai.push_back(std::bind(&TetrisCore::run_hold, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));
        }
        void init(TetrisContext const *context)
        {
            virtual_eval_.resize(context->type_max());
            TetrisCallInit<TetrisLandPointSearchEngine>(search_, context);
            next_.init(context);
        }
        Result run(TetrisMap const &map, TetrisNode const *node, std::vector<EvalParam> &history, unsigned char *next)
        {
            return run_proc_<std::false_type>(map, node, history, next);
        }
        std::pair<Result, Result> run_hold(TetrisMap const &map, TetrisNode const *node, std::vector<EvalParam> &history, unsigned char *next)
        {
            auto this_result = run_proc_<std::true_type>(map, node, history, next);
            unsigned char hold_buffer[NextLength + 1];
            if(next[NextLength] == ' ')
            {
                memcpy(hold_buffer, next + 1, NextLength - 1);
                hold_buffer[NextLength - 1] = node->status.t;
                auto hold_result = next_.run_hold(map, context_->generate(*next), history, hold_buffer).first;
                return std::make_pair(this_result, hold_result);
            }
            else
            {
                memcpy(hold_buffer, next, NextLength);
                hold_buffer[NextLength] = node->status.t;
                auto hold_result = run_proc_<std::true_type>(map, context_->generate(next[NextLength]), history, hold_buffer);
                return std::make_pair(this_result, hold_result);
            }
        }
    private:
        template<class CallRun>
        Result run_proc_(TetrisMap const &map, TetrisNode const *node, std::vector<EvalParam> &history, unsigned char *next)
        {
            std::vector<TetrisNode const *> const *land_point = search_.search(map, node);
            TetrisMap copy;
            MapEval eval = ai_.eval_map_bad();
            TetrisNode const *best_node = node;
            for(auto cit = land_point->begin(); cit != land_point->end(); ++cit)
            {
                node = *cit;
                copy = map;
                history.push_back(EvalParam(node, node->attach(copy), map));
                MapEval new_eval;
                if(*next == ' ')
                {
                    for(size_t i = 0; i < context_->type_max(); ++i)
                    {
                        TetrisNode const *next_node = context_->generate(i);
                        virtual_eval_[i] = next_node->check(copy) ? TetrisCallRun<CallRun>::run<Result>(next_, copy, next_node, history, next + 1).second : ai_.eval_map_bad();
                    }
                    new_eval = ai_.get_virtual_value(virtual_eval_.data(), virtual_eval_.size());
                }
                else
                {
                    TetrisNode const *next_node = context_->generate(*next);
                    new_eval = next_node->check(copy) ? TetrisCallRun<CallRun>::run<Result>(next_, copy, next_node, history, next + 1).second : ai_.eval_map_bad();
                }
                history.pop_back();
                if(new_eval > eval)
                {
                    eval = new_eval;
                    best_node = node;
                }
            }
            return std::make_pair(best_node, eval);
        }
    private:
        TetrisLandPointSearchEngine search_;
        TetrisContext const *context_;
        TetrisAI &ai_;
        std::vector<MapEval> virtual_eval_;
        typename TetrisCoreSelect<TetrisAI, TetrisLandPointSearchEngine, NextLength - 1, std::false_type, std::true_type, std::false_type>::type next_;
    };

    //��next
    //���������
    //��vp����
    //�м�֦
    template<class TetrisAI, class TetrisLandPointSearchEngine, size_t NextLength>
    class TetrisCore<TetrisAI, TetrisLandPointSearchEngine, NextLength, std::false_type, std::true_type, std::true_type>
    {
    public:
        typedef decltype(TetrisAI().eval_map(TetrisMap(), nullptr, 0)) MapEval;
        typedef EvalParam<> EvalParam;
        typedef std::pair<TetrisNode const *, MapEval> Result;
        typedef std::function<Result(TetrisMap const &, TetrisNode const *, std::vector<EvalParam> &history, unsigned char *)> CallAI;
        typedef std::function<std::pair<Result, Result>(TetrisMap const &, TetrisNode const *, std::vector<EvalParam> &history, unsigned char *)> CallHoldAI;
        TetrisCore(TetrisContext const *context, TetrisAI &ai, std::vector<CallAI> &call_ai, std::vector<CallHoldAI> &call_hold_ai) : context_(context), ai_(ai), next_(context, ai, call_ai, call_hold_ai)
        {
            call_ai.push_back(std::bind(&TetrisCore::run, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));
            call_hold_ai.push_back(std::bind(&TetrisCore::run_hold, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));
        }
        void init(TetrisContext const *context)
        {
            virtual_eval_.resize(context->type_max());
            TetrisCallInit<TetrisLandPointSearchEngine>(search_, context);
            next_.init(context);
        }
        Result run(TetrisMap const &map, TetrisNode const *node, std::vector<EvalParam> &history, unsigned char *next)
        {
            return run_proc_<std::false_type>(map, node, history, next);
        }
        std::pair<Result, Result> run_hold(TetrisMap const &map, TetrisNode const *node, std::vector<EvalParam> &history, unsigned char *next)
        {
            auto this_result = run_proc_<std::true_type>(map, node, history, next);
            unsigned char hold_buffer[NextLength + 1];
            if(next[NextLength] == ' ')
            {
                memcpy(hold_buffer, next + 1, NextLength - 1);
                hold_buffer[NextLength - 1] = node->status.t;
                auto hold_result = next_.run_hold(map, context_->generate(*next), history, hold_buffer).first;
                return std::make_pair(this_result, hold_result);
            }
            else
            {
                memcpy(hold_buffer, next, NextLength);
                hold_buffer[NextLength] = node->status.t;
                auto hold_result = run_proc_<std::true_type>(map, context_->generate(next[NextLength]), history, hold_buffer);
                return std::make_pair(this_result, hold_result);
            }
        }
    private:
        template<class CallRun>
        Result run_proc_(TetrisMap const &map, TetrisNode const *node, std::vector<EvalParam> &history, unsigned char *next)
        {
            std::vector<TetrisNode const *> const *land_point = search_.search(map, node);
            prune_param_.clear();
            TetrisMap copy;
            for(auto cit = land_point->begin(); cit != land_point->end(); ++cit)
            {
                node = *cit;
                copy = map;
                history.push_back(EvalParam(node, node->attach(copy), map));
                prune_param_.push_back(PruneParam<MapEval>(ai_.eval_map(copy, history.data(), history.size()), node));
                history.pop_back();
            }
            after_pruning_.resize(prune_param_.size());
            after_pruning_.resize(ai_.prune_map(prune_param_.data(), prune_param_.size(), after_pruning_.data(), NextLength));
            MapEval eval = ai_.eval_map_bad();
            TetrisNode const *best_node = node;
            for(auto cit = after_pruning_.begin(); cit != after_pruning_.end(); ++cit)
            {
                node = *cit;
                copy = map;
                history.push_back(EvalParam(node, node->attach(copy), map));
                MapEval new_eval;
                if(*next == ' ')
                {
                    for(size_t i = 0; i < context_->type_max(); ++i)
                    {
                        TetrisNode const *next_node = context_->generate(i);
                        virtual_eval_[i] = next_node->check(copy) ? TetrisCallRun<CallRun>::run<Result>(next_, copy, next_node, history, next + 1).second : ai_.eval_map_bad();
                    }
                    new_eval = ai_.get_virtual_eval(virtual_eval_.data(), virtual_eval_.size());
                }
                else
                {
                    TetrisNode const *next_node = context_->generate(*next);
                    new_eval = next_node->check(copy) ? TetrisCallRun<CallRun>::run<Result>(next_, copy, next_node, history, next + 1).second : ai_.eval_map_bad();
                }
                history.pop_back();
                if(new_eval > eval)
                {
                    eval = new_eval;
                    best_node = node;
                }
            }
            return std::make_pair(best_node, eval);
        }
    private:
        TetrisLandPointSearchEngine search_;
        TetrisContext const *context_;
        TetrisAI &ai_;
        std::vector<MapEval> virtual_eval_;
        std::vector<PruneParam<MapEval>> prune_param_;
        std::vector<TetrisNode const *> after_pruning_;
        typename TetrisCoreSelect<TetrisAI, TetrisLandPointSearchEngine, NextLength - 1, std::false_type, std::true_type, std::true_type>::type next_;
    };

    //��next
    //���������
    //��vp����
    //�޼�֦
    template<class TetrisAI, class TetrisLandPointSearchEngine, size_t NextLength>
    class TetrisCore<TetrisAI, TetrisLandPointSearchEngine, NextLength, std::true_type, std::true_type, std::false_type>
    {
    public:
        typedef decltype(TetrisAI().eval_land_point(nullptr, TetrisMap(), 0)) LandPointEval;
        typedef decltype(TetrisAI().eval_map(TetrisMap(), nullptr, 0)) MapEval;
        typedef EvalParam<LandPointEval> EvalParam;
        typedef std::pair<TetrisNode const *, MapEval> Result;
        typedef std::function<Result(TetrisMap const &, TetrisNode const *, std::vector<EvalParam> &history, unsigned char *)> CallAI;
        typedef std::function<std::pair<Result, Result>(TetrisMap const &, TetrisNode const *, std::vector<EvalParam> &history, unsigned char *)> CallHoldAI;
        TetrisCore(TetrisContext const *context, TetrisAI &ai, std::vector<CallAI> &call_ai, std::vector<CallHoldAI> &call_hold_ai) : context_(context), ai_(ai), next_(context, ai, call_ai, call_hold_ai)
        {
            call_ai.push_back(std::bind(&TetrisCore::run, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));
            call_hold_ai.push_back(std::bind(&TetrisCore::run_hold, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));
        }
        void init(TetrisContext const *context)
        {
            virtual_eval_.resize(context->type_max());
            TetrisCallInit<TetrisLandPointSearchEngine>(search_, context);
            next_.init(context);
        }
        Result run(TetrisMap const &map, TetrisNode const *node, std::vector<EvalParam> &history, unsigned char *next)
        {
            return run_proc_<std::false_type>(map, node, history, next);
        }
        std::pair<Result, Result> run_hold(TetrisMap const &map, TetrisNode const *node, std::vector<EvalParam> &history, unsigned char *next)
        {
            auto this_result = run_proc_<std::true_type>(map, node, history, next);
            unsigned char hold_buffer[NextLength + 1];
            if(next[NextLength] == ' ')
            {
                memcpy(hold_buffer, next + 1, NextLength - 1);
                hold_buffer[NextLength - 1] = node->status.t;
                auto hold_result = next_.run_hold(map, context_->generate(*next), history, hold_buffer).first;
                return std::make_pair(this_result, hold_result);
            }
            else
            {
                memcpy(hold_buffer, next, NextLength);
                hold_buffer[NextLength] = node->status.t;
                auto hold_result = run_proc_<std::true_type>(map, context_->generate(next[NextLength]), history, hold_buffer);
                return std::make_pair(this_result, hold_result);
            }
        }
    private:
        template<class CallRun>
        Result run_proc_(TetrisMap const &map, TetrisNode const *node, std::vector<EvalParam> &history, unsigned char *next)
        {
            std::vector<TetrisNode const *> const *land_point = search_.search(map, node);
            MapEval eval = ai_.eval_map_bad();
            TetrisNode const *best_node = node;
            for(auto cit = land_point->begin(); cit != land_point->end(); ++cit)
            {
                node = *cit;
                TetrisMap copy = map;
                size_t clear = node->attach(copy);
                history.push_back(EvalParam(node, clear, map, ai_.eval_land_point(node, copy, clear)));
                MapEval new_eval;
                if(*next == ' ')
                {
                    for(size_t i = 0; i < context_->type_max(); ++i)
                    {
                        TetrisNode const *next_node = context_->generate(i);
                        virtual_eval_[i] = next_node->check(map) ? TetrisCallRun<CallRun>::run<Result>(next_, copy, next_node, history, next + 1).second : ai_.eval_map_bad();
                    }
                    new_eval = ai_.get_virtual_eval(virtual_eval_.data(), virtual_eval_.size());
                }
                else
                {
                    TetrisNode const *next_node = context_->generate(*next);
                    new_eval = next_node->check(map) ? TetrisCallRun<CallRun>::run<Result>(next_, copy, next_node, history, next + 1).second : ai_.eval_map_bad();
                }
                history.pop_back();
                if(new_eval > eval)
                {
                    eval = new_eval;
                    best_node = node;
                }
            }
            return std::make_pair(best_node, eval);
        }
    private:
        TetrisLandPointSearchEngine search_;
        TetrisContext const *context_;
        TetrisAI &ai_;
        std::vector<MapEval> virtual_eval_;
        typename TetrisCoreSelect<TetrisAI, TetrisLandPointSearchEngine, NextLength - 1, std::true_type, std::true_type, std::false_type>::type next_;
    };

    //��next
    //���������
    //��vp����
    //�м�֦
    template<class TetrisAI, class TetrisLandPointSearchEngine, size_t NextLength>
    class TetrisCore<TetrisAI, TetrisLandPointSearchEngine, NextLength, std::true_type, std::true_type, std::true_type>
    {
    public:
        typedef decltype(TetrisAI().eval_land_point(nullptr, TetrisMap(), 0)) LandPointEval;
        typedef decltype(TetrisAI().eval_map(TetrisMap(), nullptr, 0)) MapEval;
        typedef EvalParam<LandPointEval> EvalParam;
        typedef std::pair<TetrisNode const *, MapEval> Result;
        typedef std::function<Result(TetrisMap const &, TetrisNode const *, std::vector<EvalParam> &history, unsigned char *)> CallAI;
        typedef std::function<std::pair<Result, Result>(TetrisMap const &, TetrisNode const *, std::vector<EvalParam> &history, unsigned char *)> CallHoldAI;
        TetrisCore(TetrisContext const *context, TetrisAI &ai, std::vector<CallAI> &call_ai, std::vector<CallHoldAI> &call_hold_ai) : context_(context), ai_(ai), next_(context, ai, call_ai, call_hold_ai)
        {
            call_ai.push_back(std::bind(&TetrisCore::run, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));
            call_hold_ai.push_back(std::bind(&TetrisCore::run_hold, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));
        }
        void init(TetrisContext const *context)
        {
            virtual_eval_.resize(context->type_max());
            TetrisCallInit<TetrisLandPointSearchEngine>(search_, context);
            next_.init(context);
        }
        Result run(TetrisMap const &map, TetrisNode const *node, std::vector<EvalParam> &history, unsigned char *next)
        {
            return run_proc_<std::false_type>(map, node, history, next);
        }
        std::pair<Result, Result> run_hold(TetrisMap const &map, TetrisNode const *node, std::vector<EvalParam> &history, unsigned char *next)
        {
            auto this_result = run_proc_<std::true_type>(map, node, history, next);
            unsigned char hold_buffer[NextLength + 1];
            if(next[NextLength] == ' ')
            {
                memcpy(hold_buffer, next + 1, NextLength - 1);
                hold_buffer[NextLength - 1] = node->status.t;
                auto hold_result = next_.run_hold(map, context_->generate(*next), history, hold_buffer).first;
                return std::make_pair(this_result, hold_result);
            }
            else
            {
                memcpy(hold_buffer, next, NextLength);
                hold_buffer[NextLength] = node->status.t;
                auto hold_result = run_proc_<std::true_type>(map, context_->generate(next[NextLength]), history, hold_buffer);
                return std::make_pair(this_result, hold_result);
            }
        }
    private:
        template<class CallRun>
        Result run_proc_(TetrisMap const &map, TetrisNode const *node, std::vector<EvalParam> &history, unsigned char *next)
        {
            std::vector<TetrisNode const *> const *land_point = search_.search(map, node);
            prune_param_.clear();
            for(auto cit = land_point->begin(); cit != land_point->end(); ++cit)
            {
                node = *cit;
                TetrisMap copy = map;
                size_t clear = node->attach(copy);
                history.push_back(EvalParam(node, clear, map, ai_.eval_land_point(node, copy, clear)));
                prune_param_.push_back(PruneParam<MapEval>(ai_.eval_map(copy, history.data(), history.size()), node));
                history.pop_back();
            }
            after_pruning_.resize(prune_param_.size());
            after_pruning_.resize(ai_.prune_map(prune_param_.data(), prune_param_.size(), after_pruning_.data(), NextLength));
            MapEval eval = ai_.eval_map_bad();
            TetrisNode const *best_node = node;
            for(auto cit = after_pruning_.begin(); cit != after_pruning_.end(); ++cit)
            {
                node = *cit;
                TetrisMap copy = map;
                size_t clear = node->attach(copy);
                history.push_back(EvalParam(node, clear, map, ai_.eval_land_point(node, copy, clear)));
                MapEval new_eval;
                if(*next == ' ')
                {
                    for(size_t i = 0; i < context_->type_max(); ++i)
                    {
                        TetrisNode const *next_node = context_->generate(i);
                        virtual_eval_[i] = next_node->check(map) ? TetrisCallRun<CallRun>::run<Result>(next_, copy, next_node, history, next + 1).second : ai_.eval_map_bad();
                    }
                    new_eval = ai_.get_virtual_eval(virtual_eval_.data(), virtual_eval_.size());
                }
                else
                {
                    TetrisNode const *next_node = context_->generate(*next);
                    new_eval = next_node->check(map) ? TetrisCallRun<CallRun>::run<Result>(next_, copy, next_node, history, next + 1).second : ai_.eval_map_bad();
                }
                history.pop_back();
                if(new_eval > eval)
                {
                    eval = new_eval;
                    best_node = node;
                }
            }
            return std::make_pair(best_node, eval);
        }
    private:
        TetrisLandPointSearchEngine search_;
        TetrisContext const *context_;
        TetrisAI &ai_;
        std::vector<MapEval> virtual_eval_;
        std::vector<PruneParam<MapEval>> prune_param_;
        std::vector<TetrisNode const *> after_pruning_;
        typename TetrisCoreSelect<TetrisAI, TetrisLandPointSearchEngine, NextLength - 1, std::true_type, std::true_type, std::true_type>::type next_;
    };

    template<class TetrisRuleSet, class TetrisAI, class TetrisLandPointSearchEngine, size_t MaxNextLength, class TetrisAIParam = TetrisAIEmptyParam>
    class TetrisEngine
    {
    private:
        typedef typename TetrisCoreSelect<TetrisAI, TetrisLandPointSearchEngine, MaxNextLength, typename TetrisAIHasLandPointEval<TetrisAI>::type, typename TetrisAIHasGetVirtualValue<TetrisAI>::type, typename TetrisAIHasPruneMap<TetrisAI>::type>::type Core;
        typename TetrisContextBuilder<TetrisRuleSet, TetrisAIParam>::TetrisContextWithParam *context_;
        TetrisAI ai_;
        TetrisLandPointSearchEngine search_;
        std::vector<typename Core::CallAI> call_ai_;
        std::vector<typename Core::CallHoldAI> call_hold_ai_;
        Core core_;
        std::vector<typename Core::EvalParam> history_;

    public:
        struct RunResult
        {
            RunResult(typename Core::MapEval const &_eval) : target(), eval(_eval), change_hold()
            {
            }
            RunResult(std::pair<TetrisNode const *, typename Core::MapEval> const &_result) : target(_result.first), eval(_result.second), change_hold()
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
        TetrisEngine() : context_(TetrisContextBuilder<TetrisRuleSet, TetrisAIParam>::build_context()), ai_(), call_ai_(), call_hold_ai_(), core_(context_, ai_, call_ai_, call_hold_ai_), history_()
        {
        }
        ~TetrisEngine()
        {
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
        size_t max_next_length() const
        {
            return MaxNextLength;
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
                TetrisCallInit<Core>(core_, context_);
                return true;
            }
            else if(result == TetrisContext::fail)
            {
                return false;
            }
            return true;
        }
        //run!
        RunResult run(TetrisMap const &map, TetrisNode const *node, unsigned char *next, size_t next_length)
        {
            if(node == nullptr || !node->check(map))
            {
                return RunResult(ai_.eval_map_bad());
            }
            return RunResult(call_ai_[std::min(MaxNextLength, next_length)](map, node, history_, next));
        }
        //��hold��run!
        RunResult run_hold(TetrisMap const &map, TetrisNode const *node, unsigned char hold, bool hold_free, unsigned char *next, size_t next_length)
        {
            if(node == nullptr || !node->check(map))
            {
                return RunResult(ai_.eval_map_bad());
            }
            next_length = std::min(MaxNextLength, next_length);
            if(!hold_free)
            {
                return std::make_pair(call_ai_[next_length](map, node, history_, next).first, false);
            }
            if(hold == ' ' && next_length == 0)
            {
                return RunResult(std::make_pair(nullptr, ai_.eval_map_bad()), true);
            }
            if(hold == ' ')
            {
                auto this_result = call_ai_[next_length - 1](map, node, history_, next);
                auto hold_result = call_ai_[next_length](map, context_->generate(*next), history_, next + 1);
                if(hold_result.second > this_result.second)
                {
                    return RunResult(hold_result, true);
                }
                else
                {
                    return RunResult(this_result, false);
                }
            }
            else
            {
                auto this_result = call_ai_[next_length](map, node, history_, next);
                auto hold_result = call_ai_[next_length](map, context_->generate(hold), history_, next);
                if(hold_result.second > this_result.second)
                {
                    return RunResult(hold_result, true);
                }
                else
                {
                    return RunResult(this_result, false);
                }
            }
        }
        //��hold�ľ�׼��run!(�ǳ���...����...)
        RunResult run_hold_accurate(TetrisMap const &map, TetrisNode const *node, unsigned char hold, bool hold_free, unsigned char *next, size_t next_length)
        {
            if(node == nullptr || !node->check(map))
            {
                return RunResult(ai_.eval_map_bad());
            }
            if(hold == ' ' && hold_free && next_length == 0)
            {
                return RunResult(ai_.eval_map_bad());
            }
            if(hold == ' ' && !hold_free)
            {
                return run(map, node, next, next_length);
            }
            next_length = std::min(MaxNextLength, next_length);
            unsigned char hold_buffer[MaxNextLength + 1];
            memcpy(hold_buffer, next, next_length);
            hold_buffer[next_length] = hold;
            auto result = call_hold_ai_[next_length](map, node, history_, hold_buffer);
            if(hold_free)
            {
                if(result.second.second > result.first.second)
                {
                    return RunResult(result.second, true);
                }
                else
                {
                    return RunResult(result.first, false);
                }
            }
            else
            {
                return RunResult(result.first, false);
            }
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
    template<unsigned char T, char X, char Y, unsigned char R, int line1, int line2, int line3, int line4>
    TetrisNode create_node(int w, int h, TetrisOpertion op)
    {
        static_assert(X < 0 || X >= 4 || Y < 0 || Y >= 4 || (line1 || line2 || line3 || line3), "data error");
        TetrisBlockStatus status =
        {
            T, X, h - Y - 1, R
        };
        TetrisNode node =
        {
            status, op, {line4, line3, line2, line1}, {}, {}, h - 4, 4, 0, 4
        };
        while(node.data[0] == 0)
        {
            ++node.row;
            --node.height;
            memmove(&node.data[0], &node.data[1], node.height * sizeof(int));
            node.data[node.height] = 0;
        }
        while(node.data[node.height - 1] == 0)
        {
            --node.height;
        }
        while(!((node.data[0] >> node.col) & 1) && !((node.data[1] >> node.col) & 1) && !((node.data[2] >> node.col) & 1) && !((node.data[3] >> node.col) & 1))
        {
            ++node.col;
            --node.width;
        }
        while(!((node.data[0] >> (node.col + node.width - 1)) & 1) && !((node.data[1] >> (node.col + node.width - 1)) & 1) && !((node.data[2] >> (node.col + node.width - 1)) & 1) && !((node.data[3] >> (node.col + node.width - 1)) & 1))
        {
            --node.width;
        }
        for(int x = node.col; x < node.col + node.width; ++x)
        {
            int y;
            for(y = node.height; y > 0; --y)
            {
                if((node.data[y - 1] >> x) & 1)
                {
                    break;
                }
            }
            if(y == 0)
            {
                node.top[x - node.col] = 0;
            }
            else
            {
                node.top[x - node.col] = node.row + y;
            }
            for(y = 0; y < node.height; ++y)
            {
                if((node.data[y] >> x) & 1)
                {
                    break;
                }
            }
            if(y == node.height)
            {
                node.bottom[x - node.col] = h;
            }
            else
            {
                node.bottom[x - node.col] = node.row + y;
            }
        }
        return node;
    }

    //һ��ͨ�õ���תģ��
    template<unsigned char R>
    bool rotate_template(TetrisNode &node, TetrisContext const *context)
    {
        TetrisBlockStatus status =
        {
            node.status.t, node.status.x, node.status.y, R
        };
        return context->create(status, node);
    }

    //����,����,����...

    bool move_left(TetrisNode &node, TetrisContext const *context);
    bool move_right(TetrisNode &node, TetrisContext const *context);
    bool move_down(TetrisNode &node, TetrisContext const *context);
}