
#pragma once

#include <unordered_map>
#include <vector>
#include <map>
#include <algorithm>
#include <functional>

namespace m_tetris
{
    const int max_height = 40;

    struct TetrisNode;
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
    };

    //方块状态
    //t:OISZLJT字符
    //[x,y]坐标,y越大高度越大
    //r:旋转状态
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

    //方块操作
    struct TetrisOpertion
    {
        //创建一个方块
        TetrisNode(*create)(int w, int h, TetrisOpertion op);
        //顺时针旋转(右旋,r-1)
        bool(*rotate_clockwise)(TetrisNode &node, TetrisContext const *context);
        //逆时针旋转(左旋,r+1)
        bool(*rotate_counterclockwise)(TetrisNode &node, TetrisContext const *context);
        //转动180°(r+2)
        bool(*rotate_opposite)(TetrisNode &node, TetrisContext const *context);
        //左移
        bool(*move_left)(TetrisNode &node, TetrisContext const *context);
        //右移
        bool(*move_right)(TetrisNode &node, TetrisContext const *context);
        //下落
        bool(*move_down)(TetrisNode &node, TetrisContext const *context);
    };

    //指针网节点
    struct TetrisNode
    {
    public:
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
        size_t index;
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
        TetrisNode const *move_down_multi[max_height];

        TetrisContext const *context;

        //检查当前块是否能够合并入场景
        bool check(TetrisMap const &map) const;
        //检查当前块是否是露天的
        bool open(TetrisMap const &map) const;
        //当前块合并入场景,同时更新场景数据
        size_t attach(TetrisMap &map) const;
        //计算当前块软降位置
        TetrisNode const *drop(TetrisMap const &map) const;
    };

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
        std::pair<TetrisNode const *, char> get(TetrisNode const *key);
        bool set(TetrisNode const *key, TetrisNode const *node, char op);
        bool mark(TetrisNode const *key);
    };

    template<class T>
    struct TetrisContextBuilder;

    class TetrisContext
    {
        template<class T>
        friend struct TetrisContextBuilder;
    private:
        TetrisContext()
        {
        }
        std::unordered_map<TetrisBlockStatus, TetrisNode, TetrisBlockStatusHash, TetrisBlockStatusEqual> node_cache_;
        std::map<std::pair<unsigned char, unsigned char>, TetrisOpertion> init_opertion_;
        std::map<unsigned char, TetrisBlockStatus(*)(TetrisContext const *)> init_generate_;
        int width_, height_;
        int full_;

        std::map<unsigned char, std::vector<TetrisNode const *>> place_cache_;
        size_t type_max_;
        TetrisNode const *generate_cache_[256];
        unsigned char index_to_type_[256];
        unsigned char type_to_index_[256];

    public:
        enum PrepareResult : int
        {
            fail = 0, ok = 1, rebuild = 2,
        };
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

    template<class LandPointEval, class MapEval>
    struct EvalData
    {
        TetrisNode const *node;
        size_t clear;
        TetrisMap map;
        LandPointEval land_point_eval;
        MapEval map_eval;
    };

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

    template<class MapEval>
    struct PruneParam
    {
        PruneParam(MapEval const &_eval) : eval(_eval), pruned(false)
        {
        }
        MapEval eval;
        bool pruned;
    };

    template<class TetrisRuleSet>
    struct TetrisContextBuilder
    {
        static TetrisContext build_context()
        {
            TetrisContext context = {};
            context.init_opertion_ = TetrisRuleSet::get_opertion_info();
            context.init_generate_ = TetrisRuleSet::get_generate_info();
            return context;
        }
    };
    
    template<class Type>
    struct TetrisCallInit
    {
        template<class T>
        struct CallInit
        {
            CallInit(Type &type, TetrisContext const *context)
            {
            }
        };
        template<>
        struct CallInit<std::true_type>
        {
            CallInit(Type &type, TetrisContext const *context)
            {
                type.init(context);
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
        TetrisCallInit(Type &type, TetrisContext const *context)
        {
            CallInit<decltype(func<Derived>(nullptr))>(type, context);
        }
    };

    template<class Type>
    struct TetrisCallProcess
    {
        template<class T>
        struct CallProcess
        {
            template<class Data, class... Param>
            static Data invoke(Type &type, Data const &data, Param const &... param)
            {
                return data;
            }
        };
        template<>
        struct CallProcess<std::true_type>
        {
            template<class Data, class... Param>
            static Data invoke(Type &type, Data const &data, Param const &... param)
            {
                return type.process(data, param...);
            }
        };
        struct Fallback
        {
            int process;
        };
        struct Derived : Type, Fallback
        {
        };
        template<typename U, U> struct Check;
        template<typename U>
        static std::false_type func(Check<int Fallback::*, &U::process> *);
        template<typename U>
        static std::true_type func(...);
    public:
        template<class Data, class... Param>
        static Data invoke(Type &type, Data const &data, Param const &... param)
        {
            return CallProcess<decltype(func<Derived>(nullptr))>::invoke(type, data, param...);
        }
    };

    template<class Type>
    struct TetrisGetVirtualValue
    {
        template<class T>
        struct CallGetVirtualValue
        {
            template<class Eval>
            static Eval invoke(Type &type, Eval const *eval, size_t eval_length)
            {
                Eval value = 0;
                for(size_t i = 0; i < eval_length; ++i)
                {
                    value += eval[i];
                }
                return value / eval_length;
            }
        };
        template<>
        struct CallGetVirtualValue<std::true_type>
        {
            template<class Eval>
            static Eval invoke(Type &type, Eval const *eval, size_t eval_length)
            {
                return type.get_vritual_eval(eval, eval_length);
            }
        };
        struct Fallback
        {
            int get_vritual_eval;
        };
        struct Derived : Type, Fallback
        {
        };
        template<typename U, U> struct Check;
        template<typename U>
        static std::false_type func(Check<int Fallback::*, &U::get_vritual_eval> *);
        template<typename U>
        static std::true_type func(...);
    public:
        template<class Eval>
        static Eval invoke(Type &type, Eval const *eval, size_t eval_length)
        {
            return CallGetVirtualValue<decltype(func<Derived>(nullptr))>::invoke(type, eval, eval_length);
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

    template<class TetrisAI, class TetrisLandPointSearchEngine, size_t NextLength, class HasLandPointEval>
    class TetrisCore
    {
    public:
        typedef decltype(TetrisAI().eval_map(TetrisMap(), nullptr, 0)) MapEval;
        typedef EvalParam<> EvalParam;
        typedef std::function<std::pair<TetrisNode const *, MapEval>(TetrisMap const &, TetrisNode const *, std::vector<EvalParam> &history, unsigned char *)> CallAI;
        TetrisCore(TetrisContext const *context, TetrisAI &ai, std::vector<CallAI> &call_ai) : context_(context), ai_(ai), next_(context, ai, call_ai)
        {
            call_ai.push_back(std::bind(&TetrisCore::run, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));
        }
        void init(TetrisContext const *context)
        {
            virtual_eval_.resize(context->type_max());
            TetrisCallInit<TetrisLandPointSearchEngine>(search_, context);
            next_.init(context);
        }
        std::pair<TetrisNode const *, MapEval> run(TetrisMap const &map, TetrisNode const *node, std::vector<EvalParam> &history, unsigned char *next)
        {
            std::vector<TetrisNode const *> const *land_point = search_.search(map, node);
            TetrisMap copy;
            MapEval eval = ai_.eval_map_bad();
            TetrisNode const *best_node = node;
            for(auto cit = land_point->begin(); cit != land_point->end(); ++cit)
            {
                node = TetrisCallProcess<TetrisLandPointSearchEngine>::invoke(search_, *cit, map);
                copy = map;
                history.push_back(EvalParam(node, node->attach(copy), map));
                MapEval new_eval;
                if(*next == ' ')
                {
                    for(size_t i = 0; i < context_->type_max(); ++i)
                    {
                        TetrisNode const *next_node = context_->generate(i);
                        virtual_eval_[i] = next_node->check(copy) ? next_.run(copy, next_node, history, next + 1).second : ai_.eval_map_bad();
                    }
                    new_eval = TetrisGetVirtualValue<TetrisAI>::invoke(ai_, virtual_eval_.data(), virtual_eval_.size());
                }
                else
                {
                    TetrisNode const *next_node = context_->generate(*next);
                    new_eval = next_node->check(copy) ? next_.run(copy, next_node, history, next + 1).second : ai_.eval_map_bad();
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
        TetrisCore<TetrisAI, TetrisLandPointSearchEngine, NextLength - 1, HasLandPointEval> next_;
    };

    template<class TetrisAI, class TetrisLandPointSearchEngine, class HasLandPointEval>
    class TetrisCore<TetrisAI, TetrisLandPointSearchEngine, 0, HasLandPointEval>
    {
    public:
        typedef decltype(TetrisAI().eval_map(TetrisMap(), nullptr, 0)) MapEval;
        typedef EvalParam<> EvalParam;
        typedef std::function<std::pair<TetrisNode const *, MapEval>(TetrisMap const &, TetrisNode const *, std::vector<EvalParam> &history, unsigned char *)> CallAI;
        TetrisCore(TetrisContext const *context, TetrisAI &ai, std::vector<CallAI> &call_ai) : context_(context), ai_(ai)
        {
            call_ai.push_back(std::bind(&TetrisCore::run, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));
        }
        void init(TetrisContext const *context)
        {
            TetrisCallInit<TetrisLandPointSearchEngine>(search_, context);
        }
        std::pair<TetrisNode const *, MapEval> run(TetrisMap const &map, TetrisNode const *node, std::vector<EvalParam> &history, unsigned char *)
        {
            std::vector<TetrisNode const *> const *land_point = search_.search(map, node);
            MapEval eval = ai_.eval_map_bad();
            TetrisNode const *best_node = node;
            for(auto cit = land_point->begin(); cit != land_point->end(); ++cit)
            {
                node = TetrisCallProcess<TetrisLandPointSearchEngine>::invoke(search_, *cit, map);
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

    template<class TetrisAI, class TetrisLandPointSearchEngine, size_t NextLength>
    class TetrisCore<TetrisAI, TetrisLandPointSearchEngine, NextLength, std::true_type>
    {
    public:
        typedef decltype(TetrisAI().eval_land_point(nullptr, TetrisMap(), 0)) LandPointEval;
        typedef decltype(TetrisAI().eval_map(TetrisMap(), nullptr, 0)) MapEval;
        typedef EvalParam<LandPointEval> EvalParam;
        typedef std::function<std::pair<TetrisNode const *, MapEval>(TetrisMap const &, TetrisNode const *, std::vector<EvalParam> &history, unsigned char *)> CallAI;
        TetrisCore(TetrisContext const *context, TetrisAI &ai, std::vector<CallAI> &call_ai) : context_(context), ai_(ai), next_(context, ai, call_ai)
        {
            call_ai.push_back(std::bind(&TetrisCore::run, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));
        }
        void init(TetrisContext const *context)
        {
            virtual_eval_.resize(context->type_max());
            TetrisCallInit<TetrisLandPointSearchEngine>(search_, context);
            next_.init(context);
        }
        std::pair<TetrisNode const *, MapEval> run(TetrisMap const &map, TetrisNode const *node, std::vector<EvalParam> &history, unsigned char *next)
        {
            std::vector<TetrisNode const *> const *land_point = search_.search(map, node);
            MapEval eval = ai_.eval_map_bad();
            TetrisNode const *best_node = node;
            for(auto cit = land_point->begin(); cit != land_point->end(); ++cit)
            {
                node = TetrisCallProcess<TetrisLandPointSearchEngine>::invoke(search_, *cit, map);
                TetrisMap copy = map;
                size_t clear = node->attach(copy);
                history.push_back(EvalParam(node, clear, map, ai_.eval_land_point(node, copy, clear)));
                MapEval new_eval;
                if(*next == ' ')
                {
                    for(size_t i = 0; i < context_->type_max(); ++i)
                    {
                        TetrisNode const *next_node = context_->generate(i);
                        if(next_node->check(map))
                        {
                            virtual_eval_[i] = next_.run(copy, next_node, history, next + 1).second;
                        }
                        else
                        {
                            virtual_eval_[i] = ai_.eval_map_bad();
                        }
                    }
                    new_eval = TetrisGetVirtualValue<TetrisAI>::invoke(ai_, virtual_eval_.data(), virtual_eval_.size());
                }
                else
                {
                    TetrisNode const *next_node = context_->generate(*next);
                    if(next_node->check(map))
                    {
                        new_eval = next_.run(copy, next_node, history, next + 1).second;
                    }
                    else
                    {
                        new_eval = ai_.eval_map_bad();
                    }
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
        TetrisCore<TetrisAI, TetrisLandPointSearchEngine, NextLength - 1, std::true_type> next_;
    };

    template<class TetrisAI, class TetrisLandPointSearchEngine>
    class TetrisCore<TetrisAI, TetrisLandPointSearchEngine, 0, std::true_type>
    {
    public:
        typedef decltype(TetrisAI().eval_land_point(nullptr, TetrisMap(), 0)) LandPointEval;
        typedef decltype(TetrisAI().eval_map(TetrisMap(), nullptr, 0)) MapEval;
        typedef EvalParam<LandPointEval> EvalParam;
        typedef std::function<std::pair<TetrisNode const *, MapEval>(TetrisMap const &, TetrisNode const *, std::vector<EvalParam> &history, unsigned char *)> CallAI;
        TetrisCore(TetrisContext const *context, TetrisAI &ai, std::vector<CallAI> &call_ai) : context_(context), ai_(ai)
        {
            call_ai.push_back(std::bind(&TetrisCore::run, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));
        }
        void init(TetrisContext const *context)
        {
            TetrisCallInit<TetrisLandPointSearchEngine>(search_, context);
        }
        std::pair<TetrisNode const *, MapEval> run(TetrisMap const &map, TetrisNode const *node, std::vector<EvalParam> &history, unsigned char *)
        {
            std::vector<TetrisNode const *> const *land_point = search_.search(map, node);
            MapEval eval = ai_.eval_map_bad();
            TetrisNode const *best_node = node;
            for(auto cit = land_point->begin(); cit != land_point->end(); ++cit)
            {
                node = TetrisCallProcess<TetrisLandPointSearchEngine>::invoke(search_, *cit, map);
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

    template<class TetrisRuleSet, class TetrisAI, class TetrisLandPointSearchEngine, size_t MaxNext>
    class TetrisEngine
    {
    private:
        typedef TetrisCore<TetrisAI, TetrisLandPointSearchEngine, MaxNext, typename TetrisAIHasLandPointEval<TetrisAI>::type> Core;
        TetrisContext context_;
        TetrisAI ai_;
        TetrisLandPointSearchEngine search_;
        std::vector<typename Core::CallAI> call_ai_;
        Core core_;
        std::vector<typename Core::EvalParam> history_;

    public:
        TetrisEngine() : context_(TetrisContextBuilder<TetrisRuleSet>::build_context()), ai_(), call_ai_(), core_(&context_, ai_, call_ai_), history_()
        {
        }
        TetrisNode const *get(TetrisBlockStatus const &status) const
        {
            return context_.get(status);
        }
        TetrisContext const *context() const
        {
            return &context_;
        }
        std::string ai_name() const
        {
            return ai_.ai_name();
        }
        bool prepare(int width, int height)
        {
            TetrisContext::PrepareResult result = context_.prepare(width, height);
            if(result == TetrisContext::rebuild)
            {
                TetrisCallInit<TetrisAI>(ai_, &context_);
                TetrisCallInit<TetrisLandPointSearchEngine>(search_, &context_);
                TetrisCallInit<Core>(core_, &context_);
                return true;
            }
            else if(result == TetrisContext::fail)
            {
                return false;
            }
            return true;
        }
        TetrisNode const *run(TetrisMap const &map, TetrisNode const *node, unsigned char *next, size_t next_length)
        {
            if(node == nullptr || !node->check(map))
            {
                return nullptr;
            }
            return call_ai_[std::min(MaxNext, next_length)](map, node, history_, next).first;
        }
        std::vector<char> path(TetrisNode const *node, TetrisNode const *land_point, TetrisMap const &map)
        {
            return search_.make_path(node, land_point, map);
        }
    };

}

namespace m_tetris_rule_tools
{
    using namespace m_tetris;
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
            node.top[x - node.col] = node.row + y;
            for(y = 0; y < node.height; ++y)
            {
                if((node.data[y] >> x) & 1)
                {
                    break;
                }
            }
            node.bottom[x - node.col] = node.row + y;
        }
        return node;
    }

    template<unsigned char R>
    bool rotate_template(TetrisNode &node, TetrisContext const *context)
    {
        TetrisBlockStatus status =
        {
            node.status.t, node.status.x, node.status.y, R
        };
        return context->create(status, node);
    }

    bool move_left(TetrisNode &node, TetrisContext const *context);
    bool move_right(TetrisNode &node, TetrisContext const *context);
    bool move_down(TetrisNode &node, TetrisContext const *context);
}