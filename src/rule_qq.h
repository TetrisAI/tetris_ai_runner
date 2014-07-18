
#pragma once

#include "tetris_core.h"
#include <map>

namespace rule_qq
{
    struct TetrisRuleSet
    {
        static std::map<std::pair<unsigned char, unsigned char>, m_tetris::TetrisOpertion> get_opertion_info();
        static std::map<unsigned char, m_tetris::TetrisBlockStatus(*)(m_tetris::TetrisContext const *)> get_generate_info();
    };
}