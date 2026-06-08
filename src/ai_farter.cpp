
//Code by ax_pokl
//Modify by ZouZhiZhang

#include "tetris_core.h"
#include "ai_farter.h"
#include "integer_utils.h"

using namespace m_tetris2;

namespace ai_farteryhr
{

    bool AI::Status::operator < (Status const &other) const
    {
        return value < other.value;
    }

    std::string AI::ai_name() const
    {
        return "farteryhr v1";
    }

    AI::Status AI::get(int eval_result, size_t depth, Status const &status) const
    {
        Status result =
        {
            eval_result + status.eval
        };
        result.value = result.eval / static_cast<int>(depth);
        return result;
    }

}
