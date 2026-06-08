
#include "tetris_core.h"
#include "integer_utils.h"
#include "bb_node.h"
#include "search_tspin.h"
#include "ai_rule_spec_utils.h"
#include <functional>

namespace ai_tag
{
    class the_ai_games_old
    {
    public:
        struct Result
        {
            double land_point, map;
            int tilt, full, count, clear, low_y, node_top;
            int roof;
            int8_t safe_cache[21]; // safe_cache[up] = map_in_danger result for that up value
        };
        struct Status
        {
            size_t combo;
            int up;
            double land_point;
            double value;
            bool operator<(Status const &) const;
        };

    public:
        template<class Spec>
        void init();
        std::string ai_name() const;

        template<class Details>
        Result eval(m_tetris2::BBNode<Details, std::monostate> const &lp, typename Details::map_type const &map, typename Details::map_type const &src_map, size_t clear) const
        {
            using MapT = typename Details::map_type;
            constexpr int map_width  = MapT::width;
            constexpr int map_height = MapT::height;
            constexpr uint32_t row_full = static_cast<uint32_t>(MapT::row_full);

            int roof = map.none() ? 0 : (map.max_y() + 1);

            double LandHeight = lp.land_height();
            double Middle = std::abs((int(lp.x) + 1) * 2 - map_width);
            double EraseCount = clear;

            const int width_m1 = map_width - 1;
            int ColTrans = 2 * (map_height - roof);
            int RowTrans = roof == map_height ? 0 : map_width;
            for (int y = 0; y < roof; ++y)
            {
                uint32_t r = static_cast<uint32_t>(map.row(y));
                if (!(r & 1))
                    ++ColTrans;
                if (!(r >> width_m1 & 1))
                    ++ColTrans;
                ColTrans += ZZZ_BitCount((r ^ (r << 1)) & col_mask_);
                if (y != 0)
                    RowTrans += ZZZ_BitCount(static_cast<uint32_t>(map.row(y - 1)) ^ r);
            }
            if (roof > 0)
            {
                RowTrans += ZZZ_BitCount(~static_cast<uint32_t>(map.row(0)) & row_mask_);
                RowTrans += ZZZ_BitCount(static_cast<uint32_t>(map.row(roof - 1)) & row_mask_);
            }
            else
            {
                RowTrans += map_width;
            }

            struct
            {
                int HoleCount;
                int HoleLine;
                int WideWellDepth[6];
                int WellDepth[32];
                int WellDepthTotle;
                int LineCoverBits;
                int HoleBits0;
                int ClearWidth0;
                int HoleBits1;
                int ClearWidth1;
                int HoleBits2;
                int ClearWidth2;
            } v;
            std::memset(&v, 0, sizeof v);
            int HolePosy0 = -1;
            int HolePosy1 = -1;
            int HolePosy2 = -1;

            for (int y = roof - 1; y >= 0; --y)
            {
                uint32_t inv_row = static_cast<uint32_t>(map.row(y)) & row_mask_;
                v.LineCoverBits |= inv_row;
                int LineHole = v.LineCoverBits ^ inv_row;
                if (LineHole != 0)
                {
                    v.HoleCount += ZZZ_BitCount(LineHole);
                    v.HoleLine++;
                    if (HolePosy0 == -1)
                    {
                        HolePosy0 = y + 1;
                        v.HoleBits0 = LineHole;
                    }
                    else if (HolePosy1 == -1)
                    {
                        HolePosy1 = y + 1;
                        v.HoleBits1 = LineHole;
                    }
                    else if (HolePosy2 == -1)
                    {
                        HolePosy2 = y + 1;
                        v.HoleBits2 = LineHole;
                    }
                }
                int WellWidth = 0;
                int MaxWellWidth = 0;
                for (int x = 0; x < map_width; ++x)
                {
                    if ((v.LineCoverBits >> x) & 1)
                    {
                        if (WellWidth > MaxWellWidth)
                            MaxWellWidth = WellWidth;
                        WellWidth = 0;
                    }
                    else
                    {
                        ++WellWidth;
                        if (x > 0 && x < width_m1)
                        {
                            if (((v.LineCoverBits >> (x - 1)) & 7) == 5)
                                v.WellDepthTotle += ++v.WellDepth[x];
                        }
                        else if (x == 0)
                        {
                            if ((v.LineCoverBits & 3) == 2)
                                v.WellDepthTotle += ++v.WellDepth[0];
                        }
                        else
                        {
                            if (((v.LineCoverBits >> (width_m1 - 1)) & 3) == 1)
                                v.WellDepthTotle += ++v.WellDepth[width_m1];
                        }
                    }
                }
                if (WellWidth > MaxWellWidth)
                    MaxWellWidth = WellWidth;
                if (MaxWellWidth >= 1 && MaxWellWidth <= 6)
                {
                    if (ZZZ_BitCount((row_full ^ static_cast<uint32_t>(map.row(y))) & row_mask_) == MaxWellWidth)
                        v.WideWellDepth[MaxWellWidth - 1] += 2;
                    else
                        v.WideWellDepth[MaxWellWidth - 1] -= 1;
                }
            }
            if (HolePosy0 >= 0)
            {
                for (int y = HolePosy0; y < roof; ++y)
                {
                    int CheckLine = v.HoleBits0 & static_cast<int>(map.row(y));
                    if (CheckLine == 0) break;
                    v.ClearWidth0 += (y + 1) * ZZZ_BitCount(CheckLine);
                }
                if (HolePosy1 >= 0)
                {
                    for (int y = HolePosy1; y < roof; ++y)
                    {
                        int CheckLine = v.HoleBits1 & static_cast<int>(map.row(y));
                        if (CheckLine == 0) break;
                        v.ClearWidth1 += (y + 1) * ZZZ_BitCount(CheckLine);
                    }
                    if (HolePosy2 >= 0)
                    {
                        for (int y = HolePosy2; y < roof; ++y)
                        {
                            int CheckLine = v.HoleBits2 & static_cast<int>(map.row(y));
                            if (CheckLine == 0) break;
                            v.ClearWidth2 += (y + 1) * ZZZ_BitCount(CheckLine);
                        }
                    }
                }
            }

            // Compute column tops dynamically (Map<W,H> does not cache top[]).
            int top[map_width];
            for (int x = 0; x < map_width; ++x)
            {
                top[x] = 0;
                for (int y = roof - 1; y >= 0; --y)
                {
                    if (map.get(x, y))
                    {
                        top[x] = y + 1;
                        break;
                    }
                }
            }

            int low_x = 1;
            for (int x = 2; x < width_m1; ++x)
            {
                if (top[x] < top[low_x])
                    low_x = x;
            }
            if (top[0] <= top[low_x])
                low_x = 0;
            if (top[width_m1] <= top[low_x])
                low_x = width_m1;
            int low_y = top[low_x];
            int full = 0;
            for (int y = roof - 1; y >= 0; --y)
            {
                if (static_cast<uint32_t>(map.row(y)) == row_full)
                {
                    full = y + 1;
                    low_y -= y;
                    break;
                }
            }
            int tilt = 0;
            for (int x = low_x, ex = std::max(0, low_x - 5); x > ex; --x)
            {
                if (top[x] > top[x + 1])
                    tilt += 2;
                else if (top[x] == top[x + 1])
                    tilt += 1;
            }
            for (int x = low_x, ex = std::min(width_m1, low_x + 5); x < ex; ++x)
            {
                if (top[x] > top[x - 1])
                    tilt += 2;
                else if (top[x] == top[x - 1])
                    tilt += 1;
            }
            Result result;
            result.land_point = (0. - LandHeight * 1750 / map_height + Middle * 2 + EraseCount * 60);
            result.map = (0. - ColTrans * 80 - RowTrans * 80 - v.HoleCount * 160 - v.HoleLine * 380 - v.ClearWidth0 * 8 - v.ClearWidth1 * 4 - v.ClearWidth2 * 1 - v.WellDepthTotle * 160 + v.WideWellDepth[5] * 1 + v.WideWellDepth[4] * 2 + v.WideWellDepth[3] * 1 + v.WideWellDepth[2] * 48 + v.WideWellDepth[1] * -8 + v.WideWellDepth[0] * 2 + (low_x == 0 ? 200 : 0));
            result.tilt = tilt;
            result.full = full;
            result.count = map.popcount();
            result.clear = static_cast<int>(clear);
            result.low_y = low_y;
            result.node_top = lp.land_height();
            result.roof = roof;
            for (int up = 0; up < 21; ++up)
                result.safe_cache[up] = static_cast<int8_t>(map_in_danger_(map, static_cast<size_t>(up)));
            return result;
        }

