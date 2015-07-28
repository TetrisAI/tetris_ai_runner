
#pragma once

#include "search_tspin.h"
#include "integer_utils.h"

using namespace m_tetris;

namespace search_tspin
{
    void Search::init(TetrisContext const *context, Config const *config)
    {
        context_ = context;
        config_ = config;
        node_mark_.init(context->node_max());
        node_mark_filtered_.init(context->node_max());
        block_data_ = block_data_buffer_ + 10;
        ::memset(block_data_buffer_, 0, sizeof block_data_buffer_);
        TetrisNode const *node = context->generate('T');
        if(node != nullptr)
        {
            int bottom = node->row, top = node->row + node->height, left = node->col, right = node->col + node->width;
            TetrisNode const *rotate = node->rotate_clockwise != nullptr ? node->rotate_clockwise : node->rotate_counterclockwise;
            if(rotate != nullptr)
            {
                bottom = std::min<int>(bottom, rotate->row);
                top = std::max<int>(top, rotate->row + rotate->height);
                left = std::min<int>(left, rotate->col);
                right = std::max<int>(right, rotate->col + rotate->width);
                x_diff_ = (right + left) / 2 - node->status.x;
                y_diff_ = (top + bottom) / 2 - node->status.y;
                for(int x = 1; x < context->width() - 1; ++x)
                {
                    block_data_[x - x_diff_] = (1 << (x - 1)) | (1 << (x + 1));
                }
                block_data_[0 - x_diff_] = (1 << 1);
                block_data_[context->width() - 1 - x_diff_] = (1 << (context->width() - 2));
            }
        }
    }

