

#include "tetris_core.h"

namespace ai_ax_1
{
    class AI
    {
    public:
        void init(m_tetris::TetrisContext const *context);
        std::string ai_name();
        double eval_land_point(m_tetris::TetrisNode const *node, m_tetris::TetrisMap const &map, size_t clear);
        double eval_map_bad();
        double eval_map(m_tetris::TetrisMap const &map, m_tetris::EvalParam<double> const *history, size_t history_length);
        double get_virtual_eval(double const *eval, size_t eval_length);
        void prune_map(m_tetris::PruneParam<double> *prune, size_t prune_length, size_t next_length);

    private:
        struct MapInDangerData
        {
            int data[4];
        };
        std::vector<MapInDangerData> map_danger_data_;
        std::vector<m_tetris::PruneParam<double> *> prune_sort_;
        m_tetris::TetrisContext const *context_;
        size_t map_in_danger_(m_tetris::TetrisMap const &map);
    };
}


namespace ai_ax_0
{
    class AI
    {
    public:
        void init(m_tetris::TetrisContext const *context);
        std::string ai_name() const;
        double eval_map_bad() const;
        double eval_map(m_tetris::TetrisMap const &map, m_tetris::EvalParam<> const *history, size_t history_length) const;

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
