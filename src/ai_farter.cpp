
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

    // get 已移至 ai_farter.h 内联模板（GetArgs = MapBefore, Landing, Parent, Depth）。

}
