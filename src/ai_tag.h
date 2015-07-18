
#include "tetris_core.h"

namespace ai_tag
{
    class the_ai_games
    {
    public:
        struct Param
        {
            int combo;
        };
    public:
        void init(m_tetris::TetrisContext const *context, Param const *param);
        std::string ai_name() const;
        struct eval_result
        {
            double land_point, map;
            size_t clear;
            int danger;
            int low_y;
            int count;
        };
        eval_result eval(m_tetris::TetrisNode const *node, m_tetris::TetrisMap const &map, m_tetris::TetrisMap const &src_map, size_t clear) const;
        double bad() const;
        double get(eval_result const *history, size_t history_length) const;

    private:
        Param const *param_;
        m_tetris::TetrisContext const *context_;
        int col_mask_, row_mask_;
        struct MapInDangerData
        {
            int data[4];
        };
        std::vector<MapInDangerData> map_danger_data_;
        size_t map_in_danger_(m_tetris::TetrisMap const &map) const;
    };

}