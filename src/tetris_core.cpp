
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

namespace m_tetris2
{

    void TetrisNode::build_snap(TetrisMap const &map, TetrisContext const *context, TetrisMapSnap &snap) const
    {
        row_t el = map.empty_line();
        for (int r = 0; r < 4; ++r)
        {
            auto block = context->get_block(status.t, r);
            if (block->count == 0)
            {
                memset(snap.row[r], 0, sizeof snap.row[r]);
            }
            else
            {
                memset(&snap.row[r][map.height], 0, sizeof(row_t) * (max_height - map.height));
                uint32_t i = 0;
                if (block->data[0].x == 0 && block->data[0].y == 0)
                {
                    for (int y = 0, ey = map.height; y < ey; ++y)
                    {
                        snap.row[r][y] = map.row[y];
                    }
                    i = 1;
                }
                else
                {
                    for (int y = 0, ey = map.height; y < ey; ++y)
                    {
                        snap.row[r][y] = el;
                    }
                }
                for (; i < block->count; ++i)
                {
                    int bx = block->data[i].x, by = block->data[i].y;
                    for (int y = 0, ey = map.height - by; y < ey; ++y)
                    {
                        snap.row[r][y] &= map.row[y + by] >> bx;
                    }
                    for (int y = map.height - by; y < map.height; ++y)
                    {
                        snap.row[r][y] = 0;
                    }
                }
            }
        }
    }

