
//by ZouZhiZhang

#include "tetris_core.h"
#include "ai_zzz.h"

using namespace m_tetris;
namespace ai_zzz
{
    namespace qq
    {
        void Attack::init(m_tetris::TetrisContext const *context)
        {
            check_line_.clear();
            const int full = (1 << 10) - 1;
            for(int x = 0; x <= 10; ++x)
            {
                check_line_.insert(full & ~(1 << x));
            }
        }

        std::string Attack::ai_name()
        {
            return "ZZZ Attack v0.1";
        }

        double Attack::eval_map_bad()
        {
            return -99999999;
        }

        double Attack::eval_map(TetrisMap const &map, EvalParam<> const *history, size_t history_length)
        {
            if(history->node->low >= 20)
            {
                return eval_map_bad();
            }
            double value = 0;
            int top = map.roof;

            int new_last_top = history->map.roof;
            for(size_t i = 0; i < history_length; ++i)
            {
                new_last_top -= history[i].clear;
            }
            const int width = 10;
            const int top_m4 = std::max(1, history->map.roof - 4);
            const int danger = std::max(history->map.roof + 4 - 20, 0);
            const int width_mul = 20 * 2560 / (width + 8) / (width + 8);

            int const *tops = map.top;

            for(int x = 0; x < width; ++x)
            {
                for(int y = top - 1; y >= 0; --y)
                {
                    if(map.full(x, y))
                    {
                        for(; y >= 0; --y)
                        {
                            if(!map.full(x, y))
                            {
                                if(x == width - 1 || map.full(x + 1, y))
                                {
                                    value -= (y + 1) * 2;
                                }
                                if(x == 0 || map.full(x - 1, y))
                                {
                                    value -= (y + 1) * 2;
                                }
                                if((y + 1) >= top_m4)
                                {
                                    value -= 512 + (y + 1) * width_mul;
                                }
                                else
                                {
                                    value -= 128 + (y + 1) * width_mul / 4;
                                }
                            }
                        }
                        break;
                    }
                    else
                    {
                        if(x == width - 1 || map.full(x + 1, y))
                        {
                            value -= (y + 1) * 2;
                        }
                        if(x == 0 || map.full(x - 1, y))
                        {
                            value -= (y + 1) * 2;
                        }
                    }
                }
            }
            int low_x = (tops[width - 1] <= tops[0]) ? width - 1 : 0;
            for(int x = width - 3; x > 1; --x)
            {
                if(tops[x] < tops[low_x])
                {
                    low_x = x;
                }
            }
            if(tops[1] < tops[low_x])
            {
                low_x = 1;
            }
            if(tops[width - 2] < tops[low_x])
            {
                low_x = width - 2;
            }
            const int low_y = tops[low_x];
            int deep = 0;
            for(int y = top - 1; y >= low_y; --y)
            {
                if(check_line_.find(map.row[y]) != check_line_.end())
                {
                    deep += 16;
                    for(--y; y >= low_y; --y)
                    {
                        if(check_line_.find(map.row[y]) != check_line_.end())
                        {
                            deep += 3;
                        }
                        else
                        {
                            deep -= 5;
                        }
                    }
                    break;
                }
                else
                {
                    deep -= 1;
                }
            }
            value += deep * width * 16 * history->map.roof / 10 / (danger * danger + 1);
            for(int x = 0; x < width; ++x)
            {
                int dx = std::abs(x - low_x);
                int dy = std::max(0, new_last_top - tops[x] - 4);
                if(dx > 1 && dy > 0)
                {
                    dx += 8;
                    if(danger == 0)
                    {
                        dy += 3;
                        value -= dy * dy * dx * width * 8;
                    }
                    else
                    {
                        value -= dy * dx * width * 16;
                    }
                }
            }
            value -= width_mul * new_last_top / 3;

            for(size_t i = 0; i < history_length; ++i)
            {
                switch(history[i].clear)
                {
                case 0:
                    break;
                case 1:
                case 2:
                    if(danger == 0)
                    {
                        value -= 4 * history->map.roof * width * history[i].clear * 128 / width_mul;
                    }
                    else if(danger >= 2)
                    {
                        value += danger * 16 * history->map.roof * width * history[i].clear * 2;
                    }
                    break;
                default:
                    value += (400 + (history_length - i) * 50) * history->map.roof * width * history[i].clear * 2;
                }
            }

            return value;
        }

        void Attack::prune_map(m_tetris::PruneParam<double> *prune, size_t prune_length, size_t next_length)
        {
            prune_sort_.resize(prune_length);
            auto dst = prune_sort_.data();
            for(auto it = prune, end = prune + prune_length; it != end; ++it, ++dst)
            {
                *dst = it;
            }
            std::sort(prune_sort_.begin(), prune_sort_.end(), [](m_tetris::PruneParam<double> *const &left, m_tetris::PruneParam<double> *const &right)
            {
                return left->eval > right->eval;
            });
            for(auto it = prune_sort_.begin() + std::min<size_t>(prune_length, 4); it != prune_sort_.end(); ++it)
            {
                (*it)->pruned = true;
            }
        }

    }

    
    std::string Dig::ai_name()
    {
        return "ZZZ Dig v0.1";
    }

    double Dig::eval_map_bad()
    {
        return -99999999;
    }

    double Dig::eval_map(TetrisMap const &map, EvalParam<> const *history, size_t history_length)
    {
        if(history->node->low >= 20)
        {
            return eval_map_bad();
        }
        double value = 0;

        const int width = 10;
        
        for(int x = 0; x < width; ++x)
        {
            for(int y = 0; y < map.roof; ++y)
            {
                if(map.full(x, y))
                {
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

        for(size_t i = 0; i < history_length; ++i)
        {
            value += history[i].clear * (history_length - i + 1) * (history[i].clear >= 2 ? map.roof : 2);
        }
        return value;
    }

    void Dig::prune_map(m_tetris::PruneParam<double> *prune, size_t prune_length, size_t next_length)
    {
        prune_sort_.resize(prune_length);
        auto dst = prune_sort_.data();
        for(auto it = prune, end = prune + prune_length; it != end; ++it, ++dst)
        {
            *dst = it;
        }
        std::sort(prune_sort_.begin(), prune_sort_.end(), [](m_tetris::PruneParam<double> *const &left, m_tetris::PruneParam<double> *const &right)
        {
            return left->eval > right->eval;
        });
        for(auto it = prune_sort_.begin() + std::min<size_t>(prune_length, 2); it != prune_sort_.end(); ++it)
        {
            (*it)->pruned = true;
        }
    }
}