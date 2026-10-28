
#include "tetris_core.h"
#include "tetris_eval_get.h"
#include "search_tspin.h"
#include "search_aspin.h"
#include "integer_utils.h"
#include "ai_rule_spec_utils.h"
#include "c2_combo_table.h"
#include "../cultris/c2_combo_dp.h"
#include <algorithm>
#include <array>

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
            // 纯盘面（map+Config）加权量；落块/消行项由 get 按原表达式原位置合成。
            struct Result
            {
                double map;       // 旧 result.map（ColTrans/RowTrans/空洞族/AttackDepth）
                double board_dz;  // 旧 BoardDeadZone（map_in_danger_ + roof==H ? 70 : 0）
                int danger;       // 旧 v.Danger
            };
            struct Status
            {
                double land_point;
                double attack;
                double rubbish;
                double value;
                bool operator<(Status const &) const;
            };
            using Landing = m_tetris2::GetLanding<std::monostate>;
            using GetArgs = m_tetris2::type_list<
                m_tetris2::args::Eval<Result>,
                m_tetris2::args::Landing<Landing>,
                m_tetris2::args::Clear,
                m_tetris2::args::Parent<Status>,
                m_tetris2::args::Depth>;

            template<class Spec>
            void init(Config const *config);
            std::string ai_name() const;

            template<class MapT>
            Result eval(MapT const &map) const
            {
                constexpr int map_width = MapT::width;
                constexpr int map_height = MapT::height;
                constexpr uint32_t row_full = static_cast<uint32_t>(MapT::row_full);

                int roof = map.none() ? 0 : (map.max_y() + 1);

                int top[map_width];
                for (int x = 0; x < map_width; ++x)
                {
                    top[x] = 0;
                    for (int y = map_height - 1; y >= 0; --y)
                        if ((static_cast<uint32_t>(map.occupied_row(y)) >> x) & 1u)
                        {
                            top[x] = y + 1;
                            break;
                        }
                }

                double BoardDeadZone = map_in_danger_(map);
                if (roof == map_height)
                    BoardDeadZone += 70;

                const int width_m1 = map_width - 1;
                int ColTrans = 2 * (map_height - roof);
                int RowTrans = 0;
                for (int y = 0; y < roof; ++y)
                {
                    uint32_t occ_row = static_cast<uint32_t>(map.occupied_row(y));
                    uint32_t empty_row = static_cast<uint32_t>(row_full) ^ occ_row;
                    ColTrans += (empty_row & 1u) + ((empty_row >> width_m1) & 1u) + ZZZ_BitCount((occ_row ^ (occ_row << 1)) & static_cast<uint32_t>(col_mask_));
                    if (y != 0)
                        RowTrans += ZZZ_BitCount(static_cast<uint32_t>(map.occupied_row(y - 1)) ^ occ_row);
                }
                RowTrans += ZZZ_BitCount(static_cast<uint32_t>(row_full ^ map.occupied_row(0)));
                RowTrans += ZZZ_BitCount(static_cast<uint32_t>(map.occupied_row(roof - 1)));
                struct
                {
                    int HoleCount;
                    int HoleLine;
                    int HolePosy;
                    int HolePiece;
                    int HoleDepth;
                    int WellDepth;
                    int HoleNum[m_tetris2::max_width];
                    int WellNum[m_tetris2::max_width];
                    double AttackDepth;
                    int Danger;
                    uint32_t LineCoverBits;
                    uint32_t TopHoleBits;
                } v;
                std::memset(&v, 0, sizeof v);

                for (int y = roof - 1; y >= 0; --y)
                {
                    uint32_t occ_row = static_cast<uint32_t>(map.occupied_row(y)) & static_cast<uint32_t>(row_full);
                    v.LineCoverBits |= occ_row;
                    uint32_t LineHole = v.LineCoverBits & ~occ_row & static_cast<uint32_t>(row_full);
                    if (LineHole != 0)
                    {
                        v.HoleCount += ZZZ_BitCount(LineHole);
                        v.HoleLine++;
                        if (v.HolePosy == 0)
                        {
                            v.HolePosy = y + 1;
                            v.TopHoleBits = LineHole;
                        }
                    }
                    for (int x = 1; x < width_m1; ++x)
                    {
                        if ((LineHole >> x) & 1)
                            v.HoleDepth += ++v.HoleNum[x];
                        else
                            v.HoleNum[x] = 0;
                        if (((v.LineCoverBits >> (x - 1)) & 7) == 5)
                            v.WellDepth += ++v.WellNum[x];
                    }
                    if (LineHole & 1)
                        v.HoleDepth += ++v.HoleNum[0];
                    else
                        v.HoleNum[0] = 0;
                    if ((v.LineCoverBits & 3) == 2)
                        v.WellDepth += ++v.WellNum[0];
                    if ((LineHole >> width_m1) & 1)
                        v.HoleDepth += ++v.HoleNum[width_m1];
                    else
                        v.HoleNum[width_m1] = 0;
                    if (((v.LineCoverBits >> (width_m1 - 1)) & 3) == 1)
                        v.WellDepth += ++v.WellNum[width_m1];
                }
                if (v.HolePosy != 0)
                {
                    for (int y = v.HolePosy; y < roof; ++y)
                    {
                        uint32_t CheckLine = v.TopHoleBits & static_cast<uint32_t>(row_full ^ map.occupied_row(y));
                        if (CheckLine == 0)
                            break;
                        v.HolePiece += (y + 1) * ZZZ_BitCount(CheckLine);
                    }
                }
                int low_x;
                if ((config_->mode & 1) == 0)
                {
                    low_x = 1;
                    for (int x = 2; x < width_m1; ++x)
                        if (top[x] < top[low_x])
                            low_x = x;
                    if (top[0] < top[low_x])
                        low_x = 0;
                    if (top[width_m1] < top[low_x])
                        low_x = width_m1;
                }
                else
                {
                    low_x = (top[width_m1 - 3] <= top[width_m1 - 4]) ? width_m1 - 3 : width_m1 - 4;
                    if (top[width_m1 - 2] <= top[low_x])
                        low_x = width_m1 - 2;
                }
                int low_y = top[low_x];
                for (int y = roof - 1; y >= low_y; --y)
                {
                    uint32_t occ_row = static_cast<uint32_t>(map.occupied_row(y));
                    if (std::binary_search<uint32_t const *>(check_line_1_, check_line_1_end_, occ_row))
                    {
                        if (y + 1 < map_height && std::binary_search<uint32_t const *>(check_line_2_, check_line_2_end_, static_cast<uint32_t>(map.occupied_row(y + 1))))
                            v.AttackDepth += 20;
                        else
                            v.AttackDepth += 16;
                        for (--y; y >= low_y; --y)
                        {
                            if (std::binary_search<uint32_t const *>(check_line_1_, check_line_1_end_, static_cast<uint32_t>(map.occupied_row(y))))
                                v.AttackDepth += 3;
                            else
                                v.AttackDepth -= 5;
                        }
                        break;
                    }
                    else
                    {
                        v.AttackDepth -= 2;
                    }
                }
                v.Danger = 5 - std::min(map_height - low_y, 5);
                if (v.Danger > 0 && v.AttackDepth < 20)
                    v.AttackDepth = 0;

                Result result;
                result.map = (0. - ColTrans * 32 - RowTrans * 32 - v.HoleCount * 400 - v.HoleLine * 38 - v.WellDepth * 16 - v.HoleDepth * 4 - v.HolePiece * 2 + v.AttackDepth * 100);
                result.board_dz = BoardDeadZone;
                result.danger = v.Danger;
                return result;
            }

            Status get(Result const &eval_result, Landing const &landing, int clear,
                       Status const &status, size_t depth) const;

        private:
            uint32_t check_line_1_[32];
            uint32_t check_line_2_[32];
            uint32_t *check_line_1_end_;
            uint32_t *check_line_2_end_;
            Config const *config_;
            size_t type_max_{0};
            int width_{0};
            int height_{0};
            int col_mask_, row_mask_;
            struct MapInDangerData
            {
                uint32_t data[4];
            };
            std::vector<MapInDangerData> map_danger_data_;
            template<class MapT>
            size_t map_in_danger_(MapT const &map) const
            {
                constexpr int H = MapT::height;
                size_t danger = 0;
                for (size_t i = 0; i < type_max_; ++i)
                {
                    uint32_t occ0 = static_cast<uint32_t>(map.occupied_row(H - 4));
                    uint32_t occ1 = static_cast<uint32_t>(map.occupied_row(H - 3));
                    uint32_t occ2 = static_cast<uint32_t>(map.occupied_row(H - 2));
                    uint32_t occ3 = static_cast<uint32_t>(map.occupied_row(H - 1));
                    if (static_cast<uint32_t>(map_danger_data_[i].data[0]) & occ0 ||
                        static_cast<uint32_t>(map_danger_data_[i].data[1]) & occ1 ||
                        static_cast<uint32_t>(map_danger_data_[i].data[2]) & occ2 ||
                        static_cast<uint32_t>(map_danger_data_[i].data[3]) & occ3)
                    {
                        ++danger;
                    }
                }
                return danger;
            }
        };

    }

    class Dig
    {
    public:
        using Result = double;
        using Status = double;  // TetrisCore 要求 AI::Status 别名；Dig 无嵌套 Status，语义同 double
        struct Config
        {
            std::array<double, 100> p =
                {
                    0,
                    1,
                    0,
                    1,
                    0,
                    1,
                    0,
                    96,
                    0,
                    160,
                    0,
                    128,
                    0,
                    60,
                    0,
                    380,
                    0,
                    100,
                    0,
                    40,
                    0,
                    50000,
                    32,
                    0.25,
                };
        };
        // 仅 Eval；恒等 get（不请求 depth/parent/env/clear）。
        using GetArgs = m_tetris2::type_list<m_tetris2::args::Eval<double>>;

        template<class Spec>
        void init(Config const *config);
        std::string ai_name() const;
        template<class MapT>
        double eval(MapT const &map) const
        {
            using row_t = typename MapT::row_t;
            constexpr int W = MapT::width;
            constexpr int H = MapT::height;
            constexpr row_t full = MapT::row_full;
            constexpr row_t col_mask = static_cast<row_t>(full & ~row_t(1));
            const int width_m1 = W - 1;

            int roof = map.none() ? 0 : (map.max_y() + 1);

            size_t ColTrans = static_cast<size_t>(2 * (H - roof));

            size_t RowTrans = 0;
            if (roof > 0)
            {
                RowTrans = static_cast<size_t>(ZZZ_BitCount(static_cast<uint32_t>(full ^ map.occupied_row(0))));
                RowTrans += static_cast<size_t>(ZZZ_BitCount(static_cast<uint32_t>(map.occupied_row(roof - 1))));
            }

            for (int y = 0; y < roof; ++y)
            {
                row_t empty_row = static_cast<row_t>(full ^ map.occupied_row(y));
                ColTrans += static_cast<size_t>((empty_row & 1)) + static_cast<size_t>((static_cast<uint32_t>(empty_row) >> width_m1) & 1u) + static_cast<size_t>(ZZZ_BitCount(static_cast<uint32_t>(static_cast<row_t>(empty_row ^ static_cast<row_t>(empty_row << 1)) & col_mask)));
                if (y != 0)
                {
                    RowTrans += static_cast<size_t>(ZZZ_BitCount(static_cast<uint32_t>(map.occupied_row(y - 1) ^ map.occupied_row(y))));
                }
            }

            struct
            {
                int HoleCount;
                int HoleLine;

                double HoleDepth;
                double WellDepth;

                int HoleNum[m_tetris2::max_width];
                int WellNum[m_tetris2::max_width];

                int LineCoverBits;
                int HolePosyIndex;
            } v;
            std::memset(&v, 0, sizeof v);
            struct
            {
                double ClearWidth;
            } a[40];

            for (int y = roof - 1; y >= 0; --y)
            {
                uint32_t occ_row = static_cast<uint32_t>(map.occupied_row(y));
                v.LineCoverBits |= static_cast<int>(occ_row);
                int LineHole = v.LineCoverBits & ~static_cast<int>(occ_row);
                if (LineHole != 0)
                {
                    v.HoleCount += ZZZ_BitCount(static_cast<uint32_t>(LineHole));
                    ++v.HoleLine;
                    a[v.HolePosyIndex].ClearWidth = 0;
                    for (int hy = y + 1; hy < roof; ++hy)
                    {
                        uint32_t CheckLine = static_cast<uint32_t>(LineHole) & static_cast<uint32_t>(map.occupied_row(hy));
                        if (CheckLine == 0)
                        {
                            break;
                        }
                        a[v.HolePosyIndex].ClearWidth += (hy + 1 + config_->p[0]) * config_->p[1] * ZZZ_BitCount(CheckLine);
                    }
                    ++v.HolePosyIndex;
                }
                for (int x = 1; x < width_m1; ++x)
                {
                    if ((LineHole >> x) & 1)
                    {
                        v.HoleDepth += (++v.HoleNum[x] + config_->p[4]) * config_->p[5];
                    }
                    else
                    {
                        v.HoleNum[x] = 0;
                    }
                    if (((v.LineCoverBits >> (x - 1)) & 7) == 5)
                    {
                        v.WellDepth += (++v.WellNum[x] + config_->p[2]) * config_->p[3];
                    }
                }
                if (LineHole & 1)
                {
                    v.HoleDepth += (++v.HoleNum[0] + config_->p[4]) * config_->p[5];
                }
                else
                {
                    v.HoleNum[0] = 0;
                }
                if ((v.LineCoverBits & 3) == 2)
                {
                    v.WellDepth += (++v.WellNum[0] + config_->p[2]) * config_->p[3];
                }
                if ((LineHole >> width_m1) & 1)
                {
                    v.HoleDepth += (++v.HoleNum[width_m1] + config_->p[4]) * config_->p[5];
                }
                else
                {
                    v.HoleNum[width_m1] = 0;
                }
                if (((v.LineCoverBits >> (width_m1 - 1)) & 3) == 1)
                {
                    v.WellDepth += (++v.WellNum[width_m1] + config_->p[2]) * config_->p[3];
                }
            }

            size_t BoardDeadZone = map_in_danger_(map);

            double value = (0. - (roof + config_->p[6]) * config_->p[7] - (ColTrans + config_->p[8]) * config_->p[9] - (RowTrans + config_->p[10]) * config_->p[11] - (v.HoleCount + config_->p[12]) * config_->p[13] - (v.HoleLine + config_->p[14]) * config_->p[15] - (v.WellDepth + config_->p[16]) * config_->p[17] - (v.HoleDepth + config_->p[18]) * config_->p[19] - (BoardDeadZone + config_->p[20]) * config_->p[21]);
            double rate = config_->p[22], mul = config_->p[23];
            for (int i = 0; i < v.HolePosyIndex; ++i, rate *= mul)
            {
                value -= a[i].ClearWidth * rate;
            }
            return value;
        }
        double get(double const &eval_result) const
        {
            return eval_result;
        }

    private:
        struct MapInDangerData
        {
            int data[4];
        };
        std::vector<MapInDangerData> map_danger_data_;
        size_t type_max_{0};
        Config const *config_;
        template<class MapT>
        size_t map_in_danger_(MapT const &map) const
        {
            constexpr int H = MapT::height;
            size_t danger = 0;
            for (size_t i = 0; i < type_max_; ++i)
            {
                uint32_t occ0 = static_cast<uint32_t>(map.occupied_row(H - 4));
                uint32_t occ1 = static_cast<uint32_t>(map.occupied_row(H - 3));
                uint32_t occ2 = static_cast<uint32_t>(map.occupied_row(H - 2));
                uint32_t occ3 = static_cast<uint32_t>(map.occupied_row(H - 1));
                if (static_cast<uint32_t>(map_danger_data_[i].data[0]) & occ0 ||
                    static_cast<uint32_t>(map_danger_data_[i].data[1]) & occ1 ||
                    static_cast<uint32_t>(map_danger_data_[i].data[2]) & occ2 ||
                    static_cast<uint32_t>(map_danger_data_[i].data[3]) & occ3)
                {
                    ++danger;
                }
            }
            return danger;
        }
        int col_mask_, row_mask_;
    };


    // TSpinHook emits the one-line-clear class; the oracle AIs derived the class per clear count in get.
    inline search_tspin::Search::TSpinType tspin_for_clear(search_tspin::Search::TSpinType spin, size_t clear)
    {
        using Search = search_tspin::Search;
        if (clear == 0)
        {
            return Search::None;
        }
        return spin == Search::TSpinMini && clear != 1 ? Search::TSpin : spin;
    }

    class TOJ_PC
    {
    public:
        typedef search_tspin::Search::TSpinType TSpinType;
        using EvalSpinType = TSpinType;
        struct Config
        {
            int const *table;
            int table_max;
        };
        // 纯盘面量；clear / t_spin 由 get 现场重放。
        struct Result
        {
            double value;
            int roof;
        };
        struct Status
        {
            int under_attack;
            int recv_attack;
            int attack;
            int like;
            int combo;
            bool b2b;
            bool pc;
            double value;
            bool operator<(Status const &) const;
        };
        using Landing = m_tetris2::GetLanding<TSpinType>;
        using GetArgs = m_tetris2::type_list<
            m_tetris2::args::Eval<Result>,
            m_tetris2::args::Clear,
            m_tetris2::args::Landing<Landing>,
            m_tetris2::args::Parent<Status>>;

        template<class Spec>
        void init(Config const *config);
        std::string ai_name() const;
        double ratio() const
        {
            return 0.5;
        }
        template<class MapT>
        Result eval(MapT const &map) const
        {
            constexpr int map_width = MapT::width;
            constexpr int map_height = MapT::height;
            constexpr uint32_t row_full = static_cast<uint32_t>(MapT::row_full);

            int roof = map.none() ? 0 : (map.max_y() + 1);

            int ColTrans = 2 * (map_height - roof);
            int RowTrans = 0;
            if (roof > 0)
            {
                for (int y = 0; y < roof; ++y)
                {
                    uint64_t row = static_cast<uint64_t>(map.occupied_row(y));
                    ColTrans += ZZZ_BitCount(row ^ (row << 1));
                    if (y != 0)
                    {
                        RowTrans += ZZZ_BitCount(static_cast<uint32_t>(map.occupied_row(y - 1)) ^ static_cast<uint32_t>(map.occupied_row(y)));
                    }
                }
                RowTrans += ZZZ_BitCount(static_cast<uint32_t>(row_full ^ map.occupied_row(0)));
                RowTrans += ZZZ_BitCount(static_cast<uint32_t>(map.occupied_row(roof - 1)));
            }

            Result result;
            result.value = (roof > 4 ? 0 : 10000) - ColTrans * 3 - RowTrans * 2;
            result.roof = roof;
            return result;
        }
        Status get(Result const &eval_result, int clear, Landing const &landing, Status const &status) const;

    private:
        Config const *config_;
        int col_mask_, row_mask_;
    };


    class Botris_PC
    {
    public:
        typedef search_aspin::Search::ASpinType ASpinType;
        using EvalSpinType = ASpinType;
        struct Config
        {
            int const *table;
            int table_max;
        };
        // 纯盘面量；clear / a_spin 由 get 现场重放（ASpin 直接赋值，不经 tspin_for_clear）。
        struct Result
        {
            double value;
            int roof;
        };
        struct Status
        {
            int under_attack;
            int recv_attack;
            int attack;
            int like;
            int combo;
            bool b2b;
            bool pc;
            double value;
            bool operator<(Status const &) const;
        };
        using Landing = m_tetris2::GetLanding<ASpinType>;
        using GetArgs = m_tetris2::type_list<
            m_tetris2::args::Eval<Result>,
            m_tetris2::args::Clear,
            m_tetris2::args::Landing<Landing>,
            m_tetris2::args::Parent<Status>>;

        template<class Spec>
        void init(Config const *config);
        std::string ai_name() const;
        double ratio() const
        {
            return 0.5;
        }
        template<class MapT>
        Result eval(MapT const &map) const
        {
            constexpr int map_width = MapT::width;
            constexpr int map_height = MapT::height;
            constexpr uint32_t row_full = static_cast<uint32_t>(MapT::row_full);

            int roof = map.none() ? 0 : (map.max_y() + 1);

            const int width_m1 = map_width - 1;
            int ColTrans = 2 * (map_height - roof);
            int RowTrans = 0;
            if (roof > 0)
            {
                for (int y = 0; y < roof; ++y)
                {
                    uint64_t row = static_cast<uint64_t>(map.occupied_row(y));
                    ColTrans += ZZZ_BitCount(row ^ (row << 1));
                    if (y != 0)
                    {
                        RowTrans += ZZZ_BitCount(static_cast<uint32_t>(map.occupied_row(y - 1)) ^ static_cast<uint32_t>(map.occupied_row(y)));
                    }
                }
                RowTrans += ZZZ_BitCount(static_cast<uint32_t>(row_full ^ map.occupied_row(0)));
                RowTrans += ZZZ_BitCount(static_cast<uint32_t>(map.occupied_row(roof - 1)));
            }

            Result result;
            result.value = (roof > 4 ? 0 : 10000) - ColTrans * 3 - RowTrans * 2;
            result.roof = roof;
            return result;
        }
        Status get(Result const &eval_result, int clear, Landing const &landing, Status const &status) const;

    private:
        Config const *config_;
        int col_mask_, row_mask_;
    };


    class TOJ_v08
    {
    public:
        typedef search_tspin::Search::TSpinType TSpinType;
        using EvalSpinType = TSpinType;
        using Landing = m_tetris2::GetLanding<TSpinType>;
        struct Config
        {
            int const *table;
            int table_max;
        };
        // Result 收缩：clear/t_spin/top_out 依赖本次落块与消行，已移到 get 现场重算；
        // 这里只保留纯盘面(map+Config)加权量。
        struct Result
        {
            double value;
            int count;
            int t2_value;
            int t3_value;
            int roof;
            int8_t safe_cache[8]; // indexed by cached piece index, size >= type_max
        };
        struct Status
        {
            int max_combo;
            int max_attack;
            int death;
            int combo;
            int attack;
            int under_attack;
            int map_rise;
            bool b2b;
            double like;
            double value;
            bool operator<(Status const &) const;
        };
        using GetArgs = m_tetris2::type_list<m_tetris2::args::Eval<Result>, m_tetris2::args::Clear,
                                             m_tetris2::args::Landing<Landing>, m_tetris2::args::Parent<Status>,
                                             m_tetris2::args::Depth, m_tetris2::args::Env>;

    public:
        template<class Spec>
        void init(Config const *config);
        std::string ai_name() const;
        double ratio() const
        {
            return 1.5;
        }
        template<class MapT>
        Result eval(MapT const &map) const
        {
            constexpr int map_width = MapT::width;
            constexpr int map_height = MapT::height;
            constexpr uint32_t row_full = static_cast<uint32_t>(MapT::row_full);

            int roof = map.none() ? 0 : (map.max_y() + 1);

            const int width_m1 = map_width - 1;
            int ColTrans = 2 * (map_height - roof);
            int RowTrans = 0;
            for (int y = 0; y < roof; ++y)
            {
                uint32_t occ_row = static_cast<uint32_t>(map.occupied_row(y));
                ColTrans += !(occ_row & 1u) + !((occ_row >> width_m1) & 1u) + ZZZ_BitCount((occ_row ^ (occ_row << 1)) & static_cast<uint32_t>(col_mask_));
                if (y != 0)
                    RowTrans += ZZZ_BitCount(static_cast<uint32_t>(map.occupied_row(y - 1)) ^ occ_row);
            }
            RowTrans += ZZZ_BitCount(static_cast<uint32_t>(row_full ^ map.occupied_row(0)));
            RowTrans += ZZZ_BitCount(static_cast<uint32_t>(map.occupied_row(roof - 1)));
            struct
            {
                int HoleCount;
                int HoleLine;
                int HoleDepth;
                int WellDepth;
                int HoleNum[m_tetris2::max_width];
                int WellNum[m_tetris2::max_width];
                int LineCoverBits;
                int HolePosyIndex;
            } v;
            std::memset(&v, 0, sizeof v);
            struct
            {
                int ClearWidth;
            } a[40];

            for (int y = roof - 1; y >= 0; --y)
            {
                // occupied bits (1=occupied) for hole detection
                uint32_t inv_row = static_cast<uint32_t>(map.occupied_row(y));
                v.LineCoverBits |= inv_row;
                uint32_t LineHole = v.LineCoverBits ^ inv_row;
                if (LineHole != 0)
                {
                    ++v.HoleLine;
                    a[v.HolePosyIndex].ClearWidth = 0;
                    for (int hy = y + 1; hy < roof; ++hy)
                    {
                        // occupied bits at hy: map.occupied_row(hy); hole bits are empty above, occupied at hy
                        uint32_t CheckLine = LineHole & static_cast<uint32_t>(map.occupied_row(hy));
                        if (CheckLine == 0)
                            break;
                        a[v.HolePosyIndex].ClearWidth += (hy + 1) * ZZZ_BitCount(CheckLine);
                    }
                    ++v.HolePosyIndex;
                }
                for (int x = 1; x < width_m1; ++x)
                {
                    if ((LineHole >> x) & 1)
                        v.HoleDepth += ++v.HoleNum[x];
                    else
                        v.HoleNum[x] = 0;
                    if (((v.LineCoverBits >> (x - 1)) & 7) == 5)
                        v.WellDepth += ++v.WellNum[x];
                }
                if (LineHole & 1)
                    v.HoleDepth += ++v.HoleNum[0];
                else
                    v.HoleNum[0] = 0;
                if ((v.LineCoverBits & 3) == 2)
                    v.WellDepth += ++v.WellNum[0];
                if ((LineHole >> width_m1) & 1)
                    v.HoleDepth += ++v.HoleNum[width_m1];
                else
                    v.HoleNum[width_m1] = 0;
                if (((v.LineCoverBits >> (width_m1 - 1)) & 3) == 1)
                    v.WellDepth += ++v.WellNum[width_m1];
            }

            // Compute occupied cell count
            int occ_count = 0;
            for (int y = 0; y < roof; ++y)
                occ_count += ZZZ_BitCount(static_cast<uint32_t>(map.occupied_row(y)));

            Result result;
            result.value = (0. - roof * 128 - ColTrans * 160 - RowTrans * 160 - v.HoleCount * 80 - v.HoleLine * 380 - v.WellDepth * 100 - v.HoleDepth * 40);
            double rate = 32, mul = 1.0 / 4;
            for (int i = 0; i < v.HolePosyIndex; ++i, rate *= mul)
                result.value -= a[i].ClearWidth * rate;
            result.count = occ_count + v.HoleCount;
            result.t2_value = 0;
            result.t3_value = 0;
            result.roof = roof;

            // Precompute safe_cache for all piece types
            std::memset(result.safe_cache, 0, sizeof(result.safe_cache));
            for (size_t ti = 0; ti < type_max_ && ti < 8; ++ti)
            {
                int safe = 0;
                while (map_in_danger_(map, ti, safe + 1) == 0)
                    ++safe;
                result.safe_cache[ti] = static_cast<int8_t>(safe);
            }

            bool finding2 = true;
            bool finding3 = true;
            for (int y = 0; (finding2 || finding3) && y < roof - 2; ++y)
            {
                // empty bits (1=empty) for T-spin shape detection
                int row0 = static_cast<int>(static_cast<uint32_t>(row_full ^ map.occupied_row(y)));
                int row1 = static_cast<int>(static_cast<uint32_t>(row_full ^ map.occupied_row(y + 1)));
                int row2 = y + 2 < map_height ? static_cast<int>(static_cast<uint32_t>(row_full ^ map.occupied_row(y + 2))) : 0;
                int row3 = y + 3 < map_height ? static_cast<int>(static_cast<uint32_t>(row_full ^ map.occupied_row(y + 3))) : 0;
                int row4 = y + 4 < map_height ? static_cast<int>(static_cast<uint32_t>(row_full ^ map.occupied_row(y + 4))) : 0;
                for (int x = 0; finding2 && x < map_width - 2; ++x)
                {
                    if (((row0 >> x) & 7) == 5 && ((row1 >> x) & 7) == 0)
                    {
                        if (ZZZ_BitCount(row0) == map_width - 1)
                        {
                            result.t2_value += 1;
                            if (ZZZ_BitCount(row1) == map_width - 3)
                            {
                                result.t2_value += 2;
                                int row2_check = (row2 >> x) & 7;
                                if (row2_check == 1 || row2_check == 4)
                                    result.t2_value += 2;
                                finding2 = false;
                            }
                        }
                    }
                }
                for (int x = 0; finding3 && x < map_width - 3; ++x)
                {
                    if (((row0 >> x) & 15) == 11 && ((row1 >> x) & 15) == 9)
                    {
                        int t3_value = 0;
                        if (ZZZ_BitCount(row0) == map_width - 1)
                        {
                            t3_value += 1;
                            if (ZZZ_BitCount(row1) == map_width - 2)
                            {
                                t3_value += 1;
                                if (((row2 >> x) & 15) == 11)
                                {
                                    t3_value += 2;
                                    if (ZZZ_BitCount(row2) == map_width - 1)
                                        t3_value += 2;
                                }
                                int row3_check = ((row3 >> x) & 15);
                                if (row3_check == 8 || row3_check == 0)
                                {
                                    t3_value += !!row3_check;
                                    int row4_check = ((row4 >> x) & 15);
                                    if (row4_check == 4 || row4_check == 12)
                                        t3_value += 1;
                                    else
                                        t3_value -= 2;
                                }
                                else
                                {
                                    t3_value = 0;
                                }
                            }
                        }
                        result.t3_value += t3_value;
                        if (t3_value > 3)
                            finding3 = false;
                    }
                    if (((row0 >> x) & 15) == 13 && ((row1 >> x) & 15) == 9)
                    {
                        int t3_value = 0;
                        if (ZZZ_BitCount(row0) == map_width - 1)
                        {
                            t3_value += 1;
                            if (ZZZ_BitCount(row1) == map_width - 2)
                            {
                                t3_value += 1;
                                if (((row2 >> x) & 15) == 13)
                                {
                                    t3_value += 2;
                                    if (ZZZ_BitCount(row2) == map_width - 1)
                                        t3_value += 2;
                                }
                                int row3_check = ((row3 >> x) & 15);
                                if (row3_check == 1 || row3_check == 0)
                                {
                                    t3_value += !!row3_check;
                                    int row4_check = ((row4 >> x) & 15);
                                    if (row4_check == 3 || row4_check == 1)
                                        t3_value += 1;
                                    else
                                        t3_value -= 2;
                                }
                                else
                                {
                                    t3_value = 0;
                                }
                            }
                        }
                        result.t3_value += t3_value;
                        if (t3_value > 3)
                            finding3 = false;
                    }
                }
            }
            return result;
        }

        // 新 get：参数按 GetArgs 声明顺序萃取。clear/t_spin/top_out 在 get 现场重算。
        Status get(Result const &eval_result, int clear, Landing const &landing, Status const &parent, size_t depth, m_tetris2::AIEnv const &env) const;

    private:
        size_t type_max_{0};
        std::array<int, 256> piece_index_{};
        Config const *config_;
        int width_{0};
        int col_mask_, row_mask_;
        int full_count_;
        struct MapInDangerData
        {
            int data[4];
        };
        std::vector<MapInDangerData> map_danger_data_;
        template<class MapT>
        size_t map_in_danger_(MapT const &map, size_t t, size_t up) const
        {
            if (up >= 20)
                return 1;
            int MH = static_cast<int>(map.height);
            size_t height = 22 - up;
            auto get_row = [](MapT const &m, int y) -> uint32_t {
                // dev uses 0=occupied/1=empty; danger masks require a 1=occupied row.
                return static_cast<uint32_t>(m.occupied_row(y));
            };
            uint32_t e0 = (height >= 4 && static_cast<int>(height - 4) < MH) ? get_row(map, static_cast<int>(height - 4)) : 0u;
            uint32_t e1 = (height >= 3 && static_cast<int>(height - 3) < MH) ? get_row(map, static_cast<int>(height - 3)) : 0u;
            uint32_t e2 = (height >= 2 && static_cast<int>(height - 2) < MH) ? get_row(map, static_cast<int>(height - 2)) : 0u;
            uint32_t e3 = (height >= 1 && static_cast<int>(height - 1) < MH) ? get_row(map, static_cast<int>(height - 1)) : 0u;
            return (static_cast<uint32_t>(map_danger_data_[t].data[0]) & e0) | (static_cast<uint32_t>(map_danger_data_[t].data[1]) & e1) | (static_cast<uint32_t>(map_danger_data_[t].data[2]) & e2) | (static_cast<uint32_t>(map_danger_data_[t].data[3]) & e3);
        }
    };

    class Botris
    {
    public:
        typedef search_aspin::Search::ASpinType ASpinType;
        using EvalSpinType = ASpinType;
        using Landing = m_tetris2::GetLanding<ASpinType>;
        struct Config
        {
            int const *table;
            int table_max;
        };
        // Result 收缩：clear/a_spin/top_out 依赖本次落块与消行，已移到 get 现场重算。
        struct Result
        {
            double value;
            int count;
            int roof;
            int8_t safe_cache[8]; // indexed by cached piece index
        };
        struct Status
        {
            int max_combo;
            int max_attack;
            int death;
            int combo;
            int attack;
            int clear;
            int under_attack;
            int map_rise;
            int b2b;
            double like;
            double value;
            bool operator<(Status const &) const;
        };
        using GetArgs = m_tetris2::type_list<m_tetris2::args::Eval<Result>, m_tetris2::args::Clear,
                                             m_tetris2::args::Landing<Landing>, m_tetris2::args::Parent<Status>,
                                             m_tetris2::args::Depth, m_tetris2::args::Env>;

    public:
        template<class Spec>
        void init(Config const *config);
        std::string ai_name() const;
        double ratio() const
        {
            return 1.5;
        }
        template<class MapT>
        Result eval(MapT const &map) const
        {
            using MapT_unused = void;
            constexpr int map_width = MapT::width;
            constexpr int map_height = MapT::height;
            constexpr uint32_t row_full = static_cast<uint32_t>(MapT::row_full);

            int roof = map.none() ? 0 : (map.max_y() + 1);

            const int width_m1 = map_width - 1;
            int ColTrans = 2 * (map_height - roof);
            int RowTrans = 0;
            if (roof > 0)
            {
                for (int y = 0; y < roof; ++y)
                {
                    uint64_t row = static_cast<uint64_t>(map.occupied_row(y));
                    ColTrans += ZZZ_BitCount(row ^ (row << 1));
                    if (y != 0)
                    {
                        RowTrans += ZZZ_BitCount(static_cast<uint32_t>(map.occupied_row(y - 1)) ^ static_cast<uint32_t>(map.occupied_row(y)));
                    }
                }
                RowTrans += ZZZ_BitCount(static_cast<uint32_t>(row_full ^ map.occupied_row(0)));
                RowTrans += ZZZ_BitCount(static_cast<uint32_t>(map.occupied_row(roof - 1)));
            }

            struct
            {
                int HoleCount;
                int HoleLine;

                int HoleDepth;
                int WellDepth;

                int HoleNum[m_tetris2::max_width];
                int WellNum[m_tetris2::max_width];

                uint32_t LineCoverBits;
                int HolePosyIndex;
            } v;
            std::memset(&v, 0, sizeof v);
            struct
            {
                int ClearWidth;
            } a[40];

            for (int y = roof - 1; y >= 0; --y)
            {
                uint32_t inv_row = static_cast<uint32_t>(map.occupied_row(y));
                v.LineCoverBits |= inv_row;
                uint32_t LineHole = v.LineCoverBits ^ inv_row;
                if (LineHole != 0)
                {
                    ++v.HoleLine;
                    a[v.HolePosyIndex].ClearWidth = 0;
                    for (int hy = y + 1; hy < roof; ++hy)
                    {
                        uint32_t CheckLine = LineHole & static_cast<uint32_t>(map.occupied_row(hy));
                        if (CheckLine == 0)
                        {
                            break;
                        }
                        a[v.HolePosyIndex].ClearWidth += (hy + 1) * ZZZ_BitCount(CheckLine);
                    }
                    ++v.HolePosyIndex;
                }
                for (int x = 1; x < width_m1; ++x)
                {
                    if ((LineHole >> x) & 1)
                    {
                        v.HoleDepth += ++v.HoleNum[x];
                    }
                    else
                    {
                        v.HoleNum[x] = 0;
                    }
                    if (((v.LineCoverBits >> (x - 1)) & 7) == 5)
                    {
                        v.WellDepth += ++v.WellNum[x];
                    }
                }
                if (LineHole & 1)
                {
                    v.HoleDepth += ++v.HoleNum[0];
                }
                else
                {
                    v.HoleNum[0] = 0;
                }
                if ((v.LineCoverBits & 3) == 2)
                {
                    v.WellDepth += ++v.WellNum[0];
                }
                if ((LineHole >> width_m1) & 1)
                {
                    v.HoleDepth += ++v.HoleNum[width_m1];
                }
                else
                {
                    v.HoleNum[width_m1] = 0;
                }
                if (((v.LineCoverBits >> (width_m1 - 1)) & 3) == 1)
                {
                    v.WellDepth += ++v.WellNum[width_m1];
                }
            }

            // Compute occupied cell count
            int occ_count = 0;
            for (int y = 0; y < roof; ++y)
                occ_count += ZZZ_BitCount(static_cast<uint32_t>(map.occupied_row(y)));

            Result result;
            result.value = (0. - roof * 128 - ColTrans * 160 - RowTrans * 160 - v.HoleCount * 80 - v.HoleLine * 380 - v.WellDepth * 100 - v.HoleDepth * 40);
            double rate = 32, mul = 1.0 / 4;
            for (int i = 0; i < v.HolePosyIndex; ++i, rate *= mul)
            {
                result.value -= a[i].ClearWidth * rate;
            }
            result.count = occ_count + v.HoleCount;
            result.roof = roof;

            // Precompute safe_cache for all piece types
            std::memset(result.safe_cache, 0, sizeof(result.safe_cache));
            for (size_t ti = 0; ti < type_max_ && ti < 8; ++ti)
            {
                int safe = 0;
                while (map_in_danger_(map, ti, safe + 1) == 0)
                    ++safe;
                result.safe_cache[ti] = static_cast<int8_t>(safe);
            }
            return result;
        }
        // 新 get：参数按 GetArgs 声明顺序萃取。clear/a_spin/top_out 在 get 现场重算。
        Status get(Result const &eval_result, int clear, Landing const &landing, Status const &parent, size_t depth, m_tetris2::AIEnv const &env) const;

    private:
        size_t type_max_{0};
        std::array<int, 256> piece_index_{};
        Config const *config_;
        int width_{0};
        int col_mask_, row_mask_;
        int full_count_;
        struct MapInDangerData
        {
            int data[4];
        };
        std::vector<MapInDangerData> map_danger_data_;
        template<class MapT>
        size_t map_in_danger_(MapT const &map, size_t t, size_t up) const
        {
            if (up >= 20)
                return 1;
            int MH = static_cast<int>(map.height);
            size_t height = 22 - up;
            auto get_row = [](MapT const &m, int y) -> uint32_t {
                // dev uses 0=occupied/1=empty; danger masks require a 1=occupied row.
                return static_cast<uint32_t>(m.occupied_row(y));
            };
            uint32_t e0 = (height >= 4 && static_cast<int>(height - 4) < MH) ? get_row(map, static_cast<int>(height - 4)) : 0u;
            uint32_t e1 = (height >= 3 && static_cast<int>(height - 3) < MH) ? get_row(map, static_cast<int>(height - 3)) : 0u;
            uint32_t e2 = (height >= 2 && static_cast<int>(height - 2) < MH) ? get_row(map, static_cast<int>(height - 2)) : 0u;
            uint32_t e3 = (height >= 1 && static_cast<int>(height - 1) < MH) ? get_row(map, static_cast<int>(height - 1)) : 0u;
            return (static_cast<uint32_t>(map_danger_data_[t].data[0]) & e0) | (static_cast<uint32_t>(map_danger_data_[t].data[1]) & e1) | (static_cast<uint32_t>(map_danger_data_[t].data[2]) & e2) | (static_cast<uint32_t>(map_danger_data_[t].data[3]) & e3);
        }
    };

    class TOJ
    {
    public:
        typedef search_tspin::Search::TSpinType TSpinType;
        using EvalSpinType = TSpinType;

        template<class MapT>
        int get_safe(MapT const &map, char type) const
        {
            int safe = 0;
            int t = piece_index_[static_cast<unsigned char>(type)];
            if (t < 0)
                return safe;
            while (map_in_danger_(map, static_cast<size_t>(t), safe + 1) == 0)
                ++safe;
            return safe;
        }
        struct Param
        {
            double base = 40;
            double roof = 160;
            double col_trans = 160;
            double row_trans = 160;
            double hole_count = 256;
            double hole_line = 256;
            double clear_width = 24;
            double wide_2 = -64;
            double wide_3 = -64;
            double wide_4 = 8;
            double safe = 16;
            double b2b = 128;
            double attack = 128;
            double hold_t = 0.25;
            double hold_i = 0.25;
            double waste_t = -16;
            double waste_i = -8;
            double clear_1 = -64;
            double clear_2 = -64;
            double clear_3 = -64;
            double clear_4 = 0;
            double t2_slot = 0.75;
            double t3_slot = 0.75;
            double tspin_mini = -2;
            double tspin_1 = 0;
            double tspin_2 = 4;
            double tspin_3 = 4;
            double combo = 80;
            double ratio = 0;
        };
        struct Config
        {
            int const *table;
            int table_max;
            int safe;
            Param param;
        };
        // Result 收缩：clear/top_out/t_spin/piece_type 依赖本次落块与消行，移到 get 现场重算。
        struct Result
        {
            double value;
            int16_t count;
            int16_t t2_value;
            int16_t t3_value;
            int roof;
            int8_t safe_cache[8];
        };
        struct Status
        {
            int8_t death;
            int8_t combo;
            int8_t under_attack;
            int8_t map_rise;
            int8_t b2b;
            int16_t t2_value;
            int16_t t3_value;
            double acc_value;
            double like;
            double value;
            bool operator<(Status const &) const;

            static void init_t_value(uint32_t const *rows, int width, int roof, int16_t &t2_value_ref, int16_t &t3_value_ref, uint32_t *out_rows = nullptr);

            template<class MapT>
            static void init_t_value(MapT const &map, int16_t &t2_value_ref, int16_t &t3_value_ref)
            {
                uint32_t occ_rows[40];
                for (int y = 0; y < MapT::height; ++y)
                    occ_rows[y] = static_cast<uint32_t>(map.occupied_row(y));
                init_t_value(occ_rows, MapT::width, map.max_y() + (map.none() ? 0 : 1), t2_value_ref, t3_value_ref);
            }
        };

    public:
        template<class Spec>
        void init(Config const *config);
        std::string ai_name() const;
        double ratio() const
        {
            return config_->param.ratio;
        }
        using Landing = m_tetris2::GetLanding<TSpinType>;
        using GetArgs = m_tetris2::type_list<m_tetris2::args::Eval<Result>, m_tetris2::args::Clear,
                                             m_tetris2::args::Landing<Landing>, m_tetris2::args::Parent<Status>,
                                             m_tetris2::args::Env>;

    public:
        template<class MapT>
        Result eval(MapT const &map) const
        {
            constexpr int map_width = MapT::width;
            constexpr int map_height = MapT::height;
            constexpr uint32_t row_full = static_cast<uint32_t>(MapT::row_full);

            int roof = map.none() ? 0 : (map.max_y() + 1);

            Result result;
            memset(&result, 0, sizeof result);

            // Build a local row array (1=occupied) and compute t-value with in-place overlay.
            uint32_t t_rows[40];
            for (int y = 0; y < roof; ++y)
                t_rows[y] = static_cast<uint32_t>(map.occupied_row(y));
            memset(t_rows + roof, 0, sizeof(uint32_t) * (40 - roof));
            Status::init_t_value(t_rows, map_width, roof, result.t2_value, result.t3_value, t_rows);

            const int width_m1 = map_width - 1;
            size_t ColTrans = 2 * (map_height - roof);
            size_t RowTrans = 0;
            if (roof > 0)
            {
                for (int y = 0; y < roof; ++y)
                {
                    uint64_t row = static_cast<uint64_t>(t_rows[y]);
                    ColTrans += ZZZ_BitCount(row ^ (row << 1));
                    if (y != 0)
                        RowTrans += ZZZ_BitCount(t_rows[y - 1] ^ t_rows[y]);
                }
                RowTrans += ZZZ_BitCount(row_full ^ t_rows[0]);
                RowTrans += ZZZ_BitCount(t_rows[roof - 1]);
            }
            struct
            {
                int HoleCount;
                int HoleLine;
                int Wide[31];
                int LineCoverBits;
                int ClearWidth;
            } v;
            std::memset(&v, 0, sizeof v);
            int WideCount = map_width - 1;

            for (int y = roof - 1; y >= 0; --y)
            {
                uint32_t inv_row = t_rows[y];
                v.LineCoverBits |= inv_row;
                int LineHole = v.LineCoverBits ^ inv_row;
                if (LineHole != 0)
                {
                    v.HoleCount += ZZZ_BitCount(LineHole);
                    ++v.HoleLine;
                    for (int hy = y + 1, hy_max = std::min(roof, hy + 8); hy < hy_max; ++hy)
                    {
                        uint32_t CheckLine = LineHole & t_rows[hy];
                        if (CheckLine > 0)
                            v.ClearWidth += ZZZ_BitCount(row_full ^ t_rows[hy]) * hy;
                    }
                }
                WideCount = std::min<int>(WideCount, map_width - ZZZ_BitCount(v.LineCoverBits));
                if (v.HoleLine == 0)
                    ++v.Wide[WideCount];
            }
            // side_roof: max of col tops at the 3 leftmost and 3 rightmost columns.
            int side_roof = 0;
            for (int x : {0, 1, 2, width_m1, width_m1 - 1, width_m1 - 2})
            {
                for (int y = roof - 1; y >= 0; --y)
                    if (map.get(x, y))
                    {
                        side_roof = std::max(side_roof, y + 1);
                        break;
                    }
            }
            auto &p = config_->param;
            result.value = (0. - side_roof * p.roof - ColTrans * p.col_trans - RowTrans * p.row_trans - v.HoleCount * p.hole_count - v.HoleLine * p.hole_line - v.ClearWidth * p.clear_width + v.Wide[2] * p.wide_2 + v.Wide[3] * p.wide_3 + v.Wide[4] * p.wide_4);
            result.count = map.popcount();
            result.roof = roof;
            // Pre-compute safe_cache for all piece types.
            for (size_t i = 0; i < type_max_ && i < 8; ++i)
            {
                int safe = 0;
                while (map_in_danger_(map, i, safe + 1) == 0)
                    ++safe;
                result.safe_cache[i] = int8_t(safe);
            }
            return result;
        }

        // 新 get：参数按 GetArgs 声明顺序萃取（无 Depth，原 depth 为死参）。
        // clear/top_out/t_spin/piece_type 在 get 现场重算。
        Status get(Result const &eval_result, int clear, Landing const &landing, Status const &parent, m_tetris2::AIEnv const &env) const;

    private:
        size_t type_max_{0};
        std::array<int, 256> piece_index_{};
        Config const *config_;
        int col_mask_, row_mask_;
        struct MapInDangerData
        {
            int data[4];
        };
        std::vector<MapInDangerData> map_danger_data_;


        template<class MapT>
        size_t map_in_danger_(MapT const &map, size_t t, size_t up) const
        {
            if (up >= 20)
                return 1;
            int MH = static_cast<int>(map.height);
            size_t height = 22 - up;
            auto get_row = [](MapT const &m, int y) -> uint32_t {
                // dev uses 0=occupied/1=empty; danger masks require a 1=occupied row.
                return static_cast<uint32_t>(m.occupied_row(y));
            };
            uint32_t e0 = (height >= 4 && static_cast<int>(height - 4) < MH) ? get_row(map, static_cast<int>(height - 4)) : 0u;
            uint32_t e1 = (height >= 3 && static_cast<int>(height - 3) < MH) ? get_row(map, static_cast<int>(height - 3)) : 0u;
            uint32_t e2 = (height >= 2 && static_cast<int>(height - 2) < MH) ? get_row(map, static_cast<int>(height - 2)) : 0u;
            uint32_t e3 = (height >= 1 && static_cast<int>(height - 1) < MH) ? get_row(map, static_cast<int>(height - 1)) : 0u;
            return (static_cast<uint32_t>(map_danger_data_[t].data[0]) & e0) | (static_cast<uint32_t>(map_danger_data_[t].data[1]) & e1) | (static_cast<uint32_t>(map_danger_data_[t].data[2]) & e2) | (static_cast<uint32_t>(map_danger_data_[t].data[3]) & e3);
        }
    };

    class C2
    {
    public:
        struct Config
        {
            std::array<double, 100> p;
            double p_rate;
            int safe;
            int mode;
            int danger;
            int soft_drop;
            double planning_bpm;
            // Configurable range for C2::iterate: 1-indexed ranks (lowest=1)
            // of the per-piece child scores to average after sorting ascending.
            // Default [4,7] drops the lowest 3 of 7 (= average of top 4),
            // preserving historical behavior.
            int iterate_low = 4;
            int iterate_high = 7;
        };
        struct Status
        {
            double attack;
            double map;
            size_t combo;
            double combo_limit; // Combo remaining time in seconds.
            double value;
            bool operator<(Status const &) const;
        };
        // Result 收缩：clear/hole/new_hole/soft_drop 依赖落块、消行与源盘，移到 get 现场重算；
        // 这里只保留纯盘面(map+Config)加权量。hole_count 为 map 洞(列顶和-popcount)，供 get 重算 new_hole。
        struct Result
        {
            double attack;
            double map;
            double fill;
            double hole_count;
            int cells = 0; // Exact post-placement population for continuation DP.
        };

        using Landing = m_tetris2::GetLanding<std::monostate>;
        // MapBefore/MapAfter 为非模板 wildcard tag，MapT 由 Engine 萃取；get 用 template<MapT> 承接。
        using GetArgs = m_tetris2::type_list<m_tetris2::args::Eval<Result>, m_tetris2::args::Landing<Landing>,
                                             m_tetris2::args::Clear, m_tetris2::args::MapBefore,
                                             m_tetris2::args::Parent<Status>, m_tetris2::args::NextNode, m_tetris2::args::NextLength>;

    public:
        template<class Spec>
        void init(Config const *config);
        std::string ai_name() const;
        template<class MapT>
        Result eval(MapT const &map) const
        {
            constexpr int map_width = MapT::width;
            constexpr int map_height = MapT::height;
            // C2's evaluator was tuned against the legacy 21-row engine. The
            // bitboard rule keeps 28 rows for movement/spawn, but scoring must
            // retain the original 21-row feature scale.
            constexpr int eval_height = 21;
            static_assert(map_height >= eval_height);
            constexpr uint32_t row_full = static_cast<uint32_t>(MapT::row_full);

            int roof = map.none() ? 0 : (map.max_y() + 1);

            const int width_m1 = map_width - 1;
            size_t ColTrans = 2 * (eval_height - roof);
            size_t RowTrans = 0;
            if (roof > 0)
            {
                for (int y = 0; y < roof; ++y)
                {
                    uint32_t occ_row = static_cast<uint32_t>(map.occupied_row(y));
                    ColTrans += !(occ_row & 1u)
                                + !((occ_row >> width_m1) & 1u)
                                + ZZZ_BitCount((occ_row ^ (occ_row << 1))
                                               & static_cast<uint32_t>(col_mask_));
                    if (y != 0)
                    {
                        RowTrans += ZZZ_BitCount(static_cast<uint32_t>(map.occupied_row(y - 1)) ^ occ_row);
                    }
                }
                RowTrans += ZZZ_BitCount(static_cast<uint32_t>(row_full ^ map.occupied_row(0)));
                RowTrans += ZZZ_BitCount(static_cast<uint32_t>(map.occupied_row(roof - 1)));
            }
            struct
            {
                int HoleCountSrc;
                int HoleCount;
                int HoleLine;

                int WideWellDepth[6];
                double HoleDepth;
                double WellDepth;

                int HoleNum[m_tetris2::max_width];
                int WellNum[m_tetris2::max_width];

                int LineCoverBits;
                int HolePosyIndex;
            } v;
            std::memset(&v, 0, sizeof v);
            struct
            {
                double ClearWidth;
            } a[40];

            for (int y = roof - 1; y >= 0; --y)
            {
                // bitboard semantics: occupied bits are 1
                uint32_t inv_row = static_cast<uint32_t>(map.occupied_row(y));
                v.LineCoverBits |= inv_row;
                int LineHole = v.LineCoverBits ^ inv_row;
                if (LineHole != 0)
                {
                    ++v.HoleLine;
                    a[v.HolePosyIndex].ClearWidth = 0;
                    for (int hy = y + 1; hy < roof; ++hy)
                    {
                        // bitboard semantics: bit=1 means occupied
                        uint32_t CheckLine = LineHole & static_cast<uint32_t>(map.occupied_row(hy));
                        if (CheckLine == 0)
                        {
                            break;
                        }
                        a[v.HolePosyIndex].ClearWidth += (hy + 1 + config_->p[0]) * config_->p[1] * ZZZ_BitCount(CheckLine);
                    }
                    ++v.HolePosyIndex;
                }
                for (int x = 1; x < width_m1; ++x)
                {
                    if ((LineHole >> x) & 1)
                    {
                        v.HoleDepth += (++v.HoleNum[x] + config_->p[4]) * config_->p[5];
                    }
                    else
                    {
                        v.HoleNum[x] = 0;
                    }
                    if (((v.LineCoverBits >> (x - 1)) & 7) == 5)
                    {
                        v.WellDepth += (++v.WellNum[x] + config_->p[2]) * config_->p[3];
                    }
                }
                if (LineHole & 1)
                {
                    v.HoleDepth += (++v.HoleNum[0] + config_->p[4]) * config_->p[5];
                }
                else
                {
                    v.HoleNum[0] = 0;
                }
                if ((v.LineCoverBits & 3) == 2)
                {
                    v.WellDepth += (++v.WellNum[0] + config_->p[2]) * config_->p[3];
                }
                if ((LineHole >> width_m1) & 1)
                {
                    v.HoleDepth += (++v.HoleNum[width_m1] + config_->p[4]) * config_->p[5];
                }
                else
                {
                    v.HoleNum[width_m1] = 0;
                }
                if (((v.LineCoverBits >> (width_m1 - 1)) & 3) == 1)
                {
                    v.WellDepth += (++v.WellNum[width_m1] + config_->p[2]) * config_->p[3];
                }
                int WellWidth = 0;
                int MaxWellWidth = 0;
                for (int x = 0; x < map_width; ++x)
                {
                    if ((v.LineCoverBits >> x) & 1)
                    {
                        if (WellWidth > MaxWellWidth)
                        {
                            MaxWellWidth = WellWidth;
                        }
                        WellWidth = 0;
                    }
                    else
                    {
                        ++WellWidth;
                    }
                }
                if (WellWidth > MaxWellWidth)
                {
                    MaxWellWidth = WellWidth;
                }
                if (MaxWellWidth >= 1 && MaxWellWidth <= 6)
                {
                    // bitboard semantics: empty count = popcount(row_full ^ row)
                    if (ZZZ_BitCount(static_cast<uint32_t>(row_full ^ map.occupied_row(y))) == MaxWellWidth)
                    {
                        v.WideWellDepth[MaxWellWidth - 1] += 2;
                    }
                    else
                    {
                        v.WideWellDepth[MaxWellWidth - 1] -= 1;
                    }
                }
            }

            // Compute col tops and occupied counts via TetrisMap<W,H> API (1=occupied).
            // top[x] = first y from bottom where get(x,y)==1, plus 1; 0 if column empty.
            {
                int map_count = map.popcount();
                for (int x = 0; x < map_width; ++x)
                {
                    int map_top_x = 0;
                    for (int y = roof - 1; y >= 0; --y)
                        if (map.get(x, y))
                        {
                            map_top_x = y + 1;
                            break;
                        }
                    v.HoleCount += map_top_x;
                }
                v.HoleCount -= map_count;
            }

            double BoardDeadZone = map_in_danger_(map);
            if (roof == eval_height)
            {
                BoardDeadZone += 70;
            }

            Result result;
            result.map = (0. - (roof + config_->p[6]) * config_->p[7] - (ColTrans + config_->p[8]) * config_->p[9] - (RowTrans + config_->p[10]) * config_->p[11] - (v.HoleCount + config_->p[12]) * config_->p[13] - (v.HoleLine + config_->p[14]) * config_->p[15] - (v.WellDepth + config_->p[16]) * config_->p[17] - (v.HoleDepth + config_->p[18]) * config_->p[19] - (BoardDeadZone + config_->p[20]) * config_->p[21]);
            double rate = config_->p[22], mul = config_->p[23];
            for (int i = 0; i < v.HolePosyIndex; ++i, rate *= mul)
            {
                result.map -= a[i].ClearWidth * rate;
            }
            result.map *= config_->p_rate;
            if (config_->mode == 0)
            {
                int attack_well = std::min(4, v.WideWellDepth[0]);
                result.attack = (0. + v.WideWellDepth[5] * 2.4 + v.WideWellDepth[4] * 3.6 + v.WideWellDepth[3] * 7.2 + v.WideWellDepth[2] * 9.6 + v.WideWellDepth[1] * -20 + ((attack_well * attack_well) + config_->p[16]) * config_->p[17] * config_->p_rate);
            }
            {
                int map_count = map.popcount();
                result.cells = map_count;
                result.fill = float(map_count) / (map_width * (eval_height - config_->safe));
            }
            result.hole_count = v.HoleCount;
            // soft_drop/hole/new_hole 已移到 get：那里有 Clear、Landing 与 MapBefore(源盘)，
            // 可按原口径重算落块悬空与源盘洞；eval 只留纯盘面(map+Config)加权量。
            return result;
        }
        // 新 get：参数按 GetArgs 声明顺序萃取（无 Depth/Env；仅 NextNode/NextLength）。
        // 模板体放头文件，可被任意 supported Rule 的 MapT（如生产 rule_c2 10x28）实例化链接；
        // 非模板评分 helper（c2_detail::kComboTable / max_reachable_combo）由 c2_combo_table.h / c2_combo_dp.h inline 提供。
        template<class MapT>
        Status get(Result const &eval_result, Landing const &landing, int clear, MapT const &src_map, Status const &parent, char next_node, size_t next_length) const
{
        // 源盘相关量在 get 顶部按原口径重算（不依赖 mode；旧 eval 已在落块时算好 hole/new_hole/soft_drop）。
        constexpr int c2_map_width = MapT::width;
        constexpr int c2_eval_height = 21;
        const int src_roof = src_map.none() ? 0 : (src_map.max_y() + 1);
        int HoleCountSrc = 0;
        {
            int src_count = src_map.popcount();
            for (int x = 0; x < c2_map_width; ++x)
            {
                int src_top_x = 0;
                for (int y = src_roof - 1; y >= 0; --y)
                    if (src_map.get(x, y)) { src_top_x = y + 1; break; }
                HoleCountSrc += src_top_x;
            }
            HoleCountSrc -= src_count;
        }
        const double new_hole = eval_result.hole_count > HoleCountSrc ? static_cast<float>(eval_result.hole_count - HoleCountSrc) / c2_eval_height : 0;
        bool need_soft_drop = false;
        {
            const int kHeight = landing.h;
            const int kWidth = landing.w;
            int shape_bottom = kHeight;
            int shape_left = kHeight;
            for (int i = 0; i < kHeight; ++i)
                if (landing.lines[i] != 0) { shape_bottom = i; break; }
            for (int i = 0; i < kHeight; ++i)
                for (int x = 0; x < kHeight; ++x)
                    if ((landing.lines[i] >> x) & 1) { shape_left = std::min(shape_left, x); break; }
            for (int j = 0; j < kWidth; ++j)
            {
                for (int i = 0; i < kHeight; ++i)
                {
                    if ((landing.lines[i] >> (j + shape_left)) & 1)
                    {
                        int col = landing.col() + j;
                        int src_top_col = 0;
                        for (int y = src_roof - 1; y >= 0; --y)
                            if (src_map.get(col, y)) { src_top_col = y + 1; break; }
                        if (landing.row() + (i - shape_bottom) < src_top_col)
                            need_soft_drop = true;
                        break;
                    }
                }
                if (need_soft_drop) break;
            }
        }

        Status result;
        result.attack = 0;
        result.combo = c2_detail::clamp_combo(parent.combo);
        result.combo_limit = parent.combo_limit;
        result.value = eval_result.map;
        if (config_->mode == 0)
        {
            // One simulated placement always costs exactly one planning interval.
            // Match Java's strict pre-lock timeout check: equality remains active.
            const double bpm = config_->planning_bpm > 0.0 ? config_->planning_bpm : 60.0;
            const double dt = 60.0 / bpm;
            if (result.combo_limit < 0.0)
            {
                result.combo_limit = 0.0;
                result.combo = 0;
            }
            if (result.combo > 0 && result.combo_limit < dt)
            {
                result.combo = 0;
                result.combo_limit = 0.0;
            }
            const size_t old_combo = result.combo;
            const size_t clamped_clear = c2_detail::clamp_clear(clear);
            const auto &combo_entry = c2_detail::kComboTable[old_combo];
            const double increment = combo_entry.increment[clamped_clear];
            if (old_combo > 0)
            {
                result.combo_limit = result.combo_limit - dt + increment;
                if (clear > 0)
                    result.combo = c2_detail::clamp_combo(old_combo + 1);
            }
            else
            {
                result.combo_limit = increment;
                result.combo = clear > 0 ? 1 : 0;
            }
            // A child is the next simulated piece's planning root. Java clears only
            // on strict expiry, so exactly zero remains active while negatives expire.
            if (result.combo_limit < 0.0)
            {
                result.combo_limit = 0.0;
                result.combo = 0;
            }
            double fill = eval_result.fill + (config_->danger ? 0.32 : 0);
            double hole = static_cast<float>(HoleCountSrc) / (c2_eval_height - config_->safe) + (config_->danger ? 0.16 : 0);
            double upstack = std::max<double>(0, 1 - hole * 3.3) * std::max<double>(0, 1 - (fill < 0.4 ? 0 : fill - 0.4) * 4);
            double downstack;
            double length_ratio = (1 << ((next_node != ' ') + static_cast<int>(next_length))) * 0.5;
            if (old_combo == 0)
            {
                downstack = std::max<double>(0.2, 1 - hole * 3.3) * std::max<double>(0.2, 1 - std::abs(fill - 0.48) * 5);
                result.attack -= 60000 * new_hole;
                result.attack += eval_result.attack * upstack;
            }
            else
            {
                downstack = std::max<double>(0.2, 1 - hole * 3.3) * std::max<double>(0.2, 1 - fill * 1.2);
            }
            if (clear > 0)
            {
                if (old_combo == 0)
                {
                    result.attack -= 8000 * upstack;
                    result.attack += combo_entry.score[clamped_clear] * downstack;
                }
                else
                {
                    result.attack += combo_entry.score[clamped_clear] * downstack * length_ratio;
                }
            }
            else if (old_combo > 0)
            {
                result.attack -= combo_entry.score[0] * downstack * length_ratio;
            }
            result.attack += parent.attack;
            // Like master's C2::get: prune only the returned continuation state.
            // This placement was scored above with its real incoming combo;
            // the parent/root state and deadline must never be rewritten.
            if (!config_->danger && result.combo > 0
                && max_reachable_combo(bpm, static_cast<int>(result.combo),
                                       eval_result.cells, result.combo_limit) < 6) {
                result.combo = 0;
            }
        }
        result.map = parent.map + eval_result.map;
        if (need_soft_drop && !config_->soft_drop)
        {
            result.map -= 9999999999;
        }
        result.value = result.attack + result.map;
        return result;
    }
        Status iterate(Status const **status, size_t status_length) const;

    private:
        Config const *config_;
        int col_mask_, row_mask_;
        struct MapInDangerData
        {
            int data[4];
        };
        std::vector<MapInDangerData> map_danger_data_;
        size_t type_max_{0};
        template<class M>
        size_t map_in_danger_(M const &map) const
        {
            size_t danger = 0;
            for (size_t i = 0; i < type_max_; ++i)
            {
                if (map_danger_data_[i].data[0] & static_cast<uint32_t>(map.occupied_row(17)) ||
                    map_danger_data_[i].data[1] & static_cast<uint32_t>(map.occupied_row(18)) ||
                    map_danger_data_[i].data[2] & static_cast<uint32_t>(map.occupied_row(19)) ||
                    map_danger_data_[i].data[3] & static_cast<uint32_t>(map.occupied_row(20)))
                {
                    ++danger;
                }
            }
            return danger;
        }
    };

    namespace detail
    {
        template<class Spec, class Data, int BaseRow = static_cast<int>(Spec::height) - 4, int DropY = 0>
        void init_spawn_danger_data(std::vector<Data> &out)
        {
            m_tetris2::ai_rule_spec::fill_spawn_danger_data<Spec, Data, BaseRow, DropY>(out);
        }
    }

    namespace qq
    {
        template<class Spec>
        void Attack::init(Config const *config)
        {
            config_ = config;
            type_max_ = m_tetris2::ai_rule_spec::piece_count<Spec>();
            width_ = static_cast<int>(Spec::width);
            height_ = static_cast<int>(Spec::height);
            check_line_1_end_ = check_line_1_;
            check_line_2_end_ = check_line_2_;
            const uint32_t full = m_tetris2::ai_rule_spec::row_mask<Spec>();
            for (size_t x = 0; x < Spec::width; ++x)
                *check_line_1_end_++ = full & ~(uint32_t(1) << x);
            for (size_t x = 0; x + 1 < Spec::width; ++x)
                *check_line_2_end_++ = full & ~(uint32_t(3) << x);
            std::sort(check_line_1_, check_line_1_end_);
            std::sort(check_line_2_, check_line_2_end_);
            detail::init_spawn_danger_data<Spec>(map_danger_data_);
            col_mask_ = static_cast<int>(full & ~1u);
            row_mask_ = static_cast<int>(full);
        }
    }

    template<class Spec>
    void Dig::init(Config const *config)
    {
        config_ = config;
        type_max_ = m_tetris2::ai_rule_spec::piece_count<Spec>();
        detail::init_spawn_danger_data<Spec>(map_danger_data_);
        col_mask_ = static_cast<int>(m_tetris2::ai_rule_spec::row_mask<Spec>() & ~1u);
        row_mask_ = static_cast<int>(m_tetris2::ai_rule_spec::row_mask<Spec>());
    }

    template<class Spec>
    void TOJ_PC::init(Config const *config)
    {
        config_ = config;
        col_mask_ = static_cast<int>(m_tetris2::ai_rule_spec::row_mask<Spec>() & ~1u);
        row_mask_ = static_cast<int>(m_tetris2::ai_rule_spec::row_mask<Spec>());
    }

    template<class Spec>
    void Botris_PC::init(Config const *config)
    {
        config_ = config;
        col_mask_ = static_cast<int>(m_tetris2::ai_rule_spec::row_mask<Spec>() & ~1u);
        row_mask_ = static_cast<int>(m_tetris2::ai_rule_spec::row_mask<Spec>());
    }

    template<class Spec>
    void TOJ_v08::init(Config const *config)
    {
        config_ = config;
        width_ = static_cast<int>(Spec::width);
        type_max_ = m_tetris2::ai_rule_spec::piece_count<Spec>();
        m_tetris2::ai_rule_spec::fill_piece_index_cache<Spec>(piece_index_);
        col_mask_ = static_cast<int>(m_tetris2::ai_rule_spec::row_mask<Spec>() & ~1u);
        row_mask_ = static_cast<int>(m_tetris2::ai_rule_spec::row_mask<Spec>());
        full_count_ = static_cast<int>(Spec::width * 24);
        detail::init_spawn_danger_data<Spec, MapInDangerData, 18>(map_danger_data_);
    }

    template<class Spec>
    void Botris::init(Config const *config)
    {
        config_ = config;
        width_ = static_cast<int>(Spec::width);
        type_max_ = m_tetris2::ai_rule_spec::piece_count<Spec>();
        m_tetris2::ai_rule_spec::fill_piece_index_cache<Spec>(piece_index_);
        col_mask_ = static_cast<int>(m_tetris2::ai_rule_spec::row_mask<Spec>() & ~1u);
        row_mask_ = static_cast<int>(m_tetris2::ai_rule_spec::row_mask<Spec>());
        full_count_ = static_cast<int>(Spec::width * 24);
        detail::init_spawn_danger_data<Spec, MapInDangerData, 18>(map_danger_data_);
    }

    template<class Spec>
    void TOJ::init(Config const *config)
    {
        config_ = config;
        type_max_ = m_tetris2::ai_rule_spec::piece_count<Spec>();
        m_tetris2::ai_rule_spec::fill_piece_index_cache<Spec>(piece_index_);
        col_mask_ = static_cast<int>(m_tetris2::ai_rule_spec::row_mask<Spec>() & ~1u);
        row_mask_ = static_cast<int>(m_tetris2::ai_rule_spec::row_mask<Spec>());
        detail::init_spawn_danger_data<Spec, MapInDangerData, 18>(map_danger_data_);
    }

    template<class Spec>
    void C2::init(Config const *config)
    {
        config_ = config;
        type_max_ = m_tetris2::ai_rule_spec::piece_count<Spec>();
        detail::init_spawn_danger_data<Spec, MapInDangerData, 17, 1>(map_danger_data_);
        col_mask_ = static_cast<int>(m_tetris2::ai_rule_spec::row_mask<Spec>() & ~1u);
        row_mask_ = static_cast<int>(m_tetris2::ai_rule_spec::row_mask<Spec>());
    }

}
