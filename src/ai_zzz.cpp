
//by ZouZhiZhang

#include "tetris_core.h"
#include "integer_utils.h"
#include "ai_zzz.h"
#include <cstdint>

using namespace m_tetris2;
using namespace zzz;

namespace
{
    enum ItemType
    {
        a3,
        a2,
        a1,
        m3,
        m2,
        m1,
        sf,
        ss
    };
    struct Item
    {
        int16_t col, type;
    };
    const Item ItemTable[20][7] =
        {
#define I(a, b) {0x##a - 1, b}
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

        bool Attack::Status::operator<(Status const &other) const
        {
            return value < other.value;
        }

        std::string Attack::ai_name() const
        {
            return "AX Attack v0.1";
        }

        Attack::Status Attack::get(Result const &eval_result, size_t depth, Status const &status) const
        {
            Status result = status;
            result.land_point += eval_result.land_point;
            double length_rate = 10. / (depth + 1);
            switch (eval_result.clear)
            {
            case 0:
                break;
            case 1:
            case 2:
                result.rubbish += eval_result.clear;
                break;
            case 3:
                if (config_->mode != 0)
                {
                    result.attack += 12;
                    break;
                }
            default:
                result.attack += (eval_result.clear * 10 * length_rate);
                break;
            }
            result.value = (0. + result.land_point / (depth + 1) + eval_result.map - result.rubbish * (eval_result.danger > 0 ? -100 : 640) + result.attack * 100);
            return result;
        }
    }

    std::string Dig::ai_name() const
    {
        return "ZZZ Dig v0.2";
    }

    double Dig::get(double const &eval_result, size_t depth) const
    {
        (void)depth;
        return eval_result;
    }

    bool TOJ::Status::operator<(Status const &other) const
    {
        return value < other.value;
    }

    std::string TOJ::ai_name() const
    {
        return "ZZZ TOJ v0.12";
    }

