
#pragma once

#include "land_point_search_simple.h"

using namespace m_tetris;

namespace land_point_search_simple
{
    std::vector<char> Search::make_path(TetrisContext const *context, TetrisNode const *node, TetrisNode const *land_point, TetrisMap const &map)
    {
        std::vector<char> path;
        if(node->status.t != land_point->status.t || node->status.y < land_point->status.y)
        {
            return path;
        }
        while(node->status.r != land_point->status.r && node->rotate_counterclockwise && node->rotate_counterclockwise->check(map))
        {
            path.push_back('z');
            node = node->rotate_counterclockwise;
        }
        while(node->status.x < land_point->status.x && node->move_right && node->move_right->check(map))
        {
            path.push_back('r');
            node = node->move_right;
        }
        while(node->status.x > land_point->status.x && node->move_left && node->move_left->check(map))
        {
            path.push_back('l');
            node = node->move_left;
        }
        if(node->drop(map) != land_point)
        {
            return std::vector<char>();
        }
        path.push_back('\0');
        return path;
    }

    std::vector<TetrisNode const *> const *Search::search(TetrisMap const &map, TetrisNode const *node)
    {
        if(node->low >= map.roof)
        {
            return node->land_point;
        }
        else
        {
            land_point_cache.clear();
            TetrisNode const *rotate = node;
            do
            {
                land_point_cache.push_back(rotate);
                TetrisNode const *left = rotate->move_left;
                while(left != nullptr && left->check(map))
                {
                    land_point_cache.push_back(left);
                    left = left->move_left;
                }
                TetrisNode const *right = rotate->move_right;
                while(right != nullptr && right->check(map))
                {
                    land_point_cache.push_back(right);
                    right = right->move_right;
                }
                rotate = rotate->rotate_counterclockwise;
            } while(rotate != nullptr  && rotate != node && rotate->check(map));
            return &land_point_cache;
        }
    }
}