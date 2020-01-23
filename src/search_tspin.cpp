
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
        std::memset(block_data_buffer_, 0, sizeof block_data_buffer_);
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
        //if (land_point.type != None)
        //{
        //    printf("T-SPIN %d\n", land_point.type);
        //}
        if (config_->is_20g)
        {
            return make_path_20g(node, land_point, map);
        }
        if(land_point.type == None && node->index_filtered == land_point->index_filtered)
        {
            return std::vector<char>();
        }
        bool allow_180 = config_->allow_180;
        const int index = land_point.type == None || land_point.last == nullptr ? land_point->index_filtered : land_point.last->index_filtered;
        auto build_path = [node, &land_point, &map, allow_180, this](TetrisNode const *node, decltype(node_mark_) &node_mark)->std::vector<char>
        {
            size_t node_index = node->index_filtered;
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
            if(node_index != land_point->index_filtered)
            {
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
            }
            return path;
        };
        bool disable_d = land_point.type == None && node->land_point != nullptr && node->low >= map.roof && land_point->open(map);
        while (true)
        {
            node_mark_.clear();
            node_search_.clear();
            node_search_.push_back(node);
            node_mark_.set(node, nullptr, '\0');
            if (node->index_filtered == index || (land_point.type != None && node->index_filtered == land_point->index_filtered && config_->last_rotate))
            {
                return build_path(node, node_mark_);
            }
            size_t cache_index = 0;
            do
            {
                for (size_t max_index = node_search_.size(); cache_index < max_index; ++cache_index)
                {
                    TetrisNode const *node = node_search_[cache_index];
                    if (disable_d)
                    {
                        //D
                        TetrisNode const *node_D = node->drop(map);
                        if (node_mark_.set(node_D, node, 'D') && node_D->index_filtered == index)
                        {
                            return build_path(node_D, node_mark_);
                        }
                    }
                    //x
                    if (allow_180)
                    {
                        for (TetrisNode const *wall_kick_node : node->wall_kick_opposite)
                        {
                            if (wall_kick_node)
                            {
                                if (wall_kick_node->check(map))
                                {
                                    if (node_mark_.set(wall_kick_node, node, 'x'))
                                    {
                                        if (wall_kick_node->index_filtered == index)
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
                    for (TetrisNode const *wall_kick_node : node->wall_kick_counterclockwise)
                    {
                        if (wall_kick_node)
                        {
                            if (wall_kick_node->check(map))
                            {
                                if (node_mark_.set(wall_kick_node, node, 'z'))
                                {
                                    if (wall_kick_node->index_filtered == index)
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
                    //c
                    for (TetrisNode const *wall_kick_node : node->wall_kick_clockwise)
                    {
                        if (wall_kick_node)
                        {
                            if (wall_kick_node->check(map))
                            {
                                if (node_mark_.set(wall_kick_node, node, 'c'))
                                {
                                    if (wall_kick_node->index_filtered == index)
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
                    //l
                    if (node->move_left && node_mark_.set(node->move_left, node, 'l') && node->move_left->check(map))
                    {
                        if (node->move_left->index_filtered == index)
                        {
                            return build_path(node->move_left, node_mark_);
                        }
                        else
                        {
                            node_search_.push_back(node->move_left);
                        }
                    }
                    //r
                    if (node->move_right && node_mark_.set(node->move_right, node, 'r') && node->move_right->check(map))
                    {
                        if (node->move_right->index_filtered == index)
                        {
                            return build_path(node->move_right, node_mark_);
                        }
                        else
                        {
                            node_search_.push_back(node->move_right);
                        }
                    }
                    //L
                    if (node->move_left && node->move_left->check(map))
                    {
                        TetrisNode const *node_L = node->move_left;
                        while (node_L->move_left && node_L->move_left->check(map))
                        {
                            node_L = node_L->move_left;
                        }
                        if (node_mark_.set(node_L, node, 'L'))
                        {
                            if (node_L->index_filtered == index)
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
                    if (node->move_right && node->move_right->check(map))
                    {
                        TetrisNode const *node_R = node->move_right;
                        while (node_R->move_right && node_R->move_right->check(map))
                        {
                            node_R = node_R->move_right;
                        }
                        if (node_mark_.set(node_R, node, 'R'))
                        {
                            if (node_R->index_filtered == index)
                            {
                                return build_path(node_R, node_mark_);
                            }
                            else
                            {
                                node_search_.push_back(node_R);
                            }
                        }
                    }
                    if (!disable_d)
                    {
                        //d
                        if (node->move_down && node_mark_.set(node->move_down, node, 'd') && node->move_down->check(map))
                        {
                            if (node->move_down->index_filtered == index)
                            {
                                return build_path(node->move_down, node_mark_);
                            }
                            else
                            {
                                node_search_.push_back(node->move_down);
                            }
                            //D
                            TetrisNode const *node_D = node->drop(map);
                            if (node_mark_.set(node_D, node, 'D'))
                            {
                                if (node_D->index_filtered == index)
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
            } while (node_search_.size() > cache_index);
            if (disable_d)
            {
                disable_d = false;
            }
            else
            {
                break;
            }
        }
        return std::vector<char>();
    }

    std::vector<Search::TetrisNodeWithTSpinType> const *Search::search(TetrisMap const &map, TetrisNode const *node, size_t depth)
    {
        land_point_cache_.clear();
        if(!node->check(map))
        {
            return &land_point_cache_;
        }
        bool allow_180 = config_->allow_180;
        bool is_20g = config_->is_20g;
        if (is_20g)
        {
            node = node->drop(map);
        }
        node_mark_.clear();
        node_mark_filtered_.clear();
        node_search_.clear();
        if(node->status.t == 'T')
        {
            return search_t(map, node, depth);
        }
        if(!is_20g && node->land_point != nullptr && node->low >= map.roof)
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
                    //z
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
                    //c
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
                    if (is_20g)
                    {
                        node = node->drop(map);
                    }
                    if (!node->move_down || !node->move_down->check(map))
                    {
                        if (node_mark_filtered_.mark(node))
                        {
                            land_point_cache_.push_back(node);
                        }
                    }
                    if(allow_180)
                    {
                        //x
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
                    //c
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


    std::vector<char> Search::make_path_20g(TetrisNode const *node, TetrisNodeWithTSpinType const &land_point, TetrisMap const &map)
    {
        node = node->drop(map);
        if (land_point.type == None && node->index_filtered == land_point->index_filtered)
        {
            return std::vector<char>();
        }
        bool allow_180 = config_->allow_180;
        const int index = land_point.type == None || land_point.last == nullptr ? land_point->index_filtered : land_point.last->index_filtered;
        auto build_path = [node, &land_point, &map, allow_180, this](TetrisNode const *node, decltype(node_mark_) &node_mark)->std::vector<char>
        {
            size_t node_index = node->index_filtered;
            std::vector<char> path;
            while (true)
            {
                auto result = node_mark.get(node);
                node = result.first;
                if (node == nullptr)
                {
                    break;
                }
                path.push_back(result.second);
            }
            std::reverse(path.begin(), path.end());
            if (node_index != land_point->index_filtered)
            {
                TetrisNode const *last = land_point.last;
                node = land_point.node;
                if (allow_180)
                {
                    //x
                    for (TetrisNode const *wall_kick_node : last->wall_kick_opposite)
                    {
                        if (wall_kick_node)
                        {
                            if (wall_kick_node->check(map))
                            {
                                wall_kick_node = wall_kick_node->drop(map);
                                if (wall_kick_node == node)
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
                for (TetrisNode const *wall_kick_node : last->wall_kick_counterclockwise)
                {
                    if (wall_kick_node)
                    {
                        if (wall_kick_node->check(map))
                        {
                            wall_kick_node = wall_kick_node->drop(map);
                            if (wall_kick_node == node)
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
                for (TetrisNode const *wall_kick_node : last->wall_kick_clockwise)
                {
                    if (wall_kick_node)
                    {
                        if (wall_kick_node->check(map))
                        {
                            wall_kick_node = wall_kick_node->drop(map);
                            if (wall_kick_node == node)
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
            }
            return path;
        };
        node_mark_.clear();
        node_search_.clear();
        node_search_.push_back(node);
        node_mark_.set(node, nullptr, '\0');
        if (node->index_filtered == index || (land_point.type != None && node->index_filtered == land_point->index_filtered && config_->last_rotate))
        {
            return build_path(node, node_mark_);
        }
        size_t cache_index = 0;
        do
        {
            for (size_t max_index = node_search_.size(); cache_index < max_index; ++cache_index)
            {
                TetrisNode const *node = node_search_[cache_index];
                TetrisNode const *node_test;
                assert(node == node->drop(map));
                //x
                if (allow_180)
                {
                    for (TetrisNode const *wall_kick_node : node->wall_kick_opposite)
                    {
                        if (wall_kick_node)
                        {
                            if (wall_kick_node->check(map))
                            {
                                wall_kick_node = wall_kick_node->drop(map);
                                if (node_mark_.set(wall_kick_node, node, 'x'))
                                {
                                    if (wall_kick_node->index_filtered == index)
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
                for (TetrisNode const *wall_kick_node : node->wall_kick_counterclockwise)
                {
                    if (wall_kick_node)
                    {
                        if (wall_kick_node->check(map))
                        {
                            wall_kick_node = wall_kick_node->drop(map);
                            if (node_mark_.set(wall_kick_node, node, 'z'))
                            {
                                if (wall_kick_node->index_filtered == index)
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
                //c
                for (TetrisNode const *wall_kick_node : node->wall_kick_clockwise)
                {
                    if (wall_kick_node)
                    {
                        if (wall_kick_node->check(map))
                        {
                            wall_kick_node = wall_kick_node->drop(map);
                            if (node_mark_.set(wall_kick_node, node, 'c'))
                            {
                                if (wall_kick_node->index_filtered == index)
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
                //l
                if ((node_test = node->move_left) && node_test->check(map) && (node_test = node_test->drop(map), node_mark_.set(node_test, node, 'l')))
                {
                    if (node_test->index_filtered == index)
                    {
                        return build_path(node_test, node_mark_);
                    }
                    else
                    {
                        node_search_.push_back(node_test);
                    }
                }
                //r
                if ((node_test = node->move_right) && node_test->check(map) && (node_test = node_test->drop(map), node_mark_.set(node_test, node, 'r')))
                {
                    TetrisNode const *move_right = node->move_right->drop(map);
                    if (move_right->index_filtered == index)
                    {
                        return build_path(move_right, node_mark_);
                    }
                    else
                    {
                        node_search_.push_back(move_right);
                    }
                }
                //L
                if (node->move_left && node->move_left->check(map))
                {
                    node_test = node->move_left->drop(map);
                    while (node_test->move_left && node_test->move_left->check(map))
                    {
                        node_test = node_test->move_left->drop(map);
                    }
                    if (node_mark_.set(node_test, node, 'L'))
                    {
                        if (node_test->index_filtered == index)
                        {
                            return build_path(node_test, node_mark_);
                        }
                        else
                        {
                            node_search_.push_back(node_test);
                        }
                    }
                }
                //R
                if (node->move_right && node->move_right->check(map))
                {
                    node_test = node->move_right->drop(map);
                    while (node_test->move_right && node_test->move_right->check(map))
                    {
                        node_test = node_test->move_right->drop(map);
                    }
                    if (node_mark_.set(node_test, node, 'R'))
                    {
                        if (node_test->index_filtered == index)
                        {
                            return build_path(node_test, node_mark_);
                        }
                        else
                        {
                            node_search_.push_back(node_test);
                        }
                    }
                }
            }
        } while (node_search_.size() > cache_index);
        return std::vector<char>();
    }

    std::vector<Search::TetrisNodeWithTSpinType> const *Search::search_t(TetrisMap const &map, TetrisNode const *node, size_t depth)
    {
        TetrisMapSnap snap;
        node->build_snap(map, context_, snap);
        bool allow_180 = config_->allow_180;
        bool is_20g = config_->is_20g;
        if (is_20g)
        {
            node = node->drop(map);
        }
        node_search_.push_back(node);
        node_mark_.mark(node);
        node_incomplete_.clear();
        size_t cache_index = 0;
        do
        {
            for(size_t max_index = node_search_.size(); cache_index < max_index; ++cache_index)
            {
                node = node_search_[cache_index];
                if (is_20g)
                {
                    node = node->drop(map);
                }
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
                    for(TetrisNode const *wall_kick_node : node->wall_kick_opposite)
                    {
                        if(wall_kick_node)
                        {
                            if(wall_kick_node->check(snap))
                            {
                                if(node_mark_.cover_if(wall_kick_node, node, ' ', 'x'))
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
                for(TetrisNode const *wall_kick_node : node->wall_kick_counterclockwise)
                {
                    if(wall_kick_node)
                    {
                        if(wall_kick_node->check(snap))
                        {
                            if(node_mark_.cover_if(wall_kick_node, node, ' ', 'z'))
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
                //c
                for(TetrisNode const *wall_kick_node : node->wall_kick_clockwise)
                {
                    if(wall_kick_node)
                    {
                        if(wall_kick_node->check(snap))
                        {
                            if(node_mark_.cover_if(wall_kick_node, node, ' ', 'c'))
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
        while(node_search_.size() > cache_index);
        for(auto const &node : node_incomplete_)
        {
            auto last = node_mark_.get(node);
            TetrisNodeWithTSpinType node_ex(node);
            node_ex.last = last.first;
            node_ex.is_check = true;
            node_ex.is_last_rotate = last.second != ' ' || (node_ex.last == nullptr && depth == 0 && config_->last_rotate);
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
            return ZZZ_BitCount(map.row[1] & row) + 2 >= 3;
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
            return ZZZ_BitCount(map.row[y - 1] & row) + ZZZ_BitCount(map.row[y + 1] & row) + count >= 3;
        }
    }

    bool Search::check_mini_ready(TetrisMapSnap const &snap, TetrisNodeWithTSpinType const &node)
    {
        return node.is_ready && !(node->rotate_opposite && node->rotate_opposite->check(snap) || node->rotate_counterclockwise && node->rotate_counterclockwise->check(snap) || node->rotate_clockwise && node->rotate_clockwise->check(snap));
    }
}