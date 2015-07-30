
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
            int miny_factor; // ��߸߶ȷ�
            int hole; // -����
            int open_hole; // -���Ŷ������ܲ��
            int v_transitions; // -ˮƽת��ϵ��
            int tspin3; // T3������

            int clear_efficient; // ����Ч��ϵ��
            int upcomeAtt; // -Ԥ����������ϵ��
            int h_factor; // -�߶Ȳ�ϵ��
            int hole_dis_factor2; // -������ϵ��
            int hole_dis; // -���ľ����
                          //int flat_factor; // ƽֱϵ��

            int hole_dis_factor; // -������ϵ��
            int tspin; // tspinϵ��
            int hold_T; // hold T��Iϵ��
            int hold_I; // hold T��Iϵ��
            int clear_useless_factor; // ��Ч��ϵ��
                                      //int ready_combo; // ����Ԥ����x

            int dif_factor; //ƫ��ֵ
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