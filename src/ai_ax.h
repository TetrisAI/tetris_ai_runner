

#include "tetris_core.h"

namespace ai_ax
{
    class AI
    {
    public:
        void init(m_tetris::TetrisContext const *context);
        std::string ai_name() const;
        struct eval_result
        {
            double land_point, map;
        };
        eval_result eval(m_tetris::TetrisNode const *node, m_tetris::TetrisMap const &map, m_tetris::TetrisMap const &src_map, size_t clear) const;
        double bad() const;
        double get(eval_result const *history, size_t history_length) const;
        eval_result iterated(eval_result const *eval, size_t eval_length) const;

    private:
        struct MapInDangerData
        {
            int data[4];
        };
        std::vector<MapInDangerData> map_danger_data_;
        m_tetris::TetrisContext const *context_;
        size_t map_in_danger_(m_tetris::TetrisMap const &map) const;
        int col_mask_, row_mask_;
    };
}