    void TOJ::Status::init_t_value(uint32_t const *rows, int width, int roof, int16_t &t2_value_ref, int16_t &t3_value_ref, uint32_t *out_rows)
    {
        int row_bit_count_global[40];
        // bitboard semantics: row[y] bit=1 means occupied; empty count = popcount(~row & row_mask)
        int row_mask_local = (1u << width) - 1;
        for (int y = 0; y < roof; ++y)
        {
            row_bit_count_global[y] = ZZZ_BitCount(~rows[y] & row_mask_local);
        }
        memset(row_bit_count_global + roof, 0, sizeof(int) * (40 - roof));
        t2_value_ref = 0;
        t3_value_ref = 0;
        for (int y = 0, ey = std::min(20, roof - 2); y < ey; ++y)
        {
            int new_y = y;
            // Flip to old "bit=1=empty" semantics so that all shape-detection
            // expressions below (which use ~rowN to find occupied cells) remain
            // unchanged under the new bitboard convention (bit=1=occupied).
            int row0 = ~rows[y];
            int row1 = ~rows[y + 1];
            int row2 = ~rows[y + 2];
            int row3 = ~rows[y + 3];
            int row4 = ~rows[y + 4];
            int row5 = ~rows[y + 5];
            int row6 = ~rows[y + 6];
            int *row_bit_count = row_bit_count_global + y;
            for (int x = 1, ex = width - 2; x < ex; ++x)
            {
                if (((~row0 >> x) & 1) & (((~row1 >> x) & 3) == 3) & ((~row2 >> x) & 1) & !((row3 >> x) & 7) & (((~row4 >> x) & 6) == 6))
                {
                    int t3_count = 0;
                    if (row_bit_count[0] == width - 1)
                    {
                        t3_count += 1;
                    }
                    if (row_bit_count[1] == width - 2)
                    {
                        t3_count += 1;
                    }
                    if (row_bit_count[2] == width - 1)
                    {
                        t3_count += 1;
                    }
                    else
                    {
                        continue;
                    }
                    int t3_bit_count = row_bit_count[0] + row_bit_count[1] + row_bit_count[2];
                    if (t3_count >= 2 && t3_bit_count > width * 2)
                    {
                        int t3_value = t3_bit_count * t3_count;
                        if ((row4 >> x) & 1)
                        {
                            t3_value += t3_bit_count + row_bit_count[3];
                        }
                        else if (((row4 >> x) & 7) == 1 && ((row5 >> x) & 7) == 1 && ((row6 >> x) & 7) == 1)
                        {
                            t3_value = 0;
                        }
                        else
                        {
                            t3_value /= 2;
                        }
                        if (((row3 >> x) & 8) != ((row4 >> x) & 8))
                        {
                            t3_value = 0;
                        }
                        if (t3_value > 0 && out_rows != nullptr)
                        {
                            out_rows[y + 0] |= 1 << x;
                            out_rows[y + 1] |= 3 << x;
                            out_rows[y + 2] |= 1 << x;
                            out_rows[y + 3] |= 1 << x;
                        }
                        t3_value_ref += t3_value;
                        new_y += 2;
                        break;
                    }
                }
            }
            if (new_y != y)
            {
                y = new_y;
                continue;
            }
            for (int x = 0, ex = width - 3; x < ex; ++x)
            {
                if (((~row0 >> x) & 4) & (((~row1 >> x) & 6) == 6) & ((~row2 >> x) & 4) & !((row3 >> x) & 7) & (((~row4 >> x) & 3) == 3))
                {
                    int t3_count = 0;
                    if (row_bit_count[0] == width - 1)
                    {
                        t3_count += 1;
                    }
                    if (row_bit_count[1] == width - 2)
                    {
                        t3_count += 1;
                    }
                    if (row_bit_count[2] == width - 1)
                    {
                        t3_count += 1;
                    }
                    else
                    {
                        continue;
                    }
                    int t3_bit_count = row_bit_count[0] + row_bit_count[1] + row_bit_count[2];
                    if (t3_count >= 2 && t3_bit_count > width * 2)
                    {
                        int t3_value = t3_bit_count * t3_count;
                        if ((row2 >> x) & 2)
                        {
                            t3_value += t3_bit_count;
                        }
                        else if (((row4 >> x) & 7) == 4 && ((row5 >> x) & 7) == 4 && ((row6 >> x) & 7) == 4)
                        {
                            t3_value = 0;
                        }
                        else
                        {
                            t3_value /= 4;
                        }
                        if (((row3 >> x) & 1) != ((row4 >> x) & 1))
                        {
                            t3_value = 0;
                        }
                        if (t3_value > 0 && out_rows != nullptr)
                        {
                            out_rows[y + 0] |= 4 << x;
                            out_rows[y + 1] |= 6 << x;
                            out_rows[y + 2] |= 4 << x;
                            out_rows[y + 3] |= 4 << x;
                        }
                        t3_value_ref += t3_value;
                        new_y += 2;
                        break;
                    }
                }
            }
            if (new_y != y)
            {
                y = new_y;
                continue;
            }
            for (int x = 0, ex = width - 2; x < ex; ++x)
            {
                if ((((row0 >> x) & 7) == 5) & !((row1 >> x) & 7))
                {
                    int row01_count = row_bit_count[0] + row_bit_count[1];
                    int t2_value = row01_count;
                    if (row01_count > width)
                    {
                        if (row_bit_count[0] == width - 1)
                        {
                            t2_value += row01_count;
                        }
                        if (row_bit_count[1] == width - 3)
                        {
                            t2_value += row01_count;
                        }
                        int row2_check = (row2 >> x) & 7;
                        switch (row2_check)
                        {
                        case 1:
                        case 4:
                            t2_value += row01_count * 3;
                            break;
                        case 2:
                        case 3:
                        case 5:
                        case 6:
                        case 7:
                            t2_value = 0;
                            break;
                        default:
                            t2_value = t2_value / 2;
                            break;
                        }
                        if (t2_value > 0 && out_rows != nullptr)
                        {
                            out_rows[y + 0] |= 2 << x;
                            out_rows[y + 1] |= 7 << x;
                        }
                        t2_value_ref += t2_value;
                        ++new_y;
                        break;
                    }
                    t2_value_ref += t2_value;
                }
            }
            y = new_y;
        }
    };

