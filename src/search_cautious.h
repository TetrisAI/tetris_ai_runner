
#pragma once

#include "tetris_core.h"
#include <vector>

namespace search_cautious
{
    class Search
    {
    public:
        struct Config
        {
            bool fast_move_down;
        };
    private:
        std::vector<m_tetris::TetrisNode const *> land_point_cache_;
        std::vector<m_tetris::TetrisNode const *> node_search_;
        m_tetris::TetrisNodeMark node_mark_;
        m_tetris::TetrisNodeMarkFiltered node_mark_filtered_;
        Config const *config_;
    public:
        void init(m_tetris::TetrisContext const *context, Config const *config);
        std::vector<char> make_path(m_tetris::TetrisNode const *node, m_tetris::TetrisNode const *land_point, m_tetris::TetrisMap const &map);
        std::vector<m_tetris::TetrisNode const *> const *search(m_tetris::TetrisMap const &map, m_tetris::TetrisNode const *node);
    };
}
