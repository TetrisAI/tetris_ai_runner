
#pragma once

#include "tetris_core.h"
#include <vector>

namespace land_point_search_path
{
    class Search
    {
    private:
        std::vector<m_tetris::TetrisNode const *> land_point_cache_;
        std::vector<m_tetris::TetrisNode const *> node_search_;
        m_tetris::TetrisNodeMark node_mark_;
        m_tetris::TetrisNodeMarkFiltered node_mark_filtered_;
    public:
        void init(m_tetris::TetrisContext const *context);
        std::vector<char> make_path(m_tetris::TetrisNode const *node, m_tetris::TetrisNode const *land_point, m_tetris::TetrisMap const &map);
        std::vector<m_tetris::TetrisNode const *> const *search(m_tetris::TetrisMap const &map, m_tetris::TetrisNode const *node);
    };
}
