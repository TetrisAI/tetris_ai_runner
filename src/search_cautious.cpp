
#pragma once

#include "search_cautious.h"

using namespace m_tetris;

namespace search_cautious
{
    void Search::init(m_tetris::TetrisContext const *context, Config const *config)
    {
        config_ = config;
        node_mark_.init(context->node_max());
        node_mark_filtered_.init(context->node_max());
    }

    std::vector<char> Search::make_path(TetrisNode const *node, TetrisNode const *land_point, TetrisMap const &map)
    {
        if(node->index_filtered == land_point->index_filtered)
        {
            return std::vector<char>();
        }
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
        node_mark_.clear();
        node_search_.clear();
        node_search_.push_back(node);
        node_mark_.set(node, nullptr, '\0');
        size_t cache_index = 0;
        do
        {
            for(size_t max_index = node_search_.size(); cache_index < max_index; ++cache_index)
            {
                TetrisNode const *node = node_search_[cache_index];
                //l
                if(node->move_left && node_mark_.set(node->move_left, node, 'l') && node->move_left->check(map))
                {
                    if(node->move_left->drop(map)->index_filtered == index)
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
                    if(node->move_right->drop(map)->index_filtered == index)
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
                        if(node_L->drop(map)->index_filtered == index)
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
                        if(node_R->drop(map)->index_filtered == index)
                        {
                            return build_path(node_R, node_mark_);
                        }
                        else
                        {
                            node_search_.push_back(node_R);
                        }
                    }
                }
                //x
                if(node->rotate_opposite && node_mark_.set(node->rotate_opposite, node, 'x') && node->rotate_opposite->check(map))
                {
                    if(node->rotate_opposite->drop(map)->index_filtered == index)
                    {
                        return build_path(node->rotate_opposite, node_mark_);
                    }
                    else
                    {
                        node_search_.push_back(node->rotate_opposite);
                    }
                }
                //z
                if(node->rotate_counterclockwise && node_mark_.set(node->rotate_counterclockwise, node, 'z') && node->rotate_counterclockwise->check(map))
                {
                    if(node->rotate_counterclockwise->drop(map)->index_filtered == index)
                    {
                        return build_path(node->rotate_counterclockwise, node_mark_);
                    }
                    else
                    {
                        node_search_.push_back(node->rotate_counterclockwise);
                    }
                }
                //c
                if(node->rotate_clockwise && node_mark_.set(node->rotate_clockwise, node, 'c') && node->rotate_clockwise->check(map))
                {
                    if(node->rotate_clockwise->drop(map)->index_filtered == index)
                    {
                        return build_path(node->rotate_clockwise, node_mark_);
                    }
                    else
                    {
                        node_search_.push_back(node->rotate_clockwise);
                    }
                }
            }
        }
        while(node_search_.size() > cache_index);
        node_mark_.clear();
        node_search_.clear();
        node_search_.push_back(node);
        node_mark_.set(node, nullptr, '\0');
        cache_index = 0;
        do
        {
            for(size_t max_index = node_search_.size(); cache_index < max_index; ++cache_index)
            {
                TetrisNode const *node = node_search_[cache_index];
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
                //x
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
                if(config_->fast_move_down)
                {
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
                else
                {
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
        }
        while(node_search_.size() > cache_index);
        return std::vector<char>();
    }

    std::vector<TetrisNode const *> const *Search::search(TetrisMap const &map, TetrisNode const *node)
    {
        land_point_cache_.clear();
        if(!node->check(map))
        {
            return &land_point_cache_;
        }
        node_mark_.clear();
        node_mark_filtered_.clear();
        node_search_.clear();
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
                if(config_->fast_move_down)
                {
                    //D
                    TetrisNode const *node_drop = node->drop(map);
                    if(node_mark_.mark(node_drop))
                    {
                        node_search_.push_back(node_drop);
                    }
                }
                else
                {
                    //d
                    if(node->move_down && node_mark_.mark(node->move_down) && node->move_down->check(map))
                    {
                        node_search_.push_back(node->move_down);
                    }
                }
            }
        }
        while(node_search_.size() > cache_index);
        return &land_point_cache_;
    }
}