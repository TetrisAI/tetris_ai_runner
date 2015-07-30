
#include "tetris_core.h"
#include "search_tspin.h"

namespace ai_misaka
{
    class misaka
    {
    public:
        typedef search_tspin::Search::TSpinType TSpinType;
        typedef search_tspin::Search::TetrisNodeWithTSpinType TetrisNodeEx;
        struct Config
        {
            int miny_factor; // 最高高度分
            int hole; // -洞分
            int open_hole; // -开放洞，可能插块
            int v_transitions; // -水平转换系数
            int tspin3; // T3基本分

            int clear_efficient; // 消行效率系数
            int upcomeAtt; // -预备攻击基本系数
            int h_factor; // -高度差系数
            int hole_dis_factor2; // -洞距离系数
            int hole_dis; // -洞的距离分
                          //int flat_factor; // 平直系数

            int hole_dis_factor; // -洞距离系数
            int tspin; // tspin系数
            int hold_T; // hold T和I系数
            int hold_I; // hold T和I系数
            int clear_useless_factor; // 无效行系数
                                      //int ready_combo; // 连击预备分x

            int dif_factor; //偏差值
            int strategy_4w;
            int const *table;
            int table_max;
        };
        struct Result
        {
            m_tetris::TetrisNode const *node;
            m_tetris::TetrisMap const *map;
            m_tetris::TetrisMap const *src_map;
            size_t clear;
            TSpinType t_spin;
        };
        struct Status
        {
            int upcomeAtt;
            int combo;
            int b2b;
            int att;
            int clear;
            int max_att;
            int max_combo;
            int strategy_4w;
            int score;
            bool operator < (Status const &) const;
        };
    public:
        void init(m_tetris::TetrisContext const *context, Config const *config);
        std::string ai_name() const;
        Result eval(TetrisNodeEx &node, m_tetris::TetrisMap const &map, m_tetris::TetrisMap const &src_map, size_t clear) const;
        Status get(Result const &eval_result, size_t depth, Status const &status, m_tetris::TetrisContext::Env const &env) const;

    private:
        m_tetris::TetrisContext const *context_;
        Config const *config_;
    };


}