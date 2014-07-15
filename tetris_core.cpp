
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

using namespace m_tetris;


inline bool TetrisNode::check(TetrisMap const &map) const
{
    switch(height)
    {
    case 4:
        if(map.row[row + 3] & data[3])
        {
            return false;
        }
    case 3:
        if(map.row[row + 2] & data[2])
        {
            return false;
        }
    case 2:
        if(map.row[row + 1] & data[1])
        {
            return false;
        }
    default:
        if(map.row[row] & data[0])
        {
            return false;
        }
    }
    return true;
}

inline bool TetrisNode::open(TetrisMap const &map) const
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

inline size_t TetrisNode::attach(TetrisMap &map) const
{
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
        if(map.row[row + i - 1] == context->full())
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

inline TetrisNode const *TetrisNode::drop(TetrisMap const &map) const
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

void TetrisNodeMark::init(size_t size)
{
    data_.clear();
    data_.resize(size);
}

inline void TetrisNodeMark::clear()
{
    if(++version_ == std::numeric_limits<size_t>::max())
    {
        version_ = 1;
        for(auto it = data_.begin(); it != data_.end(); ++it)
        {
            it->version = 0;
        }
    }
}
inline std::pair<TetrisNode const *, char> TetrisNodeMark::get(TetrisNode const *key)
{
    return data_[key->index].data;
}
inline bool TetrisNodeMark::set(TetrisNode const *key, TetrisNode const *node, char op)
{
    Mark &mark = data_[key->index];
    if(mark.version == version_)
    {
        return false;
    }
    mark.version = version_;
    mark.data.first = node;
    mark.data.second = op;
    return true;
}
inline bool TetrisNodeMark::mark(TetrisNode const *key)
{
    Mark &mark = data_[key->index];
    if(mark.version == version_)
    {
        return false;
    }
    mark.version = version_;
    return true;
}

bool TetrisContext::prepare(int width, int height)
{
    if(width == width_ && height == height_)
    {
        return true;
    }
    if(width > 32 || height > 40 || width < 4 || height < 4)
    {
        return false;
    }
    place_cache_.clear();
    node_cache_.clear();
    memset(&map_danger_data_, 0, sizeof map_danger_data_);
    width_ = width;
    height_ = height;
    type_max_ = 0;
    if(width < 32)
    {
        full_ = (1 << width) - 1;
    }
    else
    {
        full_ = std::numeric_limits<size_t>::max();
    }

    std::vector<TetrisBlockStatus> check;
    for(auto cit = init_opertion_.begin(); cit != init_opertion_.end(); ++cit)
    {
        TetrisNode node = cit->second.create(width, height, cit->second);
        check.push_back(node.status);
        node_cache_.insert(std::make_pair(node.status, node));
    }
    TetrisMap map = {{}, {}, width, height};
    size_t check_index = 0;
    do
    {
        for(size_t max_index = check.size(); check_index < max_index; ++check_index)
        {
            TetrisNode &node = node_cache_.find(check[check_index])->second;
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
/**//**//**/auto result = node_cache_.insert(std::make_pair(copy.status, copy));\
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
                    auto result = node_cache_.insert(std::make_pair(copy.status, copy));
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
    node_mark_.init(node_cache_.size());
    for(size_t i = 0; i < 7; ++i)
    {
        TetrisBlockStatus status = init_generate_[tetris[i]](this);
        TetrisNode const *node = get(status);
        generate_cache_[i] = node;
        TetrisMap copy = map;
        node->attach(copy);
        memcpy(map_danger_data_[i].data, &copy.row[map.height - 4], sizeof map_danger_data_[i].data);
        for(int y = 0; y < 3; ++y)
        {
            map_danger_data_[i].data[y + 1] |= map_danger_data_[i].data[y];
        }
    }
    for(size_t i = 0; i < 7; ++i)
    {
        TetrisNode const *node = generate(i);
        place_cache_.insert(std::make_pair(node->status.t, std::vector<TetrisNode const *>()));
        std::vector<TetrisNode const *> *place = &place_cache_.find(node->status.t)->second;
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

int TetrisContext::width() const
{
    return width_;
}

int TetrisContext::height() const
{
    return height_;
}

int TetrisContext::full() const
{
    return full_;
}

TetrisOpertion TetrisContext::get_opertion(unsigned char t, unsigned char r) const
{
    auto find = init_opertion_.find(std::make_pair(t, r));
    if(find == init_opertion_.end())
    {
        TetrisOpertion empty = {};
        return empty;
    }
    else
    {
        return find->second;
    }
}

TetrisNode const *TetrisContext::get(TetrisBlockStatus const &status) const
{
    auto find = node_cache_.find(status);
    if(find == node_cache_.end())
    {
        return nullptr;
    }
    else
    {
        return &find->second;
    }
}

TetrisNode const *TetrisContext::get(unsigned char t, char x, char y, unsigned char r) const
{
    TetrisBlockStatus status =
    {
        t, x, y, r
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






















template<unsigned char T> struct TypeToIndex{};
template<> struct TypeToIndex<'O'>{ enum { value = 0 }; };
template<> struct TypeToIndex<'I'>{ enum { value = 1 }; };
template<> struct TypeToIndex<'S'>{ enum { value = 2 }; };
template<> struct TypeToIndex<'Z'>{ enum { value = 3 }; };
template<> struct TypeToIndex<'L'>{ enum { value = 4 }; };
template<> struct TypeToIndex<'J'>{ enum { value = 5 }; };
template<> struct TypeToIndex<'T'>{ enum { value = 6 }; };


inline TetrisNode const *TetrisContext::generate(unsigned char type) const
{
    return generate_cache_[type_to_index_[type]];
}

inline TetrisNode const *TetrisContext::generate(size_t index) const
{
    return generate_cache_[index];
}

inline TetrisNode const *TetrisContext::generate() const
{
    size_t index;
    do
    {
        index = mtirand() & 7;
    } while(index >= 7);
    return generate(index);
}

inline size_t TetrisContext::map_in_danger(TetrisMap const &map) const
{
    size_t count = 0;
    for(size_t i = 0; i < type_max_; ++i)
    {
        if(map_danger_data_[i].data[0] & map.row[map.height - 4] || map_danger_data_[i].data[1] & map.row[map.height - 3] || map_danger_data_[i].data[2] & map.row[map.height - 2] || map_danger_data_[i].data[3] & map.row[map.height - 1])
        {
            ++count;
        }
    }
    return count;
}

extern "C" void attach_init()
{
    mtsrand(unsigned int(time(nullptr)));
}

namespace ai_simple
{
    std::vector<char> make_path(TetrisNode const *from, TetrisNode const *to, TetrisMap const &map)
    {
        std::vector<char> path;
        if(from->status.t != to->status.t || from->status.y < to->status.y)
        {
            return path;
        }
        while(from->status.r != to->status.r && from->rotate_counterclockwise && from->rotate_counterclockwise->check(map))
        {
            path.push_back('z');
            from = from->rotate_counterclockwise;
        }
        while(from->status.x < to->status.x && from->move_right && from->move_right->check(map))
        {
            path.push_back('r');
            from = from->move_right;
        }
        while(from->status.x > to->status.x && from->move_left && from->move_left->check(map))
        {
            path.push_back('l');
            from = from->move_left;
        }
        if(from->drop(map) != to)
        {
            return std::vector<char>();
        }
        path.push_back('\0');
        return path;
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
        std::vector<TetrisNode const *> *place = &place_cache[next_length];
        node_path.clear();
        node_search.clear();
        place->clear();
        if(node->low >= map.roof)
        {
            for(auto cit = node->place->begin(); cit != node->place->end(); ++cit)
            {
                place->push_back((*cit)->drop(map));
            }
            TetrisNode const *last_node = nullptr;
            for(auto cit = place->begin(); cit != place->end(); ++cit)
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
                        place->push_back(node);
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
                        place->push_back(node);
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
        for(size_t i = 0; i < place->size(); ++i)
        {
            node = (*place)[i];
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

namespace ai_senior
{
    std::vector<char> make_path(TetrisNode const *from, TetrisNode const *to, TetrisMap const &map)
    {
        //TODO
    }

    std::pair<TetrisNode const *, int> do_ai(TetrisMap const &map, TetrisNode const *node, unsigned char next[], size_t next_count)
    {
        //TODO
    }
}

