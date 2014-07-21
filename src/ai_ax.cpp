
//Code by ax_pokl
//Modify by ZouZhiZhang

#include "tetris_core.h"
#include "ai_ax.h"

using namespace m_tetris;

namespace ai_ax_1
{
    void AI::init(m_tetris::TetrisContext const *context)
    {
        context_ = context;
        map_danger_data_.resize(context->type_max());
        for(size_t i = 0; i < context->type_max(); ++i)
        {
            TetrisMap map =
            {
                {}, {}, context->width(), context->height()
            };
            TetrisNode const *node = context->generate(i);
            node->attach(map);
            memcpy(map_danger_data_[i].data, &map.row[map.height - 4], sizeof map_danger_data_[i].data);
            for(int y = 0; y < 3; ++y)
            {
                map_danger_data_[i].data[y + 1] |= map_danger_data_[i].data[y];
            }
        }
    }

    std::string AI::ai_name() const
    {
        return "Tetris_ax_C ZZZ Mod v1.2";
    }

    double AI::eval_land_point(TetrisNode const *node, TetrisMap const &map, size_t clear)
    {
        double LandHeight = node->status.y + 1;
        double Middle = std::abs((node->status.x + 1) * 2 - map.width);
        double EraseCount = clear;
        return (0
            - LandHeight * 200 / map.height
            + Middle  * 0.2
            + EraseCount * 6
            );
    }

    double AI::eval_map_bad() const
    {
        return -99999999;
    }

    double AI::eval_map(TetrisMap const &map, EvalParam<double> const *history, size_t history_length)
    {
        //行列变换
        int ColTrans = 2 * (map.height - map.roof);
        int RowTrans = map.roof == map.height ? 0 : map.width;
        for(int y = 0; y < map.roof; ++y)
        {
            if(!map.full(0, y))
            {
                ++ColTrans;
            }
            if(!map.full(map.width - 1, y))
            {
                ++ColTrans;
            }
            int TransBits = map.row[y] ^ (map.row[y] << 1);
            for(int x = 1; x < map.width; ++x)
            {
                if((TransBits >> x) & 1)
                {
                    ++ColTrans;
                }
            }
        }
        for(int y = 1; y < map.roof; ++y)
        {
            int TransBits = map.row[y - 1] ^ map.row[y];
            for(int x = 0; x < map.width; ++x)
            {
                if((TransBits >> x) & 1)
                {
                    ++RowTrans;
                }
            }
        }
        int Row0Bits = ~map.row[0];
        int Row1Bits = map.roof == map.height ? ~map.row[map.roof - 1] : map.row[map.roof - 1];
        for(int x = 0; x < map.width; ++x)
        {
            if((Row0Bits >> x) & 1)
            {
                ++RowTrans;
            }
            if((Row1Bits >> x) & 1)
            {
                ++RowTrans;
            }
        }

        struct
        {
            //洞数
            int HoleCount;
            //洞行数
            int HoleLine;
            //最高洞行数
            int HolePosy;
            //最高洞上方块数
            int HolePiece;

            //洞深,井深
            int HoleDepth;
            int WellDepth;

            //洞计数,井计数
            int HoleNum[32];
            int WellNum[32];

            //当前块行数
            int LandHeight;
            //设置左中右平衡破缺参数
            int Middle;
            //消行数
            int EraseCount;

            int LineCoverBits;
            int TopHoleBits;
        } v;
        memset(&v, 0, sizeof v);

        for(int y = map.roof - 1; y >= 0; --y)
        {
            v.LineCoverBits |= map.row[y];
            int LineHole = v.LineCoverBits ^ map.row[y];
            if(LineHole != 0)
            {
                v.HoleLine++;
                if(v.HolePosy == 0)
                {
                    v.HolePosy = y + 1;
                    v.TopHoleBits = LineHole;
                }
            }
            for(int x = 0; x < map.width; ++x)
            {
                if((LineHole >> x) & 1)
                {
                    ++v.HoleCount;
                    ++v.HoleNum[x];
                }
                else
                {
                    v.HoleNum[x] = 0;
                }
                v.HoleDepth += v.HoleNum[x];
                if(!((v.LineCoverBits >> x) & 1) && (x == 0 || ((v.LineCoverBits >> (x - 1)) & 1)) && (x == map.width - 1 || ((v.LineCoverBits >> (x + 1)) & 1)))
                {
                    ++v.WellNum[x];
                    v.WellDepth += v.WellNum[x];
                }
            }
        }
        if(v.HolePosy != 0)
        {
            //从最高有洞行上一行开始往上厉遍
            for(int y = v.HolePosy; y < map.roof; ++y)
            {
                int CheckLine = v.TopHoleBits & map.row[y];
                if(CheckLine == 0)
                {
                    break;
                }
                for(int x = 0; x < map.width; ++x)
                {
                    if((CheckLine >> x) & 1)
                    {
                        v.HolePiece += y + 1;
                    }
                }
            }
        }
        double land_point_value = 0;
        for(size_t i = 0; i < history_length; ++i)
        {
            land_point_value += history[i].eval;
        }

        //死亡警戒
        int BoardDeadZone = map_in_danger_(map);
        return (0
            + land_point_value / history_length
            - ColTrans * 8
            - RowTrans * 8
            - v.HoleCount * 6
            - v.HoleLine * 38
            - v.WellDepth * 10
            - v.HoleDepth * 4
            - v.HolePiece * 0.5
            - BoardDeadZone * 5000
            );
    }

