
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
            struct Result
            {
                double land_point, map;
                size_t clear;
                int danger;
            };
            struct Status
            {
                double land_point;
                double attack;
                double rubbish;
                double value;
                bool operator < (Status const &) const;
            };
        public:
            void init(m_tetris::TetrisContext const *context, Config const *config);
            std::string ai_name() const;
            Result eval(m_tetris::TetrisNode const *node, m_tetris::TetrisMap const &map, m_tetris::TetrisMap const &src_map, size_t clear) const;
            Status get(Result const &eval_result, size_t depth, char hold, Status const &status) const;

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
        struct Result
        {
            double eval;
            size_t clear;
            size_t count;
            int safe;
            double expect;
            TSpinType t_spin;
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
        Result eval(TetrisNodeEx &node, m_tetris::TetrisMap const &map, m_tetris::TetrisMap const &src_map, size_t clear) const;
        Status get(Result const &eval_result, size_t depth, char hold, Status const & status) const;
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
            size_t combo;
            double value;
            bool operator < (Status const &) const;
        };
        struct Result
        {
            double land_point, attack, map;
            size_t clear;
            int low_y;
            int count;
            bool soft_drop;
        };
    public:
        void init(m_tetris::TetrisContext const *context, Config const *config);
        std::string ai_name() const;
        Result eval(m_tetris::TetrisNode const *node, m_tetris::TetrisMap const &map, m_tetris::TetrisMap const &src_map, size_t clear) const;
        Status get(Result const &eval_result, size_t depth, char hold, Status const &status) const;
        Status iterate(Status const **status, size_t status_length) const;

    private:
        Config const *config_;
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