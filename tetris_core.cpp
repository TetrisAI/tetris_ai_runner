
#include "tetris_core.h"

#include <map>
#include <ctime>

//这里就懒得标记注释了...
//有心读的话...可以试试看调试跟踪一下...
//大概指针网比较复杂吧?
//搜索都是广度优先...
//缓存了一些中间结果提升性能...

//vp的策略不明确
//目前只是尝试使用7次评估的平均值作为结果
//来研究vp的策略吧!

std::map<std::pair<unsigned char, unsigned char>, TetrisOpertion> init_op;
std::map<unsigned char, TetrisNode const *(*)(TetrisMap const &)> op;
std::unordered_map<TetrisBlockStatus, TetrisNode> node_cache;
unsigned char const tetris[] = {'O', 'I', 'S', 'Z', 'L', 'J', 'T'};

template<unsigned char T> struct TypeToIndex{};
template<> struct TypeToIndex<'O'>{ enum { value = 0 }; };
template<> struct TypeToIndex<'I'>{ enum { value = 1 }; };
template<> struct TypeToIndex<'S'>{ enum { value = 2 }; };
template<> struct TypeToIndex<'Z'>{ enum { value = 3 }; };
template<> struct TypeToIndex<'L'>{ enum { value = 4 }; };
template<> struct TypeToIndex<'J'>{ enum { value = 5 }; };
template<> struct TypeToIndex<'T'>{ enum { value = 6 }; };

TetrisNode const *generate_cache[7];
struct
{
    int data[4];
} map_danger_data[7];
std::map<unsigned char, std::vector<TetrisNode const *>> block_place_cache;
std::vector<std::vector<TetrisNode const *>> place_cache;
std::vector<TetrisNode const *> node_search;
std::vector<EvalParam> history;

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
    return generate_cache[TypeToIndex<T>::value];
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

inline TetrisNode const *generate(unsigned char type, TetrisMap const &map)
{
    return op[type](map);
}

inline TetrisNode const *generate(size_t index, TetrisMap const &map)
{
    return generate_cache[index];
}

inline TetrisNode const *generate(TetrisMap const &map)
{
    size_t index = mtirand() & 7;
    while(index >= 7)
    {
        index = mtirand() & 7;
    }
    return generate(index, map);
}

