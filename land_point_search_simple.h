
#pragma once

#include "tetris_core.h"
#include <vector>

namespace land_point_search_simple
{
    class Search
    {
    private:
        std::vector<m_tetris::TetrisNode const *> land_point_cache;
    public:
        std::vector<char> make_path(m_tetris::TetrisNode const *node, m_tetris::TetrisNode const *land_point, m_tetris::TetrisMap const &map);
        std::vector<m_tetris::TetrisNode const *> const *search(m_tetris::TetrisMap const &map, m_tetris::TetrisNode const *node);
        m_tetris::TetrisNode const *process(m_tetris::TetrisNode const *node, m_tetris::TetrisMap const &map);
    };
}