    TOJ::Status TOJ::get(Result const &eval_result, size_t depth, Status const &status, AIEnv const &env) const
    {
        Status result;
        memcpy(&result, &status, sizeof status);
        int attack = 0;
        int t_attack = 0;
        double like = 0;
        double dislike = 0;
        auto get_combo_attack = [&](int c)
        {
            return config_->table[std::min<int>(config_->table_max - 1, c + 1)];
        };
        auto update_like = [&](double v)
        {
            v > 0 ? like += v : dislike -= v;
        };
        int next_index = env.length > 0 ? piece_index_[static_cast<unsigned char>(*env.next)] : -1;
        int safe = eval_result.top_out ? -1 : next_index >= 0 ? eval_result.safe_cache[next_index]
                                                              : eval_result.roof;
        auto &p = config_->param;
        switch (eval_result.clear)
        {
        case 0:
            result.combo = 0;
            if (status.under_attack > 0)
            {
                result.map_rise = status.under_attack;
                if (result.map_rise > safe)
                {
                    result.death = 1;
                }
                result.under_attack = 0;
            }
            update_like((eval_result.piece_type == 'I') * p.waste_i);
            update_like((eval_result.piece_type == 'T') * p.waste_t);
            break;
        case 1:
            if (eval_result.t_spin == TSpinType::TSpinMini)
            {
                attack = 1 + status.b2b;
                update_like(p.tspin_mini);
            }
            else if (eval_result.t_spin == TSpinType::TSpin)
            {
                attack = 2 + status.b2b;
                update_like(p.tspin_1);
                t_attack = 1;
            }
            else
            {
                update_like((eval_result.piece_type == 'I') * p.waste_i);
                update_like((eval_result.piece_type == 'T') * p.waste_t);
                update_like(p.clear_1);
            }
            attack += get_combo_attack(++result.combo);
            result.b2b = eval_result.t_spin != TSpinType::None;
            break;
        case 2:
            if (eval_result.t_spin != TSpinType::None)
            {
                attack += 4 + status.b2b;
                result.b2b = true;
                update_like(p.tspin_2);
                t_attack = 1;
            }
            else
            {
                ++attack;
                result.b2b = false;
                update_like((eval_result.piece_type == 'I') * p.waste_i);
                update_like((eval_result.piece_type == 'T') * p.waste_t);
                update_like(p.clear_2);
            }
            attack += get_combo_attack(++result.combo);
            break;
        case 3:
            if (eval_result.t_spin != TSpinType::None)
            {
                attack = 6 + status.b2b * 2;
                result.b2b = true;
                update_like(p.tspin_3);
                t_attack = 1;
            }
            else
            {
                result.b2b = false;
                update_like((eval_result.piece_type == 'I') * p.waste_i);
                update_like(p.clear_3);
            }
            attack += get_combo_attack(++result.combo) + 2;
            break;
        case 4:
            result.b2b = true;
            attack = get_combo_attack(++result.combo) + 4 + status.b2b;
            update_like(p.clear_4);
            break;
        }
        result.under_attack = std::max(0, result.under_attack - attack);
        int config_safe = std::max(0, config_->safe - result.under_attack - result.map_rise);
        int t_expect = [=]() -> int
        {
            if (env.hold == 'T')
            {
                return 0;
            }
            for (size_t i = 0; i < env.length; ++i)
            {
                if (env.next[i] == 'T')
                {
                    return i;
                }
            }
            return 13;
        }();
        switch (env.hold)
        {
        case 'T':
            if (eval_result.t_spin == TSpinType::None)
            {
                update_like(double(20 + config_safe) * p.hold_t);
            }
            break;
        case 'I':
            if (eval_result.clear != 4)
            {
                update_like(double(40 - config_safe) * p.hold_i);
            }
            break;
        }
        safe -= result.map_rise;
        if (safe < 0 || eval_result.top_out)
        {
            result.death = 1;
            safe = 0;
        }
        if (eval_result.count == 0 && result.map_rise == 0)
        {
            like += 999;
            attack += 6;
        }
        double field = eval_result.value * double(40 - config_safe) / 20;
        double t_like = 0;
        double t_dislike = 0;
        if (t_attack == 0)
        {
            double t2_safe = std::max(0, config_safe - 4);
            double t3_safe = std::max(0, config_safe - 10);
            if (eval_result.t2_value > status.t2_value)
            {
                t_like += (eval_result.t2_value - status.t2_value) * t2_safe * std::max(10 - t_expect, 5) * p.t2_slot;
            }
            else
            {
                t_dislike += (status.t2_value - eval_result.t2_value) * t2_safe * 3 * p.t2_slot;
            }
            if (eval_result.t3_value > status.t3_value)
            {
                t_like += (eval_result.t3_value - status.t3_value) * t3_safe * std::max(10 - t_expect, 4) * (3 + result.b2b) * p.t3_slot;
            }
            else
            {
                t_dislike += (status.t3_value - eval_result.t3_value) * t3_safe * 4 * p.t3_slot;
            }
        }
        result.t2_value = eval_result.t2_value;
        result.t3_value = eval_result.t3_value;
        result.acc_value += (0 + attack * (config_safe + 16) * p.attack + get_combo_attack(result.combo) * result.combo * (100 - config_safe) * p.combo + (result.b2b - status.b2b) * (config_safe + 16) * p.b2b - t_dislike - dislike * config_safe * (config_safe + 4) * 4 - result.death * 999999999.0);
        result.like = (status.like * 1.3 + safe * (40 - config_safe) * p.safe + like * config_safe * (config_safe + 4) * 4 + t_like);
        result.value = (result.acc_value - result.map_rise * (40 - config_safe) * p.safe + result.like + field * p.base);
        return result;
    }