        Status get(Result const &eval_result, size_t depth, Status const &status) const;
        Status iterate(Status const **status, size_t status_length) const;

    private:
        int width_{0}, height_{0};
        size_t type_max_{0};
        int col_mask_, row_mask_;
        struct MapInDangerData
        {
            int data[4];
        };
        std::vector<MapInDangerData> map_danger_data_;
        template<class MapT>
        size_t map_in_danger_(MapT const &map, size_t up) const
        {
            constexpr int MH = MapT::height;
            size_t danger = 0;
            for (size_t i = 0; i < map_danger_data_.size(); ++i)
            {
                size_t check_up = up;
                do
                {
                    int height = MH - static_cast<int>(check_up);
                    if (height >= 4
                        && (   (static_cast<uint32_t>(map_danger_data_[i].data[0]) & static_cast<uint32_t>(map.row(height - 4)))
                            || (static_cast<uint32_t>(map_danger_data_[i].data[1]) & static_cast<uint32_t>(map.row(height - 3)))
                            || (static_cast<uint32_t>(map_danger_data_[i].data[2]) & static_cast<uint32_t>(map.row(height - 2)))
                            || (static_cast<uint32_t>(map_danger_data_[i].data[3]) & static_cast<uint32_t>(map.row(height - 1)))))
                    {
                        ++danger;
                        break;
                    }
                } while (check_up-- > 0);
            }
            return danger;
        }
    };

