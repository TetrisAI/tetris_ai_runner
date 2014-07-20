
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
            std::string ai_name();
            double eval_map_bad();
            double eval_map(m_tetris::TetrisMap const &map, m_tetris::EvalParam<> const *history, size_t history_length);
            void prune_map(m_tetris::PruneParam<double> *prune, size_t prune_length, size_t next_length);

        private:
            std::vector<m_tetris::PruneParam<double> *> prune_sort_;
            std::set<int> check_line_;
        };
    }

    class Dig
    {
    public:
        std::string ai_name();
        double eval_map_bad();
        double eval_map(m_tetris::TetrisMap const &map, m_tetris::EvalParam<> const *history, size_t history_length);
        void prune_map(m_tetris::PruneParam<double> *prune, size_t prune_length, size_t next_length);

    private:
        std::vector<m_tetris::PruneParam<double> *> prune_sort_;
        std::set<int> check_line_;
    };
}