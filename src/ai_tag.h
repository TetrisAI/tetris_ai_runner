
#include "tetris_core.h"
#include <functional>

namespace ai_tag
{
    class the_ai_games
    {
    public:
        struct eval_result
        {
            double land_point, map;
            size_t clear;
            int low_y;
            int full;
            int count;
            m_tetris::TetrisMap const *save_map;
        };
        struct Param
        {
            int combo;
            int up;
            size_t length;
            size_t virtual_length;
            std::function<void(m_tetris::TetrisNode const *, m_tetris::TetrisMap const &, std::vector<m_tetris::TetrisNode const *> &)> search;
        };
    public:
        void init(m_tetris::TetrisContext const *context, Param const *param);
        std::string ai_name() const;
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

    class the_ai_games_enemy
    {
    public:
        struct eval_result
        {
            size_t clear;
            m_tetris::TetrisMap const *save_map;
        };
        struct Param
        {
            size_t combo;
            size_t length;
            size_t virtual_length;
            size_t *point_ptr;
            std::function<void(m_tetris::TetrisNode const *, m_tetris::TetrisMap const &, std::vector<m_tetris::TetrisNode const *> &)> search;
        };
    public:
        void init(m_tetris::TetrisContext const *context, Param const *param);
        std::string ai_name() const;
        eval_result eval(m_tetris::TetrisNode const *node, m_tetris::TetrisMap const &map, m_tetris::TetrisMap const &src_map, size_t clear) const;
        size_t bad() const;
        size_t get(eval_result const *history, size_t history_length) const;
        size_t get_impl(eval_result const *history, size_t history_length) const;

    private:
        Param const *param_;
        m_tetris::TetrisContext const *context_;
        mutable std::vector<eval_result> result_cache_;
        mutable std::vector<std::vector<m_tetris::TetrisNode const *>> land_point_cache_;
    };

}