    bool TOJ_PC::Status::operator<(Status const &other) const
    {
        return value < other.value;
    }

    std::string TOJ_PC::ai_name() const
    {
        return "ZZZ TOJ_PC v0.1";
    }

    TOJ_PC::Status TOJ_PC::get(Result const &eval_result, size_t depth, Status const &status) const
    {
        Status result = status;

        switch (eval_result.clear)
        {
        case 0:
            result.combo = 0;
            if (status.under_attack > 0)
            {
                result.recv_attack += std::max(0, int(status.under_attack) - status.attack);
                result.under_attack = 0;
            }
            break;
        case 1:
            if (eval_result.t_spin == TSpinType::TSpinMini)
            {
                result.attack += status.b2b ? 2 : 1;
            }
            else if (eval_result.t_spin == TSpinType::TSpin)
            {
                result.attack += status.b2b ? 3 : 2;
            }
            result.attack += config_->table[std::min(config_->table_max - 1, ++result.combo)];
            result.b2b = eval_result.t_spin != TSpinType::None;
            break;
        case 2:
            if (eval_result.t_spin != TSpinType::None)
            {
                result.attack += status.b2b ? 5 : 4;
            }
            result.attack += config_->table[std::min(config_->table_max - 1, ++result.combo)] + 1;
            result.b2b = eval_result.t_spin != TSpinType::None;
            break;
        case 3:
            if (eval_result.t_spin != TSpinType::None)
            {
                result.attack += status.b2b ? 8 : 6;
            }
            result.attack += config_->table[std::min(config_->table_max - 1, ++result.combo)] + 2;
            result.b2b = eval_result.t_spin != TSpinType::None;
            break;
        case 4:
            result.attack += config_->table[std::min(config_->table_max - 1, ++result.combo)] + (status.b2b ? 5 : 4);
            result.b2b = true;
            break;
        }
        if (eval_result.roof == 0 && result.recv_attack == 0)
        {
            result.like += 100;
            result.pc = true;
        }
        if (eval_result.roof > 4)
        {
            result.like -= 1;
        }
        result.value = eval_result.value + result.like * 1e9;
        return result;
    }

