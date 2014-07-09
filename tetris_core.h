
#pragma once

#include <unordered_map>
#include <algorithm>



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
public:
    TetrisBlockStatus status;
    TetrisOpertion op;
    int data[4];
    int top[4];
    int bottom[4];
    char row, height, col, width;
    int low;
    size_t index;

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
    bool open(TetrisMap const &map) const
    {
        return open_row(0, map) && open_row(1, map) && open_row(2, map) && open_row(3, map);
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
    inline int open_row(int offset, TetrisMap const &map) const
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
    inline void drop_rows(int offset, int &value, TetrisMap const &map) const
    {
        if(offset < width)
        {
            value = std::min<int>(value, bottom[offset] - map.top[offset + col]);
        }
    }
};

inline TetrisNode const *get(TetrisBlockStatus const &status);
inline TetrisNode const *get(unsigned char t, char x, char y, unsigned r);
inline TetrisNode const *drop(TetrisNode const *node, TetrisMap const &map);
inline TetrisNode const *generate(unsigned char type, TetrisMap const &map);

extern "C" void attach_init();
bool init_ai(int w, int h);
void build_map(char board[], int w, int h, TetrisMap &map);


namespace ai_simple
{
    std::pair<TetrisNode const *, int> do_ai(TetrisMap const &old_map, TetrisMap const &map, TetrisNode const *node, unsigned char next[], size_t next_count);
}

namespace ai_path
{
    std::vector<char> make_path(TetrisNode const *from, TetrisNode const *to, TetrisMap const &map);
    std::pair<TetrisNode const *, int> do_ai(TetrisMap const &old_map, TetrisMap const &map, TetrisNode const *node, unsigned char next[], size_t next_count);
}

namespace ege
{
    extern void mtsrand(unsigned int s);
    extern unsigned int mtirand();
    extern double mtdrand();
}
using ege::mtdrand;
using ege::mtirand;
using ege::mtsrand;

extern int do_ai_score(TetrisMap const &map, TetrisMap const &old_map, size_t *clear, size_t clear_length);