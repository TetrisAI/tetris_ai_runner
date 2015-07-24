

#include "tetris_core.h"

namespace ai_easy
{
    class AI
    {
    public:
        typedef double(*eval_func_t)(m_tetris::TetrisNode const *node, m_tetris::TetrisMap const &map, m_tetris::TetrisMap const &src_map, size_t clear);
        struct Param
        {
            eval_func_t eval_func;
            double bad_value;
        };
    public:
        void init(m_tetris::TetrisContext const *context, Param const *param)
        {
            param_ = param;
        }
        std::string ai_name() const
        {
            return std::string();
        }
        double eval(m_tetris::TetrisNode const *node, m_tetris::TetrisMap const &map, m_tetris::TetrisMap const &src_map, size_t clear) const
        {
            return param_->eval_func(node, map, src_map, clear);
        }
        double bad() const
        {
            return param_->bad_value;
        }
        double get(double const *history, size_t history_length) const
        {
            return history[history_length - 1];
        }
    private:
        Param const *param_;
    };
}
