
#include "tetris_core.h"

#include <map>
#include <ctime>

std::map<std::pair<unsigned char, unsigned char>, TetrisOpertion> init_op;
std::map<unsigned char, TetrisNode const *(*)(TetrisMap const &)> op;
std::unordered_map<TetrisBlockStatus, TetrisNode> node_cache;
unsigned char const tetris[] = {'O', 'I', 'S', 'Z', 'L', 'J', 'T'};


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
        status, op, {line4 << (status.x - 2), line3 << (status.x - 2), line2 << (status.x - 2), line1 << (status.x - 2)}, {}, {}, h - 4, 4, w / 2 - 2, 4, h - 4
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
    --node.low;
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

inline TetrisNode const *get(unsigned char t, char x, char y, unsigned char r)
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

inline TetrisNode const *generate(unsigned char type, TetrisMap const &map)
{
    return op[type](map);
}

inline TetrisNode const *generate(TetrisMap const &map)
{
    size_t index = mtirand() & 7;
    while(index >= 7)
    {
        index = mtirand() & 7;
    }
    return generate(tetris[index], map);
}

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
            node.index = check_index;
#define D(func)\
/**/do\
/**/{\
/**//**/TetrisNode copy =\
/**//**/{\
/**//**//**/node.status, node.op, {node.data[0], node.data[1], node.data[2], node.data[3]}, {node.top[0], node.top[1], node.top[2], node.top[3]}, {node.bottom[0], node.bottom[1], node.bottom[2], node.bottom[3]}, node.row, node.height, node.col, node.width, node.low\
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
/**/} while(false)\
/**/
            D(rotate_clockwise);
            D(rotate_counterclockwise);
            D(rotate_opposite);
            D(move_left);
            D(move_right);
            D(move_down);
#undef D
        }
    } while(check.size() > check_index);
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

namespace ai_simple
{
    std::vector<std::vector<TetrisNode const *>> check_cache;
    struct
    {
        std::map<unsigned char, std::vector<TetrisNode const *>> check;
        int width, height;
    } search_cache =
    {
        std::map<unsigned char, std::vector<TetrisNode const *>>(), 0, 0
    };
    std::vector<EvalParam> history;

    std::pair<TetrisNode const *, int> do_ai(TetrisMap const &primeval_map, TetrisMap const &map, TetrisNode const *node, unsigned char next[], size_t next_count)
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
            if(node->low >= map.roof && map.width == search_cache.width && map.height == search_cache.height)
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
            } while(rotate != nullptr  && rotate != node && rotate->check(map));
            if(map.height - map.roof >= 4 && node == generate(node->status.t, map))
            {
                if(map.width != search_cache.width || map.height != search_cache.height)
                {
                    search_cache.width = map.width;
                    search_cache.height = map.height;
                    search_cache.check.clear();
                }
                search_cache.check.insert(std::make_pair(node->status.t, *check));
            }
        } while(false);
        int eval = std::numeric_limits<int>::min();
        TetrisNode const *beat_node = node;
        size_t best = 0;
        TetrisMap copy;
        for(size_t i = 0; i < check->size(); ++i)
        {
            node = drop((*check)[i], map);
            copy = map;
            history.push_back(EvalParam(node, node->attach(copy), map));
            int new_eval;
            if(next_count == 0)
            {
                new_eval = ai_eval(copy, history.data(), history.size());
            }
            else
            {
                if(*next == ' ')
                {
                    long long guess_eval = 0;
                    for(int ti = 0; ti < 7; ++ti)
                    {
                        guess_eval += do_ai(primeval_map, copy, generate(tetris[ti], copy), next + 1, next_count - 1).second;
                    }
                    new_eval = int(guess_eval / 7);
                }
                else
                {
                    new_eval = do_ai(primeval_map, copy, generate(*next, copy), next + 1, next_count - 1).second;
                }
            }
            history.pop_back();
            if(new_eval > eval)
            {
                best = i;
                beat_node = node;
                eval = new_eval;
            }
        }
        return std::make_pair(beat_node, eval);
    }
}

namespace ai_path
{
    struct CacheNode
    {
        size_t version;
        std::pair<TetrisNode const *, char> data;
    };
    struct
    {
        size_t version;
        CacheNode empty;
        std::vector<CacheNode> data;

