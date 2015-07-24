
//Code by ax_pokl
//Modify by ZouZhiZhang

#include "tetris_core.h"
#include "ai_farteryhr.h"
#include "integer_utils.h"

using namespace m_tetris;

namespace
{
    enum BlockStatus
    {
        /** Empty block, except which are all solid */
        BLK_EMPTY = 0,
        /** Exists before */
        BLK_FORMER = 1,
        /** Newly put */
        BLK_NEW = 2,
        /** Out of bound */
        BLK_OOB = -1,
    };

    struct FarteryhrMap
    {
        FarteryhrMap(TetrisNode const *_node, TetrisMap const *_map, int _fhh) : node(_node), map(_map), fhh(_fhh), fh(_map->height - fhh)
        {
            memset(node_data, 0, sizeof(node_data));
            for(int y = 0; y < node->height; ++y)
            {
                node_data[y + node->row] = node->data[y];
            }
            for(int i = 0; i < max_height; ++i)
            {
                row_count[i] = i < map->height ? zzz::BitCount(map->row[i]) : 0;
            }
            for(int y = 0; y < node->height; ++y)
            {
                row_count[node->row + y] += zzz::BitCount(node->data[y]);
            }
            block_count = 0;
            for(int x = node->col; x < node->col + node->width; ++x)
            {
                for(int y = 0; y < node->height; ++y)
                {
                    if((node->data[y] >> x) & 1)
                    {
                        auto &b = block[block_count++];
                        b.x = x;
                        b.y = fh - 1 - (y + node->row);
                    }
                }
            }
        }

        TetrisNode const *node;
        TetrisMap const *map;
        int node_data[max_height];

        int fhh;
        int fh;
        size_t row_count[max_height];
        struct Point
        {
            int x, y;
        } block[16];
        size_t block_count;

        BlockStatus get(int x, int y)
        {
            y = fh - y - 1;
            if(x < 0 || x >= map->width || y < 0 || y >= map->height)
            {
                return BLK_OOB;
            }
            if((node_data[y] >> x) & 1)
            {
                return BLK_NEW;
            }
            if(map->full(x, y))
            {
                return BLK_FORMER;
            }
            return BLK_EMPTY;
        }

        int count(int y)
        {
            y = fh - y - 1;
            if(y < 0 || y >= map->height)
            {
                return 0;
            }
            return row_count[y];
        }

        int getgapdep(int x, int y)
        {
            int fw = map->width;
            int gapdep = 0;
            bool ingap = false;
            for(int j = 0; j < 6; ++j)
            {
                if(count(y + j) == fw)
                {
                    continue;
                }
                if(get(x, y + j) != BLK_EMPTY)
                {
                    break;
                }
                else if(ingap || (get(x + 1, y + j) != BLK_EMPTY && get(x - 1, y + j) != BLK_EMPTY))
                {
                    gapdep++;
                    ingap = true;
                }
            }
            return gapdep;
        }
    };
}

namespace ai_farteryhr
{
    void AI::init(m_tetris::TetrisContext const *context)
    {
        fhh = 0;
        for(size_t i = 0; i < context->type_max(); ++i)
        {
            TetrisNode const *node = context->generate(i);
            fhh = std::max<int>(fhh, context->height() - node->row);
        }

    }


    std::string AI::ai_name() const
    {
        return "farteryhr v1";
    }

