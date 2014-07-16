
#pragma once

#include "land_point_search_path.h"

using namespace m_tetris;

namespace land_point_search_path
{
    void Search::init(m_tetris::TetrisContext const *context)
    {
        node_mark.init(context->node_max());
    }

    std::vector<char> Search::make_path(TetrisContext const *context, TetrisNode const *node, TetrisNode const *land_point, TetrisMap const &map)
    {
        node_mark.clear();
        node_search.clear();

        auto build_path = [land_point](decltype(node_mark) &node_mark)->std::vector<char>
        {
            std::vector<char> path;
            TetrisNode const *node = land_point;
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
            while(!path.empty() && (path.back() == 'd' || path.back() == 'D'))
            {
                path.pop_back();
            }
            path.push_back('\0');
            return path;
        };
        node_search.push_back(node);
        node_mark.set(node, nullptr, '\0');
        size_t cache_index = 0;
        do
        {
            for(size_t max_index = node_search.size(); cache_index < max_index; ++cache_index)
            {
                TetrisNode const *node = node_search[cache_index];
                //x
                if(node->rotate_opposite && node_mark.set(node->rotate_opposite, node, 'x') && node->rotate_opposite->check(map))
                {
                    if(node->rotate_opposite == land_point)
                    {
                        return build_path(node_mark);
                    }
                    else
                    {
                        node_search.push_back(node->rotate_opposite);
                    }
                }
                //z
                if(node->rotate_counterclockwise && node_mark.set(node->rotate_counterclockwise, node, 'z') && node->rotate_counterclockwise->check(map))
                {
                    if(node->rotate_counterclockwise == land_point)
                    {
                        return build_path(node_mark);
                    }
                    else
                    {
                        node_search.push_back(node->rotate_counterclockwise);
                    }
                    //zz
                    TetrisNode const *node_r = node->rotate_counterclockwise;
                    if(node_r->rotate_counterclockwise && node_mark.set(node_r->rotate_counterclockwise, node_r, 'z') && node_r->rotate_counterclockwise->check(map))
                    {
                        if(node_r->rotate_counterclockwise == land_point)
                        {
                            return build_path(node_mark);
                        }
                        else
                        {
                            node_search.push_back(node_r->rotate_counterclockwise);
                        }
                    }
                }
                //c
                if(node->rotate_clockwise && node_mark.set(node->rotate_clockwise, node, 'c') && node->rotate_clockwise->check(map))
                {
                    if(node->rotate_clockwise == land_point)
                    {
                        return build_path(node_mark);
                    }
                    else
                    {
                        node_search.push_back(node->rotate_clockwise);
                    }
                    //cc
                    TetrisNode const *node_r = node->rotate_clockwise;
                    if(node_r->rotate_clockwise && node_mark.set(node_r->rotate_clockwise, node_r, 'c') && node_r->rotate_clockwise->check(map))
                    {
                        if(node_r->rotate_clockwise == land_point)
                        {
                            return build_path(node_mark);
                        }
                        else
                        {
                            node_search.push_back(node_r->rotate_clockwise);
                        }
                    }
                }
                //l
                if(node->move_left && node_mark.set(node->move_left, node, 'l') && node->move_left->check(map))
                {
                    if(node->move_left == land_point)
                    {
                        return build_path(node_mark);
                    }
                    else
                    {
                        node_search.push_back(node->move_left);
                    }
                }
                //r
                if(node->move_right && node_mark.set(node->move_right, node, 'r') && node->move_right->check(map))
                {
                    if(node->move_right == land_point)
                    {
                        return build_path(node_mark);
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
                    if(node_mark.set(node_L, node, 'L'))
                    {
                        if(node_L == land_point)
                        {
                            return build_path(node_mark);
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
                    if(node_mark.set(node_R, node, 'R'))
                    {
                        if(node_R == land_point)
                        {
                            return build_path(node_mark);
                        }
                        else
                        {
                            node_search.push_back(node_R);
                        }
                    }
                }
                //d
                if(node->move_down && node_mark.set(node->move_down, node, 'd') && node->move_down->check(map))
                {
                    if(node->move_down == land_point)
                    {
                        return build_path(node_mark);
                    }
                    else
                    {
                        node_search.push_back(node->move_down);
                    }
                    //D
                    TetrisNode const *node_D = node->drop(map);
                    if(node_mark.set(node_D, node, 'D'))
                    {
                        if(node_D == land_point)
                        {
                            return build_path(node_mark);
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

    std::vector<TetrisNode const *> const *Search::search(TetrisMap const &map, TetrisNode const *node)
    {
        node_mark.clear();
        node_search.clear();
        land_point_cache.clear();
        if(node->low >= map.roof)
        {
            for(auto cit = node->land_point->begin(); cit != node->land_point->end(); ++cit)
            {
                land_point_cache.push_back((*cit)->drop(map));
            }
            TetrisNode const *last_node = nullptr;
            for(auto cit = land_point_cache.begin(); cit != land_point_cache.end(); ++cit)
            {
                node = *cit;
                if(last_node != nullptr)
                {
                    if(last_node->status.r == node->status.r && std::abs(last_node->status.x - node->status.x) == 1 && std::abs(last_node->status.y - node->status.y) > 1)
                    {
                        if(last_node->status.y > node->status.y)
                        {
                            TetrisNode const *check_node = (last_node->*(last_node->status.x > node->status.x ? &TetrisNode::move_left : &TetrisNode::move_right))->move_down->move_down;
                            if(node_mark.mark(check_node))
                            {
                                node_search.push_back(check_node);
                            }
                        }
                        else
                        {
                            TetrisNode const *check_node = (node->*(node->status.x > last_node->status.x ? &TetrisNode::move_left : &TetrisNode::move_right))->move_down->move_down;
                            if(node_mark.mark(check_node))
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
                        land_point_cache.push_back(node);
                    }
                    //x
                    if(node->rotate_opposite && node_mark.mark(node->rotate_opposite) && !node->rotate_opposite->open(map) && node->rotate_opposite->check(map))
                    {
                        node_search.push_back(node->rotate_opposite);
                    }
                    //z
                    if(node->rotate_counterclockwise && node_mark.mark(node->rotate_counterclockwise) && !node->rotate_counterclockwise->open(map) && node->rotate_counterclockwise->check(map))
                    {
                        node_search.push_back(node->rotate_counterclockwise);
                    }
                    //c
                    if(node->rotate_clockwise && node_mark.mark(node->rotate_clockwise) && !node->rotate_clockwise->open(map) && node->rotate_clockwise->check(map))
                    {
                        node_search.push_back(node->rotate_clockwise);
                    }
                    //l
                    if(node->move_left && node_mark.mark(node->move_left) && !node->move_left->open(map) && node->move_left->check(map))
                    {
                        node_search.push_back(node->move_left);
                    }
                    //r
                    if(node->move_right && node_mark.mark(node->move_right) && !node->move_right->open(map) && node->move_right->check(map))
                    {
                        node_search.push_back(node->move_right);
                    }
                    //d
                    if(node->move_down && node_mark.mark(node->move_down) && node->move_down->check(map))
                    {
                        node_search.push_back(node->move_down);
                    }
                }
            } while(node_search.size() > cache_index);
        }
        else
        {
            node_search.push_back(node);
            node_mark.mark(node);
            size_t cache_index = 0;
            do
            {
                for(size_t max_index = node_search.size(); cache_index < max_index; ++cache_index)
                {
                    node = node_search[cache_index];
                    if(!node->move_down || !node->move_down->check(map))
                    {
                        land_point_cache.push_back(node);
                    }
                    //x
                    if(node->rotate_opposite && node_mark.mark(node->rotate_opposite) && node->rotate_opposite->check(map))
                    {
                        node_search.push_back(node->rotate_opposite);
                    }
                    //z
                    if(node->rotate_counterclockwise && node_mark.mark(node->rotate_counterclockwise) && node->rotate_counterclockwise->check(map))
                    {
                        node_search.push_back(node->rotate_counterclockwise);
                    }
                    //c
                    if(node->rotate_clockwise && node_mark.mark(node->rotate_clockwise) && node->rotate_clockwise->check(map))
                    {
                        node_search.push_back(node->rotate_clockwise);
                    }
                    //l
                    if(node->move_left && node_mark.mark(node->move_left) && node->move_left->check(map))
                    {
                        node_search.push_back(node->move_left);
                    }
                    //r
                    if(node->move_right && node_mark.mark(node->move_right) && node->move_right->check(map))
                    {
                        node_search.push_back(node->move_right);
                    }
                    //d
                    if(node->move_down && node_mark.mark(node->move_down) && node->move_down->check(map))
                    {
                        node_search.push_back(node->move_down);
                    }
                }
            } while(node_search.size() > cache_index);
        }
        return &land_point_cache;
    }
}