    class the_ai_games
    {
    public:
        typedef search_tspin::Search::TSpinType TSpinType;
        using EvalSpinType = TSpinType;
        struct Config
        {
            double map_low_width;
            double col_trans_width;
            double row_trans_width;
            double hold_count_width;
            double hold_focus_width;
            double well_depth_width;
            double hole_depth_width;
            double dig_clear_width;
            double line_clear_width;
            double tspin_clear_width;
            double tetris_clear_width;
            double tspin_build_width;
            double combo_add_width;
            double combo_break_minute;
        };
        struct Result
        {
            double map;
            int node_top, map_low, clear, tbuild;
            int roof;
            TSpinType t_spin;
            int8_t safe_cache[21]; // safe_cache[up] = map_in_danger result for that up value
        };
        struct Status
        {
            size_t max_combo;
            size_t combo;
            double max_attack;
            double attack;
            int up[4];
            double land_point;
            double value;
            bool operator<(Status const &) const;
        };

    public:
        template<class Spec>
        void init(Config const *config);
        std::string ai_name() const;
        template<class Details>
        Result eval(m_tetris2::BBNode<Details, TSpinType> const &lp, typename Details::map_type const &map, typename Details::map_type const &src_map, size_t clear) const
        {
            using MapT = typename Details::map_type;
            constexpr int map_width  = MapT::width;
            constexpr int map_height = MapT::height;
            constexpr uint32_t row_full = static_cast<uint32_t>(MapT::row_full);

            int roof = map.none() ? 0 : (map.max_y() + 1);

            Result result;
            const int width_m1 = map_width - 1;
            int ColTrans = 2 * (map_height - roof);
            int RowTrans = roof == map_height ? 0 : map_width;
            for (int y = 0; y < roof; ++y)
            {
                uint32_t r = static_cast<uint32_t>(map.row(y));
                if (!(r & 1))
                {
                    ++ColTrans;
                }
                if (!(r >> width_m1 & 1))
                {
                    ++ColTrans;
                }
                ColTrans += ZZZ_BitCount((r ^ (r << 1)) & col_mask_);
                if (y != 0)
                {
                    RowTrans += ZZZ_BitCount(static_cast<uint32_t>(map.row(y - 1)) ^ r);
                }
            }
            if (roof > 0)
            {
                RowTrans += ZZZ_BitCount(~static_cast<uint32_t>(map.row(0)) & row_mask_);
                RowTrans += ZZZ_BitCount(static_cast<uint32_t>(map.row(roof - 1)) & row_mask_);
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

                int HoleNum[32];
                int WellNum[32];

                int LineCoverBits;
                double ClearWidth;
            } v;
            std::memset(&v, 0, sizeof v);

            for (int y = roof - 1; y >= 0; --y)
            {
                // bitboard semantics: map.row(y) bit=1 means occupied.
                uint32_t inv_row = static_cast<uint32_t>(map.row(y)) & row_mask_;
                v.LineCoverBits |= inv_row;
                int LineHole = v.LineCoverBits ^ inv_row;
                if (LineHole != 0)
                {
                    v.HoleCount += ZZZ_BitCount(LineHole);
                    v.HoleLine++;
                    for (int hy = y + 1; hy < roof; ++hy)
                    {
                        uint32_t CheckLine = LineHole & static_cast<uint32_t>(map.row(hy));
                        if (CheckLine == 0)
                        {
                            break;
                        }
                        v.ClearWidth += ZZZ_BitCount(CheckLine);
                    }
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
            result.map = (0. - roof * config_->map_low_width - ColTrans * config_->col_trans_width - RowTrans * config_->row_trans_width - v.HoleCount * config_->hold_count_width - v.HoleLine * config_->hold_focus_width - v.WellDepth * config_->well_depth_width - v.HoleDepth * config_->hole_depth_width - v.ClearWidth * config_->dig_clear_width);
            result.map_low = 0;
            while (result.map_low < map_height && static_cast<uint32_t>(map.row(result.map_low)) == row_full)
            {
                ++result.map_low;
            }

            // Compute column tops dynamically (Map<W,H> does not cache top[]).
            int top[map_width];
            for (int x = 0; x < map_width; ++x)
            {
                top[x] = 0;
                for (int y = roof - 1; y >= 0; --y)
                {
                    if (map.get(x, y))
                    {
                        top[x] = y + 1;
                        break;
                    }
                }
            }

            int attack_x = 1;
            for (int x = 2; x < width_m1; ++x)
            {
                if (top[x] < top[attack_x])
                {
                    attack_x = x;
                }
            }
            result.node_top = lp.land_height();
            result.clear = clear;
            result.tbuild = map_for_tspin_(map, attack_x, top[attack_x], roof);
            if (result.map_low == top[attack_x])
            {
                result.tbuild *= 8;
            }
            result.roof = roof;
            result.t_spin = lp.spin;
            for (int up = 0; up < 21; ++up)
                result.safe_cache[up] = static_cast<int8_t>(map_in_danger_(map, static_cast<size_t>(up)));
            return result;
        }
        Status get(Result const &eval_result, size_t depth, Status const &status) const;
        Status iterate(Status const **status, size_t status_length) const;

    private:
        int width_{0}, height_{0};
        size_t type_max_{0};
        Config const *config_;
        int col_mask_, row_mask_;
        struct MapInDangerData
        {
            int data[4];
        };
        std::vector<MapInDangerData> map_danger_data_;

        template<class MapT>
        int map_for_tspin_(MapT const &map, int x, int y, int roof) const
        {
            constexpr int map_height = MapT::height;
            constexpr uint32_t row_full = static_cast<uint32_t>(MapT::row_full);
            auto empty_row = [&](int iy) -> uint32_t
            {
                return (iy < map_height) ? (row_full ^ static_cast<uint32_t>(map.row(iy))) & row_mask_ : 0u;
            };
            --x;
            uint32_t row0 = empty_row(y);
            uint32_t row1 = empty_row(y + 1);
            if (((~row0) & row1 & static_cast<uint32_t>(row_mask_)) != 0)
            {
                return 0;
            }
            int value = 1;
            uint32_t row2 = empty_row(y + 2);
            if (((row0 >> x) & 7) == 5)
            {
                value += 3;
                if (ZZZ_BitCount(row0) == MapT::width - 1)
                {
                    value += 3;
                }
                if (((((~row1) & row2) >> x) & ~7u) == 0 && ((row1 >> x) & 7) == 0)
                {
                    value += 4;
                    if (ZZZ_BitCount(row1) == MapT::width - 3)
                    {
                        value += 4;
                    }
                    int row2_check = (row2 >> x) & 7;
                    if (row2_check == 1 || row2_check == 4)
                    {
                        value += 2;
                    }
                }
            }
            uint32_t mask = (row1 >> x) & 7;
            for (int iy = y + 2; iy < roof; ++iy)
            {
                mask |= (empty_row(iy) >> x) & 7;
                if (ZZZ_BitCount(mask) > 1)
                {
                    return 0;
                }
            }
            return value;
        }

        template<class MapT>
        size_t map_in_danger_(MapT const &map, size_t up) const
        {
            constexpr int MH = MapT::height;
            size_t danger = 0;
            for (size_t i = 0; i < map_danger_data_.size(); ++i)
            {
                size_t check_up = up;
                do
                {
                    int height = MH - static_cast<int>(check_up);
                    if (height >= 4
                        && (   (static_cast<uint32_t>(map_danger_data_[i].data[0]) & static_cast<uint32_t>(map.row(height - 4)))
                            || (static_cast<uint32_t>(map_danger_data_[i].data[1]) & static_cast<uint32_t>(map.row(height - 3)))
                            || (static_cast<uint32_t>(map_danger_data_[i].data[2]) & static_cast<uint32_t>(map.row(height - 2)))
                            || (static_cast<uint32_t>(map_danger_data_[i].data[3]) & static_cast<uint32_t>(map.row(height - 1)))))
                    {
                        ++danger;
                        break;
                    }
                } while (check_up-- > 0);
            }
            return danger;
        }
    };

    class the_ai_games_enemy
    {
    public:
        typedef search_tspin::Search::TSpinType TSpinType;
        using EvalSpinType = TSpinType;
        struct Config
        {
            int *up_ptr;
            int *point_ptr;
        };
        struct Result
        {
            size_t clear, tspin;
        };
        struct Status
        {
            int up[4];
            int point, combo;
            bool operator<(Status const &) const;
        };

    public:
        template<class Spec>
        void init(Config const *param);
        std::string ai_name() const;
        template<class Details>
        Result eval(m_tetris2::BBNode<Details, TSpinType> const &lp, typename Details::map_type const &map, typename Details::map_type const &src_map, size_t clear) const
        {
            Result result =
                {
                    clear, lp.spin != TSpinType::None ? clear : size_t(0)};
            return result;
        }
        Status get(Result const &eval_result, size_t depth, Status const &status) const;
        Status iterate(Status const **status, size_t status_length) const;

        private:

            Config const *config_;

        };

    

        template<class Spec>

        void the_ai_games_old::init()

        {

            width_ = static_cast<int>(Spec::width);

            height_ = static_cast<int>(Spec::height);

            type_max_ = m_tetris2::ai_rule_spec::piece_count<Spec>();

            m_tetris2::ai_rule_spec::fill_spawn_danger_data<Spec, MapInDangerData, static_cast<int>(Spec::height) - 4, 1>(map_danger_data_);

            col_mask_ = static_cast<int>(m_tetris2::ai_rule_spec::row_mask<Spec>() & ~1u);

            row_mask_ = static_cast<int>(m_tetris2::ai_rule_spec::row_mask<Spec>());

        }

    

        template<class Spec>

        void the_ai_games::init(Config const *config)

        {

            width_ = static_cast<int>(Spec::width);

            height_ = static_cast<int>(Spec::height);

            type_max_ = m_tetris2::ai_rule_spec::piece_count<Spec>();

            config_ = config;

            m_tetris2::ai_rule_spec::fill_spawn_danger_data<Spec, MapInDangerData, static_cast<int>(Spec::height) - 4, 1>(map_danger_data_);

            col_mask_ = static_cast<int>(m_tetris2::ai_rule_spec::row_mask<Spec>() & ~1u);

            row_mask_ = static_cast<int>(m_tetris2::ai_rule_spec::row_mask<Spec>());

        }

    

        template<class Spec>

        void the_ai_games_enemy::init(Config const *param)

        {

            config_ = param;

        }

    }

    