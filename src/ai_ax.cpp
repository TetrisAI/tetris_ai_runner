
//Code by ax_pokl
//Modify by ZouZhiZhang

#include "tetris_core.h"
#include "ai_ax.h"
#include "integer_utils.h"

using namespace m_tetris2;

namespace ai_ax
{

    bool AI::Status::operator<(Status const &other) const
    {
        return value < other.value;
    }

    std::string AI::ai_name() const
    {
        return "Tetris_ax_C ZZZ Mod v1.2";
    }

    AI::Status AI::get(Result const &eval_result, Landing const &landing, int clear, Status const &parent, size_t depth) const
    {
        // 旧 eval 中 land_point 几何项现场从 Landing/Clear 重算：
        //   LandHeight = lp.y + 1            -> landing.y_plus1()（≠ land_height()）
        //   Middle     = |(lp.col()+1)*2-W|  -> landing.col()
        //   EraseCount = clear               -> clear
        double LandHeight = static_cast<double>(landing.y_plus1());
        double Middle = std::abs((landing.col() + 1) * 2 - map_width_);
        double EraseCount = static_cast<double>(clear);
        double land_point_eval = (0 - LandHeight * 1750 / map_height_ + Middle * 2 + EraseCount * 60);

        Status result;
        result.land_point = land_point_eval + parent.land_point;
        result.value = result.land_point / depth + eval_result.map;
        return result;
    }

    AI::Status AI::iterate(Status const **status, size_t status_length) const
    {
        Status result;
        result.land_point = 0;
        result.value = 0;
        for (size_t i = 0; i < status_length; ++i)
        {
            if (status[i] == nullptr)
            {
                result.value += -9999999999;
            }
            else
            {
                result.value += status[i]->value;
            }
        }
        result.value /= status_length;
        return result;
    }

}
