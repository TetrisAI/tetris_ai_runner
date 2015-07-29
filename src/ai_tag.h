
#include "tetris_core.h"
#include <functional>

namespace ai_tag
{
    class the_ai_games
    {
    public:
        struct Result
        {
            double land_point, map;
            int full, count, clear, low_y;
            m_tetris::TetrisMap const *save_map;
        };
        struct Status
        {
            size_t combo;
            int up;
            double land_point;
            double value;
            bool operator < (Status const &) const;
        };
    public:
        void init(m_tetris::TetrisContext const *context);
        std::string ai_name() const;
        Result eval(m_tetris::TetrisNode const *node, m_tetris::TetrisMap const &map, m_tetris::TetrisMap const &src_map, size_t clear) const;
        Status get(Result const &eval_result, size_t depth, Status const &status) const;
        Status iterate(Status const **status, size_t status_length) const;

    private:
        m_tetris::TetrisContext const *context_;
        int col_mask_, row_mask_;
        struct MapInDangerData
        {
            int data[4];
        };
        std::vector<MapInDangerData> map_danger_data_;
        size_t map_in_danger_(m_tetris::TetrisMap const &map, size_t up) const;
    };

    class the_ai_games_enemy
    {
    public:
        struct Config
        {
            int *point_ptr;
        };
        struct Status
        {
            int point, combo;
            bool operator < (Status const &) const;
        };
    public:
        void init(m_tetris::TetrisContext const *context, Config const *param);
        std::string ai_name() const;
        size_t eval(m_tetris::TetrisNode const *node, m_tetris::TetrisMap const &map, m_tetris::TetrisMap const &src_map, size_t clear) const;
        Status get(size_t clear, size_t depth, Status const &status) const;
        Status iterate(Status const **status, size_t status_length) const;

    private:
        Config const *config_;
    };

}