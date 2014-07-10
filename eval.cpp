

#include "tetris_core.h"

int ai_eval(TetrisMap const &map, EvalParam *history, size_t history_length)
{
    int value = 0;
    int top = map.roof;

    const int width = map.width;

    for(int x = 0; x < width; ++x)
    {
        for(int y = 0; y < top; ++y)
        {
            if(map.full(x, y))
            {
                continue;
            }
            if(x == 0 || map.full(x + 1, y))
            {
                value -= 3 * y + 3;
            }
            if(x == width - 1 || map.full(x - 1, y))
            {
                value -= 3 * y + 3;
            }
            if(map.full(x, y + 1))
            {
                value -= 20 * y + 24;
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
    value -= map.count * (history->map.roof + 8);

    return value;
}
