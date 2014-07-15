
#pragma once

#include <unordered_map>
#include <vector>
#include <map>
#include <algorithm>

namespace m_tetris
{

    struct TetrisNode;
    struct TetrisOpertion;
    struct TetrisMap;
    union TetrisBlockStatus;
    class TetrisContext;

    //游戏场景,下标从0开始,左下角为原点,最大支持[高度=40,宽度=32]
    struct TetrisMap
    {
        //行数据,具体用法看full函数吧...
        int row[40];
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

    //方块字符
    extern unsigned char const tetris[7];
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
        bool(*rotate_clockwise)(TetrisNode &, TetrisMap const &);
        //逆时针旋转(左旋,r+1)
        bool(*rotate_counterclockwise)(TetrisNode &, TetrisMap const &);
        //转动180°(r+2)
        bool(*rotate_opposite)(TetrisNode &, TetrisMap const &);
        //左移
        bool(*move_left)(TetrisNode &, TetrisMap const &);
        //右移
        bool(*move_right)(TetrisNode &, TetrisMap const &);
        //下落
        bool(*move_down)(TetrisNode &, TetrisMap const &);
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
        std::vector<TetrisNode const *> const *place;

        //以下是指针网的数据
        //对应操作所造成的数据改变全都预置好,不需要再计算
        //如果为空,表示已经到达场景边界或者不支持该操作

        TetrisNode const *rotate_clockwise;
        TetrisNode const *rotate_counterclockwise;
        TetrisNode const *rotate_opposite;
        TetrisNode const *move_left;
        TetrisNode const *move_right;
        TetrisNode const *move_down;
        TetrisNode const *move_down_multi[32];

        TetrisContext const *context;

        //检查当前块是否能够合并入场景
        inline bool check(TetrisMap const &map) const;
        //检查当前块是否是露天的
        inline bool open(TetrisMap const &map) const;
        //当前块合并入场景,同时更新场景数据
        inline size_t attach(TetrisMap &map) const;
        //计算当前块软降位置
        inline TetrisNode const *drop(TetrisMap const &map) const;
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
        inline void clear();
        inline std::pair<TetrisNode const *, char> get(TetrisNode const *key);
        inline bool set(TetrisNode const *key, TetrisNode const *node, char op);
        inline bool mark(TetrisNode const *key);
    };

    class TetrisContext
    {
    private:
        std::unordered_map<TetrisBlockStatus, TetrisNode, TetrisBlockStatusHash, TetrisBlockStatusEqual> node_cache_;
        std::map<std::pair<unsigned char, unsigned char>, TetrisOpertion> init_opertion_;
        std::map<unsigned char, TetrisBlockStatus(*)(TetrisContext const *)> init_generate_;
        int width_, height_;
        int full_;

        std::map<unsigned char, std::vector<TetrisNode const *>> place_cache_;
        TetrisNodeMark node_mark_;
        size_t type_max_;
        TetrisNode const *generate_cache_[256];
        struct
        {
            int data[4];
        } map_danger_data_[256];
        unsigned char index_to_type_[256];
        unsigned char type_to_index_[256];

    public:
        bool prepare(int width, int height);
        int width() const;
        int height() const;
        int full() const;
        TetrisOpertion get_opertion(unsigned char t, unsigned char r) const;
        TetrisNode const *get(TetrisBlockStatus const &status) const;
        TetrisNode const *get(unsigned char t, char x, char y, unsigned char r) const;
        inline TetrisNode const *generate(unsigned char type) const;
        inline TetrisNode const *generate(size_t index) const;
        inline TetrisNode const *generate() const;
        inline size_t map_in_danger(TetrisMap const &map) const;
    };










    //从状态获取指针网节点
    extern inline TetrisNode const *get(TetrisBlockStatus const &status);
    //从状态获取指针网节点
    extern inline TetrisNode const *get(unsigned char t, char x, char y, unsigned char r);
    //把一个方块下落到阻挡位置
    extern inline TetrisNode const *drop(TetrisNode const *node, TetrisMap const &map);
    //获取指定方块(参数必须是OISZLJT之一)
    extern inline TetrisNode const *generate(unsigned char type, TetrisMap const &map);
    //获取指定方块(参数下标,OISZLJT)
    extern inline TetrisNode const *generate(size_t index, TetrisMap const &map);
    //随机一个方块
    extern inline TetrisNode const *generate(TetrisMap const &map);
    //场景危险(下一块出现可能就挂了),返回7中方块有几种会挂掉
    extern inline size_t map_in_danger(TetrisMap const &map);

