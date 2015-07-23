
#include "tetris_core.h"
#include "search_tspin.h"

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

    class TOJ
    {
    public:
        typedef search_tspin::Search::TSpinType TSpinType;
        typedef search_tspin::Search::TetrisNodeWithTSpinType TetrisNodeEx;
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
            int safe;
            double expect;
            TSpinType t_spin;
        };
        eval_result eval(TetrisNodeEx &node, m_tetris::TetrisMap const &map, m_tetris::TetrisMap const &src_map, size_t clear) const;
        double bad() const;
        double get(eval_result const *history, size_t history_length) const;
    private:
        int col_mask_, row_mask_;
        Param const *param_;
        int danger_line_;
        int danger_data_;
        size_t full_count_;
    };

    class C2
    {
    public:
        struct Param
        {
            int safe;
            int mode;
            int combo;
            size_t length;
            size_t virtual_length;
            std::function<void(m_tetris::TetrisNode const *, m_tetris::TetrisMap const &, std::vector<m_tetris::TetrisNode const *> &)> search;
        };
    public:
        void init(m_tetris::TetrisContext const *context, Param const *param);
        std::string ai_name() const;
        struct eval_result
        {
            double land_point, attack, map;
            size_t clear;
            int low_y;
            int count;
            bool soft_drop;
            m_tetris::TetrisMap const *save_map;
        };
        eval_result eval(m_tetris::TetrisNode const *node, m_tetris::TetrisMap const &map, m_tetris::TetrisMap const &src_map, size_t clear) const;
        double bad() const;
        double get(eval_result const *history, size_t history_length) const;
        double get_impl(eval_result const *history, size_t history_length) const;

    private:
        Param const *param_;
        m_tetris::TetrisContext const *context_;
        int col_mask_, row_mask_;
        struct MapInDangerData
        {
            int data[4];
        };
        std::vector<MapInDangerData> map_danger_data_;
        mutable std::vector<eval_result> result_cache_;
        mutable std::vector<std::vector<m_tetris::TetrisNode const *>> land_point_cache_;
        size_t map_in_danger_(m_tetris::TetrisMap const &map) const;
    };

}