

#include "tetris_core.h"
#include "integer_utils.h"
#include "bb_node.h"
#include "ai_rule_spec_utils.h"

namespace ai_farteryhr
{
    namespace detail
    {
        enum BlockStatus
        {
            BLK_EMPTY = 0,
            BLK_FORMER = 1,
            BLK_NEW = 2,
            BLK_OOB = -1,
        };

        // FarteryhrMap 现在模板化，接受任意满足 Map<W,H> 接口的地图类型。
        // MapT 语义：bit=1 = occupied（1=occupied），与 Map<W,H> 一致。
        template<class LandPoint, class MapT>
        struct FarteryhrMap
        {
            FarteryhrMap(LandPoint const &_lp, MapT const &_map, int _fhh)
                : lp(_lp), map(_map), fhh(_fhh), fh(MapT::height - _fhh)
            {
                constexpr int map_width  = MapT::width;
                constexpr int map_height = MapT::height;
                constexpr uint32_t row_full = static_cast<uint32_t>(MapT::row_full);

                std::memset(node_data, 0, sizeof(node_data));
                for(int y = 0; y < LandPoint::kHeight; ++y)
                    node_data[y + lp.row()] = static_cast<int>(lp.line(y));
                for(int i = 0; i < m_tetris2::max_height; ++i)
                {
                    // 1=occupied：occupied count = popcount(map.row(i))
                    row_count[i] = (i < map_height)
                        ? ZZZ_BitCount(static_cast<uint32_t>(map.row(i)))
                        : 0;
                }
                for(int y = 0; y < LandPoint::kHeight; ++y)
                    row_count[lp.row() + y] += ZZZ_BitCount(static_cast<int>(lp.line(y)));
                block_count = 0;
                for(int x = lp.col(); x < lp.col() + lp.width(); ++x)
                {
                    for(int y = 0; y < LandPoint::kHeight; ++y)
                    {
                        if((static_cast<int>(lp.line(y)) >> x) & 1)
                        {
                            auto &b = block[block_count++];
                            b.x = x;
                            b.y = fh - 1 - (y + lp.row());
                        }
                    }
                }
            }

            LandPoint const &lp;
            MapT const &map;
            int node_data[m_tetris2::max_height];
            int fhh, fh;
            size_t row_count[m_tetris2::max_height];
            struct Point { int x, y; } block[16];
            size_t block_count;

            BlockStatus get(int x, int y)
            {
                constexpr int map_width  = MapT::width;
                constexpr int map_height = MapT::height;
                y = fh - y - 1;
                if(x < 0 || x >= map_width || y < 0 || y >= map_height)
                    return BLK_OOB;
                if((node_data[y] >> x) & 1)
                    return BLK_NEW;
                // 1=occupied：bit=1 表示已有方块 → BLK_FORMER
                if((static_cast<uint32_t>(map.row(y)) >> x) & 1u)
                    return BLK_FORMER;
                return BLK_EMPTY;
            }

            int count(int y)
            {
                constexpr int map_height = MapT::height;
                y = fh - y - 1;
                if(y < 0 || y >= map_height)
                    return 0;
                return static_cast<int>(row_count[y]);
            }

            int getgapdep(int x, int y)
            {
                constexpr int map_width = MapT::width;
                int gapdep = 0;
                bool ingap = false;
                for(int j = 0; j < 6; ++j)
                {
                    if(count(y + j) == map_width)
                        continue;
                    if(get(x, y + j) != BLK_EMPTY)
                        break;
                    else if(ingap || (get(x + 1, y + j) != BLK_EMPTY && get(x - 1, y + j) != BLK_EMPTY))
                    {
                        gapdep++;
                        ingap = true;
                    }
                }
                return gapdep;
            }
        };
    } // namespace detail

    class AI
    {
    public:
        using Result = int;
        struct Status
        {
            int eval;
            int value;
            bool operator < (Status const &) const;
        };
        template<class Spec>
        void init();
        std::string ai_name() const;

