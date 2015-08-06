
#pragma once

#include "tetris_core.h"
#include <vector>

namespace search_tag
{
    class Search
    {
    public:
        enum TSpinType
        {
            None, TSpin
        };
        struct TetrisNodeWithTSpinType
        {
            TetrisNodeWithTSpinType() : node(), last(), type(None), is_check(), is_last_rotate(), is_ready()
            {
            }
            TetrisNodeWithTSpinType(m_tetris::TetrisNode const *_node) : node(_node), last(), type(None), is_check(), is_last_rotate(), is_ready()
            {

            }
            m_tetris::TetrisNode const *node;
            m_tetris::TetrisNode const *last;
            TSpinType type;
            bool is_check;
            bool is_last_rotate;
            bool is_ready;
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
        std::vector<char> make_path(m_tetris::TetrisNode const *node, TetrisNodeWithTSpinType const &land_point, m_tetris::TetrisMap const &map);
        std::vector<TetrisNodeWithTSpinType> const *search(m_tetris::TetrisMap const &map, m_tetris::TetrisNode const *node);
    private:
        std::vector<TetrisNodeWithTSpinType> const *search_t(m_tetris::TetrisMap const &map, m_tetris::TetrisNode const *node);
        bool check_ready(m_tetris::TetrisMap const &map, m_tetris::TetrisNode const *node);
        std::vector<TetrisNodeWithTSpinType> land_point_cache_;
        std::vector<m_tetris::TetrisNode const *> node_incomplete_;
        std::vector<m_tetris::TetrisNode const *> node_search_;
        m_tetris::TetrisNodeMark node_mark_;
        m_tetris::TetrisNodeMarkFiltered node_mark_filtered_;
        uint32_t *block_data_;
        uint32_t block_data_buffer_[52];
        int x_diff_, y_diff_;
        m_tetris::TetrisContext const *context_;
    };
}