    size_t TetrisNode::attach(TetrisContext const *context, TetrisMap &map) const
    {
        switch (height)
        {
        case 4:
            map.row[row + 3] &= ~data[3];
        case 3:
            map.row[row + 2] &= ~data[2];
        case 2:
            map.row[row + 1] &= ~data[1];
        case 1:
            map.row[row] &= ~data[0];
        }
        int clear = 0;
        for (int i = height; i > 0; --i)
        {
            if (map.row[row + i - 1] == context->full())
            {
                memmove(&map.row[row + i - 1], &map.row[row + i], (map.height - i) * sizeof(int));
                map.row[map.height - 1] = map.empty_line();
                ++clear;
            }
        }
        switch (width)
        {
        case 4:
            if (top[3] > map.top[col + 3])
            {
                map.top[col + 3] = top[3];
                map.roof = std::max(top[3], map.roof);
            }
        case 3:
            if (top[2] > map.top[col + 2])
            {
                map.top[col + 2] = top[2];
                map.roof = std::max(top[2], map.roof);
            }
        case 2:
            if (top[1] > map.top[col + 1])
            {
                map.top[col + 1] = top[1];
                map.roof = std::max(top[1], map.roof);
            }
        case 1:
            if (top[0] > map.top[col])
            {
                map.top[col] = top[0];
                map.roof = std::max(top[0], map.roof);
            }
        }
        map.roof -= clear;
        map.count += 4 - clear * map.width;
        if (clear > 0)
        {
            for (int x = 0; x < map.width; ++x)
            {
                map.top[x] = 0;
                for (int y = map.roof - 1; y >= 0; --y)
                {
                    if (map.full(x, y))
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
        for (int i = 0; i < height; ++i)
        {
            if (map.row[row + i] == context->full())
            {
                return row + i;
            }
        }
        return -1;
    }

    int TetrisNode::clear_high(TetrisContext const *context, TetrisMap &map) const
    {
        for (int i = height; i > 0; --i)
        {
            if (map.row[row + i - 1] == context->full())
            {
                return row + i - 1;
            }
        }
        return -1;
    }

    TetrisNode const *TetrisNode::drop(TetrisMap const &map) const
    {
        int value = bottom[0] - map.top[col];
        if (width > 1)
        {
            value = std::min<int>(value, bottom[1] - map.top[col + 1]);
            if (width > 2)
            {
                value = std::min<int>(value, bottom[2] - map.top[col + 2]);
                if (width > 3)
                {
                    value = std::min<int>(value, bottom[3] - map.top[col + 3]);
                }
            }
        }
        if (value >= 0)
        {
            return move_down_multi[value];
        }
        else
        {
            TetrisNode const *node = this;
            while (node->move_down != nullptr && node->move_down->check(map))
            {
                node = node->move_down;
            }
            return node;
        }
    }

    template<bool Filtered>
    void TetrisNodeMarkTemplate<Filtered>::init(size_t size)
    {
        version_ = 0;
        data_.clear();
        data_.resize(size);
    }

    template<bool Filtered>
    void TetrisNodeMarkTemplate<Filtered>::clear()
    {
        if (++version_ == std::numeric_limits<size_t>::max())
        {
            version_ = 1;
            for (auto it = data_.begin(); it != data_.end(); ++it)
            {
                it->version = 0;
            }
        }
    }

    template<bool Filtered>
    std::pair<TetrisNode const *, char> TetrisNodeMarkTemplate<Filtered>::get(size_t index)
    {
        Mark &mark = data_[index];
        return mark.version == version_ ? mark.data : std::pair<TetrisNode const *, char>{nullptr, ' '};
    }

    template<bool Filtered>
    std::pair<TetrisNode const *, char> TetrisNodeMarkTemplate<Filtered>::get(TetrisNode const *key)
    {
        Mark &mark = data_[Filtered ? key->index_filtered : key->index];
        return mark.version == version_ ? mark.data : std::pair<TetrisNode const *, char>{nullptr, ' '};
    }

    template<bool Filtered>
    bool TetrisNodeMarkTemplate<Filtered>::set(TetrisNode const *key, TetrisNode const *node, char op)
    {
        Mark &mark = data_[Filtered ? key->index_filtered : key->index];
        if (mark.version == version_)
        {
            return false;
        }
        mark.version = version_;
        mark.data.first = node;
        mark.data.second = op;
        return true;
    }

    template<bool Filtered>
    bool TetrisNodeMarkTemplate<Filtered>::cover_if(TetrisNode const *key, TetrisNode const *node, char ck, char op)
    {
        Mark &mark = data_[Filtered ? key->index_filtered : key->index];
        if (mark.version == version_ && mark.data.second != ck)
        {
            return false;
        }
        mark.version = version_;
        mark.data.first = node;
        mark.data.second = op;
        return true;
    }

    template<bool Filtered>
    bool TetrisNodeMarkTemplate<Filtered>::mark(TetrisNode const *key)
    {
        Mark &mark = data_[Filtered ? key->index_filtered : key->index];
        if (mark.version == version_)
        {
            return false;
        }
        mark.version = version_;
        return true;
    }

    TetrisNode const *TetrisContext::generate(char type) const
    {
        auto find = opertion_.find(std::make_pair(type, static_cast<unsigned char>(0)));
        if (find == opertion_.end())
        {
            return nullptr;
        }

        thread_local TetrisNode node;
        node = find->second.create(width_, height_, find->second);

        int const key = int(type) + 128;
        int dx = int(spawn_x_[key]) - int(node.status.x);
        while (dx > 0)
        {
            if (!m_tetris2_rule_tools::move_right(node, this))
            {
                return nullptr;
            }
            --dx;
        }
        while (dx < 0)
        {
            if (!m_tetris2_rule_tools::move_left(node, this))
            {
                return nullptr;
            }
            ++dx;
        }

        int dy = int(spawn_y_[key]) - int(node.status.y);
        while (dy > 0)
        {
            if (!m_tetris2_rule_tools::move_up(node, this))
            {
                return nullptr;
            }
            --dy;
        }
        while (dy < 0)
        {
            if (!m_tetris2_rule_tools::move_down(node, this))
            {
                return nullptr;
            }
            ++dy;
        }
        return &node;
    }

    size_t TetrisContext::type_max() const
    {
        return type_max_;
    }

    size_t TetrisContext::convert(char type) const
    {
        return type_to_index_[int(type) + 128];
    }

    char TetrisContext::convert(size_t index) const
    {
        return index_to_type_[index];
    }

    TetrisOpertion const &TetrisContext::get_opertion(char t, unsigned char r) const
    {
        auto find = opertion_.find(std::make_pair(t, r));
        assert(find != opertion_.end());
        return find->second;
    }

    TetrisNodeBlockLocate const *TetrisContext::get_block(char t, unsigned char r) const
    {
        thread_local TetrisNodeBlockLocate block;
        block.count = 0;
        TetrisOpertion const &op = get_opertion(t, r);
        TetrisNode node = op.create(width_, height_, op);
        for (int x = node.col; x < node.col + node.width; ++x)
        {
            for (int y = 0; y < node.height; ++y)
            {
                if ((node.data[y] >> x) & row_t(1))
                {
                    auto &b = block.data[block.count++];
                    b.x = static_cast<uint32_t>(x - node.col);
                    b.y = static_cast<uint32_t>(y);
                }
            }
        }
        return &block;
    }

    template class TetrisNodeMarkTemplate<true>;
    template class TetrisNodeMarkTemplate<false>;
}

namespace m_tetris2_rule_tools
{
    TetrisNode create_node(size_t w, size_t h, char T, int8_t X, int8_t Y, uint8_t R, uint32_t line1, uint32_t line2, uint32_t line3, uint32_t line4, TetrisOpertion const &op)
    {
        (void)op;
        assert(X < 0 || X >= 4 || Y < 0 || Y >= 4 || (line1 || line2 || line3 || line3));
        TetrisBlockStatus status(T, X, int8_t(h - Y - 1), R);
        TetrisNode node =
            {
                status, {line4, line3, line2, line1}, {}, {}, char(h - 4), char(4), char(0), char(4)};
        while (node.data[0] == 0)
        {
            ++node.row;
            --node.height;
            memmove(&node.data[0], &node.data[1], node.height * sizeof(node.data[0]));
            node.data[node.height] = 0;
        }
        while (node.data[node.height - 1] == 0)
        {
            --node.height;
        }
        while (!((node.data[0] >> node.col) & 1) && !((node.data[1] >> node.col) & 1) && !((node.data[2] >> node.col) & 1) && !((node.data[3] >> node.col) & 1))
        {
            ++node.col;
            --node.width;
        }
        while (!((node.data[0] >> (node.col + node.width - 1)) & 1) && !((node.data[1] >> (node.col + node.width - 1)) & 1) && !((node.data[2] >> (node.col + node.width - 1)) & 1) && !((node.data[3] >> (node.col + node.width - 1)) & 1))
        {
            --node.width;
        }
        for (int x = node.col; x < node.col + node.width; ++x)
        {
            int y;
            for (y = node.height; y > 0; --y)
            {
                if ((node.data[y - 1] >> x) & 1)
                {
                    break;
                }
            }
            if (y == 0)
            {
                node.top[x - node.col] = 0;
            }
            else
            {
                node.top[x - node.col] = node.row + y;
            }
            for (y = 0; y < node.height; ++y)
            {
                if ((node.data[y] >> x) & 1)
                {
                    break;
                }
            }
            if (y == node.height)
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
                node.status.t, node.status.x, node.status.y, R};
        TetrisOpertion const &op = context->get_opertion(status.t, R);
        TetrisNode rotated = op.create(context->width(), context->height(), op);
        node = rotated;

        int dx = int(status.x) - int(node.status.x);
        while (dx > 0)
        {
            if (!move_right(node, context))
            {
                return false;
            }
            --dx;
        }
        while (dx < 0)
        {
            if (!move_left(node, context))
            {
                return false;
            }
            ++dx;
        }

        int dy = int(status.y) - int(node.status.y);
        while (dy > 0)
        {
            if (!move_up(node, context))
            {
                return false;
            }
            --dy;
        }
        while (dy < 0)
        {
            if (!move_down(node, context))
            {
                return false;
            }
            ++dy;
        }
        return true;
    }

    bool move_left(TetrisNode &node, TetrisContext const *context)
    {
        if (((node.data[0] | node.data[1] | node.data[2] | node.data[3]) & 1) != 0)
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
        //C-A2: width 上限提升至 64; check mask 走 row_t 算术, 避免 1<<width 的 UB.
        row_t check = context->width() >= int(sizeof(row_t) * 8) ? row_t(row_t(1) << (sizeof(row_t) * 8 - 1)) : row_t(row_t(1) << (context->width() - 1));
        if (((node.data[0] | node.data[1] | node.data[2] | node.data[3]) & check) != 0)
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
        if (node.row + node.height == max_height)
        {
            return false;
        }
        for (int x = 0; x < node.width; ++x)
        {
            if (node.top[x] != 0)
            {
                ++node.top[x];
            }
            if (node.bottom[x] != max_height)
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
        if (node.row == 0)
        {
            return false;
        }
        for (int x = 0; x < node.width; ++x)
        {
            if (node.top[x] != 0)
            {
                --node.top[x];
            }
            if (node.bottom[x] != max_height)
            {
                --node.bottom[x];
            }
        }
        --node.row;
        --node.status.y;
        return true;
    }
}
