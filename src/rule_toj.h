
#pragma once

#include "tetris_core.h"
#include <map>

namespace rule_toj
{
    struct TetrisRule
    {
        bool init(size_t w, size_t h);
        static std::map<std::pair<char, unsigned char>, m_tetris::TetrisOpertion> get_opertion();
        static std::map<char, m_tetris::TetrisBlockStatus(*)(m_tetris::TetrisContext const *)> get_generate();
    };
}