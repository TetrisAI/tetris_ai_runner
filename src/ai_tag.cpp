
//by ZouZhiZhang

#include "tetris_core.h"
#include "integer_utils.h"
#include "ai_tag.h"
#include <cstdint>

using namespace m_tetris;
using namespace zzz;


namespace ai_tag
{
    bool the_ai_games_old::Status::operator < (Status const &other) const
    {
        return value < other.value;
    }

    void the_ai_games_old::init(m_tetris::TetrisContext const *context)
    {
        context_ = context;
        map_danger_data_.resize(context->type_max());
        for(size_t i = 0; i < context->type_max(); ++i)
        {
            TetrisMap map(context->width(), context->height());
            TetrisNode const *node = context->generate(i);
            node->move_down->attach(context, map);
            std::memcpy(map_danger_data_[i].data, &map.row[map.height - 4], sizeof map_danger_data_[i].data);
            for(int y = 0; y < 3; ++y)
            {
                map_danger_data_[i].data[y + 1] |= map_danger_data_[i].data[y];
            }
        }
        col_mask_ = context->full() & ~1;
        row_mask_ = context->full();
    }

    std::string the_ai_games_old::ai_name() const
    {
        return "The AI Games (SetoSan) v0.1";
    }

    the_ai_games_old::Result the_ai_games_old::eval(TetrisNode const *node, TetrisMap const &map, TetrisMap const &src_map, size_t clear) const
    {
        double LandHeight = node->row + node->height;
        double Middle = std::abs((int(node->status.x) + 1) * 2 - int(map.width));
        double EraseCount = clear;

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
            ColTrans += ZZZ_BitCount((map.row[y] ^ (map.row[y] << 1)) & col_mask_);
            if(y != 0)
            {
                RowTrans += ZZZ_BitCount(map.row[y - 1] ^ map.row[y]);
            }
        }
        RowTrans += ZZZ_BitCount(row_mask_ & ~map.row[0]);
        RowTrans += ZZZ_BitCount(map.roof == map.height ? row_mask_ & ~map.row[map.roof - 1] : map.row[map.roof - 1]);
        struct
        {
            int HoleCount;
            int HoleLine;

            int WideWellDepth[6];
            int WellDepth[32];
            int WellDepthTotle;

            int LineCoverBits;
            int HoleBits0;
            int ClearWidth0;
            int HoleBits1;
            int ClearWidth1;
            int HoleBits2;
            int ClearWidth2;
        } v;
        std::memset(&v, 0, sizeof v);
        int HolePosy0 = -1;
        int HolePosy1 = -1;
        int HolePosy2 = -1;

