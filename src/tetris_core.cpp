
#include <map>
#include "tetris_core.h"
#include "random.h"

//这里就懒得标记注释了...
//有心读的话...可以试试看调试跟踪一下...
//大概指针网比较复杂吧?
//搜索都是广度优先...
//缓存了一些中间结果提升性能...

//vp的策略不明确
//目前只是尝试使用7次评估的平均值作为结果
//来研究vp的策略吧!

namespace m_tetris
{
    bool TetrisNode::check(TetrisMap const &map) const
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

    bool TetrisNode::open(TetrisMap const &map) const
    {
        switch(width)
        {
        case 4:
            if(bottom[3] < map.top[col + 3])
            {
                return false;
            }
        case 3:
            if(bottom[2] < map.top[col + 2])
            {
                return false;
            }
        case 2:
            if(bottom[1] < map.top[col + 1])
            {
                return false;
            }
        default:
            if(bottom[0] < map.top[col])
            {
                return false;
            }
        }
        return true;
    }

    size_t TetrisNode::attach(TetrisMap &map) const
    {
        switch(height)
        {
        case 4:
            map.row[row + 3] |= data[3];
        case 3:
            map.row[row + 2] |= data[2];
        case 2:
            map.row[row + 1] |= data[1];
        default:
            map.row[row] |= data[0];
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
        switch(width)
        {
        case 4:
            if(top[3] > map.top[col + 3])
            {
                map.top[col + 3] = top[3];
                map.roof = std::max(top[3], map.roof);
            }
        case 3:
            if(top[2] > map.top[col + 2])
            {
                map.top[col + 2] = top[2];
                map.roof = std::max(top[2], map.roof);
            }
        case 2:
            if(top[1] > map.top[col + 1])
            {
                map.top[col + 1] = top[1];
                map.roof = std::max(top[1], map.roof);
            }
        default:
            if(top[0] > map.top[col])
            {
                map.top[col] = top[0];
                map.roof = std::max(top[0], map.roof);
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

    TetrisNode const *TetrisNode::drop(TetrisMap const &map) const
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

    void TetrisNodeMark::clear()
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

    std::pair<TetrisNode const *, char> TetrisNodeMark::get(size_t index)
    {
        return data_[index].data;
    }

    std::pair<TetrisNode const *, char> TetrisNodeMark::get(TetrisNode const *key)
    {
        return data_[key->index].data;
    }

    bool TetrisNodeMark::set(TetrisNode const *key, TetrisNode const *node, char op)
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
    
    bool TetrisNodeMark::mark(TetrisNode const *key)
    {
        Mark &mark = data_[key->index];
        if(mark.version == version_)
        {
            return false;
        }
        mark.version = version_;
        return true;
    }

    void TetrisNodeMarkFiltered::init(size_t size)
    {
        data_.clear();
        data_.resize(size);
    }

    void TetrisNodeMarkFiltered::clear()
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

    std::pair<TetrisNode const *, char> TetrisNodeMarkFiltered::get(size_t index)
    {
        return data_[index].data;
    }

    std::pair<TetrisNode const *, char> TetrisNodeMarkFiltered::get(TetrisNode const *key)
    {
        return data_[key->index_filtered].data;
    }

    bool TetrisNodeMarkFiltered::set(TetrisNode const *key, TetrisNode const *node, char op)
    {
        Mark &mark = data_[key->index_filtered];
        if(mark.version == version_)
        {
            return false;
        }
        mark.version = version_;
        mark.data.first = node;
        mark.data.second = op;
        return true;
    }

    bool TetrisNodeMarkFiltered::mark(TetrisNode const *key)
    {
        Mark &mark = data_[key->index_filtered];
        if(mark.version == version_)
        {
            return false;
        }
        mark.version = version_;
        return true;
    }

    TetrisContext::PrepareResult TetrisContext::prepare(int width, int height)
    {
        if(width == width_ && height == height_)
        {
            return ok;
        }
        if(width > 32 || height > max_height || width < 4 || height < 4)
        {
            return fail;
        }
        place_cache_.clear();
        node_cache_.clear();
        width_ = width;
        height_ = height;
        type_max_ = 0;
        full_ = width == 32 ? 0xFFFFFFFF : (1 << width) - 1;
        std::vector<TetrisBlockStatus> check;
        for(auto cit = init_generate_.begin(); cit != init_generate_.end(); ++cit)
        {
            unsigned char type = cit->first;
            index_to_type_[type_max_] = type;
            type_to_index_[type] = type_max_;
            ++type_max_;
        }
        for(size_t i = 0; i < 7; ++i)
        {
            TetrisNode node;
            create(init_generate_[index_to_type_[i]](this), node);
            check.push_back(node.status);
            generate_cache_[i] = &node_cache_.insert(std::make_pair(node.status, node)).first->second;
        }
        struct IndexFilter
        {
            IndexFilter(TetrisNode const &node)
            {
                memcpy(data, node.data, sizeof node.data);
                data[4] = node.row;
            }
            int data[5];
            struct Less
            {
                bool operator()(IndexFilter const &left, IndexFilter const &right)
                {
                    return memcmp(left.data, right.data, sizeof left.data) < 0;
                }
            };
        };
        std::map<IndexFilter, int, IndexFilter::Less> index_filter;
        TetrisMap map = {{}, {}, width, height};
        size_t check_index = 0;
        do
        {
            for(size_t max_index = check.size(); check_index < max_index; ++check_index)
            {
                TetrisNode &node = node_cache_.find(check[check_index])->second;
                node.index = check_index;
                node.index_filtered = index_filter.insert(std::make_pair(node, check_index)).first->second;
                node.context = this;
#define D(func)\
/**/do\
/**/{\
/**//**/TetrisNode copy =\
/**//**/{\
/**//**//**/node.status, node.op, {node.data[0], node.data[1], node.data[2], node.data[3]}, {node.top[0], node.top[1], node.top[2], node.top[3]}, {node.bottom[0], node.bottom[1], node.bottom[2], node.bottom[3]}, node.row, node.height, node.col, node.width\
/**//**/};\
/**//**/if(copy.op.func != nullptr && copy.op.func(copy, this))\
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
                    while(copy.op.move_down != nullptr && copy.op.move_down(copy, this))
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
        for(size_t i = 0; i < 7; ++i)
        {
            TetrisNode node_generate;
            create(game_generate_[index_to_type_[i]](this), node_generate);
            TetrisNode const *node = generate_cache_[i] = get(node_generate.status);
            place_cache_.insert(std::make_pair(node->status.t, std::vector<TetrisNode const *>()));
            std::vector<TetrisNode const *> *land_point = &place_cache_.find(node->status.t)->second;
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
                    land_point->push_back(move);
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
            auto set_column_data = [land_point](TetrisNode const *node, int low)->void
            {
                do
                {
                    TetrisNode *set_node = const_cast<TetrisNode *>(node);
                    set_node->low = low--;
                    set_node->land_point = land_point;
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
        for(auto it = node_cache_.begin(); it != node_cache_.end(); ++it)
        {
            TetrisNode &node = it->second;
#define D(func)\
/**//**//**/do\
/**//**//**/{\
/**//**//**//**/if(node.op.rotate_##func != nullptr)\
/**//**//**//**/{\
/**//**//**//**//**/TetrisNode copy = *generate(node.status.t);\
/**//**//**//**//**/node.op.rotate_##func(copy, this);\
/**//**//**//**//**/TetrisBlockStatus status = copy.status;\
/**//**//**//**//**/size_t wall_kick_index = 0;\
/**//**//**//**//**/for(TetrisWallKickOpertion::WallKickNode &n : node.op.wall_kick_##func.data)\
/**//**//**//**//**/{\
/**//**//**//**//**//**/TetrisBlockStatus wall_kick_status =\
/**//**//**//**//**//**/{\
/**//**//**//**//**//**//**/status.t, node.status.x + n.x, node.status.y + n.y, status.r\
/**//**//**//**//**//**/};\
/**//**//**//**//**//**/if(create(wall_kick_status, copy))\
/**//**//**//**//**//**/{\
/**//**//**//**//**//**//**/node.wall_kick_##func[wall_kick_index++] = get(copy.status);\
/**//**//**//**//**//**/}\
/**//**//**//**//**/}\
/**//**//**//**/}\
/**//**//**/} while(false)\
/**//**//**/
            D(clockwise);
            D(counterclockwise);
            D(opposite);
        }
        return rebuild;
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

    size_t TetrisContext::type_max() const
    {
        return type_max_;
    }

    size_t TetrisContext::node_max() const
    {
        return node_cache_.size();
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

    TetrisNode const *TetrisContext::generate(unsigned char type) const
    {
        return generate_cache_[type_to_index_[type]];
    }

    TetrisNode const *TetrisContext::generate(size_t index) const
    {
        return generate_cache_[index];
    }

    TetrisNode const *TetrisContext::generate() const
    {
        size_t index;
        do
        {
            index = ege::mtirand() & 7;
        } while(index >= 7);
        return generate(index);
    }

    bool TetrisContext::create(TetrisBlockStatus const &status, TetrisNode &node) const
    {
        TetrisNode const *cache = get(status);
        if(cache != nullptr)
        {
            node = *cache;
            return true;
        }
        TetrisOpertion op = get_opertion(status.t, status.r);
        TetrisNode new_node = op.create(width_, height_, op);
        while(new_node.status.x > status.x)
        {
            if(!m_tetris_rule_tools::move_left(new_node, this))
            {
                return false;
            }
        }
        while(new_node.status.x < status.x)
        {
            if(!m_tetris_rule_tools::move_right(new_node, this))
            {
                return false;
            }
        }
        while(new_node.status.y > status.y)
        {
            if(!m_tetris_rule_tools::move_down(new_node, this))
            {
                return false;
            }
        }
        node = new_node;
        return true;
    }
}

namespace m_tetris_rule_tools
{


    bool move_left(TetrisNode &node, TetrisContext const *context)
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

    bool move_right(TetrisNode &node, TetrisContext const *context)
    {
        const int check = context->width() == 32 ? 0x80000000 : (1 << (context->width() - 1));
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

    bool move_down(TetrisNode &node, TetrisContext const *context)
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

}