inline size_t map_in_danger(TetrisMap const &map)
{
    size_t die_count = 0;
    for(size_t i = 0; i < 7; ++i)
    {
        if(map_danger_data[i].data[0] & map.row[map.height - 4] || map_danger_data[i].data[1] & map.row[map.height - 3] || map_danger_data[i].data[2] & map.row[map.height - 2] || map_danger_data[i].data[3] & map.row[map.height - 1])
        {
            ++die_count;
        }
    }
    return die_count;
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

namespace ai_simple
{

    std::pair<TetrisNode const *, int> do_ai(TetrisMap const &map, TetrisNode const *node, unsigned char next[], size_t next_length)
    {
        if(node == nullptr || !node->check(map))
        {
            return std::make_pair(nullptr, std::numeric_limits<int>::min());
        }
        if(place_cache.size() <= next_length)
        {
            place_cache.resize(next_length + 1);
        }
        std::vector<TetrisNode const *> const *place = nullptr;
        if(node->low >= map.roof)
        {
            place = node->place;
        }
        else
        {
            std::vector<TetrisNode const *> *place_make = &place_cache[next_length];
            place_make->clear();
            TetrisNode const *rotate = node;
            do
            {
                place_make->push_back(rotate);
                TetrisNode const *left = rotate->move_left;
                while(left != nullptr && left->check(map))
                {
                    place_make->push_back(left);
                    left = left->move_left;
                }
                TetrisNode const *right = rotate->move_right;
                while(right != nullptr && right->check(map))
                {
                    place_make->push_back(right);
                    right = right->move_right;
                }
                rotate = rotate->rotate_counterclockwise;
            } while(rotate != nullptr  && rotate != node && rotate->check(map));
            place = place_make;
        }
        int eval = std::numeric_limits<int>::min();
        TetrisNode const *beat_node = node;
        size_t best = 0;
        TetrisMap copy;
        for(size_t i = 0; i < place->size(); ++i)
        {
            node = (*place)[i]->drop(map);
            copy = map;
            history.push_back(EvalParam(node, node->attach(copy), map));
            int new_eval;
            if(next_length == 0)
            {
                new_eval = ai_eval(copy, history.data(), history.size());
            }
            else
            {
                if(*next == ' ')
                {
                    long long guess_eval = 0;
                    for(size_t ti = 0; ti < 7; ++ti)
                    {
                        guess_eval += do_ai(copy, generate(ti, copy), next + 1, next_length - 1).second;
                    }
                    new_eval = int(guess_eval / 7);
                }
                else
                {
                    new_eval = do_ai(copy, generate(*next, copy), next + 1, next_length - 1).second;
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
    struct NodePath
    {
        NodePath() : version(1)
        {
        }
        struct CacheNode
        {
            CacheNode() : version(0)
            {
            }
            size_t version;
            std::pair<TetrisNode const *, char> data;
        };
        size_t version;
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
        inline std::pair<TetrisNode const *, char> get(TetrisNode const *key)
        {
            return data[key->index].data;
        }
        inline bool set(TetrisNode const *key, TetrisNode const *node, char op)
        {
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
        inline bool mark(TetrisNode const *key)
        {
            CacheNode &cache = data[key->index];
            if(cache.version == version)
            {
                return false;
            }
            cache.version = version;
            return true;
        }
    } node_path;

    std::vector<char> make_path(TetrisNode const *from, TetrisNode const *to, TetrisMap const &map)
    {
        node_path.clear();
        node_search.clear();

        auto build_path = [to]()->std::vector<char>
        {
            std::vector<char> path;
            TetrisNode const *node = to;
            while(true)
            {
                auto result = node_path.get(node);
                node = result.first;
                if(node == nullptr)
                {
                    break;
                }
                path.push_back(result.second);
            }
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
                if(node->rotate_opposite && node_path.set(node->rotate_opposite, node, 'x') && node->rotate_opposite->check(map))
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
                if(node->rotate_counterclockwise && node_path.set(node->rotate_counterclockwise, node, 'z') && node->rotate_counterclockwise->check(map))
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
                    if(node_r->rotate_counterclockwise && node_path.set(node_r->rotate_counterclockwise, node_r, 'z') && node_r->rotate_counterclockwise->check(map))
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
                if(node->rotate_clockwise && node_path.set(node->rotate_clockwise, node, 'c') && node->rotate_clockwise->check(map))
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
                    if(node_r->rotate_clockwise && node_path.set(node_r->rotate_clockwise, node_r, 'c') && node_r->rotate_clockwise->check(map))
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
                if(node->move_left && node_path.set(node->move_left, node, 'l') && node->move_left->check(map))
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
                if(node->move_right && node_path.set(node->move_right, node, 'r') && node->move_right->check(map))
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
                    if(node_path.set(node_L, node, 'L'))
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
                    if(node_path.set(node_R, node, 'R'))
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
                if(node->move_down && node_path.set(node->move_down, node, 'd') && node->move_down->check(map))
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
                    TetrisNode const *node_D = node->drop(map);
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

    std::pair<TetrisNode const *, int> do_ai(TetrisMap const &map, TetrisNode const *node, unsigned char next[], size_t next_length)
    {
        if(node == nullptr || !node->check(map))
        {
            return std::make_pair(nullptr, std::numeric_limits<int>::min());
        }
        if(place_cache.size() <= next_length)
        {
            place_cache.resize(next_length + 1);
        }
        std::vector<TetrisNode const *> *place_make = &place_cache[next_length];
        node_path.clear();
        node_search.clear();
        place_make->clear();
        if(node->low >= map.roof)
        {
            for(auto cit = node->place->begin(); cit != node->place->end(); ++cit)
            {
                place_make->push_back((*cit)->drop(map));
            }
            TetrisNode const *last_node = nullptr;
            for(auto cit = place_make->begin(); cit != place_make->end(); ++cit)
            {
                node = *cit;
                if(last_node != nullptr)
                {
                    if(last_node->status.r == node->status.r && std::abs(last_node->status.x - node->status.x) == 1 && std::abs(last_node->status.y - node->status.y) > 1)
                    {
                        if(last_node->status.y > node->status.y)
                        {
                            TetrisNode const *check_node = (last_node->*(last_node->status.x > node->status.x ? &TetrisNode::move_left : &TetrisNode::move_right))->move_down->move_down;
                            if(node_path.mark(check_node))
                            {
                                node_search.push_back(check_node);
                            }
                        }
                        else
                        {
                            TetrisNode const *check_node = (node->*(node->status.x > last_node->status.x ? &TetrisNode::move_left : &TetrisNode::move_right))->move_down->move_down;
                            if(node_path.mark(check_node))
                            {
                                node_search.push_back(check_node);
                            }
                        }
                    }
                }
                last_node = node;
            }
            size_t cache_index = 0;
            do
            {
                for(size_t max_index = node_search.size(); cache_index < max_index; ++cache_index)
                {
                    node = node_search[cache_index];
                    if(!node->open(map) && (!node->move_down || !node->move_down->check(map)))
                    {
                        place_make->push_back(node);
                    }
                    //x
                    if(node->rotate_opposite && node_path.mark(node->rotate_opposite) && !node->rotate_opposite->open(map) && node->rotate_opposite->check(map))
                    {
                        node_search.push_back(node->rotate_opposite);
                    }
                    //z
                    if(node->rotate_counterclockwise && node_path.mark(node->rotate_counterclockwise) && !node->rotate_counterclockwise->open(map) && node->rotate_counterclockwise->check(map))
                    {
                        node_search.push_back(node->rotate_counterclockwise);
                    }
                    //c
                    if(node->rotate_clockwise && node_path.mark(node->rotate_clockwise) && !node->rotate_clockwise->open(map) && node->rotate_clockwise->check(map))
                    {
                        node_search.push_back(node->rotate_clockwise);
                    }
                    //l
                    if(node->move_left && node_path.mark(node->move_left) && !node->move_left->open(map) && node->move_left->check(map))
                    {
                        node_search.push_back(node->move_left);
                    }
                    //r
                    if(node->move_right && node_path.mark(node->move_right) && !node->move_right->open(map) && node->move_right->check(map))
                    {
                        node_search.push_back(node->move_right);
                    }
                    //d
                    if(node->move_down && node_path.mark(node->move_down) && node->move_down->check(map))
                    {
                        node_search.push_back(node->move_down);
                    }
                }
            } while(node_search.size() > cache_index);
        }
        else
        {
            node_search.push_back(node);
            node_path.mark(node);
            size_t cache_index = 0;
            do
            {
                for(size_t max_index = node_search.size(); cache_index < max_index; ++cache_index)
                {
                    node = node_search[cache_index];
                    if(!node->move_down || !node->move_down->check(map))
                    {
                        place_make->push_back(node);
                    }
                    //x
                    if(node->rotate_opposite && node_path.mark(node->rotate_opposite) && node->rotate_opposite->check(map))
                    {
                        node_search.push_back(node->rotate_opposite);
                    }
                    //z
                    if(node->rotate_counterclockwise && node_path.mark(node->rotate_counterclockwise) && node->rotate_counterclockwise->check(map))
                    {
                        node_search.push_back(node->rotate_counterclockwise);
                    }
                    //c
                    if(node->rotate_clockwise && node_path.mark(node->rotate_clockwise) && node->rotate_clockwise->check(map))
                    {
                        node_search.push_back(node->rotate_clockwise);
                    }
                    //l
                    if(node->move_left && node_path.mark(node->move_left) && node->move_left->check(map))
                    {
                        node_search.push_back(node->move_left);
                    }
                    //r
                    if(node->move_right && node_path.mark(node->move_right) && node->move_right->check(map))
                    {
                        node_search.push_back(node->move_right);
                    }
                    //d
                    if(node->move_down && node_path.mark(node->move_down) && node->move_down->check(map))
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
        for(size_t i = 0; i < place_make->size(); ++i)
        {
            node = (*place_make)[i];
            copy = map;
            history.push_back(EvalParam(node, node->attach(copy), map));
            int new_eval;
            if(next_length == 0)
            {
                new_eval = ai_eval(copy, history.data(), history.size());
            }
            else
            {
                if(*next == ' ')
                {
                    long long guess_eval = 0;
                    for(size_t ti = 0; ti < 7; ++ti)
                    {
                        guess_eval += do_ai(copy, generate(ti, copy), next + 1, next_length - 1).second;
                    }
                    new_eval = int(guess_eval / 7);
                }
                else
                {
                    new_eval = do_ai(copy, generate(*next, copy), next + 1, next_length - 1).second;
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
    block_place_cache.clear();
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
/**//**//**/node.status, node.op, {node.data[0], node.data[1], node.data[2], node.data[3]}, {node.top[0], node.top[1], node.top[2], node.top[3]}, {node.bottom[0], node.bottom[1], node.bottom[2], node.bottom[3]}, node.row, node.height, node.col, node.width\
/**//**/};\
/**//**/if(copy.op.func != nullptr && copy.op.func(copy, map))\
/**//**/{\
/**//**//**/auto result = node_cache.insert(std::make_pair(copy.status, copy));\
/**//**//**/if(result.second)\
/**//**//**/{\
/**//**//**//**/check.push_back(copy.status);\
/**//**//**/}\
/**//**//**/node.func = &result.first->second;\
/**//**/}\
/**/} while(false)\
/**/
            D(rotate_clockwise);
            D(rotate_counterclockwise);
            D(rotate_opposite);
            D(move_left);
            D(move_right);
            D(move_down);
            node.move_down_multi[0] = &node;
            if(node.move_down)
            {
                node.move_down_multi[1] = node.move_down;
                TetrisNode copy =
                {
                    node.move_down->status, node.move_down->op, {node.move_down->data[0], node.move_down->data[1], node.move_down->data[2], node.move_down->data[3]}, {node.move_down->top[0], node.move_down->top[1], node.move_down->top[2], node.move_down->top[3]}, {node.move_down->bottom[0], node.move_down->bottom[1], node.move_down->bottom[2], node.move_down->bottom[3]}, node.move_down->row, node.move_down->height, node.move_down->col, node.move_down->width, node.move_down->low
                };
                int index = 2;
                while(copy.op.move_down != nullptr && copy.op.move_down(copy, map))
                {
                    auto result = node_cache.insert(std::make_pair(copy.status, copy));
                    if(result.second)
                    {
                        check.push_back(copy.status);
                    }
                    node.move_down_multi[index] = &result.first->second;
                    ++index;
                }
            }
#undef D
        }
    } while(check.size() > check_index);
    ai_path::node_path.data.resize(node_cache.size(), ai_path::NodePath::CacheNode());
    for(size_t i = 0; i < 7; ++i)
    {
        TetrisBlockStatus status =
        {
            tetris[i], map_width / 2, map_height - 2, 1
        };
        TetrisNode const *node = get(status);
        generate_cache[i] = node;
        TetrisMap copy = map;
        node->attach(copy);
        memcpy(map_danger_data[i].data, &copy.row[map.height - 4], sizeof map_danger_data[i].data);
        for(int y = 0; y < 3; ++y)
        {
            map_danger_data[i].data[y + 1] |= map_danger_data[i].data[y];
        }
    }
    for(size_t i = 0; i < 7; ++i)
    {
        TetrisNode const *node = generate(i, map);
        block_place_cache.insert(std::make_pair(node->status.t, std::vector<TetrisNode const *>()));
        std::vector<TetrisNode const *> *place = &block_place_cache.find(node->status.t)->second;
        TetrisNode const *rotate = node;
        do
        {
            TetrisNode const *move = rotate;
            while(move->move_left != nullptr)
            {
                move = move->move_left;
            }
            do
            {
                place->push_back(move);
                move = move->move_right;
            } while(move != nullptr);
            rotate = rotate->rotate_counterclockwise;
        } while(rotate != nullptr  && rotate != node);
        rotate = node;
        int low = map.height;
        do
        {
            for(int y = 0; y < rotate->width; ++y)
            {
                low = std::min(low, rotate->bottom[y]);
            }
            rotate = rotate->rotate_counterclockwise;
        } while(rotate != nullptr  && rotate != node);
        auto set_column_data = [place](TetrisNode const *node, int low)->void
        {
            do
            {
                TetrisNode *set_node = const_cast<TetrisNode *>(node);
                set_node->low = low--;
                set_node->place = place;
                node = set_node->move_down;
            } while(node != nullptr);
        };
        rotate = node;
        do
        {
            TetrisNode const *move = rotate;
            while(move->move_left != nullptr)
            {
                move = move->move_left;
            }
            do
            {
                set_column_data(move, low);
                move = move->move_right;
            } while(move != nullptr);
            rotate = rotate->rotate_counterclockwise;
        } while(rotate != nullptr  && rotate != node);
    }
    return true;
}