        for(int y = map.roof - 1; y >= 0; --y)
        {
            v.LineCoverBits |= map.row[y];
            int LineHole = v.LineCoverBits ^ map.row[y];
            if(LineHole != 0)
            {
                v.HoleCount += ZZZ_BitCount(LineHole);
                v.HoleLine++;
                if(HolePosy0 == -1)
                {
                    HolePosy0 = y + 1;
                    v.HoleBits0 = LineHole;
                }
                else if(HolePosy1 == -1)
                {
                    HolePosy1 = y + 1;
                    v.HoleBits1 = LineHole;
                }
                else if(HolePosy2 == -1)
                {
                    HolePosy2 = y + 1;
                    v.HoleBits2 = LineHole;
                }
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
                if(ZZZ_BitCount(map.row[y]) + MaxWellWidth == map.width)
                {
                    v.WideWellDepth[MaxWellWidth - 1] += 2;
                }
                else
                {
                    v.WideWellDepth[MaxWellWidth - 1] -= 1;
                }
            }
        }
        if(HolePosy0 >= 0)
        {
            for(int y = HolePosy0; y < map.roof; ++y)
            {
                int CheckLine = v.HoleBits0 & map.row[y];
                if(CheckLine == 0)
                {
                    break;
                }
                v.ClearWidth0 += (y + 1) * ZZZ_BitCount(CheckLine);
            }
            if(HolePosy1 >= 0)
            {
                for(int y = HolePosy1; y < map.roof; ++y)
                {
                    int CheckLine = v.HoleBits1 & map.row[y];
                    if(CheckLine == 0)
                    {
                        break;
                    }
                    v.ClearWidth1 += (y + 1) * ZZZ_BitCount(CheckLine);
                }
                if(HolePosy2 >= 0)
                {
                    for(int y = HolePosy2; y < map.roof; ++y)
                    {
                        int CheckLine = v.HoleBits2 & map.row[y];
                        if(CheckLine == 0)
                        {
                            break;
                        }
                        v.ClearWidth2 += (y + 1) * ZZZ_BitCount(CheckLine);
                    }
                }
            }
        }
        int low_x = 1;
        for(int x = 2; x < width_m1; ++x)
        {
            if(map.top[x] < map.top[low_x])
            {
                low_x = x;
            }
        }
        if(map.top[0] <= map.top[low_x])
        {
            low_x = 0;
        }
        if(map.top[width_m1] <= map.top[low_x])
        {
            low_x = width_m1;
        }
        int low_y = map.top[low_x];
        int full = 0;
        for(int y = map.roof - 1; y >= 0; --y)
        {
            if(map.row[y] == context_->full())
            {
                full = y + 1;
                low_y -= y;
                break;
            }
        }
        int tilt = 0;
        for(int x = low_x, ex = std::max(0, low_x - 5); x > ex; --x)
        {
            if(map.top[x] > map.top[x + 1])
            {
                tilt += 2;
            }
            else if(map.top[x] == map.top[x + 1])
            {
                tilt += 1;
            }
        }
        for(int x = low_x, ex = std::min(width_m1, low_x + 5); x < ex; ++x)
        {
            if(map.top[x] > map.top[x - 1])
            {
                tilt += 2;
            }
            else if(map.top[x] == map.top[x - 1])
            {
                tilt += 1;
            }
        }
        Result result;
        result.land_point = (0.
                             - LandHeight * 1750 / map.height
                             + Middle * 2
                             + EraseCount * 60
                             );
        result.map = (0.
                      - ColTrans * 80
                      - RowTrans * 80
                      - v.HoleCount * 160
                      - v.HoleLine * 380
                      - v.ClearWidth0 * 8
                      - v.ClearWidth1 * 4
                      - v.ClearWidth2 * 1
                      - v.WellDepthTotle * 160
                      + v.WideWellDepth[5] * 1
                      + v.WideWellDepth[4] * 2
                      + v.WideWellDepth[3] * 1
                      + v.WideWellDepth[2] * 48
                      + v.WideWellDepth[1] * -8
                      + v.WideWellDepth[0] * 2
                      + (low_x == 0 ? 200 : 0)
                      );
        result.tilt = tilt;
        result.full = full;
        result.count = map.count;
        result.clear = clear;
        result.low_y = low_y;
        result.save_map = &map;
        result.node_top = node->row + node->height;
        return result;
    }

    the_ai_games_old::Status the_ai_games_old::get(Result const &eval_result, size_t depth, Status const &status) const
    {
        Status result = status;
        double BoardDeadZone = 0;
        if(eval_result.save_map->roof + status.up >= context_->height() || eval_result.node_top >= context_->height())
        {
            BoardDeadZone = context_->type_max();
        }
        else
        {
            BoardDeadZone = map_in_danger_(*eval_result.save_map, status.up);
        }
        result.land_point -= BoardDeadZone * 50000000;
        bool building = (eval_result.count - eval_result.full * context_->width()) * 3 / 2 < std::max(0, (context_->height() - 6) - (eval_result.full + status.up)) * context_->width();
        if(eval_result.clear > 0)
        {
            if(eval_result.clear == 4)
            {
                result.land_point += 1000;
            }
            if(status.combo == 0 && building)
            {
                result.land_point -= (4 - std::min<int>(4, eval_result.low_y)) * 2000;
            }
            else if(status.combo > 0)
            {
                result.land_point += status.combo * 1200;
                if(eval_result.tilt > 5)
                {
                    result.land_point += 100;
                }
            }
            ++result.combo;
        }
        else
        {
            if(status.combo > 0 && building && eval_result.low_y > 4)
            {
                result.land_point -= status.combo * 600 + 600;
            }
            result.combo = 0;
        }
        double rate = (depth - 1.f) / 5 + 1;
        result.value = result.land_point + eval_result.map * rate;
        return result;
    }

    the_ai_games_old::Status the_ai_games_old::iterate(Status const **status, size_t status_length) const
    {
        Status result;
        result.combo = 0;
        result.up = 0;
        result.land_point = 0;
        result.value = 0;
        for(size_t i = 0; i < status_length; ++i)
        {
            if(status[i] == nullptr)
            {
                result.value -= 9999999999;
            }
            else
            {
                result.value += status[i]->value;
            }
        }
        result.value /= status_length;
        return result;
    }

    size_t the_ai_games_old::map_in_danger_(m_tetris::TetrisMap const &map, size_t up) const
    {
        size_t danger = 0;
        for(size_t i = 0; i < context_->type_max(); ++i)
        {
            size_t check_up = up;
            do
            {
                size_t height = map.height - check_up;
                if(map_danger_data_[i].data[0] & map.row[height - 4] || map_danger_data_[i].data[1] & map.row[height - 3] || map_danger_data_[i].data[2] & map.row[height - 2] || map_danger_data_[i].data[3] & map.row[height - 1])
                {
                    ++danger;
                    break;
                }
            }
            while(check_up-- > 0);
        }
        return danger;
    } 

    bool the_ai_games::Status::operator < (Status const &other) const
    {
        return value < other.value;
    }

    void the_ai_games::init(m_tetris::TetrisContext const *context, Config const *config)
    {
        context_ = context;
        config_ = config;
        map_danger_data_.resize(context->type_max());
        for(size_t i = 0; i < context->type_max(); ++i)
        {
            TetrisMap map(context->width(), context->height());
            TetrisNode const *node = context->generate(i);
            node->move_down->attach(context, map);
            std::memcpy(map_danger_data_[i].data, &map.row[map.height - 4], sizeof map_danger_data_[i].data);
            for(int y = 0; y < 3; ++y)
            {
                map_danger_data_[i].data[y + 1] |= map_danger_data_[i].data[y];
            }
        }
        col_mask_ = context->full() & ~1;
        row_mask_ = context->full();
        const int full = context->full();
    }

    std::string the_ai_games::ai_name() const
    {
        return "The AI Games (SetoSan) v0.1";
    }

    the_ai_games::Result the_ai_games::eval(TetrisNodeEx const &node, TetrisMap const &map, TetrisMap const &src_map, size_t clear) const
    {
        Result result;
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
            ColTrans += ZZZ_BitCount((map.row[y] ^ (map.row[y] << 1)) & col_mask_);
            if(y != 0)
            {
                RowTrans += ZZZ_BitCount(map.row[y - 1] ^ map.row[y]);
            }
        }
        RowTrans += ZZZ_BitCount(row_mask_ & ~map.row[0]);
        RowTrans += ZZZ_BitCount(map.roof == map.height ? row_mask_ & ~map.row[map.roof - 1] : map.row[map.roof - 1]);
        struct
        {
            int HoleCount;
            int HoleLine;

            int HoleDepth;
            int WellDepth;

            int HoleNum[32];
            int WellNum[32];

            int LineCoverBits;
            double ClearWidth;
        } v;
        std::memset(&v, 0, sizeof v);

        for(int y = map.roof - 1; y >= 0; --y)
        {
            v.LineCoverBits |= map.row[y];
            int LineHole = v.LineCoverBits ^ map.row[y];
            if(LineHole != 0)
            {
                v.HoleCount += ZZZ_BitCount(LineHole);
                v.HoleLine++;
                for(int hy = y + 1; hy < map.roof; ++hy)
                {
                    uint32_t CheckLine = LineHole & map.row[hy];
                    if(CheckLine == 0)
                    {
                        break;
                    }
                    v.ClearWidth += ZZZ_BitCount(CheckLine);
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
        result.map = (0.
                      - map.roof * config_->map_low_width
                      - ColTrans * config_->col_trans_width
                      - RowTrans * config_->row_trans_width
                      - v.HoleCount * config_->hold_count_width
                      - v.HoleLine * config_->hold_focus_width
                      - v.WellDepth * config_->well_depth_width
                      - v.HoleDepth * config_->hole_depth_width
                      - v.ClearWidth * config_->dig_clear_width
                      );
        result.map_low = 0;
        while(result.map_low < map.height && map.row[result.map_low] == context_->full())
        {
            ++result.map_low;
        }
        int attack_x = 1, depth = 0;
        for(int x = 2; x < width_m1; ++x)
        {
            if(map.top[x] < map.top[attack_x])
            {
                attack_x = x;
            }
        }
        result.node_top = node->row + node->height;
        result.clear = clear;
        result.tbuild = map_for_tspin_(map, attack_x, map.top[attack_x]);
        if(result.map_low == map.top[attack_x])
        {
            result.tbuild *= 8;
        }
        result.save_map = &map;
        return result;
    }

    the_ai_games::Status the_ai_games::get(TetrisNodeEx &node, Result const &eval_result, size_t depth, Status const &status) const
    {
        int tspin = node.is_check && node.is_ready && node.is_last_rotate ? eval_result.clear : 0;
        if (tspin > 0)
        {
            node.type = TSpinType::TSpin;
        }
        Status result = status;
        double BoardDeadZone = 0;
        if(eval_result.save_map->roof + status.up[depth] >= context_->height() || eval_result.node_top >= context_->height())
        {
            BoardDeadZone = context_->type_max();
        }
        else
        {
            BoardDeadZone = map_in_danger_(*eval_result.save_map, status.up[depth]);
        }
        result.attack -= BoardDeadZone * 50000000;
        result.attack += eval_result.clear * (eval_result.clear + 1) * config_->line_clear_width;
        if(tspin > 0)
        {
            result.attack += tspin * config_->tspin_clear_width;
        }
        if(eval_result.clear == 4)
        {
            result.attack += config_->tetris_clear_width;
        }
        result.attack += eval_result.tbuild * config_->tspin_build_width;
        if(eval_result.clear > 0)
        {
            if(result.combo > 0)
            {
                result.attack += result.combo * config_->combo_add_width;
            }
            else
            {
                result.attack -= config_->combo_break_minute;
            }
            ++result.combo;
        }
        else
        {
            ++result.combo = 0;
        }
        result.value = (0.
                        + result.attack
                        + eval_result.map
                        );
        result.max_attack = std::max(status.max_attack, result.attack);
        return result;
    }

    the_ai_games::Status the_ai_games::iterate(Status const **status, size_t status_length) const
    {
        Status result;
        result.max_combo = 0;
        result.combo = 0;
        result.max_attack = 0;
        result.attack = 0;
        std::memset(result.up, 0, sizeof result.up);
        result.land_point = 0;
        result.value = 0;
        for(size_t i = 0; i < status_length; ++i)
        {
            if(status[i] == nullptr)
            {
                result.value -= 10 * 50000000;
            }
            else
            {
                result.value += status[i]->value;
            }
        }
        result.value /= status_length;
        return result;
    }

    int the_ai_games::map_for_tspin_(m_tetris::TetrisMap const &map, int x, int y) const
    {
        --x;
        int row0 = map.row[y];
        int row1 = map.row[y + 1];
        if(((~row0) & row1) != 0)
        {
            return 0;
        }
        int value = 1;
        int row2 = y + 2 < map.height ? map.row[y + 2] : 0;
        if(((row0 >> x) & 7) == 5)
        {
            value += 3;
            if(ZZZ_BitCount(row0) == map.width - 1)
            {
                value += 3;
            }
            if(((((~row1) & row2) >> x) & ~7) == 0 && ((row1 >> x) & 7) == 0)
            {
                value += 4;
                if(ZZZ_BitCount(row1) == map.width - 3)
                {
                    value += 4;
                }
                int row2_check = (row2 >> x) & 7;
                if(row2_check == 1 || row2_check == 4)
                {
                    value += 2;
                }
            }
        }
        int mask = ((row1 >> x) & 7);
        for(y += 2; y < map.roof; ++y)
        {
            mask |= ((map.row[y] >> x) & 7);
            if(ZZZ_BitCount(mask) > 1)
            {
                return 0;
            }
        }
        return value;
    }

    size_t the_ai_games::map_in_danger_(m_tetris::TetrisMap const &map, size_t up) const
    {
        size_t danger = 0;
        for(size_t i = 0; i < context_->type_max(); ++i)
        {
            size_t check_up = up;
            do
            {
                size_t height = map.height - check_up;
                if(map_danger_data_[i].data[0] & map.row[height - 4] || map_danger_data_[i].data[1] & map.row[height - 3] || map_danger_data_[i].data[2] & map.row[height - 2] || map_danger_data_[i].data[3] & map.row[height - 1])
                {
                    ++danger;
                    break;
                }
            }
            while(check_up-- > 0);
        }
        return danger;
    }

    bool the_ai_games_enemy::Status::operator < (Status const &other) const
    {
        return point < other.point;
    }

    void the_ai_games_enemy::init(m_tetris::TetrisContext const *context, Config const *config)
    {
        config_ = config;
    }

    std::string the_ai_games_enemy::ai_name() const
    {
        return "The AI Games (SetoSan) v0.1";
    }

    the_ai_games_enemy::Result the_ai_games_enemy::eval(TetrisNodeEx const &node, TetrisMap const &map, TetrisMap const &src_map, size_t clear) const
    {
        Result result =
        {
            clear, node.is_check && node.is_ready && node.is_last_rotate ? clear : 0
        };
        return result;
    }

    the_ai_games_enemy::Status the_ai_games_enemy::get(TetrisNodeEx &node, Result const &eval_result, size_t depth, Status const &status) const
    {
        Status result = status;
        if(eval_result.clear > 0)
        {
            if(eval_result.tspin > 0)
            {
                result.point += eval_result.tspin * 6;
            }
            else
            {
                switch(eval_result.clear)
                {
                case 1: result.point += 1; break;
                case 2: result.point += 3; break;
                case 3: result.point += 6; break;
                case 4: result.point += 12; break;
                }
            }
            result.point += status.combo;
            result.combo = status.combo + 1;
        }
        else
        {
            result.combo = 0;
        }
        result.up[depth] = result.point;
        if(result.point >= *config_->point_ptr)
        {
            *config_->point_ptr = result.point;
            std::copy(result.up, result.up + 4, config_->up_ptr);
        }
        return result;
    }

    the_ai_games_enemy::Status the_ai_games_enemy::iterate(Status const **status, size_t status_length) const
    {
        Status result;
        result.combo = 0;
        result.point = 0;
        for(size_t i = 0; i < status_length; ++i)
        {
            if(status[i] != nullptr && status[i]->point > result.point)
            {
                result.point = status[i]->point;
            }
        }
        return result;
    }

}