    bool TOJ_v08::Status::operator<(Status const &other) const
    {
        return value < other.value;
    }

    std::string TOJ_v08::ai_name() const
    {
        return "ZZZ TOJ v0.8";
    }

    TOJ_v08::Status TOJ_v08::get(Result const &eval_result, size_t depth, Status const &status, AIEnv const &env) const
    {
        Status result = status;
        result.value = eval_result.value;
        int next_index = env.length > 0 ? piece_index_[static_cast<unsigned char>(*env.next)] : -1;
        int safe = eval_result.top_out ? -1 : next_index >= 0 ? eval_result.safe_cache[next_index]
                                                              : eval_result.roof;
        if (safe <= 0)
        {
            result.value -= 99999;
        }
        switch (eval_result.clear)
        {
        case 0:
            if (status.combo > 0 && status.combo < 3)
            {
                result.like -= 2;
            }
            result.combo = 0;
            if (status.under_attack > 0)
            {
                result.map_rise += std::max(0, int(status.under_attack) - status.attack);
                if (result.map_rise >= safe)
                {
                    result.death += result.map_rise - safe;
                }
                result.under_attack = 0;
            }
            break;
        case 1:
            if (eval_result.t_spin == TSpinType::TSpinMini)
            {
                result.attack += status.b2b ? 2 : 1;
            }
            else if (eval_result.t_spin == TSpinType::TSpin)
            {
                result.attack += status.b2b ? 3 : 2;
            }
            result.attack += config_->table[std::min(config_->table_max - 1, ++result.combo)];
            result.b2b = eval_result.t_spin != TSpinType::None;
            break;
        case 2:
            if (eval_result.t_spin != TSpinType::None)
            {
                result.like += 8;
                result.attack += status.b2b ? 5 : 4;
            }
            result.attack += config_->table[std::min(config_->table_max - 1, ++result.combo)] + 1;
            result.b2b = eval_result.t_spin != TSpinType::None;
            break;
        case 3:
            if (eval_result.t_spin != TSpinType::None)
            {
                result.like += 12;
                result.attack += status.b2b ? 8 : 6;
            }
            result.attack += config_->table[std::min(config_->table_max - 1, ++result.combo)] + 2;
            result.b2b = eval_result.t_spin != TSpinType::None;
            break;
        case 4:
            result.attack += config_->table[std::min(config_->table_max - 1, ++result.combo)] + (status.b2b ? 5 : 4);
            result.b2b = true;
            break;
        }
        if (result.combo < 5)
        {
            result.like -= 1.5 * result.combo;
        }
        if (eval_result.count == 0 && result.map_rise == 0)
        {
            result.like += 20;
            result.attack += 6;
        }
        if (status.b2b && !result.b2b)
        {
            result.like -= 2;
        }
        size_t t_expect = [=]() -> int
        {
            if (env.hold == 'T')
            {
                return 0;
            }
            for (size_t i = 0; i < env.length; ++i)
            {
                if (env.next[i] == 'T')
                {
                    return i;
                }
            }
            return 14;
        }();
        switch (env.hold)
        {
        case 'T':
            if (eval_result.t_spin == TSpinType::None)
            {
                result.like += 4;
            }
            break;
        case 'I':
            if (eval_result.clear != 4)
            {
                result.like += 2;
            }
            break;
        }
        double rate = (1. / (depth + 1)) + 3;
        result.max_combo = std::max(result.combo, result.max_combo);
        result.max_attack = std::max(result.attack, result.max_attack);
        result.value += ((0. + result.max_attack * 40 + result.attack * 256 * rate + eval_result.t2_value * (t_expect < 8 ? 512 : 320) * 1.5 + (safe >= 12 ? eval_result.t3_value * (t_expect < 4 ? 10 : 8) * (result.b2b ? 512 : 256) / (6 + result.under_attack) : 0) + (result.b2b ? 512 : 0) + result.like * 64) * std::max<double>(0.05, (full_count_ - eval_result.count - result.map_rise * (width_ - 1)) / double(full_count_)) + result.max_combo * (result.max_combo - 1) * 40 - result.death * 999999999.0);
        return result;
    }

