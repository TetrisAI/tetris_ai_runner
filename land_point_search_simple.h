
#pragma once

#include "tetris_core.h"
#include <vector>

namespace land_point_search_simple
{
    template<class TetrisAI>
    struct Simple
    {
        std::vector<m_tetris::TetrisNode const *> land_point_cache;
        std::vector<m_tetris::EvalData<typename TetrisAI::LandPointEval, typename TetrisAI::MapEval>> land_point_data;
        std::vector<typename TetrisAI::MapEval> virtual_pieve_result;
        std::vector<m_tetris::PruneParam<typename TetrisAI::MapEval>> prune_param;

        std::vector<char> make_path(m_tetris::TetrisContext const *context, m_tetris::TetrisNode const *node, m_tetris::TetrisNode const *land_point, m_tetris::TetrisMap const &map)
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

        std::pair<m_tetris::TetrisNode const *, typename TetrisAI::MapEval> do_ai(m_tetris::TetrisContext const *context, TetrisAI const &ai, m_tetris::TetrisMap const &map, m_tetris::TetrisNode const *node, std::vector<m_tetris::EvalParam<typename TetrisAI::LandPointEval>> &history, unsigned char next[], size_t next_length)
        {
            if(node == nullptr || !node->check(map))
            {
                return std::make_pair(nullptr, std::numeric_limits<int>::min());
            }
            std::vector<m_tetris::TetrisNode const *> const *land_point = nullptr;
            if(node->low >= map.roof)
            {
                land_point = node->place;
            }
            else
            {
                land_point_cache.clear();
                m_tetris::TetrisNode const *rotate = node;
                do
                {
                    land_point_cache.push_back(rotate);
                    m_tetris::TetrisNode const *left = rotate->move_left;
                    while(left != nullptr && left->check(map))
                    {
                        land_point_cache.push_back(left);
                        left = left->move_left;
                    }
                    m_tetris::TetrisNode const *right = rotate->move_right;
                    while(right != nullptr && right->check(map))
                    {
                        land_point_cache.push_back(right);
                        right = right->move_right;
                    }
                    rotate = rotate->rotate_counterclockwise;
                } while(rotate != nullptr  && rotate != node && rotate->check(map));
                land_point = &land_point_cache;
            }
            if(next_length == 0)
            {
                m_tetris::TetrisMap copy = map;
                TetrisAI::LandPointEval land_point_eval;
                history.push_back(m_tetris::EvalParam<typename TetrisAI::LandPointEval>(nullptr, 0, map, land_point_eval));
                m_tetris::TetrisNode const *beat_node = land_point->front()->drop(map);
                history.back().node = beat_node;
                size_t clear = history.back().clear = beat_node->attach(copy);
                ai.eval_land_point(beat_node, copy, clear, land_point_eval);
                TetrisAI::MapEval result;
                ai.eval_map(copy, history.data(), history.size(), result);
                for(auto cit = land_point->begin() + 1; cit != land_point->end(); ++cit)
                {
                    m_tetris::TetrisMap copy = map;
                    history.back().node = node = (*cit)->drop(map);
                    clear = history.back().clear = node->attach(copy);
                    ai.eval_land_point(beat_node, copy, clear, land_point_eval);
                    TetrisAI::MapEval new_result;
                    ai.eval_map(copy, history.data(), history.size(), new_result);
                    if(ai.map_eval_greater(new_result, result))
                    {
                        result = new_result;
                        beat_node = node;
                    }
                }
                history.pop_back();
                return std::make_pair(beat_node, result);
            }
            else
            {
                if(TetrisAI::pruning)
                {
                    land_point_data.resize(land_point->size());
                    size_t index = 0;
                    for(auto cit = land_point->begin(); cit != land_point->end(); ++cit, ++index)
                    {
                        node = (*cit)->drop(map);
                        auto &data = land_point_data[index];
                        data.map = map;
                        data.node = node;
                        data.clear = node->attach(data.map);
                        ai.eval_land_point(node, data.map, data.clear, data.land_point_eval);
                        history.push_back(m_tetris::EvalParam<typename TetrisAI::MapEval>(node, data.clear, data.map, data.land_point_eval));
                        ai.eval_map(data.map, history.data(), history.size(), data.map_eval);
                        history.pop_back();
                    }
                    //¼ôÖ¦
                    auto *data = &land_point_data.front();
                    m_tetris::TetrisNode const *beat_node = data->node->drop(map);
                    TetrisAI::MapEval result;
                    for(auto cit = land_point_data.begin() + 1; cit != land_point_data.end(); ++cit)
                    {
                        data = &*cit;
                        TetrisAI::MapEval new_result;
                        history.push_back(m_tetris::EvalParam<typename TetrisAI::MapEval>(data->node, data->clear, data->map, data->land_point_eval));
                        if(*next == ' ')
                        {
                            virtual_pieve_result.resize(context->type_max());
                            for(size_t ti = 0; ti < context->type_max(); ++ti)
                            {
                                virtual_pieve_result[ti] = do_ai(context, ai, data->map, context->generate(ti), history, next + 1, next_length - 1).second;
                            }
                            new_result = ai.get_vritual_eval(virtual_pieve_result.data(), virtual_pieve_result.size());
                        }
                        else
                        {
                            new_result = do_ai(context, ai, data->map, context->generate(*next), history, next + 1, next_length - 1).second;
                        }
                        history.pop_back();
                        if(ai.map_eval_greater(new_result, result))
                        {
                            result = new_result;
                            beat_node = node;
                        }
                    }
                    return std::make_pair(beat_node, result);
                }
                else
                {
                    //TODO
                }
            }
        }
    };
}
