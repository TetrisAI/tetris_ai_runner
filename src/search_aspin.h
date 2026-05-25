
#pragma once

#include "tetris_core.h"
#include <vector>
#include <cstddef>

namespace search_aspin
{
    class Search
    {
    public:
        enum ASpinType
        {
            None, ASpin
        };
        struct Config
        {
            bool allow_rotate_move = false;
            bool allow_180 = true;
            bool allow_d = true;
            bool allow_D = true;
            bool allow_LR = true;
            bool is_20g = false;
        };
        struct TetrisNodeWithASpinType
        {
            TetrisNodeWithASpinType()
            {
                std::memset(this, 0, sizeof(*this));
            }
            TetrisNodeWithASpinType(m_tetris::TetrisNode const *_node) : node(_node), type(None)
            {

            }
            m_tetris::TetrisNode const *node;
            ASpinType type;

            operator m_tetris::TetrisNode const *() const
            {
                return node;
            }
            m_tetris::TetrisNode const *operator->() const
            {
                return node;
            }
            bool operator == (TetrisNodeWithASpinType const &other)
            {
                return node == other.node && type == other.type;
            }
            bool operator == (std::nullptr_t)
            {
                return node == nullptr;
            }
            bool operator != (std::nullptr_t)
            {
                return node != nullptr;
            }
        };
        void init(m_tetris::TetrisContext const *context, Config const *config);
        std::vector<char> make_path(m_tetris::TetrisNode const *node, TetrisNodeWithASpinType const &land_point, m_tetris::TetrisMap const &map);
        std::vector<TetrisNodeWithASpinType> const *search(m_tetris::TetrisMap const &map, m_tetris::TetrisNode const *node, size_t depth);
    private:
        std::vector<char> make_path_20g(m_tetris::TetrisNode const *node, TetrisNodeWithASpinType const &land_point, m_tetris::TetrisMap const &map);
        std::vector<TetrisNodeWithASpinType> land_point_cache_;
        std::vector<m_tetris::TetrisNode const *> node_search_;
        m_tetris::TetrisNodeMark node_mark_;
        m_tetris::TetrisNodeMarkFiltered node_mark_filtered_;
        Config const *config_;
        m_tetris::TetrisContext const *context_;
    };
}