    bool Botris_PC::Status::operator<(Status const &other) const
    {
        return value < other.value;
    }

    std::string Botris_PC::ai_name() const
    {
        return "ZZZ Botris_PC v0.1";
    }

    Botris_PC::Status Botris_PC::get(Result const &eval_result, size_t depth, Status const &status) const
    {
        Status result = status;
        int attack = 0;
        switch (eval_result.clear)
        {
        case 0:
            result.combo = 0;
            if (status.under_attack > 0)
            {
                result.recv_attack += std::max(0, int(status.under_attack) - status.attack);
                result.under_attack = 0;
            }
            break;
        case 1:
            if (eval_result.a_spin == ASpinType::ASpin)
            {
                attack += status.b2b ? 3 : 2;
            }
            attack += config_->table[std::min(config_->table_max - 1, ++result.combo)];
            result.b2b = eval_result.a_spin != ASpinType::None;
            break;
        case 2:
            if (eval_result.a_spin != ASpinType::None)
            {
                attack += status.b2b ? 5 : 4;
            }
            attack += config_->table[std::min(config_->table_max - 1, ++result.combo)] + 1;
            result.b2b = eval_result.a_spin != ASpinType::None;
            break;
        case 3:
            if (eval_result.a_spin != ASpinType::None)
            {
                attack += status.b2b ? 7 : 6;
            }
            attack += config_->table[std::min(config_->table_max - 1, ++result.combo)] + 2;
            result.b2b = eval_result.a_spin != ASpinType::None;
            break;
        case 4:
            attack += config_->table[std::min(config_->table_max - 1, ++result.combo)] + (status.b2b ? 5 : 4);
            result.b2b = true;
            break;
        }
        if (eval_result.roof == 0 && result.recv_attack == 0)
        {
            result.like += 100;
            result.pc = true;
            attack = 10;
        }
        result.attack += attack;
        if (eval_result.roof > 4)
        {
            result.like -= 1;
        }
        result.value = eval_result.value + result.like * 1e9;
        return result;
    }

    bool Botris::Status::operator<(Status const &other) const
    {
        return value < other.value;
    }

    std::string Botris::ai_name() const
    {
        return "ZZZ Botris v0.8";
    }

    Botris::Status Botris::get(Result const &eval_result, size_t depth, Status const &status, AIEnv const &env) const
    {
        Status result = status;
        result.value = eval_result.value;
        result.clear += eval_result.clear;
        int next_index = env.length > 0 ? piece_index_[static_cast<unsigned char>(*env.next)] : -1;
        int safe = eval_result.top_out ? -1 : next_index >= 0 ? eval_result.safe_cache[next_index]
                                                              : eval_result.roof;
        if (safe <= 0)
        {
            result.value -= 99999;
        }
        int attack = 0;
        switch (eval_result.clear)
        {
        case 0:
            if (status.combo > 0 && status.combo < 3)
            {
                result.like -= 2;
            }
            result.combo = 0;
            if (status.under_attack > 0)
            {
                result.map_rise += std::max(0, int(status.under_attack) - status.attack);
                if (result.map_rise >= safe)
                {
                    result.death += result.map_rise - safe;
                }
                result.under_attack = 0;
            }
            break;
        case 1:
            if (eval_result.a_spin == ASpinType::ASpin)
            {
                result.like += 8;
                attack += status.b2b ? 3 : 2;
            }
            attack += config_->table[std::min(config_->table_max - 1, ++result.combo)];
            result.b2b = eval_result.a_spin != ASpinType::None;
            break;
        case 2:
            if (eval_result.a_spin != ASpinType::None)
            {
                attack += status.b2b ? 5 : 4;
            }
            attack += config_->table[std::min(config_->table_max - 1, ++result.combo)] + 1;
            result.b2b = eval_result.a_spin != ASpinType::None;
            break;
        case 3:
            if (eval_result.a_spin != ASpinType::None)
            {
                attack += status.b2b ? 7 : 6;
            }
            attack += config_->table[std::min(config_->table_max - 1, ++result.combo)] + 2;
            result.b2b = eval_result.a_spin != ASpinType::None;
            break;
        case 4:
            attack += config_->table[std::min(config_->table_max - 1, ++result.combo)] + (status.b2b ? 5 : 4);
            result.b2b = true;
            break;
        }
        if (result.combo < 6)
        {
            result.like -= 3 * result.combo;
        }
        if (eval_result.count == 0 && result.map_rise == 0)
        {
            result.like -= 8;
            attack = 10;
        }
        if (env.hold == 'I' && eval_result.clear == 0)
        {
            result.like += 2;
        }
        double rate = (1. / (depth + 1)) + 3;
        result.attack += attack;
        result.max_combo = std::max(result.combo, result.max_combo);
        result.max_attack = std::max(result.attack, result.max_attack);
        result.value += ((0. + result.max_attack * 64 + result.attack * 128 * rate + (result.b2b ? 514 : 0) + result.like * 64) * std::max<double>(0.05, (full_count_ - eval_result.count - result.map_rise * (width_ - 1)) / double(full_count_)) + result.max_combo * (result.max_combo - 1) * 40 - result.death * 999999999.0);
        return result;
    }

