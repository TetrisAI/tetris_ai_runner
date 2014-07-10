
#pragma once

#include <unordered_map>
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
    //原始矩形在场景中的下沿
    int low;
    //指针网索引
    size_t index;

    //以是指针网的数据
    //对应操作所造成的数据改变全都预置好,不需要再计算
    //如果为空,表示已经到达场景边界或者不支持该操作

    TetrisNode const *rotate_clockwise;
    TetrisNode const *rotate_counterclockwise;
    TetrisNode const *rotate_opposite;
    TetrisNode const *move_left;
    TetrisNode const *move_right;
    TetrisNode const *move_down;

    //检查当前块是否能够合并入场景
    bool check(TetrisMap const &map) const
    {
        return !(check_row(0, map) || check_row(1, map) || check_row(2, map) || check_row(3, map));
    }
    //检查当前块是否是露天的
    bool open(TetrisMap const &map) const
    {
        return open_col(0, map) && open_col(1, map) && open_col(2, map) && open_col(3, map);
    }
    //当前块合并入场景,同时更新场景数据
    size_t attach(TetrisMap &map) const
    {
        const int full = (1 << map.width) - 1;
        attach_row(0, map);
        attach_row(1, map);
        attach_row(2, map);
        attach_row(3, map);
        int clear = 0;
        clear += clear_row(3, full, map);
        clear += clear_row(2, full, map);
        clear += clear_row(1, full, map);
        clear += clear_row(0, full, map);
        update_top(0, map);
        update_top(1, map);
        update_top(2, map);
        update_top(3, map);
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
    //计算当前块可以下落多少格
    size_t drop(TetrisMap const &map) const
    {
        int value = map.height;
        drop_col(0, value, map);
        drop_col(1, value, map);
        drop_col(2, value, map);
        drop_col(3, value, map);
        return value > 0 ? value : 0;
    }
private:
    inline int check_row(int offset, TetrisMap const &map) const
    {
        if(offset < height)
        {
            return map.row[row + offset] & data[offset];
        }
        return 0;
    }
    inline int open_col(int offset, TetrisMap const &map) const
    {
        if(offset < width)
        {
            return bottom[offset] >= map.top[col + offset];
        }
        return true;
    }
    inline void update_top(int offset, TetrisMap &map) const
    {
        if(offset < width)
        {
            int &value = map.top[col + offset];
            value = std::max<int>(value, top[offset]);
            if(value > map.roof)
            {
                map.roof = value;
            }
        }
    }
    inline void attach_row(int offset, TetrisMap &map) const
    {
        if(offset < height)
        {
            map.row[row + offset] |= data[offset];
        }
    }
    inline int clear_row(int offset, int const full, TetrisMap &map) const
    {
        if(offset >= height)
        {
            return 0;
        }
        int index = row + offset;
        if(map.row[index] == full)
        {
            memmove(&map.row[index], &map.row[index + 1], (map.height - index - 1) * sizeof(int));
            map.row[map.height - 1] = 0;
            return 1;
        }
        return 0;
    }
    inline void drop_col(int offset, int &value, TetrisMap const &map) const
    {
        if(offset < width)
        {
            value = std::min<int>(value, bottom[offset] - map.top[offset + col]);
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
//随机一个方块
extern inline TetrisNode const *generate(TetrisMap const &map);

//AI初始化
extern "C" void attach_init();

//初始化宽度和高度(重复调用没有额外成本)
bool init_ai(int w, int h);
//从char array构建一个场景
void build_map(char board[], int w, int h, TetrisMap &map);

//简单落点搜索(旋转,平移,坠地)
namespace ai_simple
{
    //AI入口
    extern std::pair<TetrisNode const *, int> do_ai(TetrisMap const &primeval_map, TetrisMap const &map, TetrisNode const *node, unsigned char next[], size_t next_count);
}

//复杂落点搜索(寻找所有可到达的位置)
namespace ai_path
{
    //创建一个操作路径(为空表示无法到达,末尾自带\0,路径原则是尽可能短)
    extern std::vector<char> make_path(TetrisNode const *from, TetrisNode const *to, TetrisMap const &map);
    //AI入口
    extern std::pair<TetrisNode const *, int> do_ai(TetrisMap const &primeval_map, TetrisMap const &map, TetrisNode const *node, unsigned char next[], size_t next_count);
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
//eval函数...返回评价值(history参数,越早的历史数据操作越靠前)
extern int ai_eval(TetrisMap const &map, EvalParam *history, size_t history_length);