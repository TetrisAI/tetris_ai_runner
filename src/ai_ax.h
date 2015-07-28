

#include "tetris_core.h"

namespace ai_ax
{
    class AI
    {
    public:
        void init(m_tetris::TetrisContext const *context);
        std::string ai_name() const;
        struct Result
        {
            double land_point, map;
        };
        struct Status
        {
            double land_point;
            double value;
            bool operator < (Status const &) const;
        };
        Result eval(m_tetris::TetrisNode const *node, m_tetris::TetrisMap const &map, m_tetris::TetrisMap const &src_map, size_t clear) const;
        Status get(Result const &eval_result, size_t depth, char const *next, size_t length, char hold, Status const &status) const;
        Status iterate(Status const **status, size_t status_length) const;

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
