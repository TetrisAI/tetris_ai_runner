
//by ZouZhiZhang

#include "tetris_core.h"
#include "integer_utils.h"
#include "ai_tag.h"
#include <cstdint>

using namespace m_tetris;
using namespace zzz;


namespace ai_tag
{

    void the_ai_games::init(m_tetris::TetrisContext const *context, Param const *param)
    {
        context_ = context;
        param_ = param;
        map_danger_data_.resize(context->type_max());
        for(size_t i = 0; i < context->type_max(); ++i)
        {
            TetrisMap map =
            {
                context->width(), context->height()
            };
            TetrisNode const *node = context->generate(i);
            node->move_down->attach(map);
            memcpy(map_danger_data_[i].data, &map.row[map.height - 4], sizeof map_danger_data_[i].data);
            for(int y = 0; y < 3; ++y)
            {
                map_danger_data_[i].data[y + 1] |= map_danger_data_[i].data[y];
            }
        }
        col_mask_ = context->full() & ~1;
        row_mask_ = context->full();
    }

    std::string the_ai_games::ai_name() const
    {
        return "The AI Games (SetoSan) v0.1";
    }

    the_ai_games::eval_result the_ai_games::eval(TetrisNode const *node, TetrisMap const &map, TetrisMap const &src_map, size_t clear) const
    {
        double LandHeight = node->row + node->height;
        double Middle = std::abs((node->status.x + 1) * 2 - map.width);
        double EraseCount = clear;
        double BoardDeadZone = map_in_danger_(map);
        if(map.roof + param_->up >= map.height)
        {
            BoardDeadZone += 70;
        }

        const int width_m1 = map.width - 1;
        int ColTrans = 2 * (map.height - map.roof);
        int RowTrans = map.roof == map.height ? 0 : map.width;
        for(int y = 0; y < map.roof; ++y)
        {
            if(!map.full(0, y))
            {
                ++ColTrans;
            }
            if(!map.full(width_m1, y))
            {
                ++ColTrans;
            }
            ColTrans += BitCount((map.row[y] ^ (map.row[y] << 1)) & col_mask_);
            if(y != 0)
            {
                RowTrans += BitCount(map.row[y - 1] ^ map.row[y]);
            }
        }
        RowTrans += BitCount(row_mask_ & ~map.row[0]);
        RowTrans += BitCount(map.roof == map.height ? row_mask_ & ~map.row[map.roof - 1] : map.row[map.roof - 1]);
        struct
        {
            int HoleCount;
            int HoleLine;

            int WideWellDepth[6];
            int WellDepth[32];
            int WellDepthTotle;

            int LineCoverBits;
            int HoleBits0;
            int ClearWidth0;
            int HoleBits1;
            int ClearWidth1;
            int HoleBits2;
            int ClearWidth2;
        } v;
        memset(&v, 0, sizeof v);
        int HolePosy0 = -1;
        int HolePosy1 = -1;
        int HolePosy2 = -1;

        for(int y = map.roof - 1; y >= 0; --y)
        {
            v.LineCoverBits |= map.row[y];
            int LineHole = v.LineCoverBits ^ map.row[y];
            if(LineHole != 0)
            {
                v.HoleCount += BitCount(LineHole);
                v.HoleLine++;
                if(HolePosy0 == -1)
                {
                    HolePosy0 = y + 1;
                    v.HoleBits0 = LineHole;
                }
                else if(HolePosy1 == -1)
                {
                    HolePosy1 = y + 1;
                    v.HoleBits1 = LineHole;
                }
                else if(HolePosy2 == -1)
                {
                    HolePosy2 = y + 1;
                    v.HoleBits2 = LineHole;
                }
            }
            int WellWidth = 0;
            int MaxWellWidth = 0;
            for(int x = 0; x < map.width; ++x)
            {
                if((v.LineCoverBits >> x) & 1)
                {
                    if(WellWidth > MaxWellWidth)
                    {
                        MaxWellWidth = WellWidth;
                    }
                    WellWidth = 0;
                }
                else
                {
                    ++WellWidth;
                    if(x > 0 && x < width_m1)
                    {
                        if(((v.LineCoverBits >> (x - 1)) & 7) == 5)
                        {
                            v.WellDepthTotle += ++v.WellDepth[x];
                        }
                    }
                    else if(x == 0)
                    {
                        if((v.LineCoverBits & 3) == 2)
                        {
                            v.WellDepthTotle += ++v.WellDepth[0];
                        }
                    }
                    else
                    {
                        if(((v.LineCoverBits >> (width_m1 - 1)) & 3) == 1)
                        {
                            v.WellDepthTotle += ++v.WellDepth[width_m1];
                        }
                    }
                }
            }
            if(WellWidth > MaxWellWidth)
            {
                MaxWellWidth = WellWidth;
            }
            if(MaxWellWidth >= 1 && MaxWellWidth <= 6)
            {
                ++v.WideWellDepth[MaxWellWidth - 1];
            }
        }
        if(HolePosy0 >= 0)
        {
            for(int y = HolePosy0; y < map.roof; ++y)
            {
                int CheckLine = v.HoleBits0 & map.row[y];
                if(CheckLine == 0)
                {
                    break;
                }
                v.ClearWidth0 += (y + 1) * BitCount(CheckLine);
            }
            if(HolePosy1 >= 0)
            {
                for(int y = HolePosy1; y < map.roof; ++y)
                {
                    int CheckLine = v.HoleBits1 & map.row[y];
                    if(CheckLine == 0)
                    {
                        break;
                    }
                    v.ClearWidth1 += (y + 1) * BitCount(CheckLine);
                }
                if(HolePosy2 >= 0)
                {
                    for(int y = HolePosy2; y < map.roof; ++y)
                    {
                        int CheckLine = v.HoleBits2 & map.row[y];
                        if(CheckLine == 0)
                        {
                            break;
                        }
                        v.ClearWidth2 += (y + 1) * BitCount(CheckLine);
                    }
                }
            }
        }
        int low_x = 1;
        for(int x = 2; x < width_m1; ++x)
        {
            if(map.top[x] < map.top[low_x])
            {
                low_x = x;
            }
        }
        if(map.top[0] <= map.top[low_x])
        {
            low_x = 0;
        }
        if(map.top[width_m1] <= map.top[low_x])
        {
            low_x = width_m1;
        }
        int low_y = map.top[low_x];
        int full = 0;
        for(int y = map.roof - 1; y >= 0; --y)
        {
            if(map.row[y] == context_->full())
            {
                full = y;
                low_y -= y;
                break;
            }
        }

        eval_result result;
        result.land_point = (0.
                             - LandHeight * 1750 / map.height
                             + Middle * 2
                             + EraseCount * 60
                             - BoardDeadZone * 50000000
                             );
        result.map = (0.
                      - ColTrans * 80
                      - RowTrans * 80
                      - v.HoleCount * 120
                      - v.HoleLine * 380
                      - v.ClearWidth0 * 8
                      - v.ClearWidth1 * 4
                      - v.ClearWidth2 * 1
                      - v.WellDepthTotle * 160
                      + v.WideWellDepth[5] * 16
                      + v.WideWellDepth[4] * 32
                      + v.WideWellDepth[3] * 48
                      + v.WideWellDepth[2] * 8
                      + v.WideWellDepth[1] * 4
                      + (low_x == width_m1 ? 400 : 0)
                      );
        result.clear = clear;
        result.low_y = low_y;
        result.full = std::max(0, (map.height - 4) - (full + param_->up)) * map.width;
        result.count = map.count + v.HoleCount;
        result.save_map = &map;
        return result;
    }

