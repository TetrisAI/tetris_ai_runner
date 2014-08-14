
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
                int mode;
            };
        public:
            void init(m_tetris::TetrisContext const *context, Param const *param);
            std::string ai_name() const;
            double eval_land_point(m_tetris::TetrisNode const *node, m_tetris::TetrisMap const &map, size_t clear);
            double eval_map_bad() const;
            double eval_map(m_tetris::TetrisMap const &map, m_tetris::EvalParam<double> const *history, size_t history_length);

        private:
            int check_line_1_[32];
            int check_line_2_[32];
            int *check_line_1_end_;
            int *check_line_2_end_;
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
        std::string ai_name() const;
        double eval_map_bad() const;
        double eval_map(m_tetris::TetrisMap const &map, m_tetris::EvalParam<> const *history, size_t history_length);

    };

}