    double AI::get_virtual_eval(double const *eval, size_t eval_length)
    {
        double result = 0;
        for(size_t i = 0; i < eval_length; ++i)
        {
            result += eval[i];
        }
        return result / eval_length;
    }

    size_t AI::prune_map(m_tetris::PruneParam<double> *prune, size_t prune_length, TetrisNode const **after_pruning, size_t next_length)
    {
        struct
        {
            bool operator()(m_tetris::PruneParam<double> const &left, m_tetris::PruneParam<double> const &right)
            {
                return left.eval > right.eval;
            }
        } c;
        std::sort(prune, prune + prune_length, c);
        size_t hold_count = std::min<size_t>(prune_length, 8);
        for(size_t i = 0; i < hold_count; ++i)
        {
            after_pruning[i] = prune[i].land_point;
        }
        return hold_count;
    }

    size_t AI::map_in_danger_(m_tetris::TetrisMap const &map)
    {
        size_t danger = 0;
        for(size_t i = 0; i < context_->type_max(); ++i)
        {
            if(map_danger_data_[i].data[0] & map.row[map.height - 4] || map_danger_data_[i].data[1] & map.row[map.height - 3] || map_danger_data_[i].data[2] & map.row[map.height - 2] || map_danger_data_[i].data[3] & map.row[map.height - 1])
            {
                ++danger;
            }
        }
        return danger;
    }

}


namespace ai_ax_0
{
    void AI::init(m_tetris::TetrisContext const *context)
    {
        context_ = context;
        map_danger_data_.resize(context->type_max());
        for(size_t i = 0; i < context->type_max(); ++i)
        {
            TetrisMap map =
            {
                {}, {}, context->width(), context->height()
            };
            TetrisNode const *node = context->generate(i);
            node->attach(map);
            memcpy(map_danger_data_[i].data, &map.row[map.height - 4], sizeof map_danger_data_[i].data);
            for(int y = 0; y < 3; ++y)
            {
                map_danger_data_[i].data[y + 1] |= map_danger_data_[i].data[y];
            }
        }
    }

    std::string AI::ai_name() const
    {
        return "Tetris_ax_C ZZZ Mod v1.2";
    }

    double AI::eval_map_bad() const
    {
        return -99999999;
    }
    
