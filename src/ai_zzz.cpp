
//by ZouZhiZhang

#include "tetris_core.h"
#include "ai_zzz.h"

using namespace m_tetris;

namespace
{
    int BitCount(unsigned int n)
    {
        // HD, Figure 5-2
        n = n - ((n >> 1) & 0x55555555);
        n = (n & 0x33333333) + ((n >> 2) & 0x33333333);
        n = (n + (n >> 4)) & 0x0f0f0f0f;
        n = n + (n >> 8);
        n = n + (n >> 16);
        return n & 0x3f;
    }
}

namespace ai_zzz
{
    namespace qq
    {
        void Attack::init(m_tetris::TetrisContext const *context, Param const *param)
        {
            context_ = context;
            param_ = param;
            check_line_.clear();
            const int full = context->full();
            for(int x = 0; x < context->width(); ++x)
            {
                check_line_.insert(full & ~(1 << x));
            }
            map_danger_data_.resize(context->type_max());
            for(size_t i = 0; i < context->type_max(); ++i)
            {
                TetrisMap map =
                {
                    context->width(), context->height()
                };
                TetrisNode const *node = context->generate(i);
                node->attach(map);
                memcpy(map_danger_data_[i].data, &map.row[map.height - 4], sizeof map_danger_data_[i].data);
                for(int y = 0; y < 3; ++y)
                {
                    map_danger_data_[i].data[y + 1] |= map_danger_data_[i].data[y];
                }
            }
            col_mask_ = context->full() & ~1;
            row_mask_ = context->full();
        }

        std::string Attack::ai_name() const
        {
            return "AX Attack v0.1";
        }

        double Attack::eval_land_point(TetrisNode const *node, TetrisMap const &map, size_t clear)
        {
            double LandHeight = node->low + 1;
            double Middle = std::abs((node->status.x + 1) * 2 - map.width);
            double EraseCount = clear;

            return (0
                    - LandHeight * 16
                    + Middle  * 0.2
                    + EraseCount * 6
                    );
        }

        double Attack::eval_map_bad() const
        {
            return -99999999;
        }

