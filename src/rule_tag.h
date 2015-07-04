
#pragma once

#include "tetris_core.h"
#include <map>

//rules for http://theaigames.com/
namespace rule_tag
{
    struct TetrisRuleSet
    {
        static std::map<std::pair<unsigned char, unsigned char>, m_tetris::TetrisOpertion> get_opertion();
        static std::map<unsigned char, m_tetris::TetrisBlockStatus(*)(m_tetris::TetrisContext const *)> get_generate();
    };
}