    double the_ai_games::bad() const
    {
        return -999999999999;
    }

    double the_ai_games::get(eval_result const *history, size_t history_length) const
    {
        if(param_->virtual_length == 0 || history_length < param_->length)
        {
            return get_impl(history, history_length);
        }
        if(land_point_cache_.size() != param_->virtual_length)
        {
            land_point_cache_.resize(param_->virtual_length);
        }
        if(result_cache_.size() != param_->length + param_->virtual_length)
        {
            result_cache_.resize(param_->length + param_->virtual_length);
        }
        if(history_length == param_->length)
        {
            std::copy(history, history + history_length, result_cache_.begin());
            history = result_cache_.data();
        }
        eval_result &back_result = result_cache_[history_length];
        TetrisMap const *save_map = history[history_length - 1].save_map;
        double value = 0;
        std::vector<TetrisNode const *> &land_point = land_point_cache_[history_length - param_->length];
        for(size_t i = 0; i < context_->type_max(); ++i)
        {
            double best_eval = bad();
            param_->search(context_->generate(i), *save_map, land_point);
            for(auto const &node : land_point)
            {
                TetrisMap copy = *save_map;
                size_t clear = node->attach(copy);
                back_result = eval(node, copy, *save_map, clear);
                double new_eval;
                if(history_length + 1 < param_->length + param_->virtual_length)
                {
                    new_eval = get(history, history_length + 1);
                }
                else
                {
                    new_eval = get_impl(history, history_length + 1);
                }
                if(new_eval > best_eval)
                {
                    best_eval = new_eval;
                }
            }
            value += best_eval;
        }
        return value / context_->type_max();
    }

