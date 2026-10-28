#include <chrono>
#include <thread>
#include <string>
#include <iostream>
#include <functional>
#include <array>
#include "tetris_core.h"
#include "search_tag.h"
#include "search_tspin.h"
#include "ai_tag.h"
#include "rule_tag.h"
#include "random.h"

m_tetris2::TetrisEngine<rule_tag::TetrisRule, ai_tag::the_ai_games, tag::Search> bot_1;
m_tetris2::TetrisEngine<rule_tag::TetrisRule, ai_tag::the_ai_games_enemy, tag::Search> bot_2;

namespace zzz
{
    template<size_t N>
    struct is_key_char
    {
        bool operator()(char c, char const *arr)
        {
            return arr[N - 2] == c || is_key_char<N - 1>()(c, arr);
        }
    };
    template<>
    struct is_key_char<1U>
    {
        bool operator()(char c, char const *arr)
        {
            return false;
        }
    };
    template<size_t N>
    void split(std::vector<std::string> &out, std::string const &in, char const (&arr)[N])
    {
        out.clear();
        std::string temp;
        for (auto c : in)
        {
            if (is_key_char<N>()(c, arr))
            {
                if (!temp.empty())
                {
                    out.emplace_back(std::move(temp));
                    temp.clear();
                }
            }
            else
            {
                temp += c;
            }
        }
        if (!temp.empty())
        {
            out.emplace_back(std::move(temp));
        }
    }
}

int field_width = 0, field_height = 0;
char this_piece = '?', next_piece = '?';
int this_piece_pos_x = 0, this_piece_pos_y = 0;
int row_points = 0, combo = 0, enemy_row_points = 0, enemy_combo = 0;
int game_round;
std::vector<int> field, enemy_field;
std::string my_name;

