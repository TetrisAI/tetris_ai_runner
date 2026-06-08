
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

    AI::Status AI::get(Result const &eval_result, size_t depth, Status const &status) const
    {
        Status result;
        result.land_point = eval_result.land_point + status.land_point;
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