    double the_ai_games::get_impl(eval_result const *history, size_t history_length) const
    {
        double land_point_value = 0;
        size_t combo = param_->combo;
        for(size_t i = 0; i < history_length; ++i)
        {
            bool building = history[i].count * 3 / 2 < history[i].full;
            if(history[i].clear > 0)
            {
                if(history[i].clear == 4)
                {
                    land_point_value += 2000;
                }
                if(combo == 0 && building)
                {
                    land_point_value -= (4 - std::min(4, history[i].low_y)) * 800;
                }
                else if(combo > 0)
                {
                    land_point_value += combo * 1000;
                }
                ++combo;
            }
            else
            {
                if(combo > 0 && !building)
                {
                    land_point_value -= 3200;
                }
                combo = 0;
            }
            land_point_value += history[i].land_point;
        }
        return land_point_value / history_length + history[history_length - 1].map;
    }

    size_t the_ai_games::map_in_danger_(m_tetris::TetrisMap const &map) const
    {
        size_t danger = 0, up = param_->up;
        do
        {
            size_t height = map.height - up;
            for(size_t i = 0; i < context_->type_max(); ++i)
            {
                if(map_danger_data_[i].data[0] & map.row[height - 4] || map_danger_data_[i].data[1] & map.row[height - 3] || map_danger_data_[i].data[2] & map.row[height - 2] || map_danger_data_[i].data[3] & map.row[height - 1])
                {
                    ++danger;
                }
            }
        }
        while(up-- > 0);
        return danger;
    }


    void the_ai_games_enemy::init(m_tetris::TetrisContext const *context, Param const *param)
    {
        param_ = param;
    }

    std::string the_ai_games_enemy::ai_name() const
    {
        return "The AI Games (SetoSan) v0.1";
    }

    size_t the_ai_games_enemy::eval(TetrisNode const *node, TetrisMap const &map, TetrisMap const &src_map, size_t clear) const
    {
        return clear;
    }

    size_t the_ai_games_enemy::bad() const
    {
        return 0;
    }

    size_t the_ai_games_enemy::get(size_t const *history, size_t history_length) const
    {
        size_t point = 0;
        int combo = param_->combo;
        for(size_t i = 0; i < history_length; ++i)
        {
            point += history[i];
            if(history[i] > 0)
            {
                if(history[i] == 4)
                {
                    point += 4;
                }
                point += combo++;
            }
            else
            {
                combo = 0;
            }
        }
        if(point > *param_->point_ptr)
        {
            *param_->point_ptr = point;
        }
        return point;
    }

}