        inline void clear()
        {
            if(++version == std::numeric_limits<size_t>::max())
            {
                version = 1;
                for(auto it = data.begin(); it != data.end(); ++it)
                {
                    it->version = 0;
                }
            }
        }
        inline std::pair<TetrisNode const *, char> get(TetrisNode const *node)
        {
            if(data.size() >= node->index)
            {
                return data[node->index].data;
            }
            return std::make_pair(nullptr, '\0');
        }
        inline bool set(TetrisNode const *key, TetrisNode const *node, char op)
        {
            if(data.size() <= key->index)
            {
                data.resize(key->index + 1, empty);
            }
            CacheNode &cache = data[key->index];
            if(cache.version == version)
            {
                return false;
            }
            cache.version = version;
            cache.data.first = node;
            cache.data.second = op;
            return true;
        }
    } node_path =
    {
        1, {0}, std::vector<CacheNode>()
    };
    std::vector<TetrisNode const *> node_search;
    std::vector<std::vector<TetrisNode const *>> bottom_cache;
    std::vector<std::vector<TetrisNode const *>> check_cache;
    struct
    {
        std::map<unsigned char, std::vector<TetrisNode const *>> check;
        int width, height;
    } search_cache =
    {
        std::map<unsigned char, std::vector<TetrisNode const *>>(), 0, 0
    };
    std::vector<EvalParam> history;

