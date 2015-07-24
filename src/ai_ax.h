

#include "tetris_core.h"

namespace ai_ax
{
    class AI
    {
    public:
        void init(m_tetris::TetrisContext const *context);
        std::string ai_name() const;
        struct Status
        {
            double land_point;
            size_t depth;
            double value;
            bool operator < (Status const &) const;
        };
        Status eval(m_tetris::TetrisNode const *node, m_tetris::TetrisMap const &map, m_tetris::TetrisMap const &src_map, size_t clear, Status const &status) const;
        Status iterated(Status const **status, size_t status_length) const;

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
