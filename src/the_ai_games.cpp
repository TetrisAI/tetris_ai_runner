
#include <chrono>
#include <thread>
#include <string>
#include <iostream>
#include <functional>
#include "tetris_core.h"
#include "search_tag.h"
#include "ai_tag.h"
#include "rule_tag.h"
#include "random.h"

m_tetris::TetrisEngine<rule_tag::TetrisRule, ai_tag::the_ai_games, search_tag::Search> bot_1;
m_tetris::TetrisEngine<rule_tag::TetrisRule, ai_tag::the_ai_games_enemy, search_tag::Search> bot_2;

namespace zzz
{
    template<size_t N> struct is_key_char
    {
        bool operator()(char c, char const *arr)
        {
            return arr[N - 2] == c || is_key_char<N - 1>()(c, arr);
        }
    };
    template<> struct is_key_char<1U>
    {
        bool operator()(char c, char const *arr)
        {
            return false;
        }
    };
    template<size_t N> void split(std::vector<std::string> &out, std::string const &in, char const (&arr)[N])
    {
        out.clear();
        std::string temp;
        for(auto c : in)
        {
            if(is_key_char<N>()(c, arr))
            {
                if(!temp.empty())
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
        if(!temp.empty())
        {
            out.emplace_back(std::move(temp));
        }
    }
}


int field_width = 0, field_height = 0;
char this_piece = '?', next_piece = '?';
int this_piece_pos_x = 0, this_piece_pos_y = 0;
int row_points = 0, combo = 0, enemy_row_points = 0, enemy_combo = 0;
std::vector<int> field, enemy_field;
std::string my_name;

std::map<std::string, std::function<bool(std::vector<std::string> const &)>> command_map =
{
    {
        "settings", [](std::vector<std::string> const &params)
        {
            if(params.size() < 3)
            {
                return false;
            }
            if(params[1] == "field_width" || params[1] == "field_height")
            {
                if(params[1] == "field_width")
                {
                    field_width = std::atoi(params[2].c_str());
                }
                else
                {
                    field_height = std::atoi(params[2].c_str());
                }
                if(field_width != 0 && field_height != 0)
                {
                    bot_1.prepare(field_width, field_height + 1);
                    bot_2.prepare(field_width, field_height + 1);
                }
                return true;
            }
            else if(params[1] == "your_bot")
            {
                my_name = params[2];
                return true;
            }
            return false;
        }
    },
    {
        "update", [](std::vector<std::string> const &params)
        {
            if(params.size() < 4)
            {
                return false;
            }
            if(params[2] == "this_piece_type")
            {
                this_piece = params[3].front();
                return true;
            }
            else if(params[2] == "next_piece_type")
            {
                next_piece = params[3].front();
                return true;
            }
            else if(params[2] == "this_piece_position")
            {
                std::vector<std::string> token;
                zzz::split(token, params[3], ",");
                if(token.size() < 2)
                {
                    return false;
                }
                this_piece_pos_x = std::atoi(token[0].c_str());
                this_piece_pos_y = std::atoi(token[1].c_str());
                return true;
            }
            else if(params[2] == "row_points")
            {
                if(params[1] == my_name)
                {
                    row_points = std::atoi(params[3].c_str());
                }
                else
                {
                    enemy_row_points = std::atoi(params[3].c_str());
                }
                return true;
            }
            else if(params[2] == "combo")
            {
                if(params[1] == my_name)
                {
                    combo = std::atoi(params[3].c_str());
                }
                else
                {
                    enemy_combo = std::atoi(params[3].c_str());
                }
                return true;
            }
            else if(params[2] == "field")
            {
                std::vector<std::string> token;
                token.reserve(field_width * field_height);
                zzz::split(token, params[3], ",;");
                if(token.size() < size_t(field_width * field_height))
                {
                    return false;
                }
                if(params[1] == my_name)
                {
                    field.resize(token.size());
                    for(size_t i = 0; i < token.size(); ++i)
                    {
                        field[i] = (token[i].length() == 1 && (token[i].front() == '2' || token[i].front() == '3')) ? 1 : 0;
                    }
                }
                else
                {
                    enemy_field.resize(token.size());
                    for(size_t i = 0; i < token.size(); ++i)
                    {
                        enemy_field[i] = (token[i].length() == 1 && (token[i].front() == '2' || token[i].front() == '3')) ? 1 : 0;
                    }
                }
                return true;
            }
            return false;
        }
    },
    {
        "action", [](std::vector<std::string> const &params)
        {
            std::string output;
            if(params.size() < 3 || params[1] != "moves" || !bot_1.prepare(field_width, field_height + 1) || !bot_2.prepare(field_width, field_height + 1))
            {
                output += "no_moves\n";
                std::cout << output;
                return false;
            }
            m_tetris::TetrisMap map1(field_width, field_height + 1), map2(field_width, field_height + 1);
            m_tetris::TetrisBlockStatus status(this_piece, this_piece_pos_x, this_piece_pos_y + map1.height, 0);
            m_tetris::TetrisNode const *node1 = bot_1.get(status), *target = nullptr;
            m_tetris::TetrisNode const *node2 = bot_2.get(status);
            if(node1 == nullptr)
            {
                output += "no_moves\n";
                std::cout << output;
                return true;
            }
            for(int my = 0; my < field_height; ++my)
            {
                for(int mx = 0; mx < field_width; ++mx)
                {
                    if(field[mx + field_width * (field_height - my - 1)] != 0)
                    {
                        map1.top[mx] = map1.roof = my + 1;
                        map1.row[my] |= 1 << mx;
                        ++map1.count;
                    }
                    if(enemy_field[mx + field_width * (field_height - my - 1)] != 0)
                    {
                        map2.top[mx] = map2.roof = my + 1;
                        map2.row[my] |= 1 << mx;
                        ++map2.count;
                    }
                }
            }
            char next_arr[] = {next_piece, '?'};
            std::vector<char> ai_path;
            if(node1 != nullptr)
            {
                int enemy_point_add = 0;
                if(node2 != nullptr)
                {
                    decltype(bot_2)::Status in_status2;
                    in_status2.combo = enemy_combo;
                    in_status2.point = 0;
                    bot_2.ai_config()->point_ptr = &enemy_point_add;
                    bot_2.run(map2, in_status2, node2, next_arr, 2, 20);
                }
                decltype(bot_1)::Status in_status1;
                in_status1.combo = combo;
                in_status1.depth = 0;
                in_status1.land_point = 0;
                in_status1.up = (enemy_row_points % 4 + enemy_point_add) / 4;
                in_status1.value = 0;
                target = bot_1.run(map1, in_status1, node1, next_arr, 2, std::max(50, std::atoi(params[2].c_str()) - 100)).target;
            }
            if(target != nullptr)
            {
                ai_path = bot_1.make_path(node1, target, map1);
            }
            for(auto c : ai_path)
            {
                switch(c)
                {
                case 'l':
                    output += "left,";
                    node1 = node1->move_left;
                    break;
                case 'r':
                    output += "right,";
                    node1 = node1->move_right;
                    break;
                case 'd':
                    output += "down,";
                    node1 = node1->move_down;
                    break;
                case 'L':
                    while(node1->move_left && node1->move_left->check(map1))
                    {
                        output += "left,";
                        node1 = node1->move_left;
                    }
                    break;
                case 'R':
                    while(node1->move_right && node1->move_right->check(map1))
                    {
                        output += "right,";
                        node1 = node1->move_right;
                    }
                    break;
                case 'D':
                    while(node1->move_down && node1->move_down->check(map1))
                    {
                        output += "down,";
                        node1 = node1->move_down;
                    }
                    break;
                case 'z':
                    output += "turnleft,";
                    node1 = node1->rotate_counterclockwise;
                    break;
                case 'c':
                    output += "turnright,";
                    node1 = node1->rotate_clockwise;
                    break;
                }
            }
            output += "drop\n";
            std::cout << output;
            return true;
        }
    },
};


#include <windows.h>
#include "search_tspin.h"
#include "ai_zzz.h"
#include "rule_srsx.h"

m_tetris::TetrisEngine<rule_srsx::TetrisRule, ai_zzz::TOJ, search_tspin::Search> srs_ai;

int main()
{
    srs_ai.prepare(10, 40);
    decltype(srs_ai)::Status in_status;
    in_status.combo = 0;
    in_status.b2b = false;
    in_status.under_attack = 0;
    in_status.value = 0;

    int combo_table[] = {0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 4, 5};
    srs_ai.ai_config()->table = combo_table;
    srs_ai.ai_config()->table_max = 12;

    char out[81920] = "";
    char box_0[3] = "□";
    char box_1[3] = "■";

    CONSOLE_CURSOR_INFO cursorInfo = {1, FALSE};  // 光标信息
    SetConsoleCursorInfo(GetStdHandle(STD_OUTPUT_HANDLE), &cursorInfo);  // 设置光标隐藏

    ege::mtsrand(1);
    
    std::vector<char> next;
    m_tetris::TetrisMap map(10, 21);
    char hold = ' ';
    for(; ; )
    {
        COORD cd;
        cd.X = 0;
        cd.Y = 0;
        SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), cd);

        while(next.size() <= 6)
        {
            next.push_back(srs_ai.context()->generate()->status.t);
        }
        m_tetris::TetrisNode const *node = srs_ai.context()->generate(next.front());
        next.erase(next.begin());
        
        out[0] = '\0';
        m_tetris::TetrisMap map_copy = map;
        node->attach(map_copy);
        for(int y = 21; y >= 0; --y)
        {
            for(int x = 0; x < 10; ++x)
            {
                strcat_s(out, map_copy.full(x, y) ? box_1 : box_0);
            }
            strcat_s(out, "\r\n");
        }
        strcat_s(out, "\r\n");
        printf(out);

        auto result = srs_ai.run_hold(map, in_status, node, hold, true, next.data(), next.size(), 50);
        if(result.change_hold)
        {
            hold = node->status.t;
        }
        size_t clear = 0;
        in_status.b2b = false;
        if(result.target != nullptr)
        {
            clear = result.target->attach(map);
            if(result.target.type != ai_zzz::TOJ::TSpinType::None || clear == 4)
            {
                in_status.b2b = true;
            }
        }
        if(clear > 0)
        {
            ++in_status.combo;
        }
        else
        {
            in_status.combo = 0;
        }
    }
    while(true)
    {
        std::string line;
        std::getline(std::cin, line);
        std::vector<std::string> token;
        zzz::split(token, line, " ");
        if(token.empty())
        {
            continue;
        }
        auto find = command_map.find(token.front());
        if(find == command_map.end())
        {
            continue;
        }
        if(find->second(token))
        {
            std::cout.flush();
        }
    }
}