    bool C2::Status::operator<(Status const &other) const
    {
        return value < other.value;
    }

    std::string C2::ai_name() const
    {
        return "C2 v0.1";
    }

    C2::Status C2::get(Result const &eval_result, size_t depth, Status const &status, AIEnv const &env) const
    {
        Status result;
        result.attack = 0;
        result.combo = status.combo;
        result.combo_limit = status.combo_limit > 0 ? status.combo_limit - 1 : 0;
        result.value = eval_result.map;
        if (config_->mode == 0)
        {
            if (result.combo_limit == 0)
            {
                result.combo = 0;
            }
            if (result.combo_limit < 6 && result.combo < 3)
            {
                result.combo = 0;
            }
            static const float table[][5] =
                {
                    //{     0,  3656,  4875,  6094,  7313},
                    //{   400,  1728,  2338,  2947,  3556},
                    //{   800,   707,  1009,  1312,  1614},
                    //{  1600,   238,   417,   596,   775},
                    //{  3200,   -41,    78,   198,   318},
                    //{  6400,  -241,  -155,   -69,    17},
                    //{ 12800,  -403,  -338,  -272,  -206},
                    //{ 25600,  -544,  -493,  -441,  -389},
                    //{ 51200,  -673,  -631,  -589,  -547},
                    //{102400,  -794,  -759,  -724,  -689},
                    //{204800,  -910,  -881,  -851,  -821},
                    //{409600, -1023,  -997,  -971,  -946},
                    //{819200, -1133, -1110, -1088, -1065},
                    //{819200, -1241, -1221, -1201, -1181},
                    //{819200, -1347, -1330, -1312, -1294},
                    //{819200, -1453, -1437, -1421, -1405},
                    //{819200, -1557, -1543, -1529, -1514},
                    //{819200, -1661, -1648, -1635, -1622},
                    //{819200, -1764, -1753, -1741, -1729},
                    //{819200, -1867, -1857, -1846, -1835},
                    {0, -3000, 3000, 4000, 5000},
                    {500, 500, 1000, 1000, 2000},
                    {1000, 1000, 500, 500, 500},
                    {2000, 2000, 1000, 1000, 1000},
                    {4000, 4000, 2000, 2000, 2000},
                    {6000, 6000, 3000, 3000, 2500},
                    {8000, 8000, 4000, 3333, 2500},
                    {10000, 10000, 5000, 3333, 2500},
                    {10000, 10000, 5000, 3333, 2500},
                    {10000, 10000, 5000, 3333, 2500},
                    {10000, 10000, 5000, 3333, 2500},
                    {10000, 10000, 5000, 3333, 2500},
                    {10000, 10000, 5000, 3333, 2500},
                    {10000, 10000, 5000, 3333, 2500},
                    {10000, 10000, 5000, 3333, 2500},
                    {10000, 10000, 5000, 3333, 2500},
                    {10000, 10000, 5000, 3333, 2500},
                    {10000, 10000, 5000, 3333, 2500},
                    {10000, 10000, 5000, 3333, 2500},
                };
            double fill = eval_result.fill + (config_->danger ? 0.32 : 0);
            double hole = eval_result.hole + (config_->danger ? 0.16 : 0);
            double upstack = std::max<double>(0, 1 - hole * 3.3) * std::max<double>(0, 1 - (fill < 0.4 ? 0 : fill - 0.4) * 4);
            double downstack;
            double length_ratio = (1 << ((env.node != ' ') + env.length)) * 0.5;
            if (status.combo == 0)
            {
                downstack = std::max<double>(0.2, 1 - hole * 3.3) * std::max<double>(0.2, 1 - std::abs(fill - 0.48) * 5);
                result.attack -= 32000 * eval_result.new_hole;
                result.attack += eval_result.attack * upstack;
            }
            else
            {
                downstack = std::max<double>(0.2, 1 - hole * 3.3) * std::max<double>(0.2, 1 - fill * 1.2);
            }
            if (eval_result.clear > 0)
            {
                if (status.combo == 0)
                {
                    result.attack -= 8000 * upstack;
                    result.attack += table[status.combo][eval_result.clear] * downstack;
                }
                else
                {
                    result.attack += table[status.combo][eval_result.clear] * downstack * length_ratio;
                }
                result.combo = std::min<size_t>(16, result.combo + 1);
            }
            else if (status.combo > 0)
            {
                result.attack -= table[status.combo][0] * downstack * length_ratio;
            }
            result.attack += status.attack;
        }
        result.map = status.map + eval_result.map;
        if (eval_result.soft_drop && !config_->soft_drop)
        {
            result.map -= 9999999999;
        }
        result.value = result.attack + result.map;
        return result;
    }

