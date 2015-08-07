
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

    class the_ai_games_rubbish
    {
    public:
        typedef search_tag::Search::TSpinType TSpinType;
        typedef search_tag::Search::TetrisNodeWithTSpinType TetrisNodeEx;
        struct Result
        {
            double land_point, map;
            int tilt, full, count, clear, low_y, node_top;
            int t2_clear, t2_value;
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
        void init(m_tetris::TetrisContext const *context);
        std::string ai_name() const;
        Result eval(TetrisNodeEx &node, m_tetris::TetrisMap const &map, m_tetris::TetrisMap const &src_map, size_t clear) const;
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
        struct Result
        {
            double land_point, attack, map;
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
        void init(m_tetris::TetrisContext const *context);
        std::string ai_name() const;
        Result eval(TetrisNodeEx &node, m_tetris::TetrisMap const &map, m_tetris::TetrisMap const &src_map, size_t clear) const;
        Status get(Result const &eval_result, size_t depth, Status const &status) const;
        Status iterate(Status const **status, size_t status_length) const;

    private:
        m_tetris::TetrisContext const *context_;
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