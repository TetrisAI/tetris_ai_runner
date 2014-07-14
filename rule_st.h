
#pragma once

#include "tetris_core.h"
#include <map>

namespace rule_st
{
    struct TetrisRuleSet
    {
        static std::map<std::pair<unsigned char, unsigned char>, TetrisOpertion> get_opertion_info();
        static std::map<unsigned char, m_tetris::TetrisBlockStatus(*)(TetrisContext const *)> get_generate_info();
        static size_t map_in_danger(TetrisMap const &map);
    };
}