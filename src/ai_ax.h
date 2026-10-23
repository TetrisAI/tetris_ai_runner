
#include "tetris_core.h"
#include "integer_utils.h"
#include "tetris_node.h"
#include "ai_rule_spec_utils.h"

namespace ai_ax
{
    class AI
    {
    public:
        template<class Spec>
        void init();
        std::string ai_name() const;
        struct Result
        {
            double land_point, map;
        };
        struct Status
        {
            double land_point;
            double value;
            bool operator < (Status const &) const;
        };

        template<class Details>
        Result eval(m_tetris2::TetrisNode<Details, std::monostate> const &lp, typename Details::map_type const &map, typename Details::map_type const &src_map, size_t clear) const
        {
            using MapT = typename Details::map_type;
            constexpr int map_width  = MapT::width;
            constexpr int map_height = MapT::height;
            constexpr uint32_t row_full = static_cast<uint32_t>(MapT::row_full);

            // roof: equivalent of TetrisMap::roof
            int roof = map.none() ? 0 : (map.max_y() + 1);

            double LandHeight = lp.y + 1;
            double Middle = std::abs((lp.col() + 1) * 2 - map_width);
            double EraseCount = static_cast<double>(clear);

            const int width_m1 = map_width - 1;

            // ColTrans/RowTrans: TetrisMap<W,H> uses 1=occupied semantics.
            // bottom row: empty count = popcount(row_full ^ map.occupied_row(0))
            // top row empty: empty count = popcount(row_full ^ map.occupied_row(roof-1))
            //                occupied count (roof==height case) = popcount(map.occupied_row(roof-1))
            int ColTrans = 2 * (map_height - roof);
            int RowTrans = ZZZ_BitCount(static_cast<uint32_t>(row_full ^ map.occupied_row(0))) +
                           ZZZ_BitCount(static_cast<uint32_t>(map.occupied_row(roof - 1)));
            for (int y = 0; y < roof; ++y)
            {
                uint32_t occ_row = static_cast<uint32_t>(map.occupied_row(y)) & static_cast<uint32_t>(row_full);
                uint32_t empty_row = static_cast<uint32_t>(row_full) ^ occ_row;
                if (empty_row & 1u)
                    ++ColTrans;
                if ((empty_row >> width_m1) & 1u)
                    ++ColTrans;
                ColTrans += ZZZ_BitCount((occ_row ^ (occ_row << 1)) & static_cast<uint32_t>(col_mask_));
                if (y != 0)
                    RowTrans += ZZZ_BitCount(static_cast<uint32_t>(map.occupied_row(y - 1)) ^ occ_row);
            }
            struct
            {
                int HoleCount;
                int HoleLine;
                int HolePosy;
                int HolePiece;
                int HoleDepth;
                int WellDepth;
                int HoleNum[32];
                int WellNum[32];
                uint32_t LineCoverBits;  // accumulated occupied bits
                uint32_t TopHoleBits;    // hole-column bits (set where a covered empty cell exists)
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
                    if (((occ_row >> (x - 1)) & 7u) == 5u)
                        v.WellDepth += ++v.WellNum[x];
                    else
                        v.WellNum[x] = 0;
                }
                if (LineHole & 1)
                    v.HoleDepth += ++v.HoleNum[0];
                else
                    v.HoleNum[0] = 0;
                if ((occ_row & 3u) == 2u)
                    v.WellDepth += ++v.WellNum[0];
                else
                    v.WellNum[0] = 0;
                if ((LineHole >> width_m1) & 1)
                    v.HoleDepth += ++v.HoleNum[width_m1];
                else
                    v.HoleNum[width_m1] = 0;
                if (((occ_row >> (width_m1 - 1)) & 3u) == 1u)
                    v.WellDepth += ++v.WellNum[width_m1];
                else
                    v.WellNum[width_m1] = 0;
            }
            if (v.HolePosy != 0)
            {
                for (int y = v.HolePosy; y < roof; ++y)
                {
                    // TopHoleBits uses 1=empty semantics; compare against empty bits of current row
                    uint32_t CheckLine = v.TopHoleBits & static_cast<uint32_t>(row_full ^ map.occupied_row(y));
                    if (CheckLine == 0)
                        break;
                    v.HolePiece += (y + 1) * ZZZ_BitCount(CheckLine);
                }
            }

            int BoardDeadZone = static_cast<int>(map_in_danger_(map));

            Result result;
            result.land_point = (0 - LandHeight * 1750 / map_height + Middle * 2 + EraseCount * 60);
            result.map = (0 - ColTrans * 80 - RowTrans * 80 - v.HoleCount * 60 - v.HoleLine * 380 - v.WellDepth * 100 - v.HoleDepth * 40 - v.HolePiece * 5 - BoardDeadZone * 50000);
            return result;
        }

        Status get(Result const &eval_result, size_t depth, Status const &status) const;
        Status iterate(Status const **status, size_t status_length) const;

    private:
        struct MapInDangerData
        {
            int data[4];
        };
        std::vector<MapInDangerData> map_danger_data_;
        size_t type_max_{0};
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

    template<class Spec>
    void AI::init()
    {
        type_max_ = m_tetris2::ai_rule_spec::piece_count<Spec>();
        m_tetris2::ai_rule_spec::fill_spawn_danger_data<Spec>(map_danger_data_);
        col_mask_ = static_cast<int>(m_tetris2::ai_rule_spec::row_mask<Spec>() & ~1u);
        row_mask_ = static_cast<int>(m_tetris2::ai_rule_spec::row_mask<Spec>());
    }
}
