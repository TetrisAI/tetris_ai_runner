

#include "tetris_core.h"

namespace ai_farteryhr
{
    class AI
    {
    public:
        void init(m_tetris::TetrisContext const *context);
        std::string ai_name() const;
        int eval(m_tetris::TetrisNode const *node, m_tetris::TetrisMap const &map, m_tetris::TetrisMap const &src_map, size_t clear) const;
        int bad() const;
        int get(int const *history, size_t history_length) const;
    private:
        int fhh;
    };
}
