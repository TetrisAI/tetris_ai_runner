

#include "tetris_core.h"

namespace ai_ax
{
    class AI
    {
    public:
        void init(m_tetris::TetrisContext const *context);
        typedef double LandPointEval, MapEval;
        std::string ai_name() const;
        void eval_land_point(m_tetris::TetrisNode const *node, m_tetris::TetrisMap const &map, size_t clear, LandPointEval &out_eval) const;
        void eval_map(m_tetris::TetrisMap const &map, m_tetris::EvalParam<LandPointEval> const *history, size_t history_length, MapEval &out_result) const;
        bool map_eval_greater(MapEval const &left, MapEval const &right) const;
        MapEval get_vritual_eval(MapEval const *eval, size_t eval_length) const;
        enum
        {
            pruning = 1
        };

    private:
        struct MapInDangerData
        {
            int data[4];
        };
        std::vector<MapInDangerData> map_danger_data_;
        m_tetris::TetrisContext const *context_;
        size_t map_in_danger_(m_tetris::TetrisMap const &map) const;
    };
}
