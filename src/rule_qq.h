
#pragma once

#include "tetris_core.h"
#include <map>

namespace rule_qq
{
    struct TetrisRule
    {
        static std::map<std::pair<char, uint8_t>, m_tetris::TetrisOpertion> get_opertion();
        static std::map<char, m_tetris::TetrisBlockStatus(*)(m_tetris::TetrisContext const *)> get_generate();
    };
}