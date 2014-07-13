
//Code by ax_pokl
//Modify by ZouZhiZhang

#include "tetris_core.h"

std::string ai_name()
{
    return "Tetris_ax_C Mod ZZZ v1";
}

int ai_eval(TetrisMap const &map, EvalParam *history, size_t history_length)
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
    int BoardDeadZone = map_in_danger(map);

    int hl = history_length;
    return int(0
               - v.LandHeight * 200 / map.height / hl
               + v.EraseCount * 6 / hl
               - ColTrans * 8
               - RowTrans * 8
               - v.HoleCount * 6
               - v.HoleLine * 38
               - v.WellDepth * 10
               - v.HoleDepth * 4
               - v.HolePiece * 0.5
               + v.Middle * 0.2 / hl
               - BoardDeadZone * 5000
               );
}
