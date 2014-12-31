
//by ZouZhiZhang

#include "tetris_core.h"
#include "integer_utils.h"
#include "ai_zzz.h"
#include <cstdint>

using namespace m_tetris;
using namespace zzz;

namespace
{
    enum ItemType
    {
        a3, a2, a1, m3, m2, m1, sf, ss
    };
    struct Item
    {
        int16_t col, type;
    };
    const Item ItemTable[20][7] =
    {
#define I(a,b) {0x##a-1,b}
        //LLLLLLL   JJJJJJJ   TTTTTTT   OOOOOOO   IIIIIII   ZZZZZZZ   SSSSSSS//
        {I(A, a2), I(B, a3), I(C, sf), I(1, a1), I(2, a2), I(3, a3), I(4, m1)},
        {I(4, a1), I(3, a2), I(2, a3), I(1, sf), I(C, a1), I(B, a2), I(A, a3)},
        {I(8, ss), I(9, a1), I(A, a2), I(B, a3), I(C, sf), I(1, a1), I(2, a2)},
        {I(6, m3), I(5, m2), I(4, a1), I(3, a2), I(2, a3), I(1, sf), I(C, a1)},
        {I(6, m2), I(7, m3), I(8, ss), I(9, a1), I(A, a2), I(B, a3), I(C, sf)},
        {I(8, a1), I(7, m1), I(6, m1), I(5, m1), I(4, a1), I(3, a2), I(2, a3)},
        {I(4, sf), I(5, a1), I(6, m3), I(7, m2), I(8, m1), I(9, a1), I(A, a2)},
        {I(A, a3), I(9, sf), I(8, a1), I(7, m3), I(6, m1), I(5, ss), I(4, a1)},
        {I(2, a2), I(3, a3), I(4, sf), I(5, a1), I(6, m1), I(7, m3), I(8, m2)},
        {I(C, a1), I(B, a2), I(A, a3), I(9, sf), I(8, a1), I(7, m2), I(6, m3)},
        {I(C, sf), I(1, a1), I(2, a2), I(3, a3), I(4, sf), I(5, a1), I(6, m1)},
        {I(2, a3), I(1, sf), I(C, a1), I(B, a2), I(A, a3), I(9, sf), I(8, a1)},
        {I(A, ss), I(B, a3), I(C, sf), I(1, a1), I(2, a2), I(3, a3), I(4, sf)},
        {I(4, m3), I(3, m2), I(2, a3), I(1, sf), I(C, a1), I(B, a2), I(A, a3)},
        {I(8, m2), I(9, m3), I(A, ss), I(B, a3), I(C, sf), I(1, a1), I(2, a2)},
        {I(6, a3), I(5, m1), I(4, m1), I(3, m1), I(2, a3), I(1, sf), I(C, a1)},
        {I(6, a2), I(7, a3), I(8, m3), I(9, m2), I(A, m1), I(B, a3), I(C, sf)},
        {I(8, a1), I(7, a2), I(6, a3), I(5, m3), I(4, m1), I(3, ss), I(2, a3)},
        {I(4, sf), I(5, a1), I(6, a2), I(7, a3), I(8, m1), I(9, m3), I(A, m2)},
        {I(A, a3), I(9, sf), I(8, a1), I(7, a2), I(6, a3), I(5, m2), I(4, m3)},
        //LLLLLLL   JJJJJJJ   TTTTTTT   OOOOOOO   IIIIIII   ZZZZZZZ   SSSSSSS//
#undef I
    };
    
}

