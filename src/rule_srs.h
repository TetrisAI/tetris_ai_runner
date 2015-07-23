
#pragma once

#include "tetris_core.h"
#include <map>

namespace rule_srs
{
    struct TetrisRule
    {
        bool init(int w, int h);
        static std::map<std::pair<char, uint8_t>, m_tetris::TetrisOpertion> get_opertion();
        static std::map<char, m_tetris::TetrisBlockStatus(*)(m_tetris::TetrisContext const *)> get_generate();
    };
}