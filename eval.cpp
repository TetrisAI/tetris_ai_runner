
//Code by ax_pokl
//modify by ZouZhiZhang

#include "tetris_core.h"

std::string ai_name()
{
    return "Tetris_ax_C Mod ZZZ v1";
}

int ai_eval(TetrisMap const &map, EvalParam *history, size_t history_length)
{
    //行列变换
    int ColTrans = 2 * (map.height - map.roof);
    int RowTrans = 0;
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
        for(int x = 1; x < map.width; ++x)
        {
            if(map.full(x - 1, y) ^ map.full(x, y))
            {
                ++ColTrans;
            }
        }
    }
    for(int x = 0; x < map.width; ++x)
    {
        if(!map.full(x, 0))
        {
            ++RowTrans;
        }
        if(!map.full(x, map.height - 1))
        {
            ++RowTrans;
        }
        for(int y = 1; y < map.height; ++y)
        {
            if(map.full(x, y - 1) ^ map.full(x, y))
            {
                ++RowTrans;
            }
        }
    }

    //洞数
    int HoleCount = 0;
    //洞行数
    int HoleLine = 0;
    //最高洞行数
    int HolePosy = 0;
    //最高洞上方块数
    int HolePiece = 0;

    //洞深,井深
    int HoleDepth = 0;
    int WellDepth = 0;

    //洞计数,井计数
    int HoleNum[32] = {};
    int WellNum[32] = {};

    int LineCover = 0;
    int TopHole = 0;

    for(int y = map.roof - 1; y >= 0; --y)
    {
        LineCover |= map.row[y];
        int LineHole = LineCover ^ map.row[y];
        if(LineHole != 0)
        {
            HoleLine++;
            if(HolePosy == 0)
            {
                HolePosy = y + 1;
                TopHole = LineHole;
            }
        }
        for(int x = 0; x < map.width; ++x)
        {
            if((LineHole >> x) & 1)
            {
                ++HoleCount;
                ++HoleNum[x];
            }
            else
            {
                HoleNum[x] = 0;
            }
            HoleDepth += HoleNum[x];
            if(!((LineCover >> x) & 1) && (x == 0 || ((LineCover >> (x - 1)) & 1)) && (x == map.width - 1 || ((LineCover >> (x + 1)) & 1)))
            {
                ++WellNum[x];
                WellDepth += WellNum[x];
            }
        }
    }
    if(HolePosy != 0)
    {
        //从最高有洞行上一行开始往上厉遍
        for(int y = HolePosy; y < map.roof; ++y)
        {
            int CheckLine = TopHole & map.row[y];
            if(CheckLine == 0)
            {
                break;
            }
            for(int x = 0; x < map.width; ++x)
            {
                if((CheckLine >> x) & 1)
                {
                    HolePiece += y + 1;
                }
            }
        }
    }
    //当前块行数
    int LandHeight = 0;
    //设置左中右平衡破缺参数
    int Middle = 0;
    //消行数
    int EraseCount = 0;
    for(size_t i = 0; i < history_length; ++i)
    {
        TetrisNode const *node = history[i].node;
        LandHeight += node->status.y + 1;
        Middle += std::abs((node->status.x + 1) * 2 - map.width);
        EraseCount += history[i].clear;
    }

    //死亡警戒
    int BoardDeadZone = 0;
    for(size_t i = 0; i < 7; ++i)
    {
        if(!generate(i, map)->check(map))
        {
            BoardDeadZone = 1;
        }
    }

    int hl = history_length;
    return int(0
               - LandHeight * 200 / map.height / hl
               + EraseCount * 6 / hl
               - ColTrans * 8
               - RowTrans * 8
               - HoleCount * 6
               - HoleLine * 38
               - WellDepth * 10
               - HoleDepth * 4
               - HolePiece * 0.5
               + Middle * 0.2 / hl
               - BoardDeadZone * 5000
               );
}