namespace ai_zzz
{
    namespace qq
    {
        void Attack::init(m_tetris::TetrisContext const *context, Param const *param)
        {
            context_ = context;
            param_ = param;
            check_line_1_end_ = check_line_1_;
            check_line_2_end_ = check_line_2_;
            const int full = context->full();
            for(int x = 0; x < context->width(); ++x)
            {
                *check_line_1_end_++ = full & ~(1 << x);
            }
            for(int x = 0; x < context->width() - 1; ++x)
            {
                *check_line_2_end_++ = full & ~(3 << x);
            }
            std::sort(check_line_1_, check_line_1_end_);
            std::sort(check_line_2_, check_line_2_end_);
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

        Attack::eval_result Attack::eval(TetrisNode const *node, TetrisMap const &map, TetrisMap const &src_map, size_t clear) const
        {
            double LandHeight = node->row + node->height;
            double Middle = std::abs((node->status.x + 1) * 2 - map.width);
            double EraseCount = clear;
            double DeadZone = node->row + node->height == map.height ? 500000. : 0;
            double BoardDeadZone = map_in_danger_(map);
            if(map.roof == map.height)
            {
                BoardDeadZone += 70;
            }

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

                double AttackDepth;

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
            int low_x;
            if((param_->mode & 1) == 0)
            {
                low_x = 1;
                for(int x = 2; x < width_m1; ++x)
                {
                    if(map.top[x] < map.top[low_x])
                    {
                        low_x = x;
                    }
                }
                if(map.top[0] < map.top[low_x])
                {
                    low_x = 0;
                }
                if(map.top[width_m1] < map.top[low_x])
                {
                    low_x = width_m1;
                }
            }
            else
            {
                low_x = (map.top[width_m1 - 3] <= map.top[width_m1 - 4]) ? width_m1 - 3 : width_m1 - 4;
                if(map.top[width_m1 - 2] <= map.top[low_x])
                {
                    low_x = width_m1 - 2;
                }
            }
            int low_y = map.top[low_x];
            for(int y = map.roof - 1; y >= low_y; --y)
            {
                if(std::binary_search<int const *>(check_line_1_, check_line_1_end_, map.row[y]))
                {
                    if(y + 1 < map.height && std::binary_search<int const *>(check_line_2_, check_line_2_end_, map.row[y + 1]))
                    {
                        v.AttackDepth += 20;
                    }
                    else
                    {
                        v.AttackDepth += 16;
                    }
                    for(--y; y >= low_y; --y)
                    {
                        if(std::binary_search<int const *>(check_line_1_, check_line_1_end_, map.row[y]))
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
                    v.AttackDepth -= 2;
                }
            }
            v.Danger = 5 - std::min(map.height - low_y, 5);
            if(v.Danger > 0 && v.AttackDepth < 20)
            {
                v.AttackDepth = 0;
            }

            eval_result result;
            result.land_point = (0.
                                 - LandHeight * 16
                                 + Middle  * 0.2
                                 + EraseCount * 6
                                 - DeadZone
                                 - BoardDeadZone * 500000
                                 );
            result.map = (0.
                          - ColTrans * 32
                          - RowTrans * 32
                          - v.HoleCount * 400
                          - v.HoleLine * 38
                          - v.WellDepth * 16
                          - v.HoleDepth * 4
                          - v.HolePiece * 2
                          + v.AttackDepth * 100
                          );
            result.clear = clear;
            result.danger = v.Danger;
            return result;
        }

        double Attack::bad() const
        {
            return -999999999999;
        }

        double Attack::get(eval_result const *history, size_t history_length) const
        {
            double AttackClear = 0;
            double RubbishClear = 0;

            double length_rate = 10. / history_length;

            double land_point_value = 0;
            for(size_t i = 0; i < history_length; ++i)
            {
                land_point_value += history[i].land_point;
                size_t clear = history[i].clear;
                switch(clear)
                {
                case 0:
                    break;
                case 1:
                case 2:
                    RubbishClear += clear;
                    break;
                case 3:
                    if(param_->mode != 0)
                    {
                        AttackClear += 12;
                        break;
                    }
                default:
                    AttackClear += (clear * 10 + (history_length - i)) * (1 + (history_length - i) * length_rate);
                    break;
                }
            }
            return (0.
                    + land_point_value / history_length
                    + history[history_length - 1].map
                    - RubbishClear * (history[history_length - 1].danger > 0 ? -100 : 640)
                    + AttackClear * 100
                    );
        }


        size_t Attack::map_in_danger_(m_tetris::TetrisMap const &map) const
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

        
    std::string Dig::ai_name() const
    {
        return "ZZZ Dig v0.2";
    }

    double Dig::bad() const
    {
        return -99999999;
    }

    double Dig::eval(m_tetris::TetrisNode const *node, m_tetris::TetrisMap const &map, m_tetris::TetrisMap const &src_map, size_t clear) const
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
        if(clear > 0)
        {
            value += map.height * 1000;
        }
        if(map.count == 0)
        {
            value += 99999999;
        }
        return value;
    }

    double Dig::get(double const *history, size_t history_length) const
    {
        double result = 0;
        for(size_t i = 0; i < history_length; ++i)
        {
            result += history[i];
        }
        return result / history_length;
    }

    void TOJ::init(m_tetris::TetrisContext const *context, Param const *param)
    {
        param_ = param;
        col_mask_ = context->full() & ~1;
        row_mask_ = context->full();
        danger_line_ = context->height();
        danger_data_ = 0;
        for(size_t i = 0; i < context->type_max(); ++i)
        {
            TetrisNode const *node = context->generate(i);
            danger_line_ = std::min<int>(danger_line_, node->row);
            for(int x = node->col; x < node->col + node->width; ++x)
            {
                danger_data_ |= 1 << x;
            }
        }
    }

    std::string TOJ::ai_name() const
    {
        return "ZZZ TOJ v0.2";
    }

    double TOJ::bad() const
    {
        return -99999999;
    }

    TOJ::eval_result TOJ::eval(TetrisNodeEx &node, m_tetris::TetrisMap const &map, m_tetris::TetrisMap const &src_map, size_t clear) const
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
        eval_result result;
        result.eval = value;
        result.clear = clear;
        result.count = map.count;
        int line;
        for(line = std::min<int>(danger_line_, map.roof); line > 0; --line)
        {
            if(map.row[line] && danger_data_)
            {
                break;
            }
        }
        result.safe = danger_line_ - line;
        result.t_spin = node.type;
        if(clear > 0 && node.is_check && node.is_last_rotate)
        {
            if(clear == 1 && node.is_mini_ready)
            {
                result.t_spin = TSpinType::TSpinMini;
            }
            else if(node.is_ready)
            {
                result.t_spin = TSpinType::TSpin;
            }
            node.type = result.t_spin;
        }
        result.expect = 0;
        if(true) //T-Spin
        {
            bool finding = true;
            for(int y = 0; finding && y < map.roof - 1; ++y)
            {
                int row0 = map.row[y];
                int row1 = map.row[y + 1];
                for(int x = 0; finding && x < map.width - 3; ++x)
                {
                    if((((row0 >> x) & 7) == 5) && (((row1 >> x) & 7) == 0))
                    {
                        int row2_check = ((y + 2 < map.height ? map.row[y + 2] : 0) >> x) & 7;
                        if(row2_check == 1 || row2_check == 4)
                        {
                            result.expect = 4;
                        }
                        else
                        {
                            result.expect = 3;
                        }
                        finding = false;
                    }
                }
            }
        }
        return result;
    }

