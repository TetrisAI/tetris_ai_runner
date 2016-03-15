
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

        bool Attack::Status::operator < (Status const &other) const
        {
            return value < other.value;
        }


        void Attack::init(m_tetris::TetrisContext const *context, Config const *config)
        {
            context_ = context;
            config_ = config;
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
                TetrisMap map(context->width(), context->height());
                TetrisNode const *node = context->generate(i);
                node->attach(map);
                std::memcpy(map_danger_data_[i].data, &map.row[map.height - 4], sizeof map_danger_data_[i].data);
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

        Attack::Result Attack::eval(TetrisNode const *node, TetrisMap const &map, TetrisMap const &src_map, size_t clear) const
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
            std::memset(&v, 0, sizeof v);

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
            if((config_->mode & 1) == 0)
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
                if(std::binary_search<uint32_t const *>(check_line_1_, check_line_1_end_, map.row[y]))
                {
                    if(y + 1 < map.height && std::binary_search<uint32_t const *>(check_line_2_, check_line_2_end_, map.row[y + 1]))
                    {
                        v.AttackDepth += 20;
                    }
                    else
                    {
                        v.AttackDepth += 16;
                    }
                    for(--y; y >= low_y; --y)
                    {
                        if(std::binary_search<uint32_t const *>(check_line_1_, check_line_1_end_, map.row[y]))
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

            Result result;
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

        Attack::Status Attack::get(Result const &eval_result, size_t depth, Status const &status) const
        {

            Status result = status;
            result.land_point += eval_result.land_point;
            double length_rate = 10. / depth;
            switch(eval_result.clear)
            {
            case 0:
                break;
            case 1:
            case 2:
                result.rubbish += eval_result.clear;
                break;
            case 3:
                if(config_->mode != 0)
                {
                    result.attack += 12;
                    break;
                }
            default:
                result.attack += (eval_result.clear * 10 * length_rate);
                break;
            }
            result.value = (0.
                            + result.land_point / depth
                            + eval_result.map
                            - result.rubbish * (eval_result.danger > 0 ? -100 : 640)
                            + result.attack * 100
                            );
            return result;
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
    
    void Dig::init(m_tetris::TetrisContext const *context)
    {
        context_ = context;
        map_danger_data_.resize(context->type_max());
        for(size_t i = 0; i < context->type_max(); ++i)
        {
            TetrisMap map(context->width(), context->height());
            TetrisNode const *node = context->generate(i);
            node->attach(map);
            std::memcpy(map_danger_data_[i].data, &map.row[map.height - 4], sizeof map_danger_data_[i].data);
            for(int y = 0; y < 3; ++y)
            {
                map_danger_data_[i].data[y + 1] |= map_danger_data_[i].data[y];
            }
        }
        col_mask_ = context->full() & ~1;
        row_mask_ = context->full();
    }

    std::string Dig::ai_name() const
    {
        return "ZZZ Dig v0.2";
    }

    double Dig::eval(m_tetris::TetrisNode const *node, m_tetris::TetrisMap const &map, m_tetris::TetrisMap const &src_map, size_t clear) const
    {
        const int width_m1 = map.width - 1;
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
            int HoleCount;
            int HoleLine;

            int HoleDepth;
            int WellDepth;

            int HoleNum[32];
            int WellNum[32];

            int LineCoverBits;
            int ClearWidth;
        } v;
        std::memset(&v, 0, sizeof v);

        for(int y = map.roof - 1; y >= 0; --y)
        {
            v.LineCoverBits |= map.row[y];
            int LineHole = v.LineCoverBits ^ map.row[y];
            if(LineHole != 0)
            {
                v.HoleCount += zzz::BitCount(LineHole);
                v.HoleLine++;
                for(int hy = y + 1; hy < map.roof; ++hy)
                {
                    uint32_t CheckLine = LineHole & map.row[hy];
                    if(CheckLine == 0)
                    {
                        break;
                    }
                    v.ClearWidth += zzz::BitCount(CheckLine);
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

        //��������
        int BoardDeadZone = map_in_danger_(map);

        return (0.
                - map.roof * 96
                - ColTrans * 160
                - RowTrans * 128
                - v.HoleCount * 60
                - v.HoleLine * 380
                - v.WellDepth * 100
                - v.HoleDepth * 40
                - v.ClearWidth * 16
                - BoardDeadZone * 50000
                );
    }

    double Dig::get(double const &eval_result) const
    {
        return eval_result;
    }

    size_t Dig::map_in_danger_(m_tetris::TetrisMap const &map) const
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

    bool TOJ::Status::operator < (Status const &other) const
    {
        return value < other.value;
    }

    void TOJ::init(m_tetris::TetrisContext const *context, Config const *config)
    {
        context_ = context;
        config_ = config;
        col_mask_ = context->full() & ~1;
        row_mask_ = context->full();
        full_count_ = context->width() * 24;
        map_danger_data_.resize(context->type_max());
        for(size_t i = 0; i < context->type_max(); ++i)
        {
            TetrisMap map(context->width(), context->height());
            TetrisNode const *node = context->generate(i);
            node->attach(map);
            std::memcpy(map_danger_data_[i].data, &map.row[18], sizeof map_danger_data_[i].data);
            for(int y = 0; y < 3; ++y)
            {
                map_danger_data_[i].data[y + 1] |= map_danger_data_[i].data[y];
            }
        }
    }

    std::string TOJ::ai_name() const
    {
        return "ZZZ TOJ v0.7";
    }

    TOJ::Result TOJ::eval(TetrisNodeEx &node, m_tetris::TetrisMap const &map, m_tetris::TetrisMap const &src_map, size_t clear) const
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

            int HoleDepth;
            int WellDepth;

            int HoleNum[32];
            int WellNum[32];

            int LineCoverBits;
            int ClearWidth;
        } v;
        std::memset(&v, 0, sizeof v);

        for(int y = map.roof - 1; y >= 0; --y)
        {
            v.LineCoverBits |= map.row[y];
            int LineHole = v.LineCoverBits ^ map.row[y];
            if(LineHole != 0)
            {
                v.HoleCount += zzz::BitCount(LineHole);
                v.HoleLine++;
                for(int hy = y + 1; hy < map.roof; ++hy)
                {
                    uint32_t CheckLine = LineHole & map.row[hy];
                    if(CheckLine == 0)
                    {
                        break;
                    }
                    v.ClearWidth += zzz::BitCount(CheckLine);
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

        Result result;
        result.value = (0.
                        - map.roof * 96
                        - ColTrans * 160
                        - RowTrans * 128
                        - v.HoleCount * 60
                        - v.HoleLine * 380
                        - v.WellDepth * 100
                        - v.HoleDepth * 40
                        - v.ClearWidth * 32
                      );
        result.count = map.count + v.HoleCount;
        result.clear = clear;
        result.safe = 0;
        while(map_in_danger_(map, result.safe + 1) == 0)
        {
            ++result.safe;
        }
        if(clear > 0 && node.is_check && node.is_last_rotate)
        {
            if(clear == 1 && node.is_mini_ready)
            {
                node.type = TSpinType::TSpinMini;
            }
            else if(node.is_ready)
            {
                node.type = TSpinType::TSpin;
            }
            else
            {
                node.type = TSpinType::None;
            }
        }
        result.t_spin = node.type;
        result.t2_value = 0;
        result.t3_value = 0;
        bool finding2 = true;
        bool finding3 = true;
        for(int y = 0; (finding2 || finding3) && y < map.roof - 2; ++y)
        {
            int row0 = map.row[y];
            int row1 = map.row[y + 1];
            int row2 = y + 2 < map.height ? map.row[y + 2] : 0;
            int row3 = y + 3 < map.height ? map.row[y + 3] : 0;
            int row4 = y + 4 < map.height ? map.row[y + 4] : 0;
            for(int x = 0; finding2 && x < map.width - 2; ++x)
            {
                if(((row0 >> x) & 7) == 5 && ((row1 >> x) & 7) == 0)
                {
                    if(BitCount(row0) == map.width - 1)
                    {
                        result.t2_value += 1;
                        if(BitCount(row1) == map.width - 3)
                        {
                            result.t2_value += 2;
                            int row2_check = (row2 >> x) & 7;
                            if(row2_check == 1 || row2_check == 4)
                            {
                                result.t2_value += 2;
                            }
                            finding2 = false;
                        }
                    }
                }
            }
            for(int x = 0; finding3 && x < map.width - 3; ++x)
            {
                if(((row0 >> x) & 15) == 11 && ((row1 >> x) & 15) == 9)
                {
                    int t3_value = 0;
                    if(BitCount(row0) == map.width - 1)
                    {
                        t3_value += 1;
                        if(BitCount(row1) == map.width - 2)
                        {
                            t3_value += 1;
                            if(((row2 >> x) & 15) == 11)
                            {
                                t3_value += 2;
                                if(BitCount(row2) == map.width - 1)
                                {
                                    t3_value += 2;
                                }
                            }
                            int row3_check = ((row3 >> x) & 15);
                            if(row3_check == 8 || row3_check == 0)
                            {
                                t3_value += !!row3_check;
                                int row4_check = ((row4 >> x) & 15);
                                if(row4_check == 4 || row4_check == 12)
                                {
                                    t3_value += 1;
                                }
                            }
                            else
                            {
                                t3_value = 0;
                            }
                        }
                    }
                    result.t3_value += t3_value;
                    if(t3_value > 3)
                    {
                        finding3 = false;
                    }
                }
                if(((row0 >> x) & 15) == 13 && ((row1 >> x) & 15) == 9)
                {
                    int t3_value = 0;
                    if(BitCount(row0) == map.width - 1)
                    {
                        t3_value += 1;
                        if(BitCount(row1) == map.width - 2)
                        {
                            t3_value += 1;
                            if(((row2 >> x) & 15) == 13)
                            {
                                t3_value += 2;
                                if(BitCount(row2) == map.width - 1)
                                {
                                    t3_value += 2;
                                }
                            }
                            int row3_check = ((row3 >> x) & 15);
                            if(row3_check == 1 || row3_check == 0)
                            {
                                t3_value += !!row3_check;
                                int row4_check = ((row4 >> x) & 15);
                                if(row4_check == 3 || row4_check == 1)
                                {
                                    t3_value += 1;
                                }
                            }
                            else
                            {
                                t3_value = 0;
                            }
                        }
                    }
                    result.t3_value += t3_value;
                    if(t3_value > 3)
                    {
                        finding3 = false;
                    }
                }
            }
        }
        return result;
    }

    TOJ::Status TOJ::get(Result const &eval_result, size_t depth, Status const &status, TetrisContext::Env const &env) const
    {
        Status result = status;
        result.value = eval_result.value;
        if(eval_result.safe <= 0)
        {
            result.value -= 99999;
        }
        switch(eval_result.clear)
        {
        case 0:
            if(status.combo > 0 && status.combo < 3)
            {
                result.like -= 2;
            }
            result.combo = 0;
            if(status.under_attack > 0)
            {
                result.map_rise += std::max(0, int(status.under_attack) - status.attack);
                if(result.map_rise >= eval_result.safe)
                {
                    result.death += result.map_rise - eval_result.safe;
                }
                result.under_attack = 0;
            }
            break;
        case 1:
            if(eval_result.t_spin == TSpinType::TSpinMini)
            {
                result.attack += status.b2b ? 2 : 1;
            }
            else if(eval_result.t_spin == TSpinType::TSpin)
            {
                result.attack += status.b2b ? 3 : 2;
            }
            result.attack += config_->table[std::min(config_->table_max - 1, ++result.combo)];
            result.b2b = eval_result.t_spin != TSpinType::None;
            break;
        case 2:
            if(eval_result.t_spin != TSpinType::None)
            {
                result.like += 8;
                result.attack += status.b2b ? 5 : 4;
            }
            result.attack += config_->table[std::min(config_->table_max - 1, ++result.combo)];
            result.b2b = eval_result.t_spin != TSpinType::None;
            break;
        case 3:
            if(eval_result.t_spin != TSpinType::None)
            {
                result.like += 12;
                result.attack += status.b2b ? 8 : 6;
            }
            result.attack += config_->table[std::min(config_->table_max - 1, ++result.combo)] + 2;
            result.b2b = eval_result.t_spin != TSpinType::None;
            break;
        case 4:
            result.like += 8;
            result.attack += config_->table[std::min(config_->table_max - 1, ++result.combo)] + (status.b2b ? 5 : 4);
            result.b2b = true;
            break;
        }
        if(result.combo < 5)
        {
            result.like -= 1.5 * result.combo;
        }
        if(eval_result.count == 0 && result.map_rise == 0)
        {
            result.like += 20;
            result.attack += 6;
        }
        if(status.b2b && !result.b2b)
        {
            result.like -= 2;
        }
        size_t t_expect = [=]()->int
        {
            if(env.hold == 'T')
            {
                return 0;
            }
            for(size_t i = 0; i < env.length; ++i)
            {
                if(env.next[i] == 'T')
                {
                    return i;
                }
            }
            return 14;
        }();
        switch(env.hold)
        {
        case 'T':
            if(eval_result.t_spin == TSpinType::None)
            {
                result.like += 4;
            }
            break;
        case 'I':
            if(eval_result.clear != 4)
            {
                result.like += 2;
            }
            break;
        }
        double rate = (1. / depth) + 3;
        result.max_combo = std::max(result.combo, result.max_combo);
        result.max_attack = std::max(result.attack, result.max_attack);
        result.value += ((0.
                          + result.max_attack * 40
                          + result.attack * 256 * rate
                          + eval_result.t2_value * (t_expect < 8 ? 512 : 320) * 1.5
                          + (eval_result.safe >= 12 ? eval_result.t3_value * (t_expect < 4 ? 10 : 8) * (result.b2b ? 512 : 256) / (6 + result.under_attack) : 0)
                          + (result.b2b ? 512 : 0)
                          + result.like * 64
                          ) * std::max<double>(0.05, (full_count_ - eval_result.count - result.map_rise * (context_->width() - 1)) / double(full_count_))
                         + result.max_combo * (result.max_combo - 1) * 40
                         - result.death * 999999999.0
                         );
        return result;
    }

    size_t TOJ::map_in_danger_(m_tetris::TetrisMap const &map, size_t up) const
    {
        size_t danger = 0;
        for(size_t i = 0; i < context_->type_max(); ++i)
        {
            if(up >= 20)
            {
                return context_->type_max();
            }
            size_t height = 22 - up;
            if(map_danger_data_[i].data[0] & map.row[height - 4] || map_danger_data_[i].data[1] & map.row[height - 3] || map_danger_data_[i].data[2] & map.row[height - 2] || map_danger_data_[i].data[3] & map.row[height - 1])
            {
                ++danger;
            }
        }
        return danger;
    }

    bool C2::Status::operator < (Status const &other) const
    {
        return value < other.value;
    }


    void C2::init(m_tetris::TetrisContext const *context, Config const *config)
    {
        context_ = context;
        config_ = config;
        map_danger_data_.resize(context->type_max());
        for(size_t i = 0; i < context->type_max(); ++i)
        {
            TetrisMap map(context->width(), context->height());
            TetrisNode const *node = context->generate(i);
            node->move_down->attach(map);
            std::memcpy(map_danger_data_[i].data, &map.row[map.height - 4], sizeof map_danger_data_[i].data);
            for(int y = 0; y < 3; ++y)
            {
                map_danger_data_[i].data[y + 1] |= map_danger_data_[i].data[y];
            }
        }
        col_mask_ = context->full() & ~1;
        row_mask_ = context->full();
    }

    std::string C2::ai_name() const
    {
        return "C2 v0.1";
    }

    C2::Result C2::eval(TetrisNode const *node, TetrisMap const &map, TetrisMap const &src_map, size_t clear) const
    {
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
            int HoleCountSrc;
            int HoleCount;
            int HoleLine;

            int WideWellDepth[6];
            int WellDepth[32];
            int WellDepthTotle;

            int LineCoverBits;
            int HolePosyIndex;
        } v;
        std::memset(&v, 0, sizeof v);
        struct
        {
            int ClearWidth;
        } a[40];

        for(int y = map.roof - 1; y >= 0; --y)
        {
            v.LineCoverBits |= map.row[y];
            int LineHole = v.LineCoverBits ^ map.row[y];
            if(LineHole != 0)
            {
                ++v.HoleLine;
                a[v.HolePosyIndex].ClearWidth = 0;
                for(int hy = y + 1; hy < map.roof; ++hy)
                {
                    uint32_t CheckLine = LineHole & map.row[hy];
                    if(CheckLine == 0)
                    {
                        break;
                    }
                    a[v.HolePosyIndex].ClearWidth += (hy + 1) * zzz::BitCount(CheckLine);
                }
                ++v.HolePosyIndex;
            }
            int WellWidth = 0;
            int MaxWellWidth = 0;
            for(int x = 0; x < map.width; ++x)
            {
                if((v.LineCoverBits >> x) & 1)
                {
                    if(WellWidth > MaxWellWidth)
                    {
                        MaxWellWidth = WellWidth;
                    }
                    WellWidth = 0;
                }
                else
                {
                    ++WellWidth;
                    if(x > 0 && x < width_m1)
                    {
                        if(((v.LineCoverBits >> (x - 1)) & 7) == 5)
                        {
                            v.WellDepthTotle += ++v.WellDepth[x];
                        }
                    }
                    else if(x == 0)
                    {
                        if((v.LineCoverBits & 3) == 2)
                        {
                            v.WellDepthTotle += ++v.WellDepth[0];
                        }
                    }
                    else
                    {
                        if(((v.LineCoverBits >> (width_m1 - 1)) & 3) == 1)
                        {
                            v.WellDepthTotle += ++v.WellDepth[width_m1];
                        }
                    }
                }
            }
            if(WellWidth > MaxWellWidth)
            {
                MaxWellWidth = WellWidth;
            }
            if(MaxWellWidth >= 1 && MaxWellWidth <= 6)
            {
                if(zzz::BitCount(map.row[y]) + MaxWellWidth == map.width)
                {
                    v.WideWellDepth[MaxWellWidth - 1] += 2;
                }
                else
                {
                    v.WideWellDepth[MaxWellWidth - 1] -= 1;
                }
            }
        }
        for(int x = 0; x < map.width; ++x)
        {
            v.HoleCountSrc += src_map.top[x];
            v.HoleCount += map.top[x];
        }
        v.HoleCountSrc -= src_map.count;
        v.HoleCount -= map.count;

        Result result;
        result.map = (0.
                      - map.roof * 128
                      - ColTrans * 80
                      - RowTrans * 80
                      - v.HoleCount * 80
                      - v.HoleLine * 380
                      - v.WellDepthTotle * 100
                      + (map.width - (node->col + node->col + node->width)) / 20.0
                      - node->low / 10.0
                      - node->status.r / 4.0
                      );
        double rate = 16, mul = 1.0 / 3;
        for(int i = 0; i < v.HolePosyIndex; ++i, rate *= mul)
        {
            result.map -= a[i].ClearWidth * rate;
        }
        if(config_->mode == 0)
        {
            int attack_well = std::min(4, v.WideWellDepth[0]);
            result.attack = (0.
                             + v.WideWellDepth[5] * 2.8
                             + v.WideWellDepth[4] * 3.0
                             + v.WideWellDepth[3] * 3.2
                             + v.WideWellDepth[2] * 4.8
                             + v.WideWellDepth[1] * -8
                             + attack_well * attack_well * 128
                             );
        }
        result.danger = -BoardDeadZone * 50000000;
        result.clear = clear;
        result.fill = float(map.count) / (map.width * (map.height - config_->safe));
        result.hole = float(v.HoleCountSrc) / (map.height - config_->safe);
        result.new_hole = v.HoleCount > v.HoleCountSrc ? float(v.HoleCount - v.HoleCountSrc) / map.height : 0;
        result.soft_drop = !node->open(map);
        return result;
    }

    C2::Status C2::get(Result const &eval_result, size_t depth, Status const &status) const
    {
        Status result;
        result.attack = 0;
        result.combo = status.combo;
        result.combo_limit = status.combo_limit > 0 ? status.combo_limit - 1 : 0;
        result.value = eval_result.danger;
        if(config_->mode == 0)
        {
            if(result.combo_limit == 0)
            {
                result.combo = 0;
            }
            if(result.combo_limit < 6 && result.combo < 4)
            {
                result.combo = 0;
            }
            static const float table[][4] =
            {
                { 4000,  3000,  4000,  8000},
                {    0,     0,   400,   800},
                {    0,     0,   100,   200},
                { 2500,   500,  1500,  2000},
                { 3500,  1500,  2500,  3000},
                { 4500,  2500,  3500,  4000},
                { 5500,  3500,  4500,  5000},
                { 6500,  4500,  5500,  6000},
                {92500, 90500, 91500, 92000},
                {93500, 91500, 92500, 93000},
                {94500, 92500, 93500, 94000},
                {95500, 93500, 94500, 95000},
                {96500, 94500, 95500, 96000},
                {97500, 95500, 96500, 96000},
                {98500, 96500, 97500, 97000},
                {99500, 97500, 98500, 98000},
                {99999, 99999, 99999, 99999},
                {99999, 99999, 99999, 99999},
                {99999, 99999, 99999, 99999},
                {99999, 99999, 99999, 99999},
                {99999, 99999, 99999, 99999},
                {99999, 99999, 99999, 99999},
                {99999, 99999, 99999, 99999},
                {99999, 99999, 99999, 99999},
                {99999, 99999, 99999, 99999},
            };
            double upstack = (config_->danger ? 0.2 : 1) * std::max<double>(0, 1 - eval_result.hole * 3.2) * std::max<double>(0, 1 - (eval_result.fill < 0.4 ? 0 : eval_result.fill - 0.4) * 4);
            double downstack;
            if(status.combo == 0)
            {
                downstack = (config_->danger ? 0.4 : 1) * std::max<double>(0, 1 - eval_result.hole * 2) * std::max<double>(0, 1 - std::abs(eval_result.fill - 0.48) * 5);
                result.attack -= 1600 * eval_result.new_hole;
                result.attack += eval_result.attack * upstack;
            }
            else
            {
                downstack = (config_->danger ? 0.4 : 1) * std::max<double>(0, 1 - eval_result.hole * 2) * std::max<double>(0, 1 - eval_result.fill * 1.2);
            }
            if(eval_result.clear > 0)
            {
                if(status.combo == 0)
                {
                    result.attack -= 8000 * upstack;
                    if(eval_result.clear <= 2)
                    {
                        result.attack -= table[status.combo][eval_result.clear - 1] * upstack;
                    }
                    else
                    {
                        result.attack += table[status.combo][eval_result.clear - 1] * downstack;
                    }
                }
                else
                {
                    if(config_->danger && eval_result.clear == 1)
                    {
                        result.attack += table[status.combo][1] * downstack;
                    }
                    else
                    {
                        result.attack += table[status.combo][eval_result.clear - 1] * downstack;
                    }
                }
                ++result.combo;
            }
            else if(status.combo > 0)
            {
                result.attack -= (800 + status.combo * 100) * downstack;
            }
            if(eval_result.clear == 0 && config_->danger)
            {
                if(status.combo < 3)
                {
                    result.attack -= 400;
                }
                else
                {
                    result.attack -= 100;
                }
            }
            if(eval_result.soft_drop)
            {
                result.attack -= 100;
            }
        }
        double rate = (1. / (depth + 1)) * 2;
        result.value += status.value * 1.1 + result.attack * rate + eval_result.map;
        return result;
    }

    C2::Status C2::iterate(Status const **status, size_t status_length) const
    {
        Status result;
        result.combo = 0;
        result.value = 0;
        result.combo_limit = 0;
        double
            low1 = std::numeric_limits<double>::max(),
            low2 = std::numeric_limits<double>::max();
        for(size_t i = 0; i < status_length; ++i)
        {
            double v = status[i] == nullptr ? -9999999999 : status[i]->value;
            result.value += v;
            if(v < low1)
            {
                if(low1 < low2)
                {
                    low2 = low1;
                }
                low1 = v;
            }
            else if(v < low2)
            {
                low2 = v;
            }
        }
        result.value = (result.value - low1 - low2) / (status_length - 2);
        return result;
    }

    size_t C2::map_in_danger_(m_tetris::TetrisMap const &map) const
    {
        size_t danger = 0;
        for(size_t i = 0; i < context_->type_max(); ++i)
        {
            if(map_danger_data_[i].data[0] & map.row[map.height - 4] || map_danger_data_[i].data[1] & map.row[map.height - 3] || map_danger_data_[i].data[2] & map.row[map.height - 2] || map_danger_data_[i].data[3] & map.row[map.height - 1])
            {
                ++danger;
            }
        }
        if(map.row[17] != 0)
        {
            ++danger;
        }
        return danger;
    }
}