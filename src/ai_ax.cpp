﻿
//Code by ax_pokl
//Modify by ZouZhiZhang

#include "tetris_core.h"
#include "ai_ax.h"
#include "integer_utils.h"

using namespace m_tetris;


namespace ai_ax
{
    void AI::init(m_tetris::TetrisContext const *context)
    {
        context_ = context;
        map_danger_data_.resize(context->type_max());
        for(size_t i = 0; i < context->type_max(); ++i)
        {
            TetrisMap map(context->width(), context->height());
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

    std::string AI::ai_name() const
    {
        return "Tetris_ax_C ZZZ Mod v1.2";
    }

    AI::eval_result AI::eval(TetrisNode const *node, TetrisMap const &map, TetrisMap const &src_map, size_t clear) const
    {
        //消行数
        double LandHeight = node->status.y + 1;
        //设置左中右平衡破缺参数
        double Middle = std::abs((node->status.x + 1) * 2 - map.width);
        //当前块行数
        double EraseCount = clear;

        const int width_m1 = map.width - 1;
        //行列变换
        int ColTrans = 2 * (map.height - map.roof);
        int RowTrans = zzz::BitCount(row_mask_ ^ map.row[0]) + zzz::BitCount(map.roof == map.height ? ~row_mask_ & map.row[map.roof - 1] : map.row[map.roof - 1]);
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
            ColTrans += zzz::BitCount((map.row[y] ^ (map.row[y] << 1)) & col_mask_);
            if(y != 0)
            {
                RowTrans += zzz::BitCount(map.row[y - 1] ^ map.row[y]);
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
                v.HoleCount += zzz::BitCount(LineHole);
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
            //从最高有洞行上一行开始往上厉遍
            for(int y = v.HolePosy; y < map.roof; ++y)
            {
                int CheckLine = v.TopHoleBits & map.row[y];
                if(CheckLine == 0)
                {
                    break;
                }
                v.HolePiece += (y + 1) * zzz::BitCount(CheckLine);
            }
        }

        //死亡警戒
        int BoardDeadZone = map_in_danger_(map);

        eval_result result;
        result.land_point = (0
                             - LandHeight * 1750 / map.height
                             + Middle * 2
                             + EraseCount * 60
                             );
        result.map = (0
                      - ColTrans * 80
                      - RowTrans * 80
                      - v.HoleCount * 60
                      - v.HoleLine * 380
                      - v.WellDepth * 100
                      - v.HoleDepth * 40
                      - v.HolePiece * 5
                      - BoardDeadZone * 50000
                      );
        return result;
    }

    double AI::get(eval_result const *history, size_t history_length) const
    {
        double land_point_value = 0;
        for(size_t i = 0; i < history_length; ++i)
        {
            land_point_value += history[i].land_point;
        }
        return land_point_value / history_length + history[history_length - 1].map;
    }

    AI::eval_result AI::iterated(eval_result const *eval, size_t eval_length) const
    {
        eval_result result = *eval;
        for(size_t i = 1; i < eval_length; ++i)
        {
            result.land_point += eval[i].land_point;
            result.map += eval[i].map;
        }
        result.land_point /= eval_length;
        result.map /= eval_length;
        return result;
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
