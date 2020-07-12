﻿
#include <map>
#include <iostream>
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

    void TetrisNode::build_snap(TetrisMap const &map, TetrisContext const *context, TetrisMapSnap &snap) const
    {
        for(int r = 0; r < 4; ++r)
        {
            auto block = context->get_block(status.t, r);
            if(block->count > 0)
            {
                uint32_t i = 0;
                if(block->data[0].x == 0 && block->data[0].y == 0)
                {
                    std::memcpy(snap.row[r], map.row, sizeof(uint32_t) * map.roof);
                    i = 1;
                }
                for(; i < block->count; ++i)
                {
                    int bx = block->data[i].x, by = block->data[i].y;
                    if(bx > 0)
                    {
                        uint32_t wall = 1U << (context->width() - bx);
                        int y = 0, e1 = map.roof - by, e2 = map.height - by;
                        while(y < e1)
                        {
                            snap.row[r][y] |= map.row[y + by] >> bx | wall;
                            ++y;
                        }
                        while(y < e2)
                        {
                            snap.row[r][y] |= wall;
                            ++y;
                        }
                    }
                    else
                    {
                        for(int y = 0, ey = map.roof - by; y < ey; ++y)
                        {
                            snap.row[r][y] |= map.row[y + by] >> bx;
                        }
                    }
                    if(by > 0)
                    {
                        snap.row[r][map.height - by] = context->full();
                    }
                }
            }
        }
    }

    size_t TetrisNode::attach(TetrisContext const *context, TetrisMap &map) const
    {
        switch(height)
        {
        case 4:
            map.row[row + 3] |= data[3];
        case 3:
            map.row[row + 2] |= data[2];
        case 2:
            map.row[row + 1] |= data[1];
        case 1:
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
        case 1:
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

    int TetrisNode::clear_low(TetrisContext const *context, TetrisMap &map) const
    {
        for(int i = 0; i < height; ++i)
        {
            if(map.row[row + i] == context->full())
            {
                return row + i;
            }
        }
        return -1;
    }

    int TetrisNode::clear_high(TetrisContext const *context, TetrisMap &map) const
    {
        for(int i = height; i > 0; --i)
        {
            if(map.row[row + i - 1] == context->full())
            {
                return row + i - 1;
            }
        }
        return -1;
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
        Mark &mark = data_[index];
        return mark.version == version_ ? mark.data : std::pair<TetrisNode const *, char>{ nullptr, ' ' };
    }

    std::pair<TetrisNode const *, char> TetrisNodeMark::get(TetrisNode const *key)
    {
        Mark &mark = data_[key->index];
        return mark.version == version_ ? mark.data : std::pair<TetrisNode const *, char>{ nullptr, ' ' };
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

    bool TetrisNodeMark::cover_if(TetrisNode const *key, TetrisNode const *node, char ck, char op)
    {
        Mark &mark = data_[key->index];
        if (mark.version == version_ && mark.data.second != ck)
        {
            return false;
        }
        mark.data.first = node;
        mark.data.second = op;
        mark.version = version_;
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
        Mark &mark = data_[index];
        return mark.version == version_ ? mark.data : std::pair<TetrisNode const *, char>{ nullptr, ' ' };
    }

    std::pair<TetrisNode const *, char> TetrisNodeMarkFiltered::get(TetrisNode const *key)
    {
        Mark &mark = data_[key->index_filtered];
        return mark.version == version_ ? mark.data : std::pair<TetrisNode const *, char>{ nullptr, ' ' };
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

    bool TetrisNodeMarkFiltered::cover_if(TetrisNode const *key, TetrisNode const *node, char ck, char op)
    {
        Mark &mark = data_[key->index_filtered];
        if (mark.version == version_ && mark.data.second != ck)
        {
            return false;
        }
        mark.data.first = node;
        mark.data.second = op;
        mark.version = version_;
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

    bool TetrisContext::prepare(int width, int height)
    {
        if(width > 32 || height > max_height || width < 4 || height < 4)
        {
            return false;
        }
        place_cache_.clear();
        node_index_.clear();
        node_storage_.clear();
        node_block_.clear();
        width_ = width;
        height_ = height;
        type_max_ = 0;
        full_ = width == 32 ? 0xFFFFFFFFU : (1 << width) - 1;
        std::vector<TetrisBlockStatus> check;
        for(auto cit = generate_.begin(); cit != generate_.end(); ++cit)
        {
            char type = cit->first;
            index_to_type_[type_max_] = ::toupper(type);
            type_to_index_[::tolower(type) + 128] = type_max_;
            type_to_index_[::toupper(type) + 128] = type_max_;
            ++type_max_;
        }
        node_block_.resize(type_max_ * 4);
        for(size_t i = 0; i < type_max_; ++i)
        {
            node_storage_.emplace_back();
            TetrisNode &node = node_storage_.back();
            create(generate_[convert(i)](this), node);
            check.push_back(node.status);
            node_index_.emplace(node.status, &node);
        }
        struct IndexFilter
        {
            IndexFilter(TetrisNode const &node)
            {
                std::memcpy(data, node.data, sizeof node.data);
                data[4] = node.row;
            }
            uint32_t data[5];
            struct Less
            {
                bool operator()(IndexFilter const &left, IndexFilter const &right) const
                {
                    return std::memcmp(left.data, right.data, sizeof left.data) < 0;
                }
            };
        };
        std::map<IndexFilter, uint32_t, IndexFilter::Less> index_filter;
        TetrisMap map(width, height);
        size_t check_index = 0;
        do
        {
            for(size_t max_index = check.size(); check_index < max_index; ++check_index)
            {
                TetrisNode &node = *node_index_.find(check[check_index])->second;
                node.index = check_index;
                node.index_filtered = index_filter.insert(std::make_pair(node, uint32_t(check_index))).first->second;
#define ROTATE(func)\
/**//**//**//**/do\
/**//**//**//**/{\
/**//**//**//**//**/TetrisNode copy =\
/**//**//**//**//**/{\
/**//**//**//**//**//**/node.status, node.op, {node.data[0], node.data[1], node.data[2], node.data[3]}, {node.top[0], node.top[1], node.top[2], node.top[3]}, {node.bottom[0], node.bottom[1], node.bottom[2], node.bottom[3]}, node.row, node.height, node.col, node.width\
/**//**//**//**//**/};\
/**//**//**//**//**/if(copy.op.func != nullptr && copy.op.func(copy, this))\
/**//**//**//**//**/{\
/**//**//**//**//**//**/auto find = node_index_.find(copy.status);\
/**//**//**//**//**//**/if(find == node_index_.end())\
/**//**//**//**//**//**/{\
/**//**//**//**//**//**//**/check.push_back(copy.status);\
/**//**//**//**//**//**//**/node_storage_.emplace_back(copy);\
/**//**//**//**//**//**//**/find = node_index_.emplace(copy.status, &node_storage_.back()).first;\
/**//**//**//**//**//**/}\
/**//**//**//**//**//**/node.func = find->second;\
/**//**//**//**//**/}\
/**//**//**//**/} while(false)\
/**//**//**//**/
                ROTATE(rotate_clockwise);
                ROTATE(rotate_counterclockwise);
                ROTATE(rotate_opposite);
#undef ROTATE
#define MOVE(func)\
/**//**//**//**/do\
/**//**//**//**/{\
/**//**//**//**//**/TetrisNode copy =\
/**//**//**//**//**/{\
/**//**//**//**//**//**/node.status, node.op, {node.data[0], node.data[1], node.data[2], node.data[3]}, {node.top[0], node.top[1], node.top[2], node.top[3]}, {node.bottom[0], node.bottom[1], node.bottom[2], node.bottom[3]}, node.row, node.height, node.col, node.width\
/**//**//**//**//**/};\
/**//**//**//**//**/if(m_tetris_rule_tools::func(copy, this))\
/**//**//**//**//**/{\
/**//**//**//**//**//**/auto find = node_index_.find(copy.status);\
/**//**//**//**//**//**/if(find == node_index_.end())\
/**//**//**//**//**//**/{\
/**//**//**//**//**//**//**/check.push_back(copy.status);\
/**//**//**//**//**//**//**/node_storage_.emplace_back(copy);\
/**//**//**//**//**//**//**/find = node_index_.emplace(copy.status, &node_storage_.back()).first;\
/**//**//**//**//**//**/}\
/**//**//**//**//**//**/node.func = find->second;\
/**//**//**//**//**/}\
/**//**//**//**/} while(false)\
/**//**//**//**/
                MOVE(move_left);
                MOVE(move_right);
                MOVE(move_down);
                MOVE(move_up);
#undef MOVE
                node.move_down_multi[0] = &node;
                if(node.move_down)
                {
                    node.move_down_multi[1] = node.move_down;
                    TetrisNode copy =
                    {
                        node.move_down->status, node.move_down->op, {node.move_down->data[0], node.move_down->data[1], node.move_down->data[2], node.move_down->data[3]}, {node.move_down->top[0], node.move_down->top[1], node.move_down->top[2], node.move_down->top[3]}, {node.move_down->bottom[0], node.move_down->bottom[1], node.move_down->bottom[2], node.move_down->bottom[3]}, node.move_down->row, node.move_down->height, node.move_down->col, node.move_down->width, node.move_down->low
                    };
                    int index = 2;
                    while(m_tetris_rule_tools::move_down(copy, this))
                    {
                        auto find = node_index_.find(copy.status);
                        if (find == node_index_.end())
                        {
                            check.push_back(copy.status);
                            node_storage_.emplace_back(copy);
                            find = node_index_.emplace(copy.status, &node_storage_.back()).first;
                        }
                        node.move_down_multi[index] = find->second;
                        ++index;
                    }
                }
                auto &block = node_block_[convert(node.status.t) * 4 + node.status.r];
                if(block.count == 0)
                {
                    for(int x = node.col; x < node.col + node.width; ++x)
                    {
                        for(int y = 0; y < node.height; ++y)
                        {
                            if((node.data[y] >> x) & 1)
                            {
                                auto &b = block.data[block.count++];
                                b.x = x - node.col;
                                b.y = y;
                            }
                        }
                    }
                }
            }
        }
        while(check.size() > check_index);
        for(size_t i = 0; i < type_max_; ++i)
        {
            TetrisNode node_generate;
            create(generate_[convert(i)](this), node_generate);
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
                }
                while(move != nullptr);
                rotate = rotate->rotate_counterclockwise != nullptr ? rotate->rotate_counterclockwise : rotate->rotate_clockwise;
            }
            while(rotate != nullptr  && rotate != node);
            rotate = node;
            int low = map.height;
            do
            {
                for(int y = 0; y < rotate->width; ++y)
                {
                    low = std::min(low, rotate->bottom[y]);
                }
                rotate = rotate->rotate_counterclockwise != nullptr ? rotate->rotate_counterclockwise : rotate->rotate_clockwise;
            }
            while(rotate != nullptr  && rotate != node);
            auto set_column_data = [land_point](TetrisNode const *node, int low)->void
            {
                do
                {
                    TetrisNode *set_node = const_cast<TetrisNode *>(node);
                    set_node->low = low--;
                    set_node->land_point = land_point;
                    node = set_node->move_down;
                }
                while(node != nullptr);
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
                }
                while(move != nullptr);
                rotate = rotate->rotate_counterclockwise;
            }
            while(rotate != nullptr  && rotate != node);
        }
        for(auto it = node_index_.begin(); it != node_index_.end(); ++it)
        {
            TetrisNode &node = *it->second;
#define WALL_KICK(func)\
/**//**//**/do\
/**//**//**/{\
/**//**//**//**/size_t wall_kick_index = 0;\
/**//**//**//**/if(node.op.rotate_##func != nullptr)\
/**//**//**//**/{\
/**//**//**//**//**/if(node.rotate_##func != nullptr)\
/**//**//**//**//**/{\
/**//**//**//**//**//**/++wall_kick_index;\
/**//**//**//**//**/}\
/**//**//**//**//**/TetrisNode copy = *generate(node.status.t);\
/**//**//**//**//**/node.op.rotate_##func(copy, this);\
/**//**//**//**//**/TetrisBlockStatus status = copy.status;\
/**//**//**//**//**/for(size_t i = 0; i < node.op.wall_kick_##func.length; ++i)\
/**//**//**//**//**/{\
/**//**//**//**//**//**/TetrisWallKickOpertion::WallKickNode &n = node.op.wall_kick_##func.data[i];\
/**//**//**//**//**//**/TetrisBlockStatus wall_kick_status(status.t, node.status.x + n.x, node.status.y + n.y, status.r);\
/**//**//**//**//**//**/if(create(wall_kick_status, copy))\
/**//**//**//**//**//**/{\
/**//**//**//**//**//**//**/node.wall_kick_##func[wall_kick_index++] = get(copy.status);\
/**//**//**//**//**//**/}\
/**//**//**//**//**/}\
/**//**//**//**/}\
/**//**//**//**/node.wall_kick_##func[wall_kick_index++] = nullptr;\
/**//**//**/} while(false)\
/**//**//**/
            WALL_KICK(clockwise);
            WALL_KICK(counterclockwise);
            WALL_KICK(opposite);
#undef WALL_KICK
        }
        return true;
    }

    int32_t TetrisContext::width() const
    {
        return width_;
    }

    int32_t TetrisContext::height() const
    {
        return height_;
    }

    uint32_t TetrisContext::full() const
    {
        return full_;
    }

    size_t TetrisContext::type_max() const
    {
        return type_max_;
    }

    size_t TetrisContext::node_max() const
    {
        return node_index_.size();
    }

    size_t TetrisContext::convert(char type) const
    {
        return type_to_index_[int(type) + 128];
    }

    char TetrisContext::convert(size_t index) const
    {
        return index_to_type_[index];
    }

    TetrisOpertion TetrisContext::get_opertion(char t, unsigned char r) const
    {
        auto find = opertion_.find(std::make_pair(t, r));
        if(find == opertion_.end())
        {
            TetrisOpertion empty = {};
            return empty;
        }
        else
        {
            return find->second;
        }
    }

    TetrisNodeBlockLocate const *TetrisContext::get_block(char t, unsigned char r) const
    {
        return &node_block_[convert(t) * 4 + r];
    }

    TetrisNode const *TetrisContext::get(TetrisBlockStatus const &status) const
    {
        auto find = node_index_.find(status);
        if(find == node_index_.end())
        {
            return nullptr;
        }
        else
        {
            return find->second;
        }
    }

    TetrisNode const *TetrisContext::get(char t, int8_t x, int8_t y, uint8_t r) const
    {
        TetrisBlockStatus status =
        {
            t, x, y, r
        };
        return get(status);
    }

    TetrisNode const *TetrisContext::generate(char type) const
    {
        return generate_cache_[convert(type)];
    }

    TetrisNode const *TetrisContext::generate(size_t index) const
    {
        return generate_cache_[index];
    }

    TetrisNode const *TetrisContext::generate() const
    {
        if(type_max_ == 7)
        {
            size_t index;
            do
            {
                index = ege::mtirand() & 7;
            }
            while(index >= 7);
            return generate(index);
        }
        else
        {
            return generate(static_cast<size_t>(ege::mtdrand() * type_max_));
        }
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
        while(new_node.status.y < status.y)
        {
            if(!m_tetris_rule_tools::move_up(new_node, this))
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
    TetrisNode create_node(size_t w, size_t h, char T, int8_t X, int8_t Y, uint8_t R, uint32_t line1, uint32_t line2, uint32_t line3, uint32_t line4, TetrisOpertion const &op)
    {
        assert(X < 0 || X >= 4 || Y < 0 || Y >= 4 || (line1 || line2 || line3 || line3));
        TetrisBlockStatus status(T, X, int8_t(h - Y - 1), R);
        TetrisNode node =
        {
            status, op, {line4, line3, line2, line1}, {}, {}, char(h - 4), char(4), char(0), char(4)
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
            if(y == 0)
            {
                node.top[x - node.col] = 0;
            }
            else
            {
                node.top[x - node.col] = node.row + y;
            }
            for(y = 0; y < node.height; ++y)
            {
                if((node.data[y] >> x) & 1)
                {
                    break;
                }
            }
            if(y == node.height)
            {
                node.bottom[x - node.col] = max_height;
            }
            else
            {
                node.bottom[x - node.col] = node.row + y;
            }
        }
        return node;
    }

    bool rotate_default(TetrisNode &node, unsigned char R, TetrisContext const *context)
    {
        TetrisBlockStatus status =
        {
            node.status.t, node.status.x, node.status.y, R
        };
        return context->create(status, node);
    }

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

    bool move_up(TetrisNode &node, TetrisContext const *context)
    {
        if(node.row + node.height == max_height)
        {
            return false;
        }
        for(int x = 0; x < node.width; ++x)
        {
            if(node.top[x] != 0)
            {
                ++node.top[x];
            }
            if(node.bottom[x] != max_height)
            {
                ++node.bottom[x];
            }
        }
        ++node.row;
        ++node.status.y;
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
            if(node.top[x] != 0)
            {
                --node.top[x];
            }
            if(node.bottom[x] != max_height)
            {
                --node.bottom[x];
            }
        }
        --node.row;
        --node.status.y;
        return true;
    }

}
