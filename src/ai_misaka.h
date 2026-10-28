
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
        using Landing = m_tetris2::GetLanding<TSpinType>;
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
        // Result 收缩：clear / piece(t) / t_spin 依赖本次落块与消行，已移到 get
        // 现场从 args::Clear 与 Landing 重算；这里只保留纯盘面(map+Config)量：
        // raw map.row 底向上拷贝（0=occupied/1=empty 极性，VirtualRow 原样消费）与占用格数。
        struct Result
        {
            m_tetris2::row_t map_rows[22]; // copy of map rows, index 0 = bottom (y=0)
            int map_count;                // occupied cell count
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
        // 冻结 GetArgs（shard D）：Eval, Clear, Landing<L>, Parent<Status>, Env。
        // 无 Depth：旧 get 的 curdepth 仅出现在 if(0) 死分支，对数值无影响。
        using GetArgs = m_tetris2::type_list<m_tetris2::args::Eval<Result>,
                                             m_tetris2::args::Clear,
                                             m_tetris2::args::Landing<Landing>,
                                             m_tetris2::args::Parent<Status>,
                                             m_tetris2::args::Env>;

    public:
        template<class Spec>
        void init(Config const *config);
        std::string ai_name() const;
        // eval 唯一显式参数 = 落块消行后盘面。只做 raw map.rows 底向上拷贝 + popcount，
        // 极性与 VirtualRow 翻转原组合保持不变；10x22 static_assert 保留。
        template<class MapT>
        Result eval(MapT const &map) const
        {
            constexpr int MW = MapT::width;
            constexpr int MH = MapT::height;
            static_assert(MW == 10, "ai_misaka only supports width=10");
            static_assert(MH == 22, "ai_misaka only supports height=22");
            Result r;
            // dev uses 0=occupied/1=empty，VirtualRow 正好消费该原始行表示。
            // MH == 22 is guaranteed by static_assert above; fill all 22 rows.
            for (int y = 0; y < MH; ++y)
                r.map_rows[y] = static_cast<m_tetris2::row_t>(map.row(y));
            r.map_count = map.popcount();
            return r;
        }
        // get 参数按 GetArgs 声明顺序萃取：
        //   eval_result(纯盘面), clear(本次消行), landing(落块几何/piece/spin),
        //   status(父 Status), env(旧全量 env，hold/next/length 仍需要)。
        Status get(Result const &eval_result, int clear, Landing const &landing, Status const &status, m_tetris2::AIEnv const &env) const;

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
