
#include "tetris_core.h"
#include "land_point_search_tspin.h"

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
            struct eval_result
            {
                double land_point, map;
                size_t clear;
                int danger;
            };
            eval_result eval(m_tetris::TetrisNode const *node, m_tetris::TetrisMap const &map, m_tetris::TetrisMap const &src_map, size_t clear) const;
            double bad() const;
            double get(eval_result const *history, size_t history_length) const;

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
            size_t map_in_danger_(m_tetris::TetrisMap const &map) const;
        };
    }

    class Dig
    {
    public:
        std::string ai_name() const;
        double eval(m_tetris::TetrisNode const *node, m_tetris::TetrisMap const &map, m_tetris::TetrisMap const &src_map, size_t clear) const;
        double bad() const;
        double get(double const *history, size_t history_length) const;

    };

    class SRS
    {
    public:
        typedef land_point_search_tspin::Search::SpinType SpinType;
        typedef land_point_search_tspin::Search::SpinInfo LandPoint;
        struct Param
        {
            size_t combo;
            size_t under_attack;
            bool b2b;
            int const *table;
            size_t table_max;
        };
    public:
        void init(m_tetris::TetrisContext const *context, Param const *param);
        std::string ai_name() const;
        struct eval_result
        {
            double eval;
            size_t clear;
            size_t count;
            int roof;
            bool t_spin;
        };
        eval_result eval(LandPoint const &node, m_tetris::TetrisMap const &map, m_tetris::TetrisMap const &src_map, size_t clear) const;
        double bad() const;
        double get(eval_result const *history, size_t history_length) const;
    private:
        int col_mask_, row_mask_;
        Param const *param_;
    };

}