    std::vector<char> Search::make_path(TetrisNode const *node, TetrisNodeWithTSpinType const &land_point, TetrisMap const &map)
    {
        if(node->index_filtered == land_point->index_filtered)
        {
            return std::vector<char>();
        }
        if(land_point.type == TSpinMini)
        {
            return make_path_t(node, land_point, map);
        }
        bool allow_180 = config_->allow_180;
        node_mark_.clear();
        node_search_.clear();
        const int index = land_point->index_filtered;
        auto build_path = [](TetrisNode const *node, decltype(node_mark_) &node_mark)->std::vector < char >
        {
            std::vector<char> path;
            while(true)
            {
                auto result = node_mark.get(node);
                node = result.first;
                if(node == nullptr)
                {
                    break;
                }
                path.push_back(result.second);
            }
            std::reverse(path.begin(), path.end());
            return path;
        };
        node_search_.push_back(node);
        node_mark_.set(node, nullptr, '\0');
        size_t cache_index = 0;
        do
        {
            for(size_t max_index = node_search_.size(); cache_index < max_index; ++cache_index)
            {
                TetrisNode const *node = node_search_[cache_index];
                //x
                if(allow_180)
                {
                    if(node->rotate_opposite && node->rotate_opposite->check(map))
                    {
                        if(node_mark_.set(node->rotate_opposite, node, 'x'))
                        {
                            if(node->rotate_opposite->index_filtered == index)
                            {
                                return build_path(node->rotate_opposite, node_mark_);
                            }
                            else
                            {
                                node_search_.push_back(node->rotate_opposite);
                            }
                        }
                    }
                    else
                    {
                        for(TetrisNode const *wall_kick_node : node->wall_kick_opposite)
                        {
                            if(wall_kick_node)
                            {
                                if(wall_kick_node->check(map))
                                {
                                    if(node_mark_.set(wall_kick_node, node, 'x'))
                                    {
                                        if(wall_kick_node->index_filtered == index)
                                        {
                                            return build_path(wall_kick_node, node_mark_);
                                        }
                                        else
                                        {
                                            node_search_.push_back(wall_kick_node);
                                        }
                                    }
                                    break;
                                }
                            }
                            else
                            {
                                break;
                            }
                        }
                    }
                }
                //z
                if(node->rotate_counterclockwise && node->rotate_counterclockwise->check(map))
                {
                    if(node_mark_.set(node->rotate_counterclockwise, node, 'z'))
                    {
                        if(node->rotate_counterclockwise->index_filtered == index)
                        {
                            return build_path(node->rotate_counterclockwise, node_mark_);
                        }
                        else
                        {
                            node_search_.push_back(node->rotate_counterclockwise);
                        }
                    }
                }
                else
                {
                    for(TetrisNode const *wall_kick_node : node->wall_kick_counterclockwise)
                    {
                        if(wall_kick_node)
                        {
                            if(wall_kick_node->check(map))
                            {
                                if(node_mark_.set(wall_kick_node, node, 'z'))
                                {
                                    if(wall_kick_node->index_filtered == index)
                                    {
                                        return build_path(wall_kick_node, node_mark_);
                                    }
                                    else
                                    {
                                        node_search_.push_back(wall_kick_node);
                                    }
                                }
                                break;
                            }
                        }
                        else
                        {
                            break;
                        }
                    }
                }
                //c
                if(node->rotate_clockwise && node->rotate_clockwise->check(map))
                {
                    if(node_mark_.set(node->rotate_clockwise, node, 'c'))
                    {
                        if(node->rotate_clockwise->index_filtered == index)
                        {
                            return build_path(node->rotate_clockwise, node_mark_);
                        }
                        else
                        {
                            node_search_.push_back(node->rotate_clockwise);
                        }
                    }
                }
                else
                {
                    for(TetrisNode const *wall_kick_node : node->wall_kick_clockwise)
                    {
                        if(wall_kick_node)
                        {
                            if(wall_kick_node->check(map))
                            {
                                if(node_mark_.set(wall_kick_node, node, 'c'))
                                {
                                    if(wall_kick_node->index_filtered == index)
                                    {
                                        return build_path(wall_kick_node, node_mark_);
                                    }
                                    else
                                    {
                                        node_search_.push_back(wall_kick_node);
                                    }
                                }
                                break;
                            }
                        }
                        else
                        {
                            break;
                        }
                    }
                }
                //l
                if(node->move_left && node_mark_.set(node->move_left, node, 'l') && node->move_left->check(map))
                {
                    if(node->move_left->index_filtered == index)
                    {
                        return build_path(node->move_left, node_mark_);
                    }
                    else
                    {
                        node_search_.push_back(node->move_left);
                    }
                }
                //r
                if(node->move_right && node_mark_.set(node->move_right, node, 'r') && node->move_right->check(map))
                {
                    if(node->move_right->index_filtered == index)
                    {
                        return build_path(node->move_right, node_mark_);
                    }
                    else
                    {
                        node_search_.push_back(node->move_right);
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
                    if(node_mark_.set(node_L, node, 'L'))
                    {
                        if(node_L->index_filtered == index)
                        {
                            return build_path(node_L, node_mark_);
                        }
                        else
                        {
                            node_search_.push_back(node_L);
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
                    if(node_mark_.set(node_R, node, 'R'))
                    {
                        if(node_R->index_filtered == index)
                        {
                            return build_path(node_R, node_mark_);
                        }
                        else
                        {
                            node_search_.push_back(node_R);
                        }
                    }
                }
                //d
                if(node->move_down && node_mark_.set(node->move_down, node, 'd') && node->move_down->check(map))
                {
                    if(node->move_down->index_filtered == index)
                    {
                        return build_path(node->move_down, node_mark_);
                    }
                    else
                    {
                        node_search_.push_back(node->move_down);
                    }
                    //D
                    TetrisNode const *node_D = node->drop(map);
                    if(node_mark_.set(node_D, node, 'D'))
                    {
                        if(node_D->index_filtered == index)
                        {
                            return build_path(node_D, node_mark_);
                        }
                        else
                        {
                            node_search_.push_back(node_D);
                        }
                    }
                }
            }
        }
        while(node_search_.size() > cache_index);
        return std::vector<char>();
    }

    std::vector<Search::TetrisNodeWithTSpinType> const *Search::search(TetrisMap const &map, TetrisNode const *node)
    {
        land_point_cache_.clear();
        if(!node->check(map))
        {
            return &land_point_cache_;
        }
        bool allow_180 = config_->allow_180;
        node_mark_.clear();
        node_mark_filtered_.clear();
        node_search_.clear();
        if(node->status.t == 'T')
        {
            return search_t(map, node);
        }
        if(node->land_point != nullptr && node->low >= map.roof)
        {
            for(auto cit = node->land_point->begin(); cit != node->land_point->end(); ++cit)
            {
                TetrisNode const *drop_node = (*cit)->drop(map);
                if(node_mark_filtered_.mark(drop_node))
                {
                    land_point_cache_.push_back(drop_node);
                }
                node_search_.push_back(drop_node);
            }
            TetrisNode const *last_node = nullptr;
            size_t cache_index = node_search_.size();
            for(size_t i = 0; i != cache_index; ++i)
            {
                node = node_search_[i];
                if(last_node != nullptr)
                {
                    if(last_node->status.r == node->status.r && std::abs(last_node->status.x - node->status.x) == 1 && std::abs(last_node->status.y - node->status.y) > 1)
                    {
                        if(last_node->status.y > node->status.y)
                        {
                            TetrisNode const *check_node = (last_node->*(last_node->status.x > node->status.x ? &TetrisNode::move_left : &TetrisNode::move_right))->move_down->move_down;
                            if(node_mark_.mark(check_node))
                            {
                                node_search_.push_back(check_node);
                            }
                        }
                        else
                        {
                            TetrisNode const *check_node = (node->*(node->status.x > last_node->status.x ? &TetrisNode::move_left : &TetrisNode::move_right))->move_down->move_down;
                            if(node_mark_.mark(check_node))
                            {
                                node_search_.push_back(check_node);
                            }
                        }
                    }
                }
                last_node = node;
            }
            do
            {
                for(size_t max_index = node_search_.size(); cache_index < max_index; ++cache_index)
                {
                    node = node_search_[cache_index];
                    if(!node->open(map) && (!node->move_down || !node->move_down->check(map)))
                    {
                        if(node_mark_filtered_.mark(node))
                        {
                            land_point_cache_.push_back(node);
                        }
                    }
                    if(allow_180)
                    {
                        //x
                        if(node->rotate_opposite && node->rotate_opposite->check(map))
                        {
                            if(node_mark_.mark(node->rotate_opposite))
                            {
                                node_search_.push_back(node->rotate_opposite);
                            }
                        }
                        else
                        {
                            for(TetrisNode const *wall_kick_node : node->wall_kick_opposite)
                            {
                                if(wall_kick_node)
                                {
                                    if(wall_kick_node->check(map))
                                    {
                                        if(node_mark_.mark(wall_kick_node) && !wall_kick_node->open(map))
                                        {
                                            node_search_.push_back(wall_kick_node);
                                        }
                                        break;
                                    }
                                }
                                else
                                {
                                    break;
                                }
                            }
                        }
                    }
                    //z
                    if(node->rotate_counterclockwise && node->rotate_counterclockwise->check(map))
                    {
                        if(node_mark_.mark(node->rotate_counterclockwise))
                        {
                            node_search_.push_back(node->rotate_counterclockwise);
                        }
                    }
                    else
                    {
                        for(TetrisNode const *wall_kick_node : node->wall_kick_counterclockwise)
                        {
                            if(wall_kick_node)
                            {
                                if(wall_kick_node->check(map))
                                {
                                    if(node_mark_.mark(wall_kick_node) && !wall_kick_node->open(map))
                                    {
                                        node_search_.push_back(wall_kick_node);
                                    }
                                    break;
                                }
                            }
                            else
                            {
                                break;
                            }
                        }
                    }
                    //c
                    if(node->rotate_clockwise && node->rotate_clockwise->check(map))
                    {
                        if(node_mark_.mark(node->rotate_clockwise))
                        {
                            node_search_.push_back(node->rotate_clockwise);
                        }
                    }
                    else
                    {
                        for(TetrisNode const *wall_kick_node : node->wall_kick_clockwise)
                        {
                            if(wall_kick_node)
                            {
                                if(wall_kick_node->check(map))
                                {
                                    if(node_mark_.mark(wall_kick_node) && !wall_kick_node->open(map))
                                    {
                                        node_search_.push_back(wall_kick_node);
                                    }
                                    break;
                                }
                            }
                            else
                            {
                                break;
                            }
                        }
                    }
                    //l
                    if(node->move_left && node_mark_.mark(node->move_left) && !node->move_left->open(map) && node->move_left->check(map))
                    {
                        node_search_.push_back(node->move_left);
                    }
                    //r
                    if(node->move_right && node_mark_.mark(node->move_right) && !node->move_right->open(map) && node->move_right->check(map))
                    {
                        node_search_.push_back(node->move_right);
                    }
                    //d
                    if(node->move_down && node_mark_.mark(node->move_down) && node->move_down->check(map))
                    {
                        node_search_.push_back(node->move_down);
                    }
                }
            }
            while(node_search_.size() > cache_index);
        }
        else
        {
            node_search_.push_back(node);
            node_mark_.mark(node);
            size_t cache_index = 0;
            do
            {
                for(size_t max_index = node_search_.size(); cache_index < max_index; ++cache_index)
                {
                    node = node_search_[cache_index];
                    if(!node->move_down || !node->move_down->check(map))
                    {
                        if(node_mark_filtered_.mark(node))
                        {
                            land_point_cache_.push_back(node);
                        }
                    }
                    if(allow_180)
                    {
                        //x
                        if(node->rotate_opposite && node->rotate_opposite->check(map))
                        {
                            if(node_mark_.mark(node->rotate_opposite))
                            {
                                node_search_.push_back(node->rotate_opposite);
                            }
                        }
                        else
                        {
                            for(TetrisNode const *wall_kick_node : node->wall_kick_opposite)
                            {
                                if(wall_kick_node)
                                {
                                    if(wall_kick_node->check(map))
                                    {
                                        if(node_mark_.mark(wall_kick_node))
                                        {
                                            node_search_.push_back(wall_kick_node);
                                        }
                                        break;
                                    }
                                }
                                else
                                {
                                    break;
                                }
                            }
                        }
                    }
                    //z
                    if(node->rotate_counterclockwise && node->rotate_counterclockwise->check(map))
                    {
                        if(node_mark_.mark(node->rotate_counterclockwise))
                        {
                            node_search_.push_back(node->rotate_counterclockwise);
                        }
                    }
                    else
                    {
                        for(TetrisNode const *wall_kick_node : node->wall_kick_counterclockwise)
                        {
                            if(wall_kick_node)
                            {
                                if(wall_kick_node->check(map))
                                {
                                    if(node_mark_.mark(wall_kick_node))
                                    {
                                        node_search_.push_back(wall_kick_node);
                                    }
                                    break;
                                }
                            }
                            else
                            {
                                break;
                            }
                        }
                    }
                    //c
                    if(node->rotate_clockwise && node->rotate_clockwise->check(map))
                    {
                        if(node_mark_.mark(node->rotate_clockwise))
                        {
                            node_search_.push_back(node->rotate_clockwise);
                        }
                    }
                    else
                    {
                        for(TetrisNode const *wall_kick_node : node->wall_kick_clockwise)
                        {
                            if(wall_kick_node)
                            {
                                if(wall_kick_node->check(map))
                                {
                                    if(node_mark_.mark(wall_kick_node))
                                    {
                                        node_search_.push_back(wall_kick_node);
                                    }
                                    break;
                                }
                            }
                            else
                            {
                                break;
                            }
                        }
                    }
                    //l
                    if(node->move_left && node_mark_.mark(node->move_left) && node->move_left->check(map))
                    {
                        node_search_.push_back(node->move_left);
                    }
                    //r
                    if(node->move_right && node_mark_.mark(node->move_right) && node->move_right->check(map))
                    {
                        node_search_.push_back(node->move_right);
                    }
                    //d
                    if(node->move_down && node_mark_.mark(node->move_down) && node->move_down->check(map))
                    {
                        node_search_.push_back(node->move_down);
                    }
                }
            }
            while(node_search_.size() > cache_index);
        }
        return &land_point_cache_;
    }

    std::vector<char> Search::make_path_t(TetrisNode const *node, TetrisNodeWithTSpinType const &land_point, TetrisMap const &map)
    {
        bool allow_180 = config_->allow_180;
        node_mark_.clear();
        node_search_.clear();
        const int index = land_point.last->index;
        auto build_path = [&land_point, &map, allow_180](TetrisNode const *node, decltype(node_mark_) &node_mark)->std::vector<char>
        {
            std::vector<char> path;
            while(true)
            {
                auto result = node_mark.get(node);
                node = result.first;
                if(node == nullptr)
                {
                    break;
                }
                path.push_back(result.second);
            }
            std::reverse(path.begin(), path.end());
            TetrisNode const *last = land_point.last;
            node = land_point.node;
            if(allow_180)
            {
                //x
                for(TetrisNode const *wall_kick_node : last->wall_kick_opposite)
                {
                    if(wall_kick_node)
                    {
                        if(wall_kick_node->check(map))
                        {
                            if(wall_kick_node == node)
                            {
                                path.push_back('x');
                                return path;
                            }
                        }
                    }
                    else
                    {
                        break;
                    }
                }
            }
            //z
            for(TetrisNode const *wall_kick_node : last->wall_kick_counterclockwise)
            {
                if(wall_kick_node)
                {
                    if(wall_kick_node->check(map))
                    {
                        if(wall_kick_node == node)
                        {
                            path.push_back('z');
                            return path;
                        }
                    }
                }
                else
                {
                    break;
                }
            }
            //c
            for(TetrisNode const *wall_kick_node : last->wall_kick_clockwise)
            {
                if(wall_kick_node)
                {
                    if(wall_kick_node->check(map))
                    {
                        if(wall_kick_node == node)
                        {
                            path.push_back('c');
                            return path;
                        }
                    }
                }
                else
                {
                    break;
                }
            }
            return path;
        };
        node_search_.push_back(node);
        node_mark_.set(node, nullptr, '\0');
        size_t cache_index = 0;
        do
        {
            for(size_t max_index = node_search_.size(); cache_index < max_index; ++cache_index)
            {
                TetrisNode const *node = node_search_[cache_index];
                if(allow_180)
                {
                    //x
                    if(node->rotate_opposite && node->rotate_opposite->check(map))
                    {
                        if(node_mark_.set(node->rotate_opposite, node, 'x'))
                        {
                            if(node->rotate_opposite->index == index)
                            {
                                return build_path(node->rotate_opposite, node_mark_);
                            }
                            else
                            {
                                node_search_.push_back(node->rotate_opposite);
                            }
                        }
                    }
                    else
                    {
                        for(TetrisNode const *wall_kick_node : node->wall_kick_opposite)
                        {
                            if(wall_kick_node)
                            {
                                if(wall_kick_node->check(map))
                                {
                                    if(node_mark_.set(wall_kick_node, node, 'x'))
                                    {
                                        if(wall_kick_node->index == index)
                                        {
                                            return build_path(wall_kick_node, node_mark_);
                                        }
                                        else
                                        {
                                            node_search_.push_back(wall_kick_node);
                                        }
                                    }
                                    break;
                                }
                            }
                            else
                            {
                                break;
                            }
                        }
                    }
                }
                //z
                if(node->rotate_counterclockwise && node->rotate_counterclockwise->check(map))
                {
                    if(node_mark_.set(node->rotate_counterclockwise, node, 'z'))
                    {
                        if(node->rotate_counterclockwise->index == index)
                        {
                            return build_path(node->rotate_counterclockwise, node_mark_);
                        }
                        else
                        {
                            node_search_.push_back(node->rotate_counterclockwise);
                        }
                    }
                }
                else
                {
                    for(TetrisNode const *wall_kick_node : node->wall_kick_counterclockwise)
                    {
                        if(wall_kick_node)
                        {
                            if(wall_kick_node->check(map))
                            {
                                if(node_mark_.set(wall_kick_node, node, 'z'))
                                {
                                    if(wall_kick_node->index == index)
                                    {
                                        return build_path(wall_kick_node, node_mark_);
                                    }
                                    else
                                    {
                                        node_search_.push_back(wall_kick_node);
                                    }
                                }
                                break;
                            }
                        }
                        else
                        {
                            break;
                        }
                    }
                }
                //c
                if(node->rotate_clockwise && node->rotate_clockwise->check(map))
                {
                    if(node_mark_.set(node->rotate_clockwise, node, 'c'))
                    {
                        if(node->rotate_clockwise->index == index)
                        {
                            return build_path(node->rotate_clockwise, node_mark_);
                        }
                        else
                        {
                            node_search_.push_back(node->rotate_clockwise);
                        }
                    }
                }
                else
                {
                    for(TetrisNode const *wall_kick_node : node->wall_kick_clockwise)
                    {
                        if(wall_kick_node)
                        {
                            if(wall_kick_node->check(map))
                            {
                                if(node_mark_.set(wall_kick_node, node, 'c'))
                                {
                                    if(wall_kick_node->index == index)
                                    {
                                        return build_path(wall_kick_node, node_mark_);
                                    }
                                    else
                                    {
                                        node_search_.push_back(wall_kick_node);
                                    }
                                }
                                break;
                            }
                        }
                        else
                        {
                            break;
                        }
                    }
                }
                //l
                if(node->move_left && node_mark_.set(node->move_left, node, 'l') && node->move_left->check(map))
                {
                    if(node->move_left->index == index)
                    {
                        return build_path(node->move_left, node_mark_);
                    }
                    else
                    {
                        node_search_.push_back(node->move_left);
                    }
                }
                //r
                if(node->move_right && node_mark_.set(node->move_right, node, 'r') && node->move_right->check(map))
                {
                    if(node->move_right->index == index)
                    {
                        return build_path(node->move_right, node_mark_);
                    }
                    else
                    {
                        node_search_.push_back(node->move_right);
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
                    if(node_mark_.set(node_L, node, 'L'))
                    {
                        if(node_L->index == index)
                        {
                            return build_path(node_L, node_mark_);
                        }
                        else
                        {
                            node_search_.push_back(node_L);
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
                    if(node_mark_.set(node_R, node, 'R'))
                    {
                        if(node_R->index == index)
                        {
                            return build_path(node_R, node_mark_);
                        }
                        else
                        {
                            node_search_.push_back(node_R);
                        }
                    }
                }
                //d
                if(node->move_down && node_mark_.set(node->move_down, node, 'd') && node->move_down->check(map))
                {
                    if(node->move_down->index == index)
                    {
                        return build_path(node->move_down, node_mark_);
                    }
                    else
                    {
                        node_search_.push_back(node->move_down);
                    }
                    //D
                    TetrisNode const *node_D = node->drop(map);
                    if(node_mark_.set(node_D, node, 'D'))
                    {
                        if(node_D->index == index)
                        {
                            return build_path(node_D, node_mark_);
                        }
                        else
                        {
                            node_search_.push_back(node_D);
                        }
                    }
                }
            }
        }
        while(node_search_.size() > cache_index);
        return std::vector<char>();
    }

    std::vector<Search::TetrisNodeWithTSpinType> const *Search::search_t(TetrisMap const &map, TetrisNode const *node)
    {
        TetrisMapSnap snap;
        node->build_snap(map, context_, snap);
        bool allow_180 = config_->allow_180;
        node_search_.push_back(node);
        node_mark_.mark(node);
        node_incomplete_.clear();
        size_t cache_index = 0;
        do
        {
            for(size_t max_index = node_search_.size(); cache_index < max_index; ++cache_index)
            {
                node = node_search_[cache_index];
                if(!node->move_down || !node->move_down->check(snap))
                {
                    if(node_mark_filtered_.mark(node))
                    {
                        node_incomplete_.push_back(node);
                    }
                }
                //d
                if(node->move_down && node_mark_.set(node->move_down, node, ' ') && node->move_down->check(snap))
                {
                    node_search_.push_back(node->move_down);
                }
                //l
                if(node->move_left && node_mark_.set(node->move_left, node, ' ') && node->move_left->check(snap))
                {
                    node_search_.push_back(node->move_left);
                }
                //r
                if(node->move_right && node_mark_.set(node->move_right, node, ' ') && node->move_right->check(snap))
                {
                    node_search_.push_back(node->move_right);
                }
                if(allow_180)
                {
                    //x
                    if(node->rotate_opposite && node->rotate_opposite->check(snap))
                    {
                        if(node_mark_.cover(node->rotate_opposite, node, 'x'))
                        {
                            node_search_.push_back(node->rotate_opposite);
                        }
                    }
                    else
                    {
                        for(TetrisNode const *wall_kick_node : node->wall_kick_opposite)
                        {
                            if(wall_kick_node)
                            {
                                if(wall_kick_node->check(snap))
                                {
                                    if(node_mark_.cover(wall_kick_node, node, 'x'))
                                    {
                                        node_search_.push_back(wall_kick_node);
                                    }
                                    break;
                                }
                            }
                            else
                            {
                                break;
                            }
                        }
                    }
                }
                //z
                if(node->rotate_counterclockwise && node->rotate_counterclockwise->check(snap))
                {
                    if(node_mark_.cover(node->rotate_counterclockwise, node, 'z'))
                    {
                        node_search_.push_back(node->rotate_counterclockwise);
                    }
                }
                else
                {
                    for(TetrisNode const *wall_kick_node : node->wall_kick_counterclockwise)
                    {
                        if(wall_kick_node)
                        {
                            if(wall_kick_node->check(snap))
                            {
                                if(node_mark_.cover(wall_kick_node, node, 'z'))
                                {
                                    node_search_.push_back(wall_kick_node);
                                }
                                break;
                            }
                        }
                        else
                        {
                            break;
                        }
                    }
                }
                //c
                if(node->rotate_clockwise && node->rotate_clockwise->check(snap))
                {
                    if(node_mark_.cover(node->rotate_clockwise, node, 'c'))
                    {
                        node_search_.push_back(node->rotate_clockwise);
                    }
                }
                else
                {
                    for(TetrisNode const *wall_kick_node : node->wall_kick_clockwise)
                    {
                        if(wall_kick_node)
                        {
                            if(wall_kick_node->check(snap))
                            {
                                if(node_mark_.cover(wall_kick_node, node, 'c'))
                                {
                                    node_search_.push_back(wall_kick_node);
                                }
                                break;
                            }
                        }
                        else
                        {
                            break;
                        }
                    }
                }
            }
        }
        while(node_search_.size() > cache_index);
        for(auto const &node : node_incomplete_)
        {
            auto last = node_mark_.get(node);
            TetrisNodeWithTSpinType node_ex(node);
            node_ex.last = last.first;
            node_ex.is_check = true;
            node_ex.is_last_rotate = last.second != ' ';
            node_ex.is_ready = check_ready(map, node);
            node_ex.is_mini_ready = check_mini_ready(snap, node_ex);
            land_point_cache_.push_back(node_ex);
        }
        return &land_point_cache_;
    }

    bool Search::check_ready(TetrisMap const &map, TetrisNode const *node)
    {
        int count;
        int y = node->status.y + y_diff_;
        int row = block_data_[node->status.x];
        if(y == 0)
        {
            return zzz::BitCount(map.row[1] & row) + 2 >= 3;
        }
        else
        {
            int x = node->status.x + x_diff_;
            if(x == 0 || x == map.width - 1)
            {
                count = 2;
            }
            else
            {
                count = 0;
            }
            return zzz::BitCount(map.row[y - 1] & row) + zzz::BitCount(map.row[y + 1] & row) + count >= 3;
        }
    }

    bool Search::check_mini_ready(TetrisMapSnap const &snap, TetrisNodeWithTSpinType const &node)
    {
        return node.is_ready && !(node->rotate_opposite && node->rotate_opposite->check(snap) || node->rotate_counterclockwise && node->rotate_counterclockwise->check(snap) || node->rotate_clockwise && node->rotate_clockwise->check(snap));
    }
}