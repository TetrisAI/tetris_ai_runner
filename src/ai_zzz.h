
#include "tetris_core.h"
#include "search_tspin.h"

namespace ai_zzz
{
    namespace qq
    {
        class Attack
        {
        public:
            struct Config
            {
                size_t level;
                int mode;
            };
            struct Status
            {
                double land_point;
                double attack;
                size_t deepth;
                double value;
                bool operator < (Status const &) const;
            };
        public:
            void init(m_tetris::TetrisContext const *context, Config const *config);
            std::string ai_name() const;
            Status eval(m_tetris::TetrisNode const *node, m_tetris::TetrisMap const &map, m_tetris::TetrisMap const &src_map, size_t clear, Status const &status) const;

        private:
            uint32_t check_line_1_[32];
            uint32_t check_line_2_[32];
            uint32_t *check_line_1_end_;
            uint32_t *check_line_2_end_;
            Config const *config_;
            m_tetris::TetrisContext const *context_;
            int col_mask_, row_mask_;
            struct MapInDangerData
            {
                uint32_t data[4];
            };
            std::vector<MapInDangerData> map_danger_data_;
            size_t map_in_danger_(m_tetris::TetrisMap const &map) const;
        };
    }

    class Dig
    {
    public:
        std::string ai_name() const;
        double eval(m_tetris::TetrisNode const *node, m_tetris::TetrisMap const &map, m_tetris::TetrisMap const &src_map, size_t clear, double const &status) const;

    };

    class TOJ
    {
    public:
        typedef search_tspin::Search::TSpinType TSpinType;
        typedef search_tspin::Search::TetrisNodeWithTSpinType TetrisNodeEx;
        struct Config
        {
            int const *table;
            size_t table_max;
        };
        struct Status
        {
            size_t combo;
            size_t under_attack;
            bool b2b;
            int attack;
            double like;
            double value;
            bool operator < (Status const &) const;
        };
    public:
        void init(m_tetris::TetrisContext const *context, Config const *config);
        std::string ai_name() const;
        Status eval(TetrisNodeEx &node, m_tetris::TetrisMap const &map, m_tetris::TetrisMap const &src_map, size_t clear, Status const & status) const;
    private:
        int col_mask_, row_mask_;
        Config const *config_;
        int danger_line_;
        int danger_data_;
        size_t full_count_;
    };

    class C2
    {
    public:
        struct Config
        {
            int safe;
            int mode;
        };
        struct Status
        {
            double land_point;
            size_t deepth;
            int combo;
            double value;
            bool operator < (Status const &) const;
        };
    public:
        void init(m_tetris::TetrisContext const *context, Config const *config);
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
        Status eval(m_tetris::TetrisNode const *node, m_tetris::TetrisMap const &map, m_tetris::TetrisMap const &src_map, size_t clear, Status const &status) const;

    private:
        Config const *config_;
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