
#include "tetris_core.h"
#include "search_tag.h"
#include <functional>

namespace ai_tag
{
    class the_ai_games_old
    {
    public:
        struct Result
        {
            double land_point, map;
            int tilt, full, count, clear, low_y, node_top;
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

    class the_ai_games
    {
    public:
        typedef search_tag::Search::TSpinType TSpinType;
        typedef search_tag::Search::TetrisNodeWithTSpinType TetrisNodeEx;
        struct Config
        {
            double map_low_width;
            double col_trans_width;
            double row_trans_width;
            double hold_count_width;
            double hold_focus_width;
            double well_depth_width;
            double hole_depth_width;
            double dig_block_multi;
            double dig_height_add;
            double dig_clear_width;
            double attack_depth_add;
            double attack_depth_minute;
            double line_clear_width;
            double map_safe_multi;
            double tspin_safe_width;
            double tspin_unsafe_width;
            double tetris_safe_width;
            double tetris_unsafe_width;
            double combo_add_width;
            double combo_break_minute;
        };
        struct Result
        {
            double attack, map;
            int node_top, map_low, clear, tspin, count, full;
            m_tetris::TetrisMap const *save_map;
        };
        struct Status
        {
            size_t max_combo;
            size_t combo;
            double max_attack;
            double attack;
            int up;
            double land_point;
            double value;
            bool operator < (Status const &) const;
        };
    public:
        void init(m_tetris::TetrisContext const *context, Config const *config);
        std::string ai_name() const;
        Result eval(TetrisNodeEx &node, m_tetris::TetrisMap const &map, m_tetris::TetrisMap const &src_map, size_t clear) const;
        Status get(Result const &eval_result, size_t depth, Status const &status) const;
        Status iterate(Status const **status, size_t status_length) const;

    private:
        m_tetris::TetrisContext const *context_;
        Config const *config_;
        uint32_t check_line_1_[32];
        uint32_t *check_line_1_end_;
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
        typedef search_tag::Search::TSpinType TSpinType;
        typedef search_tag::Search::TetrisNodeWithTSpinType TetrisNodeEx;
        struct Config
        {
            int *point_ptr;
        };
        struct Result
        {
            size_t clear, tspin;
        };
        struct Status
        {
            int point, combo;
            bool operator < (Status const &) const;
        };
    public:
        void init(m_tetris::TetrisContext const *context, Config const *param);
        std::string ai_name() const;
        Result eval(TetrisNodeEx const &node, m_tetris::TetrisMap const &map, m_tetris::TetrisMap const &src_map, size_t clear) const;
        Status get(Result const &eval_result, size_t depth, Status const &status) const;
        Status iterate(Status const **status, size_t status_length) const;

    private:
        Config const *config_;
    };

}