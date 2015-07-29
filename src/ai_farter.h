

#include "tetris_core.h"

namespace ai_farteryhr
{
    class AI
    {
    public:
        struct Status
        {
            int eval;
            int value;
            bool operator < (Status const &) const;
        };
        void init(m_tetris::TetrisContext const *context);
        std::string ai_name() const;
        int eval(m_tetris::TetrisNode const *node, m_tetris::TetrisMap const &map, m_tetris::TetrisMap const &src_map, size_t clear) const;
        Status get(int eval_result, size_t depth, Status const &status) const;
    private:
        int fhh;
    };
}
