
#include "tetris_core.h"
#include "search_tspin.h"
#include "ai_rule_spec_utils.h"

namespace ai_misaka
{
    class misaka
    {
    public:
        typedef search_tspin::Search::TSpinType TSpinType;
        using EvalSpinType = TSpinType;
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
            int hold_t; // hold T和I系数
            int hold_i; // hold T和I系数
            int clear_useless_factor; // 无效行系数
                                      //int ready_combo; // 连击预备分x

            int dif_factor; //偏差值
            int strategy_4w;
            int const *table;
            int table_max;
        };
        struct Result
        {
            char t;
            m_tetris2::row_t map_rows[22]; // copy of map rows, index 0 = bottom (y=0)
            int map_count;                // occupied cell count
            size_t clear;
            TSpinType t_spin;
        };
        struct Status
        {
            int upcomeAtt;
            int combo;
            int b2b;
            int total_clear_att;
            int total_clears;
            int clearScore;
            int att;
            int max_att;
            int max_combo;
            int strategy_4w;
            int score;
            bool operator < (Status const &) const;
        };
    public:
        template<class Spec>
        void init(Config const *config);
        std::string ai_name() const;
        template<class Details>
        Result eval(m_tetris2::BBNode<Details, TSpinType> const &lp, typename Details::map_type const &map, typename Details::map_type const &src_map, size_t clear) const
        {
            using MapT = typename Details::map_type;
            constexpr int MH = MapT::height;
            static_assert(MH <= 22, "map height exceeds Result::map_rows capacity");
            Result r;
            r.t = Details::t_value;
            // Copy rows in 1=empty semantics (as expected by VirtualRow in ai_misaka.cpp).
            // Map<W,H> stores 1=occupied, so we bitwise-invert each row.
            for (int y = 0; y < MH; ++y)
                r.map_rows[y] = ~static_cast<m_tetris2::row_t>(map.row(y));
            for (int y = MH; y < 22; ++y)
                r.map_rows[y] = m_tetris2::row_t(0); // sentinel: all empty (outside top)
            r.map_count = map.count();
            r.clear = clear;
            r.t_spin = lp.spin;
            return r;
        }
        Status get(Result const &eval_result, size_t depth, Status const &status, m_tetris2::AIEnv const &env) const;

    private:
        Config const *config_;
        int height_{0};
        int row_mask_{0};
    };

    template<class Spec>
    void misaka::init(Config const *config)
    {
        config_ = config;
        height_ = static_cast<int>(Spec::height);
        row_mask_ = static_cast<int>(m_tetris2::ai_rule_spec::row_mask<Spec>());
    }
}