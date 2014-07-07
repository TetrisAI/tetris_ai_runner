#define _CRT_SECURE_NO_DEPRECATE
#define _CRT_SECURE_NO_WARNINGS

#include <string.h>

#define DECLSPEC_EXPORT __declspec(dllexport)
#define WINAPI __stdcall

char gName[64]; // 返回名字用，必须全局

#ifdef __cplusplus
extern "C" {
#endif

// 返回AI名字，会显示在界面上
DECLSPEC_EXPORT
char* WINAPI Name()
{
	strcpy(gName, "ZouZhiZhang v0.1");
	return gName;
}

#ifdef __cplusplus
}
#endif

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

#include <unordered_map>
#include <unordered_set>
#include <map>
#include <set>
#include <algorithm>
#include <fstream>
#include <array>
#include <functional>
#include <string>
#include <ctime>

struct TetrisNode;
struct TetrisOpertion;
struct TetrisMap;
union TetrisBlockStatus;

struct TetrisMap
{
    int row[32];
    int top[32];
    int width;
    int height;
    int roof;
    int count;
    inline bool full(int x, int y) const
    {
        return (row[y] >> x) & 1;
    }
};

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

struct TetrisOpertion
{
    TetrisNode(*create)(int w, int h, TetrisOpertion op);
    TetrisNode const *(*generate)(TetrisMap const &);
    bool(*rotate_clockwise)(TetrisNode &, TetrisMap const &);
    bool(*rotate_counterclockwise)(TetrisNode &, TetrisMap const &);
    bool(*rotate_opposite)(TetrisNode &, TetrisMap const &);
    bool(*move_left)(TetrisNode &, TetrisMap const &);
    bool(*move_right)(TetrisNode &, TetrisMap const &);
    bool(*move_down)(TetrisNode &, TetrisMap const &);
};

struct TetrisNode
{
    TetrisBlockStatus status;
    TetrisOpertion op;
    int data[4];
    int top[4];
    int bottom[4];
    char row, height, col, width;

    TetrisNode const *rotate_clockwise;
    TetrisNode const *rotate_counterclockwise;
    TetrisNode const *rotate_opposite;
    TetrisNode const *move_left;
    TetrisNode const *move_right;
    TetrisNode const *move_down;

    bool check(TetrisMap const &map) const
    {
        return !(check_row(0, map) || check_row(1, map) || check_row(2, map) || check_row(3, map));
    }
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
        else
        {
            update_top(0, map);
            update_top(1, map);
            update_top(2, map);
            update_top(3, map);
        }
        return clear;
    }
    size_t drop(TetrisMap const &map) const
    {
        int value = map.height;
        drop_rows(0, value, map);
        drop_rows(1, value, map);
        drop_rows(2, value, map);
        drop_rows(3, value, map);
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
    inline void drop_rows(int offset, int &value, TetrisMap const &map) const
    {
        if(offset < width)
        {
            value = std::min<int>(value, bottom[offset] - map.top[offset + col]);
        }
    }
};

typedef std::unordered_map<TetrisBlockStatus, TetrisNode> NodeCache_t;
NodeCache_t node_cache;

inline TetrisNode const *get(TetrisBlockStatus const &status)
{
    auto find = node_cache.find(status);
    if(find == node_cache.end())
    {
        return nullptr;
    }
    else
    {
        return &find->second;
    }
}

inline TetrisNode const *get(unsigned char t, char x, char y, unsigned r)
{
    TetrisBlockStatus status =
    {
        t, x, y, r
    };
    return get(status);
}

inline TetrisNode const *drop(TetrisNode const *node, TetrisMap const &map)
{
    TetrisBlockStatus status =
    {
        node->status.t, node->status.x, node->status.y - node->drop(map), node->status.r
    };
    return get(status);
}
std::map<std::pair<unsigned char, unsigned char>, TetrisOpertion> init_op;
std::map<unsigned char, TetrisNode const *(*)(TetrisMap const &)> op;

////////////////////////////////////////////////////////////////////////////////