    C2::Status C2::iterate(Status const **status, size_t status_length) const
    {
        Status result;
        result.combo = 0;
        result.value = 0;
        result.combo_limit = 0;
        static constexpr double max_val = 1e20;
        double
            lower1 = +max_val,
            lower2 = +max_val,
            lower3 = +max_val;
        for (size_t i = 0; i < status_length; ++i)
        {
            double v = status[i] == nullptr ? -max_val : status[i]->value;
            result.value += v;
            if (v < lower1)
            {
                if (lower1 < lower2)
                {
                    if (lower2 < lower3)
                    {
                        lower3 = lower2;
                    }
                    lower2 = lower1;
                }
                lower1 = v;
            }
            else if (v < lower2)
            {
                if (lower2 < lower3)
                {
                    lower3 = lower2;
                }
                lower2 = v;
            }
            else if (v < lower3)
            {
                lower3 = v;
            }
        }
        result.value = (result.value - lower1 - lower2 - lower3) / 4;
        //double
        //    upper1 = -max_val,
        //    upper2 = -max_val,
        //    upper3 = -max_val;
        //for (size_t i = 0; i < status_length; ++i)
        //{
        //    double v = status[i] == nullptr ? -max_val : status[i]->value;
        //    result.value += v;
        //    if (v > upper1)
        //    {
        //        if (upper1 > upper2)
        //        {
        //            if (upper2 > upper3)
        //            {
        //                upper3 = upper2;
        //            }
        //            upper2 = upper1;
        //        }
        //        upper1 = v;
        //    }
        //    else if (v > upper2)
        //    {
        //        if (upper2 > upper3)
        //        {
        //            upper3 = upper2;
        //        }
        //        upper2 = v;
        //    }
        //    else if (v > upper3)
        //    {
        //        upper3 = v;
        //    }
        //}
        //result.value = (upper1 + upper2 + upper3) / 3;
        return result;
    }

}
