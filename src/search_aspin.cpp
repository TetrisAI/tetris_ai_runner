#include "search_aspin.h"
#include "integer_utils.h"

using namespace m_tetris;

namespace search_aspin
{
    void Search::init(TetrisContext const *context, Config const *config)
    {
        context_ = context;
        config_ = config;
        node_mark_.init(context->node_max());
        node_mark_filtered_.init(context->node_max());
    }

    std::vector<char> Search::make_path(TetrisNode const *node, TetrisNodeWithASpinType const &land_point, TetrisMap const &map)
    {
        //if (land_point.type != None)
        //{
        //    printf("A-SPIN %d\n", land_point.type);
        //}
        const int index = land_point->index_filtered;
        if (config_->is_20g)
        {
            return make_path_20g(node, land_point, map);
        }
        if (node->index_filtered == index)
        {
            return std::vector<char>();
        }
        bool allow_180 = config_->allow_180;
        bool allow_LR = config_->allow_LR;
        bool allow_D = config_->allow_D;
        auto build_path = [&land_point, &map, allow_180, this](TetrisNode const *node, decltype(node_mark_) &node_mark)->std::vector<char>
        {
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
            return path;
        };
        bool disable_d = land_point.type == None && node->land_point != nullptr && node->low >= map.roof && land_point->open(map);
        while (true)
        {
            node_mark_.clear();
            node_search_.clear();
            node_search_.push_back(node);
            node_mark_.set(node, nullptr, '\0');
            if (node->index_filtered == index)
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
                        if (config_->allow_rotate_move)
                        {
                            TetrisNode const *node_left = node->move_left;
                            //X
                            if (allow_180 && node_left->rotate_opposite && node_mark_.set(node_left->rotate_opposite, node_left, 'X') && node_left->rotate_opposite->check(map))
                            {
                                if (node_left->rotate_opposite->index_filtered == index)
                                {
                                    return build_path(node_left->rotate_opposite, node_mark_);
                                }
                                else
                                {
                                    node_search_.push_back(node_left->rotate_opposite);
                                }
                            }
                            //Z
                            if (node_left->rotate_counterclockwise && node_mark_.set(node_left->rotate_counterclockwise, node_left, 'Z') && node_left->rotate_counterclockwise->check(map))
                            {
                                if (node_left->rotate_counterclockwise->index_filtered == index)
                                {
                                    return build_path(node_left->rotate_counterclockwise, node_mark_);
                                }
                                else
                                {
                                    node_search_.push_back(node_left->rotate_counterclockwise);
                                }
                            }
                            //C
                            if (node_left->rotate_clockwise && node_mark_.set(node_left->rotate_clockwise, node_left, 'C') && node_left->rotate_clockwise->check(map))
                            {
                                if (node_left->rotate_clockwise->index_filtered == index)
                                {
                                    return build_path(node_left->rotate_clockwise, node_mark_);
                                }
                                else
                                {
                                    node_search_.push_back(node_left->rotate_clockwise);
                                }
                            }
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
                        if (config_->allow_rotate_move)
                        {
                            TetrisNode const *node_right = node->move_right;
                            //X
                            if (allow_180 && node_right->rotate_opposite && node_mark_.set(node_right->rotate_opposite, node_right, 'X') && node_right->rotate_opposite->check(map))
                            {
                                if (node_right->rotate_opposite->index_filtered == index)
                                {
                                    return build_path(node_right->rotate_opposite, node_mark_);
                                }
                                else
                                {
                                    node_search_.push_back(node_right->rotate_opposite);
                                }
                            }
                            //Z
                            if (node_right->rotate_counterclockwise && node_mark_.set(node_right->rotate_counterclockwise, node_right, 'Z') && node_right->rotate_counterclockwise->check(map))
                            {
                                if (node_right->rotate_counterclockwise->index_filtered == index)
                                {
                                    return build_path(node_right->rotate_counterclockwise, node_mark_);
                                }
                                else
                                {
                                    node_search_.push_back(node_right->rotate_counterclockwise);
                                }
                            }
                            //C
                            if (node_right->rotate_clockwise && node_mark_.set(node_right->rotate_clockwise, node_right, 'C') && node_right->rotate_clockwise->check(map))
                            {
                                if (node_right->rotate_clockwise->index_filtered == index)
                                {
                                    return build_path(node_right->rotate_clockwise, node_mark_);
                                }
                                else
                                {
                                    node_search_.push_back(node_right->rotate_clockwise);
                                }
                            }
                        }
                    }
                    //L
                    if (allow_LR && node->move_left && node->move_left->check(map))
                    {
                        TetrisNode const* node_L = node->move_left;
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
                    if (allow_LR && node->move_right && node->move_right->check(map))
                    {
                        TetrisNode const* node_R = node->move_right;
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
                        if (config_->allow_d)
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
                                if (allow_D) {
                                    TetrisNode const* node_D = node->drop(map);
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
                        else
                        {
                            //D
                            if (allow_D) {
                                TetrisNode const* node_D = node->drop(map);
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
        search(map, node, 0);
        return build_path(land_point, node_mark_);
    }

    std::vector<Search::TetrisNodeWithASpinType> const *Search::search(TetrisMap const &map, TetrisNode const *node, size_t depth)
    {
        land_point_cache_.clear();
        if (!node->check(map))
        {
          return &land_point_cache_;
        }
        bool allow_180 = config_->allow_180;
        bool is_20g = config_->is_20g;
        TetrisMapSnap snap;
        node->build_snap(map, context_, snap);
        node_mark_.clear();
        node_mark_.set(node, nullptr, '\0');
        if (is_20g)
        {
            node = node->drop(map);
        }
        else
        {
            while (node->low >= map.roof && node->move_down && node->move_down->check(snap))
            {
                node_mark_.set(node->move_down, node, 'd');
                node = node->move_down;
            }
        }
        node_mark_filtered_.clear();
        node_search_.clear();
        node_search_.push_back(node);
        size_t cache_index = 0;
        do
        {
            for (size_t max_index = node_search_.size(); cache_index < max_index; ++cache_index)
            {
                node = node_search_[cache_index];
                if (is_20g)
                {
                    node = node->drop(map);
                }
                if (!node->move_down || !node->move_down->check(snap))
                {
                    if (node_mark_filtered_.mark(node))
                    {
                        TetrisNodeWithASpinType node_ex(node);
                        if ((!node->move_down || !node->move_down->check(snap)) && (!node->move_up || !node->move_up->check(snap)) && (!node->move_left || !node->move_left->check(snap)) && (!node->move_right || !node->move_right->check(snap)))
                        {
                            node_ex.type = ASpin;
                        }
                        land_point_cache_.push_back(node_ex);
                    }
                }
                //d
                if (node->move_down && node_mark_.set(node->move_down, node, 'd') && node->move_down->check(snap))
                {
                    node_search_.push_back(node->move_down);
                }
                //l
                if (node->move_left && node_mark_.set(node->move_left, node, 'l') && node->move_left->check(snap))
                {
                    node_search_.push_back(node->move_left);
                }
                //r
                if (node->move_right && node_mark_.set(node->move_right, node, 'r') && node->move_right->check(snap))
                {
                    node_search_.push_back(node->move_right);
                }
                if (allow_180)
                {
                    //x
                    for (TetrisNode const *wall_kick_node : node->wall_kick_opposite)
                    {
                        if (wall_kick_node)
                        {
                            if (wall_kick_node->check(snap))
                            {
                                if (node_mark_.set(wall_kick_node, node, 'x'))
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
                for (TetrisNode const *wall_kick_node : node->wall_kick_counterclockwise)
                {
                    if (wall_kick_node)
                    {
                        if (wall_kick_node->check(snap))
                        {
                            if (node_mark_.set(wall_kick_node, node, 'z'))
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
                for (TetrisNode const *wall_kick_node : node->wall_kick_clockwise)
                {
                    if (wall_kick_node)
                    {
                        if (wall_kick_node->check(snap))
                        {
                            if (node_mark_.set(wall_kick_node, node, 'c'))
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
        } while (node_search_.size() > cache_index);
        return &land_point_cache_;
    }


    std::vector<char> Search::make_path_20g(TetrisNode const *node, TetrisNodeWithASpinType const &land_point, TetrisMap const &map)
    {
        node = node->drop(map);
        const int index = land_point->index_filtered;
        if (node->index_filtered == index)
        {
            return std::vector<char>();
        }
        bool allow_180 = config_->allow_180;
        bool allow_LR = config_->allow_LR;
        bool allow_D = config_->allow_D;
        auto build_path = [&land_point, &map, allow_180, this](TetrisNode const *node, decltype(node_mark_) &node_mark)->std::vector<char>
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
            return path;
        };
        node_mark_.clear();
        node_search_.clear();
        node_search_.push_back(node);
        node_mark_.set(node, nullptr, '\0');
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
                if (allow_LR && node->move_left && node->move_left->check(map))
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
                if (allow_LR && node->move_right && node->move_right->check(map))
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
}
