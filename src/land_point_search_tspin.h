
#pragma once

#include "tetris_core.h"
#include <vector>

namespace land_point_search_tspin
{
    class Search
    {
    public:
        enum SpinType
        {
            None, TSpin, TSpinMini
        };
        struct SpinInfo
        {
            SpinInfo()
            {
            }
            SpinInfo(m_tetris::TetrisNode const *_node) : node(_node), type(None)
            {

            }
            m_tetris::TetrisNode const *node;
            SpinType type;
            operator m_tetris::TetrisNode const *() const
            {
                return node;
            }
            m_tetris::TetrisNode const *operator->() const
            {
                return node;
            }
        };
        void init(m_tetris::TetrisContext const *context);
        std::vector<char> make_path(m_tetris::TetrisNode const *node, SpinInfo const &land_point, m_tetris::TetrisMap const &map);
        std::vector<SpinInfo> const *search(m_tetris::TetrisMap const &map, m_tetris::TetrisNode const *node);
    private:
        std::vector<SpinInfo> const *search_t(m_tetris::TetrisMap const &map, m_tetris::TetrisNode const *node);
        std::vector<SpinInfo> land_point_cache_;
        std::vector<m_tetris::TetrisNode const *> node_search_;
        m_tetris::TetrisNodeMark node_mark_;
        m_tetris::TetrisNodeMarkFiltered node_mark_filtered_;
    };
}