    double TOJ::get(eval_result const *history, size_t history_length) const
    {
        int under_attack = param_->under_attack;
        int up = 0;
        bool b2b = param_->b2b;
        size_t combo = param_->combo;
        int attack = 0;
        size_t rubbish = 0;
        for(size_t i = 0; i < history_length; ++i)
        {
            TSpinType t_spin = history[i].t_spin;
            switch(history[i].clear)
            {
            case 0:
                combo = 0;
                if(under_attack > 0)
                {
                    up = std::max<int>(0, under_attack - attack);
                    if(up >= history[i].safe)
                    {
                        return bad();
                    }
                    under_attack = 0;
                }
                break;
            case 1:
                if(t_spin == TSpinType::TSpinMini)
                {
                    attack += b2b ? 2 : 1;
                }
                else if(t_spin == TSpinType::TSpin)
                {
                    attack += b2b ? 3 : 2;
                }
                attack += param_->table[std::min(param_->table_max - 1, ++combo)];
                b2b = t_spin != TSpinType::None;
                break;
            case 2:
                if(t_spin != TSpinType::None)
                {
                    attack += b2b ? 5 : 4;
                }
                attack += param_->table[std::min(param_->table_max - 1, ++combo)];
                b2b = t_spin != TSpinType::None;
                break;
            case 3:
                if(t_spin != TSpinType::None)
                {
                    attack += b2b ? 8 : 6;
                }
                attack += param_->table[std::min(param_->table_max - 1, ++combo)] + 2;
                b2b = t_spin != TSpinType::None;
                break;
            case 4:
                attack += param_->table[std::min(param_->table_max - 1, ++combo)] + (b2b ? 5 : 4);
                b2b = true;
                break;
            }
            if(history[i].count == 0 && up == 0)
            {
                attack += 6;
            }
        }
        eval_result const &last = history[history_length - 1];
        return last.eval + attack * 400 - up * 40 + last.expect * 50 + (b2b ? 200 : 0);
    }

}