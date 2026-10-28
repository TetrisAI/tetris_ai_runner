


#include "tetris_core.h"
#include "integer_utils.h"
#include "tetris_node.h"
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

        // FarteryhrMap 模板化，接受任意满足 TetrisMap<W,H> 接口的地图类型，
        // 以及 GetLanding 几何视图（LandPoint）。
        // MapT 语义：bit=1 = occupied（1=occupied），与 TetrisMap<W,H> 一致。
        // 迁移说明：旧版以 TetrisNode<Details,...> 为 LandPoint，kHeight 为编译期常量；
        // 新版 LandPoint = GetLanding，piece 高度为运行期成员 h（与旧 piece_h 同值）。
        // 这里仅把循环上界由静态 kHeight 改为运行期 lp.h，y 翻转 / line 几何 / 满行支撑 /
        // 整数除法等全部原算法不变。
        template<class LandPoint, class MapT>
        struct FarteryhrMap
        {
            FarteryhrMap(LandPoint const &_lp, MapT const &_map, int _fhh)
                : lp(_lp), map(_map), fhh(_fhh), fh(MapT::height - _fhh)
            {
                constexpr int map_width  = MapT::width;
                constexpr int map_height = MapT::height;
                constexpr uint32_t row_full = static_cast<uint32_t>(MapT::row_full);
                // 旧 LandPoint::kHeight（编译期）-> 运行期 lp.h（同值）。
                const int kH = static_cast<int>(lp.h);

                std::memset(node_data, 0, sizeof(node_data));
                for(int y = 0; y < kH; ++y)
                    node_data[y + lp.row()] = static_cast<int>(lp.line(y));
                for(int i = 0; i < m_tetris2::max_height; ++i)
                {
                    // 1=occupied：occupied count = popcount(map.occupied_row(i))
                    row_count[i] = (i < map_height)
                        ? ZZZ_BitCount(static_cast<uint32_t>(map.occupied_row(i)))
                        : 0;
                }
                for(int y = 0; y < kH; ++y)
                    row_count[lp.row() + y] += ZZZ_BitCount(static_cast<int>(lp.line(y)));
                block_count = 0;
                for(int x = lp.col(); x < lp.col() + lp.width(); ++x)
                {
                    for(int y = 0; y < kH; ++y)
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
                if((static_cast<uint32_t>(map.occupied_row(y)) >> x) & 1u)
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
        using Landing = m_tetris2::GetLanding<std::monostate>;
        // Result 收缩为空 struct：farter 的全部加权量依赖 src_map(落块前盘) + 落块几何，
        // 无纯盘面(落块后盘)量，故 eval 为空、全部计算移进 get。
        struct Result {};
        struct Status
        {
            int eval;
            int value;
            bool operator < (Status const &) const;
        };
        template<class Spec>
        void init();
        std::string ai_name() const;
        // 冻结 GetArgs（shard D）：MapBefore, Landing<L>, Parent<Status>, Depth。
        // 无 Eval/Clear/Env。MapBefore 即旧 src_map（落块前源盘）。
        using GetArgs = m_tetris2::type_list<m_tetris2::args::MapBefore,
                                             m_tetris2::args::Landing<Landing>,
                                             m_tetris2::args::Parent<Status>,
                                             m_tetris2::args::Depth>;

        // eval 为空：farter 不消费落块后盘。engine 在 0 eval 下仍须能取到并跑 get。
        template<class MapT>
        Result eval(MapT const & /*map*/) const
        {
            return Result{};
        }

        // get：以 src_map(MapBefore) + Landing 合成 FarteryhrMap，跑原整数加权累积 pts，
        // 再按旧 get 与父 Status 累积、depth 整除。仅 MapBefore/Landing/Parent/Depth。
        template<class MapT>
        Status get(MapT const &src_map, Landing const &landing, Status const &parent, size_t depth) const
        {
            constexpr int fw = MapT::width;
            constexpr int fh_full = MapT::height;
            int fh = fh_full - fhh;

            detail::FarteryhrMap<Landing, MapT> fmap(landing, src_map, fhh);
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

            // 旧 get：result.eval = pts + parent.eval；result.value = result.eval / depth。
            Status result;
            result.eval = pts + parent.eval;
            result.value = result.eval / static_cast<int>(depth);
            return result;
        }
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