    std::vector<char> make_path(TetrisNode const *from, TetrisNode const *to, TetrisMap const &map)
    {
        if(check_cache.size() == 0)
        {
            bottom_cache.resize(1);
            check_cache.resize(1);
        }
        node_path.clear();
        node_search.clear();

        auto build_path = [to]()->std::vector<char>
        {
            std::vector<char> path;
            TetrisNode const *node = to;
            do
            {
                auto result = node_path.get(node);
                node = result.first;
                path.push_back(result.second);
            } while(node != nullptr);
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
        node_path.set(from, nullptr, '\0');
        size_t cache_index = 0;
        do
        {
            for(size_t max_index = node_search.size(); cache_index < max_index; ++cache_index)
            {
                TetrisNode const *node = node_search[cache_index];
                //x
                if(node->rotate_opposite && node->rotate_opposite->check(map) && node_path.set(node->rotate_opposite, node, 'x'))
                {
                    if(node->rotate_opposite == to)
                    {
                        return build_path();
                    }
                    else
                    {
                        node_search.push_back(node->rotate_opposite);
                    }
                }
                //z
                if(node->rotate_counterclockwise && node->rotate_counterclockwise->check(map) && node_path.set(node->rotate_counterclockwise, node, 'z'))
                {
                    if(node->rotate_counterclockwise == to)
                    {
                        return build_path();
                    }
                    else
                    {
                        node_search.push_back(node->rotate_counterclockwise);
                    }
                    //zz
                    TetrisNode const *node_r = node->rotate_counterclockwise;
                    if(node_r->rotate_counterclockwise && node_r->rotate_counterclockwise->check(map) && node_path.set(node_r->rotate_counterclockwise, node_r, 'z'))
                    {
                        if(node_r->rotate_counterclockwise == to)
                        {
                            return build_path();
                        }
                        else
                        {
                            node_search.push_back(node_r->rotate_counterclockwise);
                        }
                    }
                }
                //c
                if(node->rotate_clockwise && node->rotate_clockwise->check(map) && node_path.set(node->rotate_clockwise, node, 'c'))
                {
                    if(node->rotate_clockwise == to)
                    {
                        return build_path();
                    }
                    else
                    {
                        node_search.push_back(node->rotate_clockwise);
                    }
                    //cc
                    TetrisNode const *node_r = node->rotate_clockwise;
                    if(node_r->rotate_clockwise && node_r->rotate_clockwise->check(map) && node_path.set(node_r->rotate_clockwise, node_r, 'c'))
                    {
                        if(node_r->rotate_clockwise == to)
                        {
                            return build_path();
                        }
                        else
                        {
                            node_search.push_back(node_r->rotate_clockwise);
                        }
                    }
                }
                //l
                if(node->move_left && node->move_left->check(map) && node_path.set(node->move_left, node, 'l'))
                {
                    if(node->move_left == to)
                    {
                        return build_path();
                    }
                    else
                    {
                        node_search.push_back(node->move_left);
                    }
                }
                //r
                if(node->move_right && node->move_right->check(map) && node_path.set(node->move_right, node, 'r'))
                {
                    if(node->move_right == to)
                    {
                        return build_path();
                    }
                    else
                    {
                        node_search.push_back(node->move_right);
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
                    if(node_L != node->move_left && node_path.set(node_L, node, 'L'))
                    {
                        if(node_L == to)
                        {
                            return build_path();
                        }
                        else
                        {
                            node_search.push_back(node_L);
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
                    if(node_R != node->move_right && node_path.set(node_R, node, 'R'))
                    {
                        if(node_R == to)
                        {
                            return build_path();
                        }
                        else
                        {
                            node_search.push_back(node_R);
                        }
                    }
                }
                //d
                if(node->move_down && node->move_down->check(map) && node_path.set(node->move_down, node, 'd'))
                {
                    if(node->move_down == to)
                    {
                        return build_path();
                    }
                    else
                    {
                        node_search.push_back(node->move_down);
                    }
                    //D
                    TetrisNode const *node_D = drop(node, map);
                    if(node_path.set(node_D, node, 'D'))
                    {
                        if(node_D == to)
                        {
                            return build_path();
                        }
                        else
                        {
                            node_search.push_back(node_D);
                        }
                    }
                }
            }
        } while(node_search.size() > cache_index);
        return std::vector<char>();
    }

    std::pair<TetrisNode const *, int> do_ai(TetrisMap const &primeval_map, TetrisMap const &map, TetrisNode const *node, unsigned char next[], size_t next_count)
    {
        if(node == nullptr || !node->check(map))
        {
            return std::make_pair(nullptr, std::numeric_limits<int>::min());
        }
        if(check_cache.size() <= next_count)
        {
            bottom_cache.resize(next_count + 1);
            check_cache.resize(next_count + 1);
        }
        std::vector<TetrisNode const *> &bottom = bottom_cache[next_count];
        std::vector<TetrisNode const *> *check = &check_cache[next_count];
        node_path.clear();
        node_search.clear();
        bottom.clear();
        if(node->low >= map.roof)
        {
            do
            {
                if(map.height - map.roof >= 4 && map.width == search_cache.width && map.height == search_cache.height)
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
                    } while(left != nullptr);
                    rotate = rotate->rotate_counterclockwise;
                } while(rotate != nullptr  && rotate != node);
                if(map.height - map.roof >= 4 && node == generate(node->status.t, map))
                {
                    if(map.width != search_cache.width || map.height != search_cache.height)
                    {
                        search_cache.width = map.width;
                        search_cache.height = map.height;
                        search_cache.check.clear();
                    }
                    search_cache.check.insert(std::make_pair(node->status.t, *check));
                }
            } while(false);
            for(size_t i = 0; i < check->size(); ++i)
            {
                bottom.push_back(drop((*check)[i], map));
            }
            TetrisBlockStatus const *last_status = nullptr;
            for(auto cit = bottom.begin(); cit != bottom.end(); ++cit)
            {
                node = *cit;
                if(last_status != nullptr)
                {
                    if(last_status->r == node->status.r && std::abs(last_status->x - node->status.x) == 1)
                    {
                        if(last_status->y - node->status.y >= 2)
                        {
                            TetrisNode const *check_node = get(last_status->t, node->status.x, last_status->y - 1, last_status->r);
                            if(node_path.set(check_node, nullptr, '\0'))
                            {
                                node_search.push_back(check_node);
                            }
                        }
                        else if(node->status.y - last_status->y >= 2)
                        {
                            TetrisNode const *check_node = get(last_status->t, last_status->x, node->status.y - 1, last_status->r);
                            if(node_path.set(check_node, nullptr, '\0'))
                            {
                                node_search.push_back(check_node);
                            }
                        }
                    }
                }
                last_status = &node->status;
            }
            size_t cache_index = 0;
            do
            {
                for(size_t max_index = node_search.size(); cache_index < max_index; ++cache_index)
                {
                    node = node_search[cache_index];
                    if(!node->open(map) && (!node->move_down || !node->move_down->check(map)))
                    {
                        bottom.push_back(node);
                    }
                    //x
                    if(node->rotate_opposite && !node->rotate_opposite->open(map) && node->rotate_opposite->check(map) && node_path.set(node->rotate_opposite, node, 'x'))
                    {
                        node_search.push_back(node->rotate_opposite);
                    }
                    //z
                    if(node->rotate_counterclockwise && !node->rotate_counterclockwise->open(map) && node->rotate_counterclockwise->check(map) && node_path.set(node->rotate_counterclockwise, node, 'z'))
                    {
                        node_search.push_back(node->rotate_counterclockwise);
                    }
                    //c
                    if(node->rotate_clockwise && !node->rotate_clockwise->open(map) && node->rotate_clockwise->check(map) && node_path.set(node->rotate_clockwise, node, 'c'))
                    {
                        node_search.push_back(node->rotate_clockwise);
                    }
                    //l
                    if(node->move_left && !node->move_left->open(map) && node->move_left->check(map) && node_path.set(node->move_left, node, 'l'))
                    {
                        node_search.push_back(node->move_left);
                    }
                    //r
                    if(node->move_right && !node->move_right->open(map) && node->move_right->check(map) && node_path.set(node->move_right, node, 'r'))
                    {
                        node_search.push_back(node->move_right);
                    }
                    //d
                    if(node->move_down && node->move_down->check(map) && node_path.set(node->move_down, node, 'd'))
                    {
                        node_search.push_back(node->move_down);
                    }
                }
            } while(node_search.size() > cache_index);
        }
        else
        {
            check->clear();
            node_search.push_back(node);
            node_path.set(node, nullptr, 0);
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
                    if(node->rotate_opposite && node->rotate_opposite->check(map) && node_path.set(node->rotate_opposite, node, 'x'))
                    {
                        node_search.push_back(node->rotate_opposite);
                    }
                    //z
                    if(node->rotate_counterclockwise && node->rotate_counterclockwise->check(map) && node_path.set(node->rotate_counterclockwise, node, 'z'))
                    {
                        node_search.push_back(node->rotate_counterclockwise);
                    }
                    //c
                    if(node->rotate_clockwise && node->rotate_clockwise->check(map) && node_path.set(node->rotate_clockwise, node, 'c'))
                    {
                        node_search.push_back(node->rotate_clockwise);
                    }
                    //l
                    if(node->move_left && node->move_left->check(map) && node_path.set(node->move_left, node, 'l'))
                    {
                        node_search.push_back(node->move_left);
                    }
                    //r
                    if(node->move_right && node->move_right->check(map) && node_path.set(node->move_right, node, 'r'))
                    {
                        node_search.push_back(node->move_right);
                    }
                    //d
                    if(node->move_down && node->move_down->check(map) && node_path.set(node->move_down, node, 'd'))
                    {
                        node_search.push_back(node->move_down);
                    }
                }
            } while(node_search.size() > cache_index);
        }
        int eval = std::numeric_limits<int>::min();
        TetrisNode const *beat_node = node;
        size_t best = 0;
        TetrisMap copy;
        for(size_t i = 0; i < bottom.size(); ++i)
        {
            node = bottom[i];
            copy = map;
            history.push_back(EvalParam(node, node->attach(copy), map));
            int new_eval;
            if(next_count == 0)
            {
                new_eval = ai_eval(copy, history.data(), history.size());
            }
            else
            {
                if(*next == ' ')
                {
                    long long guess_eval = 0;
                    for(int ti = 0; ti < 7; ++ti)
                    {
                        guess_eval += do_ai(primeval_map, copy, generate(tetris[ti], copy), next + 1, next_count - 1).second;
                    }
                    new_eval = int(guess_eval / 7);
                }
                else
                {
                    new_eval = do_ai(primeval_map, copy, generate(*next, copy), next + 1, next_count - 1).second;
                }
            }
            history.pop_back();
            if(new_eval > eval)
            {
                best = i;
                beat_node = node;
                eval = new_eval;
            }
        }
        return std::make_pair(beat_node, eval);
    }
}