std::map<std::string, std::function<bool(std::vector<std::string> const &)>> command_map =
    {
        {"settings", [](std::vector<std::string> const &params)
         {
             if (params.size() < 3)
             {
                 return false;
             }
             if (params[1] == "field_width" || params[1] == "field_height")
             {
                 if (params[1] == "field_width")
                 {
                     field_width = std::atoi(params[2].c_str());
                 }
                 else
                 {
                     field_height = std::atoi(params[2].c_str());
                 }
                 if (field_width != 0 && field_height != 0)
                 {
                     bot_1.prepare(field_width, field_height + 1);
                     bot_2.prepare(field_width, field_height + 1);
                 }
                 return true;
             }
             else if (params[1] == "your_bot")
             {
                 my_name = params[2];
                 return true;
             }
             return false;
         }},
        {"update", [](std::vector<std::string> const &params)
         {
             if (params.size() < 4)
             {
                 return false;
             }
             if (params[2] == "round")
             {
                 game_round = std::atoi(params[3].c_str());
                 ;
                 return true;
             }
             if (params[2] == "this_piece_type")
             {
                 this_piece = params[3].front();
                 return true;
             }
             else if (params[2] == "next_piece_type")
             {
                 next_piece = params[3].front();
                 return true;
             }
             else if (params[2] == "this_piece_position")
             {
                 std::vector<std::string> token;
                 zzz::split(token, params[3], ",");
                 if (token.size() < 2)
                 {
                     return false;
                 }
                 this_piece_pos_x = std::atoi(token[0].c_str());
                 this_piece_pos_y = std::atoi(token[1].c_str());
                 return true;
             }
             else if (params[2] == "row_points")
             {
                 if (params[1] == my_name)
                 {
                     row_points = std::atoi(params[3].c_str());
                 }
                 else
                 {
                     enemy_row_points = std::atoi(params[3].c_str());
                 }
                 return true;
             }
             else if (params[2] == "combo")
             {
                 if (params[1] == my_name)
                 {
                     combo = std::atoi(params[3].c_str());
                 }
                 else
                 {
                     enemy_combo = std::atoi(params[3].c_str());
                 }
                 return true;
             }
             else if (params[2] == "field")
             {
                 std::vector<std::string> token;
                 token.reserve(field_width * field_height);
                 zzz::split(token, params[3], ",;");
                 if (token.size() < size_t(field_width * field_height))
                 {
                     return false;
                 }
                 if (params[1] == my_name)
                 {
                     field.resize(token.size());
                     for (size_t i = 0; i < token.size(); ++i)
                     {
                         field[i] = (token[i].length() == 1 && (token[i].front() == '2' || token[i].front() == '3')) ? 1 : 0;
                     }
                 }
                 else
                 {
                     enemy_field.resize(token.size());
                     for (size_t i = 0; i < token.size(); ++i)
                     {
                         enemy_field[i] = (token[i].length() == 1 && (token[i].front() == '2' || token[i].front() == '3')) ? 1 : 0;
                     }
                 }
                 return true;
             }
             return false;
         }},
        {"action", [](std::vector<std::string> const &params)
         {
             std::string output;
             if (params.size() < 3 || params[1] != "moves" || !bot_1.prepare(field_width, field_height + 1) || !bot_2.prepare(field_width, field_height + 1))
             {
                 output += "no_moves\n";
                 std::cout << output;
                 return false;
             }
             decltype(bot_1)::map_t map1{}, map2{};
             m_tetris2::TetrisBlockStatus status(this_piece, this_piece_pos_x, this_piece_pos_y + map1.height, 0);
             for (int my = 0; my < field_height; ++my)
             {
                 for (int mx = 0; mx < field_width; ++mx)
                 {
                    if (field[mx + field_width * (field_height - my - 1)] != 0)
                    {
                        map1.set(mx, my);
                    }
                    if (enemy_field[mx + field_width * (field_height - my - 1)] != 0)
                    {
                        map2.set(mx, my);
                    }
                 }
             }
             char next_arr[] = {next_piece, '?'};
             std::vector<char> ai_path;
             decltype(bot_1)::LandPoint target;
             int enemy_point_calc = 0;
             int up[4];
             bot_2.status()->combo = enemy_combo;
             bot_2.status()->point = 0;
             {
                 // point_ptr/up_ptr 指向本帧栈上局部；enemy get 写穿它们是输出，不是 eval 输入。
                 auto cfg = *bot_2.ai_config();
                 cfg.point_ptr = &enemy_point_calc;
                 cfg.up_ptr = up;
                 bot_2.set_ai_config(cfg);
             }
             bot_2.run(map2, status, next_arr, 2, 20);
             bot_1.status()->max_combo = combo;
             bot_1.status()->combo = combo;
             bot_1.status()->max_attack = 0;
             bot_1.status()->attack = 0;
             bot_1.status()->up[0] = (enemy_row_points % 4 + up[0]) / 4 + ((game_round + 0) % 20 == 0 ? 1 : 0);
             bot_1.status()->up[1] = (enemy_row_points % 4 + up[1]) / 4 + ((game_round + 1) % 20 == 0 ? 1 : 0);
             bot_1.status()->up[2] = (enemy_row_points % 4 + up[2]) / 4 + ((game_round + 2) % 20 == 0 ? 1 : 0);
             bot_1.status()->land_point = 0;
             bot_1.status()->value = 0;
             target = bot_1.run(map1, status, next_arr, 2, std::max(50, std::atoi(params[2].c_str()) - 100)).target;
             if (target == nullptr)
             {
                 output += "no_moves\n";
                 std::cout << output;
                 return true;
             }
             ai_path = bot_1.make_path(status, target, map1);

             for (auto c : ai_path)
             {
                 switch (c)
                 {
                 case 'l':
                 case 'L':
                     output += "left,";
                     break;
                 case 'r':
                 case 'R':
                     output += "right,";
                     break;
                 case 'd':
                 case 'D':
                     output += "down,";
                     break;
                 case 'z':
                     output += "turnleft,";
                     break;
                 case 'c':
                     output += "turnright,";
                     break;
                 }
             }
             if (target.type == ai_tag::the_ai_games::TSpinType::None)
             {
                 output += "drop\n";
             }
             else
             {
                 output.back() = '\n';
             }
             std::cout << output;
             return true;
         }},
};


int main()
{
    ai_tag::the_ai_games::Config config =
        {
            /*map_low_width       = */ 128.000000,
            /*col_trans_width     = */ 170.000000,
            /*row_trans_width     = */ 128.000000,
            /*hold_count_width    = */ 80.000000,
            /*hold_focus_width    = */ 400.000000,
            /*well_depth_width    = */ 100.000000,
            /*hole_depth_width    = */ 40.000000,
            /*dig_clear_width     = */ 36.000000,
            /*line_clear_width    = */ 80.000000,
            /*tspin_clear_width   = */ 800.000000,
            /*tetris_clear_width  = */ 4096.000000,
            /*tspin_build_width   = */ 2.400000,
            /*combo_add_width     = */ 56.000000,
            /*combo_break_minute  = */ 64.000000,
        };
    bot_1.set_ai_config(config);
    while (true)
    {
        std::string line;
        std::getline(std::cin, line);
        std::vector<std::string> token;
        zzz::split(token, line, " ");
        if (token.empty())
        {
            continue;
        }
        auto find = command_map.find(token.front());
        if (find == command_map.end())
        {
            continue;
        }
        if (find->second(token))
        {
            std::cout.flush();
        }
    }
}

