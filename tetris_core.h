
#pragma once

#include <unordered_map>
#include <vector>
#include <algorithm>


struct TetrisNode;
struct TetrisOpertion;
struct TetrisMap;
union TetrisBlockStatus;

//游戏场景,下标从0开始,左下角为原点,最大支持[高度=32,宽度=32]
struct TetrisMap
{
    //行数据,具体用法看full函数吧...
    int row[32];
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

template<>
struct std::hash<TetrisBlockStatus>
{
    size_t operator()(TetrisBlockStatus const &block) const
    {
        return std::hash<size_t>()(block.status);
    };
};

template<>
struct std::equal_to<TetrisBlockStatus>
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
    //从指针网获取得初始方块
    TetrisNode const *(*generate)(TetrisMap const &);
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

    //以是指针网的数据
    //对应操作所造成的数据改变全都预置好,不需要再计算
    //如果为空,表示已经到达场景边界或者不支持该操作

    TetrisNode const *rotate_clockwise;
    TetrisNode const *rotate_counterclockwise;
    TetrisNode const *rotate_opposite;
    TetrisNode const *move_left;
    TetrisNode const *move_right;
    TetrisNode const *move_down;
    TetrisNode const *move_down_multi[32];

    //检查当前块是否能够合并入场景
    inline bool check(TetrisMap const &map) const
    {
        if(map.row[row] & data[0])
        {
            return false;
        }
        if(height == 1)
        {
            return true;
        }
        if(map.row[row + 1] & data[1])
        {
            return false;
        }
        if(height == 2)
        {
            return true;
        }
        if(map.row[row + 2] & data[2])
        {
            return false;
        }
        if(height == 3)
        {
            return true;
        }
        if(map.row[row + 3] & data[3])
        {
            return false;
        }
        return true;
    }
    //检查当前块是否是露天的
    inline bool open(TetrisMap const &map) const
    {
        if(bottom[0] < map.top[col])
        {
            return false;
        }
        if(width == 1)
        {
            return true;
        }
        if(bottom[1] < map.top[col + 1])
        {
            return false;
        }
        if(width == 2)
        {
            return true;
        }
        if(bottom[2] < map.top[col + 2])
        {
            return false;
        }
        if(width == 3)
        {
            return true;
        }
        if(bottom[3] < map.top[col + 3])
        {
            return false;
        }
        return true;
    }
    //当前块合并入场景,同时更新场景数据
    inline size_t attach(TetrisMap &map) const
    {
        const int full = (1 << map.width) - 1;
        map.row[row] |= data[0];
        if(height > 1)
        {
            map.row[row + 1] |= data[1];
            if(height > 2)
            {
                map.row[row + 2] |= data[2];
                if(height > 3)
                {
                    map.row[row + 3] |= data[3];
                }
            }
        }
        int clear = 0;
        for(int i = height; i > 0; --i)
        {
            if(map.row[row + i - 1] == full)
            {
                memmove(&map.row[row + i - 1], &map.row[row + i], (map.height - i) * sizeof(int));
                map.row[map.height - 1] = 0;
                ++clear;
            }
        }
        if(top[0] > map.top[col])
        {
            map.top[col] = top[0];
            map.roof = std::max(top[0], map.roof);
        }
        if(width > 1)
        {
            if(top[1] > map.top[col + 1])
            {
                map.top[col + 1] = top[1];
                map.roof = std::max(top[1], map.roof);
            }
            if(width > 2)
            {
                if(top[2] > map.top[col + 2])
                {
                    map.top[col + 2] = top[2];
                    map.roof = std::max(top[2], map.roof);
                }
                if(width > 3)
                {
                    if(top[3] > map.top[col + 3])
                    {
                        map.top[col + 3] = top[3];
                        map.roof = std::max(top[3], map.roof);
                    }
                }
            }
        }
        map.roof -= clear;
        map.count += 4 - clear * map.width;
        if(clear > 0)
        {
            for(int x = 0; x < map.width; ++x)
            {
                map.top[x] = 0;
                for(int y = map.roof - 1; y >= 0; --y)
                {
                    if(map.full(x, y))
                    {
                        map.top[x] = y + 1;
                        break;
                    }
                }
            }
        }
        return clear;
    }
    //计算当前块软降位置
    inline TetrisNode const *drop(TetrisMap const &map) const
    {
        int value = bottom[0] - map.top[col];
        if(width > 1)
        {
            value = std::min<int>(value, bottom[1] - map.top[col + 1]);
            if(width > 2)
            {
                value = std::min<int>(value, bottom[2] - map.top[col + 2]);
                if(width > 3)
                {
                    value = std::min<int>(value, bottom[3] - map.top[col + 3]);
                }
            }
        }
        if(value >= 0)
        {
            return move_down_multi[value];
        }
        else
        {
            TetrisNode const *node = this;
            while(node->move_down != nullptr && node->move_down->check(map))
            {
                node = node->move_down;
            }
            return node;
        }
    }
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
//场景危险(下一块出现可能就挂了)
extern inline bool map_in_danger(TetrisMap const &map);

//AI初始化(需要调用,且只调用一次)
extern "C" void attach_init();

//初始化宽度和高度,这里最大支持32x32的场景(重复调用没有额外成本)
bool init_ai(int w, int h);

//简单落点搜索,无软降(旋转,平移,坠地)
namespace ai_simple
{
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