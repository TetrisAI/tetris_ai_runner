
//by ZouZhiZhang

#include "tetris_core.h"
#include "integer_utils.h"
#include "ai_tag.h"
#include <cstdint>

using namespace m_tetris2;
using namespace zzz;

namespace ai_tag
{
    bool the_ai_games_old::Status::operator<(Status const &other) const
    {
        return value < other.value;
    }

    std::string the_ai_games_old::ai_name() const
    {
        return "The AI Games (SetoSan) v0.1";
    }

    the_ai_games_old::Status the_ai_games_old::get(Result const &eval_result, size_t depth, Status const &status) const
    {
        Status result = status;
        double BoardDeadZone = 0;
        if (eval_result.roof + status.up >= height_ || eval_result.node_top >= height_)
        {
            BoardDeadZone = type_max_;
        }
        else
        {
            BoardDeadZone = eval_result.safe_cache[status.up];
        }
        result.land_point -= BoardDeadZone * 50000000;
        bool building = (eval_result.count - eval_result.full * width_) * 3 / 2 < std::max(0, (height_ - 6) - (eval_result.full + status.up)) * width_;
        if (eval_result.clear > 0)
        {
            if (eval_result.clear == 4)
            {
                result.land_point += 1000;
            }
            if (status.combo == 0 && building)
            {
                result.land_point -= (4 - std::min<int>(4, eval_result.low_y)) * 2000;
            }
            else if (status.combo > 0)
            {
                result.land_point += status.combo * 1200;
                if (eval_result.tilt > 5)
                {
                    result.land_point += 100;
                }
            }
            ++result.combo;
        }
        else
        {
            if (status.combo > 0 && building && eval_result.low_y > 4)
            {
                result.land_point -= status.combo * 600 + 600;
            }
            result.combo = 0;
        }
        double rate = (depth - 1.f) / 5 + 1;
        result.value = result.land_point + eval_result.map * rate;
        return result;
    }

    the_ai_games_old::Status the_ai_games_old::iterate(Status const **status, size_t status_length) const
    {
        Status result;
        result.combo = 0;
        result.up = 0;
        result.land_point = 0;
        result.value = 0;
        for (size_t i = 0; i < status_length; ++i)
        {
            if (status[i] == nullptr)
            {
                result.value -= 9999999999;
            }
            else
            {
                result.value += status[i]->value;
            }
        }
        result.value /= status_length;
        return result;
    }

    bool the_ai_games::Status::operator<(Status const &other) const
    {
        return value < other.value;
    }

    std::string the_ai_games::ai_name() const
    {
        return "The AI Games (SetoSan) v0.1";
    }

    the_ai_games::Status the_ai_games::get(Result const &eval_result, size_t depth, Status const &status) const
    {
        int tspin = eval_result.t_spin != TSpinType::None ? eval_result.clear : 0;
        Status result = status;
        double BoardDeadZone = 0;
        if (eval_result.roof + status.up[depth] >= height_ || eval_result.node_top >= height_)
        {
            BoardDeadZone = type_max_;
        }
        else
        {
            BoardDeadZone = eval_result.safe_cache[status.up[depth]];
        }
        result.attack -= BoardDeadZone * 50000000;
        result.attack += eval_result.clear * (eval_result.clear + 1) * config_->line_clear_width;
        if (tspin > 0)
        {
            result.attack += tspin * config_->tspin_clear_width;
        }
        if (eval_result.clear == 4)
        {
            result.attack += config_->tetris_clear_width;
        }
        result.attack += eval_result.tbuild * config_->tspin_build_width;
        if (eval_result.clear > 0)
        {
            if (result.combo > 0)
            {
                result.attack += result.combo * config_->combo_add_width;
            }
            else
            {
                result.attack -= config_->combo_break_minute;
            }
            ++result.combo;
        }
        else
        {
            ++result.combo = 0;
        }
        result.value = (0. + result.attack + eval_result.map);
        result.max_attack = std::max(status.max_attack, result.attack);
        return result;
    }

    the_ai_games::Status the_ai_games::iterate(Status const **status, size_t status_length) const
    {
        Status result;
        result.max_combo = 0;
        result.combo = 0;
        result.max_attack = 0;
        result.attack = 0;
        std::memset(result.up, 0, sizeof result.up);
        result.land_point = 0;
        result.value = 0;
        for (size_t i = 0; i < status_length; ++i)
        {
            if (status[i] == nullptr)
            {
                result.value -= 10 * 50000000;
            }
            else
            {
                result.value += status[i]->value;
            }
        }
        result.value /= status_length;
        return result;
    }

    bool the_ai_games_enemy::Status::operator<(Status const &other) const
    {
        return point < other.point;
    }

    std::string the_ai_games_enemy::ai_name() const
    {
        return "The AI Games (SetoSan) v0.1";
    }

    the_ai_games_enemy::Status the_ai_games_enemy::get(Result const &eval_result, size_t depth, Status const &status) const
    {
        Status result = status;
        if (eval_result.clear > 0)
        {
            if (eval_result.tspin > 0)
            {
                result.point += eval_result.tspin * 6;
            }
            else
            {
                switch (eval_result.clear)
                {
                case 1:
                    result.point += 1;
                    break;
                case 2:
                    result.point += 3;
                    break;
                case 3:
                    result.point += 6;
                    break;
                case 4:
                    result.point += 12;
                    break;
                }
            }
            result.point += status.combo;
            result.combo = status.combo + 1;
        }
        else
        {
            result.combo = 0;
        }
        result.up[depth] = result.point;
        if (result.point >= *config_->point_ptr)
        {
            *config_->point_ptr = result.point;
            std::copy(result.up, result.up + 4, config_->up_ptr);
        }
        return result;
    }

    the_ai_games_enemy::Status the_ai_games_enemy::iterate(Status const **status, size_t status_length) const
    {
        Status result;
        result.combo = 0;
        result.point = 0;
        for (size_t i = 0; i < status_length; ++i)
        {
            if (status[i] != nullptr && status[i]->point > result.point)
            {
                result.point = status[i]->point;
            }
        }
        return result;
    }

}