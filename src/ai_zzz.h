
#include <set>
#include "tetris_core.h"

namespace ai_zzz
{
    namespace qq
    {
        class Attack
        {
        public:
            void init(m_tetris::TetrisContext const *context);
            std::string ai_name() const;
            double eval_map_bad() const;
            double eval_map(m_tetris::TetrisMap const &map, m_tetris::EvalParam<> const *history, size_t history_length);
            size_t prune_map(m_tetris::PruneParam<double> *prune, size_t prune_length, m_tetris::TetrisNode const **after_pruning, size_t next_length);

        private:
            std::vector<m_tetris::PruneParam<double> *> prune_sort_;
            std::set<int> check_line_;
        };
    }

    class Dig
    {
    public:
        struct Param
        {
            int b2b, combo;
        };
    public:
        void init(m_tetris::TetrisContext const *context, Param const *param);
        std::string ai_name() const;
        double eval_map_bad() const;
        double eval_map(m_tetris::TetrisMap const &map, m_tetris::EvalParam<> const *history, size_t history_length);
        size_t prune_map(m_tetris::PruneParam<double> *prune, size_t prune_length, m_tetris::TetrisNode const **after_pruning, size_t next_length);

    private:
        Param const *param_;
    };
}