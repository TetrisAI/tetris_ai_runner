
#include <set>
#include "tetris_core.h"

namespace ai_zzz
{
    namespace qq
    {
        class Attack
        {
        public:
            struct Param
            {
                size_t next_length;
                size_t level;
            };
        public:
            void init(m_tetris::TetrisContext const *context, Param const *param);
            std::string ai_name() const;
            double eval_land_point(m_tetris::TetrisNode const *node, m_tetris::TetrisMap const &map, size_t clear);
            double eval_map_bad() const;
            double eval_map(m_tetris::TetrisMap const &map, m_tetris::EvalParam<double> const *history, size_t history_length);
            size_t prune_map(m_tetris::PruneParam<double> *prune, size_t prune_length, m_tetris::TetrisNode const **after_pruning, size_t next_length);

        private:
            std::set<int> check_line_;
            Param const *param_;
            m_tetris::TetrisContext const *context_;
            int col_mask_, row_mask_;
            struct MapInDangerData
            {
                int data[4];
            };
            std::vector<MapInDangerData> map_danger_data_;
            size_t map_in_danger_(m_tetris::TetrisMap const &map);
        };
    }

    class Dig
    {
    public:
        void init(m_tetris::TetrisContext const *context, size_t const *param);
        std::string ai_name() const;
        double eval_map_bad() const;
        double eval_map(m_tetris::TetrisMap const &map, m_tetris::EvalParam<> const *history, size_t history_length);
        size_t prune_map(m_tetris::PruneParam<double> *prune, size_t prune_length, m_tetris::TetrisNode const **after_pruning, size_t next_length);

    private:
        size_t const *next_length_ptr_;
    };

}