    int AI::eval(TetrisNode const *node, TetrisMap const &map, TetrisMap const &src_map, size_t clear) const
    {
        FarteryhrMap fmap =
        {
            node, &src_map, fhh
        };
        int pts = 0;

        int fw = map.width;
        int fh = map.height - fhh;

        for(size_t i = 0; i < fmap.block_count; ++i)
        {
            int cx = fmap.block[i].x;
            int cy = fmap.block[i].y;
            if(fmap.get(cx, cy - 1) == BLK_FORMER)
            {
                pts += 100;
            }
            if(fmap.get(cx - 1, cy) == BLK_FORMER)
            {
                pts += 50;
            }
            if(fmap.get(cx - 1, cy) == BLK_OOB)
            {
                pts += 60;
            }
            if(fmap.get(cx + 1, cy) == BLK_FORMER)
            {
                pts += 50;
            }
            if(fmap.get(cx + 1, cy) == BLK_OOB)
            {
                pts += 60;
            }

            if(fmap.count(cy) == fw)
            {
                continue;
            }

            pts -= (fh - cy - 1) * 50;

            if(fmap.get(cx, cy + 1) == BLK_EMPTY)
                pts -= 20;

            {
                int maxgapside = 0;
                for(int dx = -1; dx <= 1; dx += 2)
                {
                    if(fmap.get(cx + dx, cy) != BLK_EMPTY)
                        continue;
                    int getgaptmp = fmap.getgapdep(cx + dx, cy);
                    if(maxgapside<getgaptmp)
                        maxgapside = getgaptmp;
                }
                if(maxgapside >= 6)
                {
                    pts -= 600;
                }
                else if(maxgapside > 3)
                {
                    pts -= 300;
                }
                else if(maxgapside == 3)
                {
                    pts -= 250;
                }
                else if(maxgapside == 2)
                {
                    pts -= 50;
                }

                if(fmap.get(cx, cy + 1) != BLK_EMPTY)
                {
                    int gapcvr = 0;
                    gapcvr = fmap.getgapdep(cx, cy + 1);
                    if(gapcvr == 0)
                    {
                        pts -= 50;
                    }
                    else
                    {
                        pts -= 200;
                    }
                }
            }
            int sumempty = 0;
            int scrcover = 0;
            int dist;
            for(int j = 0; j < 4 && j + cy < fh; j++)
            { // look downward
                if(fmap.get(cx, cy + j) != BLK_EMPTY)
                {
                    // solid, see how many in current line are empty
                    int emptyC = fw - fmap.count(cy + j);
                    sumempty += emptyC;
                    for(int dx = -1; dx <= 1; dx += 2)
                    {
                        if(fmap.get(cx + dx, cy + j) == BLK_EMPTY)
                        {
                            if(fmap.get(cx + dx + dx, cy + j) != BLK_EMPTY)
                            {
                                sumempty += 3;
                            }
                            sumempty += 1;
                        }
                    }
                }
                else //empty, should be punished
                {
                    dist = 0; // how many unfilled lines
                    for(int k = 0; k <= j; k++)
                    {
                        if(fmap.count(cy + k) < fw)
                            dist++;
                    }
                    if(dist == 0)
                        continue; //all clear above line j
                    if(fmap.get(cx - 1, cy + j) != BLK_EMPTY || fmap.get(cx + 1, cy + j) != BLK_EMPTY)
                    {
                        for(int dx = -1; dx <= 1; dx += 2)
                        {
                            if(fmap.get(cx + dx, cy) == BLK_EMPTY)
                            {
                                scrcover += 100 / dist;
                            }
                            else
                            {
                                scrcover += 50 / dist;
                            }
                        }
                    }
                    else
                    {
                        scrcover += 50 / dist;
                    }
                }
            }
            pts += scrcover*sumempty*(-1);
        }
        int lcC = 0, lcW = 1;
        for(int i = fhh * (-1); i < fh; i++)
        {
            if(fmap.count(i) == fw)
            {
                lcC++;
                if(i<6)
                    lcW = 5;
            }
        }
        switch(lcC)
        {
        case 0:
            break;
        case 1:
            pts += 20 * lcW;
            break;
        case 2:
            pts += 50 * lcW;
            break;
        case 3:
            pts += 200 * lcW;
            break;
        case 4:
            pts += 1000 * lcW;
            break;
        }


        return pts;
    }

    int AI::get(int const *history, size_t history_length) const
    {
        int result = 0;
        for(size_t i = 0; i < history_length; ++i)
        {
            result += history[i];
        }
        return result / history_length;
        //return history[history_length - 1];
    }

}