    //AI初始化(需要调用,且只调用一次)
    extern "C" void attach_init();

    //初始化宽度和高度,这里最大支持32x32的场景(重复调用没有额外成本)
    bool init_ai(int w, int h);

    //简单落点搜索,无软降(旋转,平移,坠地)
    namespace ai_simple
    {
        //创建一个操作路径(为空表示无法到达,末尾自带\0)
        extern std::vector<char> make_path(TetrisNode const *from, TetrisNode const *to, TetrisMap const &map);
        //AI入口
        extern std::pair<TetrisNode const *, int> do_ai(TetrisMap const &map, TetrisNode const *node, unsigned char next[], size_t next_count);
    }

    //复杂落点搜索,软降(寻找所有可到达的位置),性能比无软降版略低(仅仅是低一点而已)
    namespace ai_path
    {
        //创建一个操作路径(为空表示无法到达,末尾自带\0,路径原则是操作尽可能少)
        extern std::vector<char> make_path(TetrisNode const *from, TetrisNode const *to, TetrisMap const &map);
        //AI入口
        extern std::pair<TetrisNode const *, int> do_ai(TetrisMap const &map, TetrisNode const *node, unsigned char next[], size_t next_count);
    }

    //高级落点搜索,软降,踢墙(这里就不解释了),性能还不知道...TODO
    namespace ai_senior
    {
        //创建一个操作路径(为空表示无法到达,末尾自带\0)
        extern std::vector<char> make_path(TetrisNode const *from, TetrisNode const *to, TetrisMap const &map);
        //AI入口
        extern std::pair<TetrisNode const *, int> do_ai(TetrisMap const &map, TetrisNode const *node, unsigned char next[], size_t next_count);
    }

    //misakamm那里得到的mt随机
    namespace ege
    {
        extern void mtsrand(unsigned int s);
        extern unsigned int mtirand();
        extern double mtdrand();
    }
    using ege::mtdrand;
    using ege::mtirand;
    using ege::mtsrand;

    //eavl参数
    struct EvalParam
    {
        EvalParam(TetrisNode const *_node, size_t _clear, TetrisMap const &_map) : node(_node), clear(_clear), map(_map)
        {
        }
        //当前块
        TetrisNode const *node;
        //消行数
        size_t clear;
        //合并前的场景
        TetrisMap const &map;
    };
    //给你的AI返回一个名字吧...
    extern std::string ai_name();
    //eval函数...返回评价值(history参数,越早的历史数据操作越靠前)
    extern int ai_eval(TetrisMap const &map, EvalParam *history, size_t history_length);

    namespace m_tetris_rule_toole
    {

        template<unsigned char T, char X, char Y, unsigned char R, int line1, int line2, int line3, int line4>
        TetrisNode create_node(int w, int h, TetrisOpertion op)
        {
            static_assert(line1 || line2 || line3 || line3, "data error");
            TetrisBlockStatus status =
            {
                T, X, Y, R
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
            TetrisNode const *cache = context->get(status);
            if(cache != nullptr)
            {
                node = *cache;
                return true;
            }
            TetrisOpertion op = context->get_opertion(node.status.t, R);
            TetrisNode new_node = op.create(context->width(), context->height(), op);
            while(new_node.status.x < node.status.x)
            {
                if(!move_right(new_node, map))
                {
                    return false;
                }
            }
            while(new_node.status.y > node.status.y)
            {
                if(!move_down(new_node, map))
                {
                    return false;
                }
            }
            node = new_node;
            return true;
        }

        bool move_left(TetrisNode &node, TetrisMap const &map);

        bool move_right(TetrisNode &node, TetrisMap const &map);

        bool move_down(TetrisNode &node, TetrisMap const &map);
    }

    template<class TetrisRuleSet>
    class TetrisContextBuilder
    {
        void build_context(TetrisContext *context)
        {

        }
    };


    template<class TetrisRuleSet, class TetrisLandPointSearchEngine, class TetrisMapEval, class TetrisLandPointEval, class TetrisPruning, size_t MaxDeep>
    class TetrisEngine
    {

    };

}