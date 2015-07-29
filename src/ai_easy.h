

#include "tetris_core.h"

namespace ai_easy
{
    class AI
    {
    public:
        typedef double(*eval_func_t)(m_tetris::TetrisNode const *node, m_tetris::TetrisMap const &map, m_tetris::TetrisMap const &src_map, size_t clear);
        struct Config
        {
            eval_func_t eval_func;
            std::string str_name;
        };
    public:
        void init(m_tetris::TetrisContext const *context, Config const *config)
        {
            config_ = config;
        }
        std::string ai_name() const
        {
            return config_->str_name;
        }
        double eval(m_tetris::TetrisNode const *node, m_tetris::TetrisMap const &map, m_tetris::TetrisMap const &src_map, size_t clear) const
        {
            return config_->eval_func(node, map, src_map, clear);
        }
        double get(double const &eval_result) const
        {
            return eval_result;
        }
    private:
        Config const *config_;
    };
}