        double Attack::eval_map(TetrisMap const &map, EvalParam<double> const *history, size_t history_length)
        {
            const int width_m1 = map.width - 1;
            int ColTrans = 2 * (map.height - map.roof);
            int RowTrans = map.roof == map.height ? 0 : map.width;
            for(int y = 0; y < map.roof; ++y)
            {
                if(!map.full(0, y))
                {
                    ++ColTrans;
                }
                if(!map.full(width_m1, y))
                {
                    ++ColTrans;
                }
                ColTrans += BitCount((map.row[y] ^ (map.row[y] << 1)) & col_mask_);
                if(y != 0)
                {
                    RowTrans += BitCount(map.row[y - 1] ^ map.row[y]);
                }
            }
            RowTrans += BitCount(row_mask_ & ~map.row[0]);
            RowTrans += BitCount(map.roof == map.height ? row_mask_ & ~map.row[map.roof - 1] : map.row[map.roof - 1]);
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

                int AttackDepth;
                int AttackClear;
                int RubbishClear;
                int Danger;

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
                    v.HoleCount += BitCount(LineHole);
                    v.HoleLine++;
                    if(v.HolePosy == 0)
                    {
                        v.HolePosy = y + 1;
                        v.TopHoleBits = LineHole;
                    }
                }
                for(int x = 1; x < width_m1; ++x)
                {
                    if((LineHole >> x) & 1)
                    {
                        v.HoleDepth += ++v.HoleNum[x];
                    }
                    else
                    {
                        v.HoleNum[x] = 0;
                    }
                    if(((v.LineCoverBits >> (x - 1)) & 7) == 5)
                    {
                        v.WellDepth += ++v.WellNum[x];
                    }
                }
                if(LineHole & 1)
                {
                    v.HoleDepth += ++v.HoleNum[0];
                }
                else
                {
                    v.HoleNum[0] = 0;
                }
                if((v.LineCoverBits & 3) == 2)
                {
                    v.WellDepth += ++v.WellNum[0];
                }
                if((LineHole >> width_m1) & 1)
                {
                    v.HoleDepth += ++v.HoleNum[width_m1];
                }
                else
                {
                    v.HoleNum[width_m1] = 0;
                }
                if(((v.LineCoverBits >> (width_m1 - 1)) & 3) == 1)
                {
                    v.WellDepth += ++v.WellNum[width_m1];
                }
            }
            if(v.HolePosy != 0)
            {
                for(int y = v.HolePosy; y < map.roof; ++y)
                {
                    int CheckLine = v.TopHoleBits & map.row[y];
                    if(CheckLine == 0)
                    {
                        break;
                    }
                    v.HolePiece += (y + 1) * BitCount(CheckLine);
                }
            }
            double land_point_value = 0;
            for(size_t i = 0; i < history_length; ++i)
            {
                land_point_value += history[i].eval;
            }
            int low_x = (map.top[map.width - 1] <= map.top[0]) ? map.width - 1 : 0;
            for(int x = map.width - 3; x > 1; --x)
            {
                if(map.top[x] < map.top[low_x])
                {
                    low_x = x;
                }
            }
            if(map.top[1] < map.top[low_x])
            {
                low_x = 1;
            }
            if(map.top[map.width - 2] < map.top[low_x])
            {
                low_x = map.width - 2;
            }
            const int low_y = map.top[low_x];
            for(int y = map.roof - 1; y >= low_y; --y)
            {
                if(check_line_.find(map.row[y]) != check_line_.end())
                {
                    v.AttackDepth += 16;
                    for(--y; y >= low_y; --y)
                    {
                        if(check_line_.find(map.row[y]) != check_line_.end())
                        {
                            v.AttackDepth += 3;
                        }
                        else
                        {
                            v.AttackDepth -= 5;
                        }
                    }
                    break;
                }
                else
                {
                    v.AttackDepth -= 1;
                }
            }
            if((low_x > 1 || map.top[low_x - 2] > map.top[low_x - 1]) || (low_x < width_m1 - 1 || map.top[low_x + 2] > map.top[low_x + 1]))
            {
                v.AttackDepth += 2;
            }
            int BoardDeadZone = map_in_danger_(map);
            if(map.roof == map.height)
            {
                BoardDeadZone += 70;
            }
            for(size_t i = 0; i < history_length; ++i)
            {
                TetrisMap const &history_map = history[i].map;
                if(history_map.roof == history_map.height)
                {
                    BoardDeadZone += 70;
                }
                switch(history[i].clear)
                {
                case 0:
                    break;
                case 1:
                case 2:
                    v.RubbishClear += history[i].clear;
                    break;
                case 3:
                default:
                    v.AttackClear += history[i].clear * (history_length - i);
                }
            }
            return (0.
                    + land_point_value / history_length
                    - ColTrans * 8
                    - RowTrans * 8
                    - v.HoleCount * 400
                    - v.HoleLine * 38
                    - v.WellDepth * 10
                    - v.HoleDepth * 4
                    - v.HolePiece * 2
                    + v.AttackDepth * 100
                    - v.RubbishClear * (v.Danger > 0 ? 0 : 64 * (3 - param_->mode))
                    + v.AttackClear * 1000
                    - BoardDeadZone * 500000
                    );
        }

        size_t Attack::prune_map(m_tetris::PruneParam<double> *prune, size_t prune_length, TetrisNode const **after_pruning, size_t next_length)
        {
            struct
            {
                bool operator()(m_tetris::PruneParam<double> const &left, m_tetris::PruneParam<double> const &right)
                {
                    return left.eval > right.eval;
                }
            } c;
            std::sort(prune, prune + prune_length, c);
            size_t hold_count = prune_length;
            switch(param_->next_length)
            {
            case 0: case 1: case 2:
                break;
            default:
                if(next_length <= param_->next_length)
                {
                    if(next_length + 2 > param_->next_length)
                    {
                        if(hold_count > 4)
                        {
                            hold_count = 4;
                        }
                    }
                    else
                    {
                        hold_count = 1;
                    }
                }
            }
            for(size_t i = 0; i < hold_count; ++i)
            {
                after_pruning[i] = prune[i].land_point;
            }
            return hold_count;
        }

        size_t Attack::map_in_danger_(m_tetris::TetrisMap const &map)
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


    void Dig::init(m_tetris::TetrisContext const *context, size_t const *param)
    {
        next_length_ptr_ = param;
    }
    
    std::string Dig::ai_name() const
    {
        return "ZZZ Dig v0.2";
    }

    double Dig::eval_map_bad() const
    {
        return -99999999;
    }

    double Dig::eval_map(TetrisMap const &map, EvalParam<> const *history, size_t history_length)
    {
        double value = 0;

        const int width = map.width;
        
        for(int x = 0; x < width; ++x)
        {
            for(int y = 0; y < map.roof; ++y)
            {
                if(map.full(x, y))
                {
                    value -= 2 * (y + 1);
                    continue;
                }
                if(x == width - 1 || map.full(x + 1, y))
                {
                    value -= 3 * (y + 1);
                }
                if(x == 0 || map.full(x - 1, y))
                {
                    value -= 3 * (y + 1);
                }
                if(map.full(x, y + 1))
                {
                    value -= 10 * (y + 1);
                    if(map.full(x, y + 2))
                    {
                        value -= 4;
                        if(map.full(x, y + 3))
                        {
                            value -= 3;
                            if(map.full(x, y + 4))
                            {
                                value -= 2;
                            }
                        }
                    }
                }
            }
        }
        double clear = 0;
        for(size_t i = 0; i < history_length; ++i)
        {
            clear += history[i].clear;
        }
        value += clear * history->map.roof * 10 / history_length;
        return value;
    }

    size_t Dig::prune_map(m_tetris::PruneParam<double> *prune, size_t prune_length, TetrisNode const **after_pruning, size_t next_length)
    {
        struct
        {
            bool operator()(m_tetris::PruneParam<double> const &left, m_tetris::PruneParam<double> const &right)
            {
                return left.eval > right.eval;
            }
        } c;
        std::sort(prune, prune + prune_length, c);
        size_t hold_count = std::min<size_t>(prune_length, next_length == *next_length_ptr_ ? prune_length : next_length == *next_length_ptr_ - 1 ? 2 : 1);
        for(size_t i = 0; i < hold_count; ++i)
        {
            after_pruning[i] = prune[i].land_point;
        }
        return hold_count;
    }
}