template<unsigned char T, unsigned char R, int line1, int line2, int line3, int line4>
TetrisNode create_template(int w, int h, TetrisOpertion op)
{
    static_assert(line1 || line2 || line3 || line3, "data error");
    TetrisBlockStatus status =
    {
        T, w / 2, h - 2, R
    };
    TetrisNode node =
    {
        status, op, {line4 << (status.x - 2), line3 << (status.x - 2), line2 << (status.x - 2), line1 << (status.x - 2)}, {}, {}, h - 4, 4, w / 2 - 2, 4
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

template<unsigned char T>
TetrisNode const *generate_template(TetrisMap const &map)
{
    TetrisBlockStatus status =
    {
        T, map.width / 2, map.height - 2, 1
    };
    return get(status);
}

bool move_left(TetrisNode &node, TetrisMap const &map)
{
    if((node.data[0] & 1) || (node.data[1] & 1) || (node.data[2] & 1) || (node.data[3] & 1))
    {
        return false;
    }
    node.data[0] >>= 1;
    node.data[1] >>= 1;
    node.data[2] >>= 1;
    node.data[3] >>= 1;
    --node.col;
    --node.status.x;
    return true;
}

bool move_right(TetrisNode &node, TetrisMap const &map)
{
    const int check = 1 << (map.width - 1);
    if((node.data[0] & check) || (node.data[1] & check) || (node.data[2] & check) || (node.data[3] & check))
    {
        return false;
    }
    node.data[0] <<= 1;
    node.data[1] <<= 1;
    node.data[2] <<= 1;
    node.data[3] <<= 1;
    ++node.col;
    ++node.status.x;
    return true;
}

bool move_down(TetrisNode &node, TetrisMap const &map)
{
    if(node.row == 0)
    {
        return false;
    }
    for(int x = 0; x < node.width; ++x)
    {
        --node.top[x];
        --node.bottom[x];
    }
    --node.row;
    --node.status.y;
    return true;
}

template<unsigned char R>
bool rotate_template(TetrisNode &node, TetrisMap const &map)
{
    TetrisBlockStatus status =
    {
        node.status.t, node.status.x, node.status.y, R
    };
    TetrisNode const *cache = get(status);
    if(cache != nullptr)
    {
        node = *cache;
        return true;
    }
    TetrisOpertion op = init_op[std::make_pair(node.status.t, R)];
    TetrisNode new_node = op.create(map.width, map.height, op);
    while(new_node.status.x > node.status.x)
    {
        if(!move_left(new_node, map))
        {
            return false;
        }
    }
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

////////////////////////////////////////////////////////////////////////////////
namespace ege
{
    extern void mtsrand(unsigned int s);
    extern unsigned int mtirand();
    extern double mtdrand();
}
using ege::mtdrand;
using ege::mtirand;
using ege::mtsrand;

extern "C" void attach_init()
{
#define T(a, b, c, d) (((a) ? 1 : 0) | ((b) ? 2 : 0) | ((c) ? 4 : 0) | ((d) ? 8 : 0))
    TetrisOpertion op_O1 =
    {
        create_template<'O', 1,
        T(0, 0, 0, 0),
        T(0, 1, 1, 0),
        T(0, 1, 1, 0),
        T(0, 0, 0, 0)>,
        generate_template<'O'>,
        nullptr,
        nullptr,
        nullptr,
        move_left,
        move_right,
        move_down,
    };
    TetrisOpertion op_I1 =
    {
        create_template<'I', 1,
        T(0, 0, 0, 0),
        T(1, 1, 1, 1),
        T(0, 0, 0, 0),
        T(0, 0, 0, 0)>,
        generate_template<'I'>,
        rotate_template<2>,
        rotate_template<2>,
        nullptr,
        move_left,
        move_right,
        move_down,
    };
    TetrisOpertion op_I2 =
    {
        create_template<'I', 2,
        T(0, 0, 1, 0),
        T(0, 0, 1, 0),
        T(0, 0, 1, 0),
        T(0, 0, 1, 0)>,
        generate_template<'I'>,
        rotate_template<1>,
        rotate_template<1>,
        nullptr,
        move_left,
        move_right,
        move_down,
    };
    TetrisOpertion op_S1 =
    {
        create_template<'S', 1,
        T(0, 0, 0, 0),
        T(0, 0, 1, 1),
        T(0, 1, 1, 0),
        T(0, 0, 0, 0)>,
        generate_template<'S'>,
        rotate_template<2>,
        rotate_template<2>,
        nullptr,
        move_left,
        move_right,
        move_down,
    };
    TetrisOpertion op_S2 =
    {
        create_template<'S', 2,
        T(0, 0, 1, 0),
        T(0, 0, 1, 1),
        T(0, 0, 0, 1),
        T(0, 0, 0, 0)>,
        generate_template<'S'>,
        rotate_template<1>,
        rotate_template<1>,
        nullptr,
        move_left,
        move_right,
        move_down,
    };
    TetrisOpertion op_Z1 =
    {
        create_template<'Z', 1,
        T(0, 0, 0, 0),
        T(0, 1, 1, 0),
        T(0, 0, 1, 1),
        T(0, 0, 0, 0)>,
        generate_template<'Z'>,
        rotate_template<2>,
        rotate_template<2>,
        nullptr,
        move_left,
        move_right,
        move_down,
    };
    TetrisOpertion op_Z2 =
    {
        create_template<'Z', 2,
        T(0, 0, 0, 1),
        T(0, 0, 1, 1),
        T(0, 0, 1, 0),
        T(0, 0, 0, 0)>,
        generate_template<'Z'>,
        rotate_template<1>,
        rotate_template<1>,
        nullptr,
        move_left,
        move_right,
        move_down,
    };
    TetrisOpertion op_L1 =
    {
        create_template<'L', 1,
        T(0, 0, 0, 0),
        T(0, 1, 1, 1),
        T(0, 1, 0, 0),
        T(0, 0, 0, 0)>,
        generate_template<'L'>,
        rotate_template<4>,
        rotate_template<2>,
        nullptr,
        move_left,
        move_right,
        move_down,
    };
    TetrisOpertion op_L2 =
    {
        create_template<'L', 2,
        T(0, 0, 1, 0),
        T(0, 0, 1, 0),
        T(0, 0, 1, 1),
        T(0, 0, 0, 0)>,
        generate_template<'L'>,
        rotate_template<1>,
        rotate_template<3>,
        nullptr,
        move_left,
        move_right,
        move_down,
    };
    TetrisOpertion op_L3 =
    {
        create_template<'L', 3,
        T(0, 0, 0, 1),
        T(0, 1, 1, 1),
        T(0, 0, 0, 0),
        T(0, 0, 0, 0)>,
        generate_template<'L'>,
        rotate_template<2>,
        rotate_template<4>,
        nullptr,
        move_left,
        move_right,
        move_down,
    };
    TetrisOpertion op_L4 =
    {
        create_template<'L', 4,
        T(0, 1, 1, 0),
        T(0, 0, 1, 0),
        T(0, 0, 1, 0),
        T(0, 0, 0, 0)>,
        generate_template<'L'>,
        rotate_template<3>,
        rotate_template<1>,
        nullptr,
        move_left,
        move_right,
        move_down,
    };
    TetrisOpertion op_J1 =
    {
        create_template<'J', 1,
        T(0, 0, 0, 0),
        T(0, 1, 1, 1),
        T(0, 0, 0, 1),
        T(0, 0, 0, 0)>,
        generate_template<'J'>,
        rotate_template<4>,
        rotate_template<2>,
        nullptr,
        move_left,
        move_right,
        move_down,
    };
    TetrisOpertion op_J2 =
    {
        create_template<'J', 2,
        T(0, 0, 1, 1),
        T(0, 0, 1, 0),
        T(0, 0, 1, 0),
        T(0, 0, 0, 0)>,
        generate_template<'J'>,
        rotate_template<1>,
        rotate_template<3>,
        nullptr,
        move_left,
        move_right,
        move_down,
    };
    TetrisOpertion op_J3 =
    {
        create_template<'J', 3,
        T(0, 1, 0, 0),
        T(0, 1, 1, 1),
        T(0, 0, 0, 0),
        T(0, 0, 0, 0)>,
        generate_template<'J'>,
        rotate_template<2>,
        rotate_template<4>,
        nullptr,
        move_left,
        move_right,
        move_down,
    };
    TetrisOpertion op_J4 =
    {
        create_template<'J', 4,
        T(0, 0, 1, 0),
        T(0, 0, 1, 0),
        T(0, 1, 1, 0),
        T(0, 0, 0, 0)>,
        generate_template<'J'>,
        rotate_template<3>,
        rotate_template<1>,
        nullptr,
        move_left,
        move_right,
        move_down,
    };
    TetrisOpertion op_T1 =
    {
        create_template<'T', 1,
        T(0, 0, 0, 0),
        T(0, 1, 1, 1),
        T(0, 0, 1, 0),
        T(0, 0, 0, 0)>,
        generate_template<'T'>,
        rotate_template<4>,
        rotate_template<2>,
        nullptr,
        move_left,
        move_right,
        move_down,
    };
    TetrisOpertion op_T2 =
    {
        create_template<'T', 2,
        T(0, 0, 1, 0),
        T(0, 0, 1, 1),
        T(0, 0, 1, 0),
        T(0, 0, 0, 0)>,
        generate_template<'T'>,
        rotate_template<1>,
        rotate_template<3>,
        nullptr,
        move_left,
        move_right,
        move_down,
    };
    TetrisOpertion op_T3 =
    {
        create_template<'T', 3,
        T(0, 0, 1, 0),
        T(0, 1, 1, 1),
        T(0, 0, 0, 0),
        T(0, 0, 0, 0)>,
        generate_template<'T'>,
        rotate_template<2>,
        rotate_template<4>,
        nullptr,
        move_left,
        move_right,
        move_down,
    };
    TetrisOpertion op_T4 =
    {
        create_template<'T', 4,
        T(0, 0, 1, 0),
        T(0, 1, 1, 0),
        T(0, 0, 1, 0),
        T(0, 0, 0, 0)>,
        generate_template<'T'>,
        rotate_template<3>,
        rotate_template<1>,
        nullptr,
        move_left,
        move_right,
        move_down,
    };
#undef T
    init_op.insert(std::make_pair(std::make_pair('O', 1), op_O1));
    init_op.insert(std::make_pair(std::make_pair('I', 1), op_I1));
    init_op.insert(std::make_pair(std::make_pair('I', 2), op_I2));
    init_op.insert(std::make_pair(std::make_pair('S', 1), op_S1));
    init_op.insert(std::make_pair(std::make_pair('S', 2), op_S2));
    init_op.insert(std::make_pair(std::make_pair('Z', 1), op_Z1));
    init_op.insert(std::make_pair(std::make_pair('Z', 2), op_Z2));
    init_op.insert(std::make_pair(std::make_pair('L', 1), op_L1));
    init_op.insert(std::make_pair(std::make_pair('L', 2), op_L2));
    init_op.insert(std::make_pair(std::make_pair('L', 3), op_L3));
    init_op.insert(std::make_pair(std::make_pair('L', 4), op_L4));
    init_op.insert(std::make_pair(std::make_pair('J', 1), op_J1));
    init_op.insert(std::make_pair(std::make_pair('J', 2), op_J2));
    init_op.insert(std::make_pair(std::make_pair('J', 3), op_J3));
    init_op.insert(std::make_pair(std::make_pair('J', 4), op_J4));
    init_op.insert(std::make_pair(std::make_pair('T', 1), op_T1));
    init_op.insert(std::make_pair(std::make_pair('T', 2), op_T2));
    init_op.insert(std::make_pair(std::make_pair('T', 3), op_T3));
    init_op.insert(std::make_pair(std::make_pair('T', 4), op_T4));
    op.insert(std::make_pair('O', &generate_template<'O'>));
    op.insert(std::make_pair('I', &generate_template<'I'>));
    op.insert(std::make_pair('S', &generate_template<'S'>));
    op.insert(std::make_pair('Z', &generate_template<'Z'>));
    op.insert(std::make_pair('L', &generate_template<'L'>));
    op.insert(std::make_pair('J', &generate_template<'J'>));
    op.insert(std::make_pair('T', &generate_template<'T'>));
    mtsrand(unsigned int(time(nullptr)));
}

int map_width = 0, map_height = 0;

bool init_ai(int w, int h)
{
    if(w == map_width && h == map_height)
    {
        return true;
    }
    if(w > 32 || h > 32)
    {
        return false;
    }
    node_cache.clear();
    map_width = w;
    map_height = h;

    std::vector<TetrisBlockStatus> check;
    for(auto cit = init_op.begin(); cit != init_op.end(); ++cit)
    {
        TetrisNode node = cit->second.create(w, h, cit->second);
        check.push_back(node.status);
        node_cache.insert(std::make_pair(node.status, node));
    }
    TetrisMap map = {{}, {}, w, h};
    size_t check_index = 0;
    do
    {
        for(size_t max_index = check.size(); check_index < max_index; ++check_index)
        {
            TetrisNode &node = node_cache.find(check[check_index])->second;
#define D(func)\
/**/do{\
/**//**/TetrisNode copy =\
/**//**/{\
/**//**//**/node.status, node.op, {node.data[0], node.data[1], node.data[2], node.data[3]}, {node.top[0], node.top[1], node.top[2], node.top[3]}, {node.bottom[0], node.bottom[1], node.bottom[2], node.bottom[3]}, node.row, node.height, node.col, node.width\
/**//**/};\
/**//**/if(copy.op.func != nullptr && copy.op.func(copy, map))\
/**//**/{\
/**//**//**/auto result = node_cache.insert(std::make_pair(copy.status, copy));\
/**//**//**/if(result.second)\
/**//**//**/{\
/**//**//**//**/check.push_back(copy.status);\
/**//**//**/}\
/**//**//**/if(node.func == nullptr)\
/**//**//**/{\
/**//**//**//**/node.func = &result.first->second;\
/**//**//**/}\
/**//**/}\
/**/}\
/**/while(false)\
/**/
            D(rotate_clockwise);
            D(rotate_counterclockwise);
            D(rotate_opposite);
            D(move_left);
            D(move_right);
            D(move_down);
#undef D
        }
    }
    while(check.size() > check_index);
    return true;
}

void build_map(char board[], int w, int h, TetrisMap &map)
{
    memset(&map, 0, sizeof map);
    map.width = w;
    map.height = h;
    for(int y = 0, add = 0; y < h; ++y, add += w)
    {
        for(int x = 0; x < w; ++x)
        {
            if(board[x + add] == '1')
            {
                map.top[x] = map.roof = y + 1;
                map.row[y] |= 1 << x;
                ++map.count;
            }
        }
    }
}

int do_ai_score(TetrisNode const *node, TetrisMap const &map, TetrisMap const &old_map, size_t *clear, size_t clear_length)
{
    int value = 0;
    int top = map.roof;

    const int width = map.width;

    for(int x = 0; x < width; ++x)
    {
        for(int y = 0; y < top; ++y)
        {
            if(map.full(x, y))
            {
                continue;
            }
            if(x == 0 || map.full(x + 1, y))
            {
                value -= 3 * y + 3;
            }
            if(x == width - 1 || map.full(x - 1, y))
            {
                value -= 3 * y + 3;
            }
            if(map.full(x, y + 1))
            {
                value -= 20 * y + 20;
                if(map.full(x, y + 2))
                {
                    value -= 4;
                    if(map.full(x, y + 3))
                    {
                        value -= 3;
                        if(map.full(x, y + 4))
                        {
                            value -= 2;
                        }
                    }
                }
            }
        }
    }

    for(size_t i = 0; i < clear_length; ++i)
    {
        value += clear[i] * (clear_length - 1);
    }

    return value;
}

namespace ai_simple
{
    std::vector<std::vector<TetrisNode const *>> check_cache;
    struct
    {
        std::map<unsigned char, std::vector<TetrisNode const *>> check;
        int width, height;
    } search_cache =
    {
        {}, 0, 0
    };
    std::vector<size_t> clear;

    std::pair<TetrisNode const *, int> do_ai(TetrisMap const &map, TetrisNode const *node, int next[], size_t next_count)
    {
        if(node == nullptr || !node->check(map))
        {
            return std::make_pair(nullptr, std::numeric_limits<int>::min());
        }
        if(check_cache.size() <= next_count)
        {
            check_cache.resize(next_count + 1);
        }
        std::vector<TetrisNode const *> *check = &check_cache[next_count];
        do
        {
            if(map.height - map.roof >= 4 && map.width == search_cache.width && map.height == search_cache.width)
            {
                auto find = search_cache.check.find(node->status.t);
                if(find != search_cache.check.end())
                {
                    std::vector<TetrisNode const *> *check = &check_cache[next_count];
                    check = &find->second;
                    break;
                }
            }
            check->clear();
            TetrisNode const *rotate = node;
            do
            {
                check->push_back(rotate);
                TetrisNode const *left = rotate->move_left;
                while(left != nullptr && left->check(map))
                {
                    check->push_back(left);
                    left = left->move_left;
                }
                TetrisNode const *right = rotate->move_right;
                while(right != nullptr && right->check(map))
                {
                    check->push_back(right);
                    right = right->move_right;
                }
                rotate = rotate->rotate_counterclockwise;
            }
            while(rotate != nullptr  && rotate != node && rotate->check(map));
            if(map.height - map.roof >= 4)
            {
                if(map.width != search_cache.width || map.height != search_cache.width)
                {
                    search_cache.width = map.width;
                    search_cache.height = map.height;
                    search_cache.check.clear();
                }
                search_cache.check.insert(std::make_pair(node->status.t, *check));
            }
        }
        while(false);
        int score = std::numeric_limits<int>::min();
        TetrisNode const *beat_node = node;
        size_t best = 0;
        TetrisMap map_copy;
        for(size_t i = 0; i < check->size(); ++i)
        {
            node = drop((*check)[i], map);
            map_copy = map;
            clear.push_back(node->attach(map_copy));
            int new_score = next_count == 0 ? do_ai_score(node, map_copy, map, clear.data(), clear.size()) : do_ai(map_copy, op[*next](map_copy), next + 1, next_count - 1).second;
            clear.pop_back();
            if(new_score > score)
            {
                best = i;
                beat_node = node;
                score = new_score;
            }
        }
        return std::make_pair(beat_node, score);
    }
}

namespace ai_path
{
    struct TetrisNodePathCacheTool_t
    {
        struct Equal
        {
            bool operator()(TetrisNode const *const &left, TetrisNode const *const &right)
            {
                return std::equal_to<size_t>()(left->status.status, right->status.status);
            }
        };
        struct Hash
        {
            size_t operator()(TetrisNode const *const &node)
            {
                return std::hash<size_t>()(node->status.status);
            }
        };
    };
    std::vector<std::unordered_map<TetrisNode const *, std::pair<TetrisNode const *, char>, TetrisNodePathCacheTool_t::Hash, TetrisNodePathCacheTool_t::Equal>> node_path_cache;
    std::vector<std::vector<TetrisNode const *>> node_search_cache;
    std::vector<std::vector<TetrisNode const *>> bottom_cache;
    std::vector<std::vector<TetrisNode const *>> check_cache;
    struct
    {
        std::map<unsigned char, std::vector<TetrisNode const *>> check;
        int width, height;
    } search_cache =
    {
        {}, 0, 0
    };
    std::vector<size_t> clear;

    std::vector<char> make_path(TetrisNode const *from, TetrisNode const *to, TetrisMap const &map)
    {
        if(check_cache.size() == 0)
        {
            node_path_cache.resize(1);
            node_search_cache.resize(1);
            bottom_cache.resize(1);
            check_cache.resize(1);
        }
        std::unordered_map<TetrisNode const *, std::pair<TetrisNode const *, char>, TetrisNodePathCacheTool_t::Hash, TetrisNodePathCacheTool_t::Equal> &node_path = node_path_cache[0];
        std::vector<TetrisNode const *> &node_search = node_search_cache[0];
        node_path.clear();
        node_search.clear();

        auto build_path = [&node_path, to]()->std::vector<char>
        {
            std::vector<char> path;
            TetrisNode const *node = to;
            do
            {
                path.push_back('\0');
                std::tie(node, path.back()) = node_path.find(node)->second;
            }
            while(node != nullptr);
            path.pop_back();
            std::reverse(path.begin(), path.end());
            while(!path.empty() && (path.back() == 'd' || path.back() == 'D'))
            {
                path.pop_back();
            }
            path.push_back('\0');
            return path;
        };
        node_search.push_back(from);
        node_path.insert(std::make_pair(from, std::make_pair(nullptr, '\0')));
        size_t cache_index = 0;
        do
        {
            for(size_t max_index = node_search.size(); cache_index < max_index; ++cache_index)
            {
                TetrisNode const *node = node_search[cache_index];
                //x
                if(node->rotate_opposite && node_path.find(node->rotate_opposite) == node_path.end() && node->rotate_opposite->check(map))
                {
                    node_search.push_back(node->rotate_opposite);
                    node_path.insert(std::make_pair(node->rotate_opposite, std::make_pair(node, 'x')));
                    if(node->rotate_opposite == to)
                    {
                        return build_path();
                    }
                }
                //z
                if(node->rotate_counterclockwise && node_path.find(node->rotate_counterclockwise) == node_path.end() && node->rotate_counterclockwise->check(map))
                {
                    node_search.push_back(node->rotate_counterclockwise);
                    node_path.insert(std::make_pair(node->rotate_counterclockwise, std::make_pair(node, 'z')));
                    if(node->rotate_counterclockwise == to)
                    {
                        return build_path();
                    }
                    //zz
                    TetrisNode const *node_r = node->rotate_counterclockwise;
                    if(node_r->rotate_counterclockwise && node_path.find(node_r->rotate_counterclockwise) == node_path.end() && node_r->rotate_counterclockwise->check(map))
                    {
                        node_search.push_back(node_r->rotate_counterclockwise);
                        node_path.insert(std::make_pair(node_r->rotate_counterclockwise, std::make_pair(node_r, 'z')));
                        if(node_r->rotate_counterclockwise == to)
                        {
                            return build_path();
                        }
                    }
                }
                //c
                if(node->rotate_clockwise && node_path.find(node->rotate_clockwise) == node_path.end() && node->rotate_clockwise->check(map))
                {
                    node_search.push_back(node->rotate_clockwise);
                    node_path.insert(std::make_pair(node->rotate_clockwise, std::make_pair(node, 'c')));
                    if(node->rotate_clockwise == to)
                    {
                        return build_path();
                    }
                    //cc
                    TetrisNode const *node_r = node->rotate_clockwise;
                    if(node_r->rotate_clockwise && node_path.find(node_r->rotate_clockwise) == node_path.end() && node_r->rotate_clockwise->check(map))
                    {
                        node_search.push_back(node_r->rotate_clockwise);
                        node_path.insert(std::make_pair(node_r->rotate_clockwise, std::make_pair(node_r, 'c')));
                        if(node_r->rotate_clockwise == to)
                        {
                            return build_path();
                        }
                    }
                }
                //l
                if(node->move_left && node_path.find(node->move_left) == node_path.end() && node->move_left->check(map))
                {
                    node_search.push_back(node->move_left);
                    node_path.insert(std::make_pair(node->move_left, std::make_pair(node, 'l')));
                    if(node->move_left == to)
                    {
                        return build_path();
                    }
                }
                //r
                if(node->move_right && node_path.find(node->move_right) == node_path.end() && node->move_right->check(map))
                {
                    node_search.push_back(node->move_right);
                    node_path.insert(std::make_pair(node->move_right, std::make_pair(node, 'r')));
                    if(node->move_right == to)
                    {
                        return build_path();
                    }
                }
                //L
                if(node->move_left && node->move_left->check(map))
                {
                    TetrisNode const *node_L = node->move_left;
                    while(node_L->move_left && node_L->move_left->check(map))
                    {
                        node_L = node_L->move_left;
                    }
                    if(node_L != node->move_left && node_path.find(node_L) == node_path.end())
                    {
                        node_search.push_back(node_L);
                        node_path.insert(std::make_pair(node_L, std::make_pair(node, 'L')));
                        if(node_L == to)
                        {
                            return build_path();
                        }
                    }
                }
                //R
                if(node->move_right && node->move_right->check(map))
                {
                    TetrisNode const *node_R = node->move_right;
                    while(node_R->move_right && node_R->move_right->check(map))
                    {
                        node_R = node_R->move_right;
                    }
                    if(node_R != node->move_right && node_path.find(node_R) == node_path.end())
                    {
                        node_search.push_back(node_R);
                        node_path.insert(std::make_pair(node_R, std::make_pair(node, 'R')));
                        if(node_R == to)
                        {
                            return build_path();
                        }
                    }
                }
                //d
                if(node->move_down && node_path.find(node->move_down) == node_path.end() && node->move_down->check(map))
                {
                    node_search.push_back(node->move_down);
                    node_path.insert(std::make_pair(node->move_down, std::make_pair(node, 'd')));
                    if(node->move_down == to)
                    {
                        return build_path();
                    }
                    //D
                    TetrisNode const *node_D = drop(node, map);
                    if(node_path.find(node_D) == node_path.end())
                    {
                        node_search.push_back(node_D);
                        node_path.insert(std::make_pair(node_D, std::make_pair(node, 'D')));
                        if(node_D == to)
                        {
                            return build_path();
                        }
                    }
                }
            }
        }
        while(node_search.size() > cache_index);
        return std::vector<char>();
    }

    std::pair<TetrisNode const *, int> do_ai(TetrisMap const &map, TetrisNode const *node, int next[], size_t next_count)
    {
        if(node == nullptr || !node->check(map))
        {
            return std::make_pair(nullptr, std::numeric_limits<int>::min());
        }
        if(check_cache.size() <= next_count)
        {
            node_path_cache.resize(next_count + 1);
            node_search_cache.resize(next_count + 1);
            bottom_cache.resize(next_count + 1);
            check_cache.resize(next_count + 1);
        }
        std::unordered_map<TetrisNode const *, std::pair<TetrisNode const *, char>, TetrisNodePathCacheTool_t::Hash, TetrisNodePathCacheTool_t::Equal> &node_path = node_path_cache[next_count];
        std::vector<TetrisNode const *> &node_search = node_search_cache[next_count];
        std::vector<TetrisNode const *> &bottom = bottom_cache[next_count];
        std::vector<TetrisNode const *> *check = &check_cache[next_count];
        node_path.clear();
        node_search.clear();
        bottom.clear();
        if(map.height - map.roof >= 4 && node == op[node->status.t](map))
        {
            do
            {
                if(map.width == search_cache.width && map.height == search_cache.height)
                {
                    auto find = search_cache.check.find(node->status.t);
                    if(find != search_cache.check.end())
                    {
                        check = &find->second;
                        break;
                    }
                }
                check->clear();
                TetrisNode const *rotate = node;
                do
                {
                    TetrisNode const *left = rotate;
                    while(left->move_left != nullptr)
                    {
                        left = left->move_left;
                    }
                    do
                    {
                        check->push_back(left);
                        left = left->move_right;
                    }
                    while(left != nullptr);
                    rotate = rotate->rotate_counterclockwise;
                }
                while(rotate != nullptr  && rotate != node);
                if(map.width != search_cache.width || map.height != search_cache.height)
                {
                    search_cache.width = map.width;
                    search_cache.height = map.height;
                    search_cache.check.clear();
                }
                search_cache.check.insert(std::make_pair(node->status.t, *check));
            }
            while(false);
            for(size_t i = 0; i < check->size(); ++i)
            {
                bottom.push_back(drop((*check)[i], map));
            }
            TetrisBlockStatus const *last_status = nullptr;
            for(auto cit = bottom.begin(); cit != bottom.end(); ++cit)
            {
                if(last_status == nullptr)
                {
                    last_status = &node->status;
                }
                else
                {
                    if(last_status->r == node->status.r && std::abs(last_status->x - node->status.x) == 1)
                    {
                        if(last_status->y - node->status.y >= 2)
                        {
                            node_search.push_back(get(last_status->t, node->status.x, last_status->y - 1, last_status->r));
                            node_path.insert(std::make_pair(node_search.back(), std::make_pair(nullptr, 0)));
                        }
                        else if(node->status.y - last_status->y >= -2)
                        {
                            node_search.push_back(get(last_status->t, last_status->x, node->status.y - 1, last_status->r));
                            node_path.insert(std::make_pair(node_search.back(), std::make_pair(nullptr, 0)));
                        }
                    }
                }
            }
        }
        else
        {
            check->clear();
            node_search.push_back(node);
            node_path.insert(std::make_pair(node, std::make_pair(nullptr, 0)));
        }
        size_t cache_index = 0;
        do
        {
            for(size_t max_index = node_search.size(); cache_index < max_index; ++cache_index)
            {
                node = node_search[cache_index];
                if(!node->move_down || !node->move_down->check(map))
                {
                    bottom.push_back(node);
                }
                //x
                if(node->rotate_opposite && node_path.find(node->rotate_opposite) == node_path.end() && node->rotate_opposite->check(map))
                {
                    node_search.push_back(node->rotate_opposite);
                    node_path.insert(std::make_pair(node->rotate_opposite, std::make_pair(node, 'x')));
                }
                //z
                if(node->rotate_counterclockwise && node_path.find(node->rotate_counterclockwise) == node_path.end() && node->rotate_counterclockwise->check(map))
                {
                    node_search.push_back(node->rotate_counterclockwise);
                    node_path.insert(std::make_pair(node->rotate_counterclockwise, std::make_pair(node, 'z')));
                }
                //c
                if(node->rotate_clockwise && node_path.find(node->rotate_clockwise) == node_path.end() && node->rotate_clockwise->check(map))
                {
                    node_search.push_back(node->rotate_clockwise);
                    node_path.insert(std::make_pair(node->rotate_clockwise, std::make_pair(node, 'c')));
                }
                //l
                if(node->move_left && node_path.find(node->move_left) == node_path.end() && node->move_left->check(map))
                {
                    node_search.push_back(node->move_left);
                    node_path.insert(std::make_pair(node->move_left, std::make_pair(node, 'l')));
                }
                //r
                if(node->move_right && node_path.find(node->move_right) == node_path.end() && node->move_right->check(map))
                {
                    node_search.push_back(node->move_right);
                    node_path.insert(std::make_pair(node->move_right, std::make_pair(node, 'r')));
                }
                //d
                if(node->move_down && node_path.find(node->move_down) == node_path.end() && node->move_down->check(map))
                {
                    node_search.push_back(node->move_down);
                    node_path.insert(std::make_pair(node->move_down, std::make_pair(node, 'd')));
                }
            }
        }
        while(node_search.size() > cache_index);
        int score = std::numeric_limits<int>::min();
        TetrisNode const *beat_node = node;
        size_t best = 0;
        TetrisMap map_copy;
        for(size_t i = 0; i < bottom.size(); ++i)
        {
            node = bottom[i];
            map_copy = map;
            clear.push_back(node->attach(map_copy));
            int new_score = next_count == 0 ? do_ai_score(node, map_copy, map, clear.data(), clear.size()) : do_ai(map_copy, op[*next](map_copy), next + 1, next_count - 1).second;
            clear.pop_back();
            if(new_score > score)
            {
                best = i;
                beat_node = node;
                score = new_score;
            }
        }
        return std::make_pair(beat_node, score);

    }
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
extern "C" {
#endif

/*
 * board是一个boardW*boardH长度用01组成的字符串，原点于左下角，先行后列。
 * 例如8*3的场地实际形状：
 * 00000000
 * 00011001
 * 01111111
 * 则参数board的内容为："011111110001100100000000"。
 *
 * Piece参数使用字符 OISZLJT 及空格表示方块形状。
 * nextPiece为' '时表示无预览。
 * curR是方向朝向，1是初始方向，2是逆时针90度，3是180度，4是顺时针90度。
 * curX,curY的坐标，是以当前块4*4矩阵的上数第二行，右数第二列为基准，
 *     左下角为x=1,y=1；右下角为x=boardW,y=1；左上角为x=1,y=boardH
 *     具体方块形状参阅上一级目录下的pieces_orientations.jpg
 *
 * bestX,bestRotation 用于返回最优位置，与curX,curR的规则相同。
 *
 * 注意：方块操作次序规定为先旋转，再平移，最后放下。
 *       若中间有阻挡而AI程序没有判断则会导致错误摆放。
 *       该函数在出现新方块的时候被调用，一个方块调用一次。
 */
DECLSPEC_EXPORT int WINAPI AI(int boardW, int boardH, char board[], char curPiece, int curX, int curY, int curR, char nextPiece, int *bestX, int *bestRotation)
{
    if(!init_ai(boardW, boardH))
    {
        return 0;
    }
    TetrisMap map;
    TetrisBlockStatus status =
    {
        curPiece, curX - 1, curY - 1, curR
    };
    int next_count = 0;
    int next;
    if(nextPiece != ' ')
    {
        next = nextPiece;
        next_count = 1;
    }

    build_map(board, boardW, boardH, map);
    auto result = ai_simple::do_ai(map, get(status), &next, next_count).first;

    if(result != nullptr)
    {
        *bestX = result->status.x + 1;
        *bestRotation = result->status.r;
    }

    return 0;
}

/*
 * path 用于接收操作过程并返回，操作字符集：
 *      'l': 左移一格
 *      'r': 右移一格
 *      'd': 下移一格
 *      'L': 左移到头
 *      'R': 右移到头
 *      'D': 下移到底（但不粘上，可继续移动）
 *      'z': 逆时针旋转
 *      'c': 顺时针旋转
 * 字符串末尾要加'\0'，表示落地操作（或硬降落）
 *
 * 本函数支持任意路径操作，若不需要此函数只想使用上面一个的话，则删掉本函数即可
 */
DECLSPEC_EXPORT int WINAPI AIPath(int boardW, int boardH, char board[], char curPiece, int curX, int curY, int curR, char nextPiece, char path[])
{
    if(!init_ai(boardW, boardH))
    {
        return 0;
    }
    TetrisMap map;
    TetrisBlockStatus status =
    {
        curPiece, curX - 1, curY - 1, curR
    };
    int next_count = 0;
    int next;
    if(nextPiece != ' ')
    {
        next = nextPiece;
        next_count = 1;
    }

    build_map(board, boardW, boardH, map);
    TetrisNode const *node = get(status);
    auto result = ai_path::do_ai(map, node, &next, next_count);

    if(result.first != nullptr)
    {
        std::vector<char> ai_path = ai_path::make_path(node, result.first, map);
        memcpy(path, ai_path.data(), ai_path.size());
    }
    return 0;
}

#ifdef __cplusplus
}
#endif



#ifndef WINVER                          // 指定要求的最低平台是 Windows Vista。
#define WINVER 0x0500           // 将此值更改为相应的值，以适用于 Windows 的其他版本。
#endif

#ifndef _WIN32_WINNT            // 指定要求的最低平台是 Windows Vista。
#define _WIN32_WINNT 0x0501     // 将此值更改为相应的值，以适用于 Windows 的其他版本。
#endif
#define WIN32_LEAN_AND_MEAN             // 从 Windows 头中排除极少使用的资料

// Windows 头文件:
#include <windows.h>

int wmain(unsigned int argc, wchar_t *argv[], wchar_t *eve[])
{
    attach_init();
    //zzz_ai_run();
    if(argc < 2)
    {
        return 0;
    }
    HMODULE hDll = LoadLibrary(argv[1]);
    if(hDll == nullptr)
    {
        return 0;
    }
    void *name = nullptr;
    name = GetProcAddress(hDll, "_Name@0");
    if(name == nullptr)
    {
        name = GetProcAddress(hDll, "Name@0");
    }
    if(name == nullptr)
    {
        name = GetProcAddress(hDll, "Name");
    }
    void *ai[2] = {};
    ai[0] = GetProcAddress(hDll, "_AIPath@36");
    if(ai[0] == NULL)
    {
        ai[0] = GetProcAddress(hDll, "AIPath@36");
    }
    if(ai[0] == NULL)
    {
        ai[0] = GetProcAddress(hDll, "AIPath");
    }
    ai[1] = GetProcAddress(hDll, "_AI@40");
    if(ai[1] == NULL)
    {
        ai[1] = GetProcAddress(hDll, "AI@40");
    }
    if(ai[1] == NULL)
    {
        ai[1] = GetProcAddress(hDll, "AI");
    }
    
    if(name == nullptr)
    {
        return 0;
    }
    int version = -1;
    for(int i = 0; i < sizeof ai / sizeof ai[0]; ++i)
    {
        if(ai[i] != nullptr)
        {
            version = i;
            break;
        }
    }
    if(version == -1)
    {
        return 0;
    }
    SetWindowTextA(GetConsoleWindow(), ((char *(*)())name)());
    int w = 10, h = 20;
    TetrisMap map =
    {
        {}, {}, w, h
    };
    char *param_map = new char[w * h];
    char *path = new char[1024];
    init_ai(w, h);
    clock_t log_start = clock();
    clock_t log_time = log_start;
    clock_t log_new_time;

    clock_t log_interval = 10000;
    long long log_rows = 0, log_piece = 0;

    long long total_lines = 0;
    long long this_lines = 0;
    long long max_line = 0;
    long long game_count = 0;

    while(true)
    {
        unsigned char const tetris[] = "OISZLJT";
        TetrisNode const *node = op[tetris[size_t(mtdrand() * 7)]](map);
        log_new_time = clock();
        if(log_new_time - log_time > log_interval)
        {
            printf("{\"time\":%.2lf,\"current\":%lld,\"rows_ps\":%lld,\"piece_ps\":%lld}\n", (log_new_time - log_start) / 1000., this_lines, log_rows * 1000 / log_interval, log_piece * 1000 / log_interval);
            log_time += log_interval;
            log_rows = 0;
            log_piece = 0;
        }
        if(!node->check(map))
        {
            total_lines += this_lines;
            if(this_lines > max_line)
            {
                max_line = this_lines;
            }
            ++game_count;
            printf("{\"avg\":%.2lf,\"max\":%lld,\"count\":%lld,\"current\":%lld}\n", game_count == 0 ? 0. : double(total_lines) / game_count, max_line, game_count, this_lines);
            this_lines = 0;
            map.count = 0;
            map.roof = 0;
            memset(map.top, 0, sizeof map.top);
            memset(map.row, 0, sizeof map.row);
        }
        for(int y = 0; y < h; ++y)
        {
            int row = y * w;
            for(int x = 0; x < w; ++x)
            {
                param_map[x + row] = map.full(x, y) ? '1' : '0';
            }
        }
        if(version == 0)
        {
            memset(path, 0, 1024);
            typedef int(__stdcall *ai_run_t)(int boardW, int boardH, char board[], char curPiece, int curX, int curY, int curR, char nextPiece, char path[]);
            ((ai_run_t)ai[version])(w, h, param_map, node->status.t, node->status.x + 1, node->status.y + 1, node->status.r, ' ', path);
            char *move = path, *move_end = path + 1024;
            //printf("%c->%s\n", node->status.t, path);
            while(move != move_end && *move != '\0')
            {
                switch(*move++)
                {
                case 'l':
                    if(node->move_left && (node->row >= map.roof || node->move_left->check(map)))
                    {
                        node = node->move_left;
                    }
                    break;
                case 'r':
                    if(node->move_right && (node->row >= map.roof || node->move_right->check(map)))
                    {
                        node = node->move_right;
                    }
                    break;
                case 'd':
                    if(node->move_down && (node->row > map.roof || node->move_down->check(map)))
                    {
                        node = node->move_down;
                    }
                    break;
                case 'L':
                    while(node->move_left && (node->row >= map.roof || node->move_left->check(map)))
                    {
                        node = node->move_left;
                    }
                    break;
                case 'R':
                    while(node->move_right && (node->row >= map.roof || node->move_right->check(map)))
                    {
                        node = node->move_right;
                    }
                    break;
                case 'D':
                    node = drop(node, map);
                    break;
                case 'z':
                    if(node->rotate_counterclockwise && node->rotate_counterclockwise->check(map))
                    {
                        node = node->rotate_counterclockwise;
                    }
                    break;
                case 'c':
                    if(node->rotate_clockwise && node->rotate_clockwise->check(map))
                    {
                        node = node->rotate_clockwise;
                    }
                    break;
                case 'x':
                    if(node->rotate_opposite && node->rotate_opposite->check(map))
                    {
                        node = node->rotate_opposite;
                    }
                    break;
                default:
                    move = move_end;
                    break;
                }
            }
        }
        else
        {
            typedef int(__stdcall *ai_run_t)(int boardW, int boardH, char board[], char curPiece, int curX, int curY, int curR, char nextPiece, int *bestX, int *bestRotation);
            int best_x = node->status.x + 1, best_r = node->status.r;
            ((ai_run_t)ai[version])(w, h, param_map, node->status.t, best_x, node->status.y + 1, best_r, ' ', &best_x, &best_r);
            --best_x;
            int r = node->status.r;
            while(best_r > r && node->rotate_counterclockwise && node->rotate_counterclockwise->check(map))
            {
                ++r;
                node = node->rotate_counterclockwise;
            }
            while(best_r < r && node->rotate_clockwise && node->rotate_clockwise->check(map))
            {
                --r;
                node = node->rotate_clockwise;
            }
            while(best_x > node->status.x && node->move_right && (node->row >= map.roof || node->move_right->check(map)))
            {
                node = node->move_right;
            }
            while(best_x < node->status.x && node->move_left && (node->row >= map.roof || node->move_left->check(map)))
            {
                node = node->move_left;
            }
        }
        node = drop(node, map);
        int clear = node->attach(map);
        this_lines += clear;
        log_rows += clear;
        ++log_piece;
    }
}