    double AI::eval_map(TetrisMap const &map, EvalParam<> const *history, size_t history_length) const
    {
        //行列变换
        int ColTrans = 2 * (map.height - map.roof);
        int RowTrans = map.roof == map.height ? 0 : map.width;
        for(int y = 0; y < map.roof; ++y)
        {
            if(!map.full(0, y))
            {
                ++ColTrans;
            }
            if(!map.full(map.width - 1, y))
            {
                ++ColTrans;
            }
            int TransBits = map.row[y] ^ (map.row[y] << 1);
            for(int x = 1; x < map.width; ++x)
            {
                if((TransBits >> x) & 1)
                {
                    ++ColTrans;
                }
            }
        }
        for(int y = 1; y < map.roof; ++y)
        {
            int TransBits = map.row[y - 1] ^ map.row[y];
            for(int x = 0; x < map.width; ++x)
            {
                if((TransBits >> x) & 1)
                {
                    ++RowTrans;
                }
            }
        }
        int Row0Bits = ~map.row[0];
        int Row1Bits = map.roof == map.height ? ~map.row[map.roof - 1] : map.row[map.roof - 1];
        for(int x = 0; x < map.width; ++x)
        {
            if((Row0Bits >> x) & 1)
            {
                ++RowTrans;
            }
            if((Row1Bits >> x) & 1)
            {
                ++RowTrans;
            }
        }

        struct
        {
            //洞数
            int HoleCount;
            //洞行数
            int HoleLine;
            //最高洞行数
            int HolePosy;
            //最高洞上方块数
            int HolePiece;

            //洞深,井深
            int HoleDepth;
            int WellDepth;

            //洞计数,井计数
            int HoleNum[32];
            int WellNum[32];

            //当前块行数
            int LandHeight;
            //设置左中右平衡破缺参数
            int Middle;
            //消行数
            int EraseCount;

            int LineCoverBits;
            int TopHoleBits;
        } v;
        memset(&v, 0, sizeof v);

        for(int y = map.roof - 1; y >= 0; --y)
        {
            v.LineCoverBits |= map.row[y];
            int LineHole = v.LineCoverBits ^ map.row[y];
            if(LineHole != 0)
            {
                v.HoleLine++;
                if(v.HolePosy == 0)
                {
                    v.HolePosy = y + 1;
                    v.TopHoleBits = LineHole;
                }
            }
            for(int x = 0; x < map.width; ++x)
            {
                if((LineHole >> x) & 1)
                {
                    ++v.HoleCount;
                    ++v.HoleNum[x];
                }
                else
                {
                    v.HoleNum[x] = 0;
                }
                v.HoleDepth += v.HoleNum[x];
                if(!((v.LineCoverBits >> x) & 1) && (x == 0 || ((v.LineCoverBits >> (x - 1)) & 1)) && (x == map.width - 1 || ((v.LineCoverBits >> (x + 1)) & 1)))
                {
                    ++v.WellNum[x];
                    v.WellDepth += v.WellNum[x];
                }
            }
        }
        if(v.HolePosy != 0)
        {
            //从最高有洞行上一行开始往上厉遍
            for(int y = v.HolePosy; y < map.roof; ++y)
            {
                int CheckLine = v.TopHoleBits & map.row[y];
                if(CheckLine == 0)
                {
                    break;
                }
                for(int x = 0; x < map.width; ++x)
                {
                    if((CheckLine >> x) & 1)
                    {
                        v.HolePiece += y + 1;
                    }
                }
            }
        }

        for(size_t i = 0; i < history_length; ++i)
        {
            TetrisNode const *node = history[i].node;
            v.LandHeight += node->status.y + 1;
            v.Middle += std::abs((node->status.x + 1) * 2 - map.width);
            v.EraseCount += history[i].clear;
        }

        //死亡警戒
        int BoardDeadZone = map_in_danger_(map);
        int hl = history_length;
        return (0
                - v.LandHeight * 200 / map.height / hl
                + v.Middle * 0.2 / hl
                + v.EraseCount * 6 / hl
                - ColTrans * 8
                - RowTrans * 8
                - v.HoleCount * 6
                - v.HoleLine * 38
                - v.WellDepth * 10
                - v.HoleDepth * 4
                - v.HolePiece * 0.5
                - BoardDeadZone * 5000
                );
    }
    
    size_t AI::map_in_danger_(m_tetris::TetrisMap const &map) const
    {
        size_t danger = 0;
        for(size_t i = 0; i < context_->type_max(); ++i)
        {
            if(map_danger_data_[i].data[0] & map.row[map.height - 4] || map_danger_data_[i].data[1] & map.row[map.height - 3] || map_danger_data_[i].data[2] & map.row[map.height - 2] || map_danger_data_[i].data[3] & map.row[map.height - 1])
            {
                ++danger;
            }
        }
        return danger;
    }

}