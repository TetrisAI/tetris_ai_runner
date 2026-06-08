
#include "tetris_core.h"
#include "search_tspin.h"
#include "search_aspin.h"
#include "integer_utils.h"
#include "ai_rule_spec_utils.h"
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
                bool operator<(Status const &) const;
            };

        public:
            template<class Spec>
            void init(Config const *config);
            std::string ai_name() const;

            template<class Details>
            Result eval(m_tetris2::BBNode<Details, std::monostate> const &lp, typename Details::map_type const &map, typename Details::map_type const &src_map, size_t clear) const
            {
                using MapT = typename Details::map_type;
                constexpr int map_width = MapT::width;
                constexpr int map_height = MapT::height;
                constexpr uint32_t row_full = static_cast<uint32_t>(MapT::row_full);

                // roof: equivalent of TetrisMap::roof
                int roof = map.none() ? 0 : (map.max_y() + 1);

                // Precompute column heights (equivalent of TetrisMap::top[x])
                int top[map_width];
                for (int x = 0; x < map_width; ++x)
                {
                    top[x] = 0;
                    for (int y = map_height - 1; y >= 0; --y)
                        if ((static_cast<uint32_t>(map.row(y)) >> x) & 1u)
                        {
                            top[x] = y + 1;
                            break;
                        }
                }

                double LandHeight = lp.land_height();
                double Middle = std::fabs((lp.x + 1) * 2 - map_width);
                double EraseCount = clear;
                double DeadZone = lp.land_height() == map_height ? 500000. : 0;
                double BoardDeadZone = map_in_danger_(map);
                if (roof == map_height)
                    BoardDeadZone += 70;

                const int width_m1 = map_width - 1;
                int ColTrans = 2 * (map_height - roof);
                int RowTrans = roof == map_height ? 0 : map_width;
                for (int y = 0; y < roof; ++y)
                {
                    uint32_t occ_row = static_cast<uint32_t>(map.row(y));
                    // 1=occupied: bit set = occupied edge transition
                    ColTrans += (occ_row & 1u) + ((occ_row >> width_m1) & 1u) + ZZZ_BitCount((occ_row ^ (occ_row << 1)) & static_cast<uint32_t>(col_mask_));
                    if (y != 0)
                        RowTrans += ZZZ_BitCount(static_cast<uint32_t>(map.row(y - 1)) ^ occ_row);
                }
                // bottom boundary: occupied count
                RowTrans += ZZZ_BitCount(static_cast<uint32_t>(map.row(0)));
                // top boundary: occupied (if full) or empty count
                RowTrans += roof == map_height
                                ? ZZZ_BitCount(static_cast<uint32_t>(map.row(roof - 1)))
                                : ZZZ_BitCount(static_cast<uint32_t>(row_full ^ map.row(roof - 1)));
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
                    uint32_t LineCoverBits; // accumulated empty-bits (1=empty semantics)
                    uint32_t TopHoleBits; // hole-column bits (1=empty semantics)
                } v;
                std::memset(&v, 0, sizeof v);

                for (int y = roof - 1; y >= 0; --y)
                {
                    // empty bits (1=empty): invert 1=occupied row
                    uint32_t inv_row = static_cast<uint32_t>(row_full ^ map.row(y));
                    v.LineCoverBits |= inv_row;
                    uint32_t LineHole = v.LineCoverBits ^ inv_row;
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
                        // TopHoleBits is 1=empty; compare against empty bits of current row
                        uint32_t CheckLine = v.TopHoleBits & static_cast<uint32_t>(row_full ^ map.row(y));
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
                    // check_line_1_/2_ are stored in 1=occupied semantics (see init())
                    uint32_t occ_row = static_cast<uint32_t>(map.row(y));
                    if (std::binary_search<uint32_t const *>(check_line_1_, check_line_1_end_, occ_row))
                    {
                        if (y + 1 < map_height && std::binary_search<uint32_t const *>(check_line_2_, check_line_2_end_, static_cast<uint32_t>(map.row(y + 1))))
                            v.AttackDepth += 20;
                        else
                            v.AttackDepth += 16;
                        for (--y; y >= low_y; --y)
                        {
                            if (std::binary_search<uint32_t const *>(check_line_1_, check_line_1_end_, static_cast<uint32_t>(map.row(y))))
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
                result.land_point = (0. - LandHeight * 16 + Middle * 0.2 + EraseCount * 6 - DeadZone - BoardDeadZone * 500000);
                result.map = (0. - ColTrans * 32 - RowTrans * 32 - v.HoleCount * 400 - v.HoleLine * 38 - v.WellDepth * 16 - v.HoleDepth * 4 - v.HolePiece * 2 + v.AttackDepth * 100);
                result.clear = clear;
                result.danger = v.Danger;
                return result;
            }

            Status get(Result const &eval_result, size_t depth, Status const &status) const;

        private:
            uint32_t check_line_1_[32];
            uint32_t check_line_2_[32];
            uint32_t *check_line_1_end_;
            uint32_t *check_line_2_end_;
            Config const *config_;
            size_t type_max_{0};
            int width_{0};
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
                    uint32_t occ0 = static_cast<uint32_t>(map.row(H - 4));
                    uint32_t occ1 = static_cast<uint32_t>(map.row(H - 3));
                    uint32_t occ2 = static_cast<uint32_t>(map.row(H - 2));
                    uint32_t occ3 = static_cast<uint32_t>(map.row(H - 1));
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
        template<class Spec>
        void init(Config const *config);
        std::string ai_name() const;
        template<class Details>
        double eval(m_tetris2::BBNode<Details, std::monostate> node,
                    typename Details::map_type const &map,
                    typename Details::map_type const &src_map, int clear) const
        {
            using MapT = typename Details::map_type;
            using row_t = typename MapT::row_t;
            // Map<W,H>: bit=1 means occupied (1=occupied semantics).
            // TetrisMap: bit=1 means empty (1=empty semantics). Conversion done in bridge.
            constexpr int W = MapT::width;
            constexpr int H = MapT::height;
            constexpr row_t full = MapT::row_full;
            constexpr row_t col_mask = static_cast<row_t>(full & ~row_t(1));
            const int width_m1 = W - 1;

            // roof: index of the lowest all-empty row above all occupied rows (0 if board is empty).
            int roof = map.none() ? 0 : (map.max_y() + 1);

            // ColTrans: start with 2 transitions per empty row above roof (left+right wall).
            size_t ColTrans = static_cast<size_t>(2 * (H - roof));

            // RowTrans: boundary transitions at bottom (row[0] vs floor) and top (row[roof-1] vs ceiling).
            // In 1=occupied semantics:
            //   - bottom boundary: number of occupied cells in row[0] (each occupied cell transitions with the empty floor)
            //   - top boundary (roof==H): number of occupied cells in row[roof-1]
            //   - top boundary (roof<H):  number of empty cells in row[roof-1]
            //     (in old 1=empty code: map.row[roof-1] & mask = empty bits; here: full ^ map.row(roof-1))
            size_t RowTrans = 0;
            if (roof > 0)
            {
                RowTrans = static_cast<size_t>(ZZZ_BitCount(static_cast<uint32_t>(map.row(0))));
                if (roof == H)
                    RowTrans += static_cast<size_t>(ZZZ_BitCount(static_cast<uint32_t>(map.row(roof - 1))));
                else
                    RowTrans += static_cast<size_t>(ZZZ_BitCount(static_cast<uint32_t>(full ^ map.row(roof - 1))));
            }

            for (int y = 0; y < roof; ++y)
            {
                // empty_row: bits set where the cell is empty (inverse of occupied).
                row_t empty_row = static_cast<row_t>(full ^ map.row(y));
                // ColTrans: left wall (x=0 is empty), right wall (x=width_m1 is empty),
                // adjacent-column transitions within the row.
                ColTrans += static_cast<size_t>((empty_row & 1)) + static_cast<size_t>((static_cast<uint32_t>(empty_row) >> width_m1) & 1u) + static_cast<size_t>(ZZZ_BitCount(static_cast<uint32_t>(static_cast<row_t>(empty_row ^ static_cast<row_t>(empty_row << 1)) & col_mask)));
                if (y != 0)
                {
                    RowTrans += static_cast<size_t>(ZZZ_BitCount(static_cast<uint32_t>(map.row(y - 1) ^ map.row(y))));
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

            // LineCoverBits tracks occupied bits seen so far (scanning top-down).
            // A hole at column x in row y means: LineCoverBits has x set (occupied above y)
            // but map.row(y) does NOT have x set (empty at y).
            // In 1=occupied: LineCoverBits = union of occupied bits; LineHole = covered but empty here.
            for (int y = roof - 1; y >= 0; --y)
            {
                uint32_t occ_row = static_cast<uint32_t>(map.row(y));
                v.LineCoverBits |= static_cast<int>(occ_row);
                int LineHole = v.LineCoverBits & ~static_cast<int>(occ_row);
                if (LineHole != 0)
                {
                    v.HoleCount += ZZZ_BitCount(static_cast<uint32_t>(LineHole));
                    ++v.HoleLine;
                    a[v.HolePosyIndex].ClearWidth = 0;
                    for (int hy = y + 1; hy < roof; ++hy)
                    {
                        // CheckLine: the hole bits that are also occupied above (covered overhead).
                        uint32_t CheckLine = static_cast<uint32_t>(LineHole) & static_cast<uint32_t>(map.row(hy));
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
                    // Well at x: x-1 and x+1 are occupied (covered), x is empty (not in LineCoverBits yet,
                    // or in LineCoverBits but not covered by occ_row — but here we use the cover pattern).
                    // Pattern in LineCoverBits: bits (x-1, x, x+1) = 0b101 (5): neighbors covered, x not.
                    // In 1=occupied LineCoverBits: covered = occupied above. Well = not covered at x
                    // but covered at x±1. Pattern remains == 5 (same bit positions, same meaning).
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
                // Left-edge well: x=0 not covered, x=1 covered. LineCoverBits bits 0,1 = 0b10 (2).
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
                // Right-edge well: x=width_m1 not covered, x=width_m1-1 covered.
                // LineCoverBits bits (width_m1-1, width_m1) = 0b01 (1).
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
        double get(double const &eval_result, size_t depth) const;

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
                uint32_t occ0 = static_cast<uint32_t>(map.row(H - 4));
                uint32_t occ1 = static_cast<uint32_t>(map.row(H - 3));
                uint32_t occ2 = static_cast<uint32_t>(map.row(H - 2));
                uint32_t occ3 = static_cast<uint32_t>(map.row(H - 1));
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
        struct Result
        {
            double value;
            int clear;
            int roof;
            TSpinType t_spin;
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

    public:
        template<class Spec>
        void init(Config const *config);
        std::string ai_name() const;
        double ratio() const
        {
            return 0.5;
        }
        template<class Details>
        Result eval(m_tetris2::BBNode<Details, TSpinType> const &lp, typename Details::map_type const &map, typename Details::map_type const &, size_t clear) const
        {
            using MapT = typename Details::map_type;
            constexpr int map_width = MapT::width;
            constexpr int map_height = MapT::height;
            constexpr uint32_t row_full = static_cast<uint32_t>(MapT::row_full);

            int roof = map.none() ? 0 : (map.max_y() + 1);

            int ColTrans = 2 * (map_height - roof);
            int RowTrans = roof == map_height ? 0 : map_width;
            if (roof > 0)
            {
                for (int y = 0; y < roof; ++y)
                {
                    uint64_t row = static_cast<uint64_t>(map.row(y));
                    ColTrans += ZZZ_BitCount(row ^ (row << 1));
                    if (y != 0)
                    {
                        RowTrans += ZZZ_BitCount(static_cast<uint32_t>(map.row(y - 1)) ^ static_cast<uint32_t>(map.row(y)));
                    }
                }
                RowTrans += ZZZ_BitCount(static_cast<uint32_t>(map.row(0)));
                RowTrans += roof == map_height
                                ? ZZZ_BitCount(static_cast<uint32_t>(row_full ^ map.row(roof - 1)))
                                : ZZZ_BitCount(static_cast<uint32_t>(map.row(roof - 1)));
            }
            else
            {
                RowTrans += map_width;
            }

            Result result;
            result.value = (roof > 4 ? 0 : 10000) - ColTrans * 3 - RowTrans * 2;
            result.clear = clear;
            result.roof = roof;
            result.t_spin = lp.spin;
            return result;
        }
        Status get(Result const &eval_result, size_t depth, Status const &status) const;

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
        struct Result
        {
            double value;
            int clear;
            int roof;
            ASpinType a_spin;
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

    public:
        template<class Spec>
        void init(Config const *config);
        std::string ai_name() const;
        double ratio() const
        {
            return 0.5;
        }
        template<class Details>
        Result eval(m_tetris2::BBNode<Details, ASpinType> const &lp, typename Details::map_type const &map, typename Details::map_type const &, size_t clear) const
        {
            using MapT = typename Details::map_type;
            constexpr int map_width = MapT::width;
            constexpr int map_height = MapT::height;
            constexpr uint32_t row_full = static_cast<uint32_t>(MapT::row_full);

            int roof = map.none() ? 0 : (map.max_y() + 1);

            const int width_m1 = map_width - 1;
            int ColTrans = 2 * (map_height - roof);
            int RowTrans = roof == map_height ? 0 : map_width;
            if (roof > 0)
            {
                for (int y = 0; y < roof; ++y)
                {
                    uint64_t row = static_cast<uint64_t>(map.row(y));
                    ColTrans += ZZZ_BitCount(row ^ (row << 1));
                    if (y != 0)
                    {
                        RowTrans += ZZZ_BitCount(static_cast<uint32_t>(map.row(y - 1)) ^ static_cast<uint32_t>(map.row(y)));
                    }
                }
                RowTrans += ZZZ_BitCount(static_cast<uint32_t>(map.row(0)));
                RowTrans += roof == map_height
                                ? ZZZ_BitCount(static_cast<uint32_t>(row_full ^ map.row(roof - 1)))
                                : ZZZ_BitCount(static_cast<uint32_t>(map.row(roof - 1)));
            }
            else
            {
                RowTrans += map_width;
            }

            Result result;
            result.value = (roof > 4 ? 0 : 10000) - ColTrans * 3 - RowTrans * 2;
            result.clear = clear;
            result.roof = roof;
            result.a_spin = lp.spin;
            return result;
        }
        Status get(Result const &eval_result, size_t depth, Status const &status) const;

    private:
        Config const *config_;
        int col_mask_, row_mask_;
    };

    class TOJ_v08
    {
    public:
        typedef search_tspin::Search::TSpinType TSpinType;
        using EvalSpinType = TSpinType;
        struct Config
        {
            int const *table;
            int table_max;
        };
        struct Result
        {
            double value;
            int clear;
            int count;
            int t2_value;
            int t3_value;
            int roof;
            int8_t safe_cache[8]; // indexed by cached piece index, size >= type_max
            TSpinType t_spin;
            int8_t top_out;
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

    public:
        template<class Spec>
        void init(Config const *config);
        std::string ai_name() const;
        double ratio() const
        {
            return 1.5;
        }
        template<class Details>
        Result eval(m_tetris2::BBNode<Details, TSpinType> const &lp, typename Details::map_type const &map, typename Details::map_type const &src_map, size_t clear) const
        {
            using MapT = typename Details::map_type;
            constexpr int map_width = MapT::width;
            constexpr int map_height = MapT::height;
            constexpr uint32_t row_full = static_cast<uint32_t>(MapT::row_full);

            int roof = map.none() ? 0 : (map.max_y() + 1);

            const int width_m1 = map_width - 1;
            int ColTrans = 2 * (map_height - roof);
            int RowTrans = roof == map_height ? 0 : map_width;
            for (int y = 0; y < roof; ++y)
            {
                uint32_t occ_row = static_cast<uint32_t>(map.row(y));
                ColTrans += (occ_row & 1u) + ((occ_row >> width_m1) & 1u) + ZZZ_BitCount((occ_row ^ (occ_row << 1)) & static_cast<uint32_t>(col_mask_));
                if (y != 0)
                    RowTrans += ZZZ_BitCount(static_cast<uint32_t>(map.row(y - 1)) ^ occ_row);
            }
            RowTrans += ZZZ_BitCount(static_cast<uint32_t>(map.row(0)));
            RowTrans += roof == map_height
                            ? ZZZ_BitCount(static_cast<uint32_t>(map.row(roof - 1)))
                            : ZZZ_BitCount(static_cast<uint32_t>(row_full ^ map.row(roof - 1)));
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
                // empty bits (1=empty): invert 1=occupied row
                uint32_t inv_row = static_cast<uint32_t>(row_full ^ map.row(y));
                v.LineCoverBits |= inv_row;
                uint32_t LineHole = v.LineCoverBits ^ inv_row;
                if (LineHole != 0)
                {
                    ++v.HoleLine;
                    a[v.HolePosyIndex].ClearWidth = 0;
                    for (int hy = y + 1; hy < roof; ++hy)
                    {
                        // occupied bits at hy: map.row(hy); hole bits are empty above, occupied at hy
                        uint32_t CheckLine = LineHole & static_cast<uint32_t>(map.row(hy));
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
                occ_count += ZZZ_BitCount(static_cast<uint32_t>(map.row(y)));

            Result result;
            result.value = (0. - roof * 128 - ColTrans * 160 - RowTrans * 160 - v.HoleCount * 80 - v.HoleLine * 380 - v.WellDepth * 100 - v.HoleDepth * 40);
            double rate = 32, mul = 1.0 / 4;
            for (int i = 0; i < v.HolePosyIndex; ++i, rate *= mul)
                result.value -= a[i].ClearWidth * rate;
            result.count = occ_count + v.HoleCount;
            result.clear = clear;
            result.t2_value = 0;
            result.t3_value = 0;
            result.roof = roof;

            // Precompute safe_cache for all piece types
            std::memset(result.safe_cache, 0, sizeof(result.safe_cache));
            using Spec = typename Details::spec_type;
            for (size_t ti = 0; ti < m_tetris2::ai_rule_spec::piece_count<Spec>() && ti < 8; ++ti)
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
                int row0 = static_cast<int>(static_cast<uint32_t>(row_full ^ map.row(y)));
                int row1 = static_cast<int>(static_cast<uint32_t>(row_full ^ map.row(y + 1)));
                int row2 = y + 2 < map_height ? static_cast<int>(static_cast<uint32_t>(row_full ^ map.row(y + 2))) : 0;
                int row3 = y + 3 < map_height ? static_cast<int>(static_cast<uint32_t>(row_full ^ map.row(y + 3))) : 0;
                int row4 = y + 4 < map_height ? static_cast<int>(static_cast<uint32_t>(row_full ^ map.row(y + 4))) : 0;
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
            result.t_spin = lp.spin;
            result.top_out = lp.row() >= 20;
            return result;
        }

        Status get(Result const &eval_result, size_t depth, Status const &status, m_tetris2::AIEnv const &env) const;

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
            constexpr int MH = MapT::height;
            size_t height = 22 - up;
            uint32_t e0 = (height >= 4 && static_cast<int>(height - 4) < MH) ? static_cast<uint32_t>(map.row(static_cast<int>(height - 4))) : 0u;
            uint32_t e1 = (height >= 3 && static_cast<int>(height - 3) < MH) ? static_cast<uint32_t>(map.row(static_cast<int>(height - 3))) : 0u;
            uint32_t e2 = (height >= 2 && static_cast<int>(height - 2) < MH) ? static_cast<uint32_t>(map.row(static_cast<int>(height - 2))) : 0u;
            uint32_t e3 = (height >= 1 && static_cast<int>(height - 1) < MH) ? static_cast<uint32_t>(map.row(static_cast<int>(height - 1))) : 0u;
            return (static_cast<uint32_t>(map_danger_data_[t].data[0]) & e0) | (static_cast<uint32_t>(map_danger_data_[t].data[1]) & e1) | (static_cast<uint32_t>(map_danger_data_[t].data[2]) & e2) | (static_cast<uint32_t>(map_danger_data_[t].data[3]) & e3);
        }
    };

    class Botris
    {
    public:
        typedef search_aspin::Search::ASpinType ASpinType;
        using EvalSpinType = ASpinType;
        struct Config
        {
            int const *table;
            int table_max;
        };
        struct Result
        {
            double value;
            int clear;
            int count;
            int roof;
            int8_t safe_cache[8]; // indexed by cached piece index
            ASpinType a_spin;
            int8_t top_out;
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

    public:
        template<class Spec>
        void init(Config const *config);
        std::string ai_name() const;
        double ratio() const
        {
            return 1.5;
        }
        template<class Details>
        Result eval(m_tetris2::BBNode<Details, ASpinType> const &lp, typename Details::map_type const &map, typename Details::map_type const &src_map, size_t clear) const
        {
            using MapT = typename Details::map_type;
            constexpr int map_width = MapT::width;
            constexpr int map_height = MapT::height;
            constexpr uint32_t row_full = static_cast<uint32_t>(MapT::row_full);

            int roof = map.none() ? 0 : (map.max_y() + 1);

            const int width_m1 = map_width - 1;
            int ColTrans = 2 * (map_height - roof);
            int RowTrans = roof == map_height ? 0 : map_width;
            if (roof > 0)
            {
                for (int y = 0; y < roof; ++y)
                {
                    uint64_t row = static_cast<uint64_t>(map.row(y));
                    ColTrans += ZZZ_BitCount(row ^ (row << 1));
                    if (y != 0)
                    {
                        RowTrans += ZZZ_BitCount(static_cast<uint32_t>(map.row(y - 1)) ^ static_cast<uint32_t>(map.row(y)));
                    }
                }
                RowTrans += ZZZ_BitCount(static_cast<uint32_t>(map.row(0)));
                RowTrans += roof == map_height
                                ? ZZZ_BitCount(static_cast<uint32_t>(row_full ^ map.row(roof - 1)))
                                : ZZZ_BitCount(static_cast<uint32_t>(map.row(roof - 1)));
            }
            else
            {
                RowTrans += map_width;
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
                uint32_t inv_row = static_cast<uint32_t>(row_full ^ map.row(y));
                v.LineCoverBits |= inv_row;
                uint32_t LineHole = v.LineCoverBits ^ inv_row;
                if (LineHole != 0)
                {
                    ++v.HoleLine;
                    a[v.HolePosyIndex].ClearWidth = 0;
                    for (int hy = y + 1; hy < roof; ++hy)
                    {
                        uint32_t CheckLine = LineHole & static_cast<uint32_t>(map.row(hy));
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
                occ_count += ZZZ_BitCount(static_cast<uint32_t>(map.row(y)));

            Result result;
            result.value = (0. - roof * 128 - ColTrans * 160 - RowTrans * 160 - v.HoleCount * 80 - v.HoleLine * 380 - v.WellDepth * 100 - v.HoleDepth * 40);
            double rate = 32, mul = 1.0 / 4;
            for (int i = 0; i < v.HolePosyIndex; ++i, rate *= mul)
            {
                result.value -= a[i].ClearWidth * rate;
            }
            result.count = occ_count + v.HoleCount;
            result.clear = clear;
            result.roof = roof;

            // Precompute safe_cache for all piece types
            std::memset(result.safe_cache, 0, sizeof(result.safe_cache));
            using Spec = typename Details::spec_type;
            for (size_t ti = 0; ti < m_tetris2::ai_rule_spec::piece_count<Spec>() && ti < 8; ++ti)
            {
                int safe = 0;
                while (map_in_danger_(map, ti, safe + 1) == 0)
                    ++safe;
                result.safe_cache[ti] = static_cast<int8_t>(safe);
            }
            result.a_spin = lp.spin;
            result.top_out = lp.row() >= 20;
            return result;
        }
        Status get(Result const &eval_result, size_t depth, Status const &status, m_tetris2::AIEnv const &env) const;

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
            constexpr int MH = MapT::height;
            size_t height = 22 - up;
            uint32_t e0 = (height >= 4 && static_cast<int>(height - 4) < MH) ? static_cast<uint32_t>(map.row(static_cast<int>(height - 4))) : 0u;
            uint32_t e1 = (height >= 3 && static_cast<int>(height - 3) < MH) ? static_cast<uint32_t>(map.row(static_cast<int>(height - 3))) : 0u;
            uint32_t e2 = (height >= 2 && static_cast<int>(height - 2) < MH) ? static_cast<uint32_t>(map.row(static_cast<int>(height - 2))) : 0u;
            uint32_t e3 = (height >= 1 && static_cast<int>(height - 1) < MH) ? static_cast<uint32_t>(map.row(static_cast<int>(height - 1))) : 0u;
            return (static_cast<uint32_t>(map_danger_data_[t].data[0]) & e0) | (static_cast<uint32_t>(map_danger_data_[t].data[1]) & e1) | (static_cast<uint32_t>(map_danger_data_[t].data[2]) & e2) | (static_cast<uint32_t>(map_danger_data_[t].data[3]) & e3);
        }
    };

    class TOJ
    {
    public:
        typedef search_tspin::Search::TSpinType TSpinType;
        using EvalSpinType = TSpinType;

        // legacy bridge for ppt_pso.cpp
        int get_safe(m_tetris2::TetrisMap const &map, char type) const
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
        struct Result
        {
            double value;
            int8_t clear;
            int8_t top_out;
            int16_t count;
            int16_t t2_value;
            int16_t t3_value;
            TSpinType t_spin;
            int roof;
            int8_t safe_cache[8];
            char piece_type;
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

            // legacy bridge for ppt_pso.cpp
            static void init_t_value(m_tetris2::TetrisMap const &map, int16_t &t2_value_ref, int16_t &t3_value_ref)
            {
                uint32_t occ_rows[40];
                uint32_t mask = map.empty_line();
                for (int y = 0; y < map.height; ++y)
                    occ_rows[y] = ~map.row[y] & mask;
                init_t_value(occ_rows, map.width, map.roof, t2_value_ref, t3_value_ref);
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
        template<class Details>
        Result eval(m_tetris2::BBNode<Details, std::monostate> const &lp, typename Details::map_type const &map, typename Details::map_type const &, size_t clear) const
        {
            using MapT = typename Details::map_type;
            constexpr int map_width = MapT::width;
            constexpr int map_height = MapT::height;
            constexpr uint32_t row_full = static_cast<uint32_t>(MapT::row_full);

            int roof = map.none() ? 0 : (map.max_y() + 1);

            Result result;
            memset(&result, 0, sizeof result);

            // Build a local row array (1=occupied) and compute t-value with in-place overlay.
            uint32_t t_rows[40];
            for (int y = 0; y < roof; ++y)
                t_rows[y] = static_cast<uint32_t>(map.row(y));
            memset(t_rows + roof, 0, sizeof(uint32_t) * (40 - roof));
            Status::init_t_value(t_rows, map_width, roof, result.t2_value, result.t3_value, t_rows);

            const int width_m1 = map_width - 1;
            size_t ColTrans = 2 * (map_height - roof);
            size_t RowTrans = roof == map_height ? 0 : map_width;
            if (roof > 0)
            {
                for (int y = 0; y < roof; ++y)
                {
                    uint64_t row = static_cast<uint64_t>(t_rows[y]);
                    ColTrans += ZZZ_BitCount(row ^ (row << 1));
                    if (y != 0)
                        RowTrans += ZZZ_BitCount(t_rows[y - 1] ^ t_rows[y]);
                }
                RowTrans += ZZZ_BitCount(t_rows[0]);
                RowTrans += roof == map_height
                                ? ZZZ_BitCount(row_full ^ t_rows[roof - 1])
                                : ZZZ_BitCount(t_rows[roof - 1]);
            }
            else
            {
                RowTrans += map_width;
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
            result.clear = int8_t(clear);
            result.top_out = lp.row() >= 20;
            result.roof = roof;
            result.piece_type = m_tetris2::BBNodeBase<Details>::T;
            // Pre-compute safe_cache for all piece types.
            using Spec = typename Details::spec_type;
            for (size_t i = 0; i < m_tetris2::ai_rule_spec::piece_count<Spec>() && i < 8; ++i)
            {
                int safe = 0;
                while (map_in_danger_(map, i, safe + 1) == 0)
                    ++safe;
                result.safe_cache[i] = int8_t(safe);
            }
            return result;
        }

        Status get(Result const &eval_result, size_t depth, Status const &status, m_tetris2::AIEnv const &env) const;

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

        // legacy bridge for ppt_pso.cpp
        size_t map_in_danger_(m_tetris2::TetrisMap const &map, size_t t, size_t up) const
        {
            if (up >= 20)
                return 1;
            int height = 22 - (int)up;
            uint32_t mask = static_cast<uint32_t>(map.empty_line());
            uint32_t e0 = (height >= 4 && height - 4 < map.height) ? ((~static_cast<uint32_t>(map.row[height - 4])) & mask) : 0u;
            uint32_t e1 = (height >= 3 && height - 3 < map.height) ? ((~static_cast<uint32_t>(map.row[height - 3])) & mask) : 0u;
            uint32_t e2 = (height >= 2 && height - 2 < map.height) ? ((~static_cast<uint32_t>(map.row[height - 2])) & mask) : 0u;
            uint32_t e3 = (height >= 1 && height - 1 < map.height) ? ((~static_cast<uint32_t>(map.row[height - 1])) & mask) : 0u;
            return (static_cast<uint32_t>(map_danger_data_[t].data[0]) & e0) | (static_cast<uint32_t>(map_danger_data_[t].data[1]) & e1) | (static_cast<uint32_t>(map_danger_data_[t].data[2]) & e2) | (static_cast<uint32_t>(map_danger_data_[t].data[3]) & e3);
        }

        template<class MapT>
        size_t map_in_danger_(MapT const &map, size_t t, size_t up) const
        {
            if (up >= 20)
                return 1;
            constexpr int MH = MapT::height;
            size_t height = 22 - up;
            uint32_t e0 = (height >= 4 && static_cast<int>(height - 4) < MH) ? static_cast<uint32_t>(map.row(static_cast<int>(height - 4))) : 0u;
            uint32_t e1 = (height >= 3 && static_cast<int>(height - 3) < MH) ? static_cast<uint32_t>(map.row(static_cast<int>(height - 3))) : 0u;
            uint32_t e2 = (height >= 2 && static_cast<int>(height - 2) < MH) ? static_cast<uint32_t>(map.row(static_cast<int>(height - 2))) : 0u;
            uint32_t e3 = (height >= 1 && static_cast<int>(height - 1) < MH) ? static_cast<uint32_t>(map.row(static_cast<int>(height - 1))) : 0u;
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
        };
        struct Status
        {
            double attack;
            double map;
            size_t combo;
            size_t combo_limit;
            double value;
            bool operator<(Status const &) const;
        };
        struct Result
        {
            double attack;
            double map;
            size_t clear;
            double fill;
            double hole;
            double new_hole;
            bool soft_drop;
        };

    public:
        template<class Spec>
        void init(Config const *config);
        std::string ai_name() const;
        template<class Details>
        Result eval(m_tetris2::BBNode<Details, std::monostate> const &lp, typename Details::map_type const &map, typename Details::map_type const &src_map, size_t clear) const
        {
            using MapT = typename Details::map_type;
            constexpr int map_width = MapT::width;
            constexpr int map_height = MapT::height;
            constexpr uint32_t row_full = static_cast<uint32_t>(MapT::row_full);

            int roof = map.none() ? 0 : (map.max_y() + 1);
            int src_roof = src_map.none() ? 0 : (src_map.max_y() + 1);

            const int width_m1 = map_width - 1;
            size_t ColTrans = 2 * (map_height - roof);
            size_t RowTrans = roof == map_height ? 0 : map_width;
            if (roof > 0)
            {
                for (int y = 0; y < roof; ++y)
                {
                    uint64_t row = static_cast<uint64_t>(map.row(y));
                    ColTrans += ZZZ_BitCount(row ^ (row << 1));
                    if (y != 0)
                    {
                        RowTrans += ZZZ_BitCount(static_cast<uint32_t>(map.row(y - 1)) ^ static_cast<uint32_t>(map.row(y)));
                    }
                }
                RowTrans += ZZZ_BitCount(static_cast<uint32_t>(map.row(0)));
                RowTrans += roof == map_height
                                ? ZZZ_BitCount(static_cast<uint32_t>(row_full ^ map.row(roof - 1)))
                                : ZZZ_BitCount(static_cast<uint32_t>(map.row(roof - 1)));
            }
            else
            {
                RowTrans += map_width;
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
                uint32_t inv_row = static_cast<uint32_t>(map.row(y));
                v.LineCoverBits |= inv_row;
                int LineHole = v.LineCoverBits ^ inv_row;
                if (LineHole != 0)
                {
                    ++v.HoleLine;
                    a[v.HolePosyIndex].ClearWidth = 0;
                    for (int hy = y + 1; hy < roof; ++hy)
                    {
                        // bitboard semantics: bit=1 means occupied
                        uint32_t CheckLine = LineHole & static_cast<uint32_t>(map.row(hy));
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
                    if (ZZZ_BitCount(static_cast<uint32_t>(row_full ^ map.row(y))) == MaxWellWidth)
                    {
                        v.WideWellDepth[MaxWellWidth - 1] += 2;
                    }
                    else
                    {
                        v.WideWellDepth[MaxWellWidth - 1] -= 1;
                    }
                }
            }

            // Compute col tops and occupied counts via Map<W,H> API (1=occupied).
            // top[x] = first y from bottom where get(x,y)==1, plus 1; 0 if column empty.
            {
                int map_count = map.popcount();
                int src_count = src_map.popcount();
                for (int x = 0; x < map_width; ++x)
                {
                    int map_top_x = 0;
                    for (int y = roof - 1; y >= 0; --y)
                        if (map.get(x, y))
                        {
                            map_top_x = y + 1;
                            break;
                        }
                    int src_top_x = 0;
                    for (int y = src_roof - 1; y >= 0; --y)
                        if (src_map.get(x, y))
                        {
                            src_top_x = y + 1;
                            break;
                        }
                    v.HoleCount += map_top_x;
                    v.HoleCountSrc += src_top_x;
                }
                v.HoleCount -= map_count;
                v.HoleCountSrc -= src_count;
            }

            double BoardDeadZone = map_in_danger_(map);
            if (roof == map_height)
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
            result.clear = clear;
            {
                int map_count = map.popcount();
                result.fill = float(map_count) / (map_width * (map_height - config_->safe));
            }
            result.hole = float(v.HoleCountSrc) / (map_height - config_->safe);
            result.new_hole = v.HoleCount > v.HoleCountSrc ? float(v.HoleCount - v.HoleCountSrc) / map_height : 0;
            // open() equivalent: piece can free-fall iff for each column j,
            // the piece's bottom row (y-up) >= src_map.top[col+j].
            // kLines is y-down (i=0=top, i=kHeight-1=bottom).
            bool need_soft_drop = false;
            for (int j = 0; j < lp.kWidth; ++j)
            {
                for (int i = lp.kHeight - 1; i >= 0; --i)
                {
                    if ((lp.kLines[i] >> j) & 1)
                    {
                        // compute src_top for column lp.col()+j on demand
                        int col = lp.col() + j;
                        int src_top_col = 0;
                        for (int y = src_roof - 1; y >= 0; --y)
                            if (src_map.get(col, y))
                            {
                                src_top_col = y + 1;
                                break;
                            }
                        if (lp.row() + (lp.kHeight - 1 - i) < src_top_col)
                            need_soft_drop = true;
                        break;
                    }
                }
            }
            result.soft_drop = need_soft_drop;
            return result;
        }
        Status get(Result const &eval_result, size_t depth, Status const &status, m_tetris2::AIEnv const &env) const;
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
            constexpr int kH = M::height;
            for (size_t i = 0; i < type_max_; ++i)
            {
                if (map_danger_data_[i].data[0] & static_cast<uint32_t>(map.row(kH - 4)) ||
                    map_danger_data_[i].data[1] & static_cast<uint32_t>(map.row(kH - 3)) ||
                    map_danger_data_[i].data[2] & static_cast<uint32_t>(map.row(kH - 2)) ||
                    map_danger_data_[i].data[3] & static_cast<uint32_t>(map.row(kH - 1)))
                {
                    ++danger;
                }
            }
            // bitboard semantics: row[17] bit=1 means occupied
            if (static_cast<uint32_t>(map.row(17)) != 0)
            {
                ++danger;
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
        detail::init_spawn_danger_data<Spec, MapInDangerData, static_cast<int>(Spec::height) - 4, 1>(map_danger_data_);
        col_mask_ = static_cast<int>(m_tetris2::ai_rule_spec::row_mask<Spec>() & ~1u);
        row_mask_ = static_cast<int>(m_tetris2::ai_rule_spec::row_mask<Spec>());
    }

}