        template<class Details>
        int eval(m_tetris2::BBNode<Details, std::monostate> const &lp, typename Details::map_type const &map, typename Details::map_type const &src_map, size_t clear) const
        {
            using MapT = typename Details::map_type;
            constexpr int fw = MapT::width;
            constexpr int fh_full = MapT::height;
            int fh = fh_full - fhh;

            detail::FarteryhrMap<m_tetris2::BBNode<Details, std::monostate>, MapT> fmap(lp, src_map, fhh);
            int pts = 0;

            for(size_t i = 0; i < fmap.block_count; ++i)
            {
                int cx = fmap.block[i].x;
                int cy = fmap.block[i].y;
                if(fmap.get(cx, cy - 1) == detail::BLK_FORMER)
                    pts += 100;
                if(fmap.get(cx - 1, cy) == detail::BLK_FORMER)
                    pts += 50;
                if(fmap.get(cx - 1, cy) == detail::BLK_OOB)
                    pts += 60;
                if(fmap.get(cx + 1, cy) == detail::BLK_FORMER)
                    pts += 50;
                if(fmap.get(cx + 1, cy) == detail::BLK_OOB)
                    pts += 60;

                if(fmap.count(cy) == fw)
                    continue;

                pts -= (fh - cy - 1) * 50;

                if(fmap.get(cx, cy + 1) == detail::BLK_EMPTY)
                    pts -= 20;

                {
                    int maxgapside = 0;
                    for(int dx = -1; dx <= 1; dx += 2)
                    {
                        if(fmap.get(cx + dx, cy) != detail::BLK_EMPTY)
                            continue;
                        int getgaptmp = fmap.getgapdep(cx + dx, cy);
                        if(maxgapside < getgaptmp)
                            maxgapside = getgaptmp;
                    }
                    if(maxgapside >= 6)
                        pts -= 600;
                    else if(maxgapside > 3)
                        pts -= 300;
                    else if(maxgapside == 3)
                        pts -= 250;
                    else if(maxgapside == 2)
                        pts -= 50;

                    if(fmap.get(cx, cy + 1) != detail::BLK_EMPTY)
                    {
                        int gapcvr = fmap.getgapdep(cx, cy + 1);
                        pts -= (gapcvr == 0) ? 50 : 200;
                    }
                }

                int sumempty = 0;
                int scrcover = 0;
                int dist;
                for(int j = 0; j < 4 && j + cy < fh; j++)
                {
                    if(fmap.get(cx, cy + j) != detail::BLK_EMPTY)
                    {
                        int emptyC = fw - fmap.count(cy + j);
                        sumempty += emptyC;
                        for(int dx = -1; dx <= 1; dx += 2)
                        {
                            if(fmap.get(cx + dx, cy + j) == detail::BLK_EMPTY)
                            {
                                if(fmap.get(cx + dx + dx, cy + j) != detail::BLK_EMPTY)
                                    sumempty += 3;
                                sumempty += 1;
                            }
                        }
                    }
                    else
                    {
                        dist = 0;
                        for(int k = 0; k <= j; k++)
                        {
                            if(fmap.count(cy + k) < fw)
                                dist++;
                        }
                        if(dist == 0)
                            continue;
                        if(fmap.get(cx - 1, cy + j) != detail::BLK_EMPTY || fmap.get(cx + 1, cy + j) != detail::BLK_EMPTY)
                        {
                            for(int dx = -1; dx <= 1; dx += 2)
                            {
                                if(fmap.get(cx + dx, cy) == detail::BLK_EMPTY)
                                    scrcover += 100 / dist;
                                else
                                    scrcover += 50 / dist;
                            }
                        }
                        else
                        {
                            scrcover += 50 / dist;
                        }
                    }
                }
                pts += scrcover * sumempty * (-1);
            }

            int lcC = 0, lcW = 1;
            for(int i = fhh * (-1); i < fh; i++)
            {
                if(fmap.count(i) == fw)
                {
                    lcC++;
                    if(i < 6)
                        lcW = 5;
                }
            }
            switch(lcC)
            {
            case 0: break;
            case 1: pts += 20 * lcW; break;
            case 2: pts += 50 * lcW; break;
            case 3: pts += 200 * lcW; break;
            case 4: pts += 1000 * lcW; break;
            }

            return pts;
        }

        Status get(int eval_result, size_t depth, Status const &status) const;
    private:
        template<class Spec, std::size_t I = 0>
        void init_fhh_()
        {
            if constexpr (I < m_tetris2::ai_rule_spec::piece_count<Spec>())
            {
                constexpr char T = m_tetris2::ai_rule_spec::piece_type_at<Spec>(I);
                fhh = std::max<int>(fhh, static_cast<int>(Spec::height) - m_tetris2::ai_rule_spec::spawn_min_row<Spec, T>());
                init_fhh_<Spec, I + 1>();
            }
        }

        int fhh;
    };

    template<class Spec>
    void AI::init()
    {
        fhh = 0;
        init_fhh_<Spec>();
    }
}
