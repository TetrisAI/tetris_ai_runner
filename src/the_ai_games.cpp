
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
int game_round;
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
            if(params[2] == "round")
            {
                game_round = std::atoi(params[3].c_str());;
                return true;
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
                    bot_2.status()->combo = enemy_combo;
                    bot_2.status()->point = 0;
                    bot_2.ai_config()->point_ptr = &enemy_point_add;
                    bot_2.run(map2, node2, next_arr, 2, 20);
                }
                bot_1.status()->max_combo = combo;
                bot_1.status()->combo = combo;
                bot_1.status()->max_attack = 0;
                bot_1.status()->attack = 0;
                bot_1.status()->up = (enemy_row_points % 4 + enemy_point_add) / 4 + (game_round % 20 == 0 ? 1 : 0);
                bot_1.status()->land_point = 0;
                bot_1.status()->value = 0;
                target = bot_1.run(map1, node1, next_arr, 2, std::max(50, std::atoi(params[2].c_str()) - 100)).target;
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

#if 0

int main()
{
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

#else

#include <windows.h>

int main()
{
    m_tetris::TetrisEngine<rule_tag::TetrisRule, ai_tag::the_ai_games_rubbish, search_tag::Search> ai2;
    auto &ai = bot_1;
    int add_point;
    ai.prepare(10, 21);
    ai.status()->max_combo = 0;
    ai.status()->combo = 0;
    ai.status()->max_attack = 0;
    ai.status()->attack = 0;
    ai.status()->up = 0;
    ai.status()->land_point = 0;
    ai.status()->value = 0;
    ai2.prepare(10, 21);
    ai2.status()->land_point = 0;
    ai2.status()->up = 0;
    ai2.status()->combo = 0;
    ai2.status()->value = 0;
    bot_2.prepare(10, 21);
    bot_2.status()->combo = 0;
    bot_2.status()->point = 0;
    bot_2.ai_config()->point_ptr = &add_point;


    char out[81920] = "";
    char box_0[3] = "□";
    char box_1[3] = "■";

    CONSOLE_CURSOR_INFO cursorInfo = {1, FALSE};  // 光标信息
    SetConsoleCursorInfo(GetStdHandle(STD_OUTPUT_HANDLE), &cursorInfo);  // 设置光标隐藏

    ege::mtsrand(5);

    auto under_attack = [](auto &map, auto &ai, int line, int hole)
    {
        if(line == 0)
        {
            return;
        }
        int full = 0;
        int w = map.width, h = map.height;
        for(int y = h - 1; y >= line; --y)
        {
            if(map.row[y - line] == ai.context()->full())
            {
                full = y - line + 1;
                break;
            }
            map.row[y] = map.row[y - line];
        }
        uint32_t new_line = ai.context()->full();
        if(hole != -1)
        {
            new_line &= ~(1 << hole);
        }
        for(int y = full; y < line + full; ++y)
        {
            map.row[y] = new_line;
        }
        map.count = 0;
        for(int my = 0; my < map.height; ++my)
        {
            for(int mx = 0; mx < map.width; ++mx)
            {
                if(map.full(mx, my))
                {
                    map.top[mx] = map.roof = my + 1;
                    ++map.count;
                }
            }
        }
    };

    std::vector<char> next;
    m_tetris::TetrisMap map(10, 21);
    m_tetris::TetrisMap map2(10, 21);
    int point = 0, combo = 0;
    int point2 = 0, combo2 = 0;
    int win = 0, win2 = 0;
    int round = 0;
    for(; ; )
    {
        ++round;
        COORD cd;
        cd.X = 0;
        cd.Y = 0;
        SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), cd);

        while(next.size() <= 1)
        {
            next.push_back(ai.context()->generate()->status.t);
        }
        m_tetris::TetrisNode const *node = ai.context()->generate(next.front());
        m_tetris::TetrisNode const *node2 = ai2.context()->generate(next.front());
        m_tetris::TetrisNode const *node3 = bot_2.context()->generate(next.front());
        next.erase(next.begin());
        if(!node->check(map) || map.roof >= map.height)
        {
            ++win2;
            round = 0;
        }
        if(!node2->check(map2) || map2.roof >= map2.height)
        {
            ++win;
            round = 0;
        }
        if(round == 0)
        {
            next.clear();
            map = m_tetris::TetrisMap(10, 21);
            map2 = m_tetris::TetrisMap(10, 21);
            point = 0, combo = 0;
            point2 = 0, combo2 = 0;
            continue;
        }

        out[0] = '\0';
        snprintf(out, sizeof out, "%d\t%d\t%d\t%d\t%d\t%d\r\n", win, point, combo, win2, point2, combo2);
        m_tetris::TetrisMap map_copy = map;
        m_tetris::TetrisMap map_copy2 = map2;
        node->attach(map_copy);
        node->attach(map_copy2);
        for(int y = 21; y >= 0; --y)
        {
            for(int x = 0; x < 10; ++x)
            {
                strcat_s(out, map_copy.full(x, y) ? box_1 : box_0);
            }
            strcat_s(out, "  ");
            for(int x = 0; x < 10; ++x)
            {
                strcat_s(out, map_copy2.full(x, y) ? box_1 : box_0);
            }
            strcat_s(out, "\r\n");
        }
        strcat_s(out, "\r\n");
        printf(out);

        ai.status()->combo = combo;
        next.push_back('?');
        time_t begin = clock();
        add_point = 0;
        bot_2.status()->combo = combo2;
        bot_2.run(map2, node3, next.data(), next.size(), 20);
        ai.status()->up = (point % 4 + add_point) / 4 + (round % 20 == 0 ? 1 : 0);
        ai.status()->combo = combo;
        auto result = ai.run(map, node, next.data(), next.size(), 10000);
        add_point = 0;
        bot_2.status()->combo = combo;
        bot_2.run(map, node3, next.data(), next.size(), 20);
        ai2.status()->up = (point2 % 4 + add_point) / 4 + (round % 20 == 0 ? 1 : 0);
        ai2.status()->combo = combo2;
        auto result2 = ai2.run(map2, node2, next.data(), next.size(), 10000);
        time_t end = clock();
        if(begin + 500 > end)
        {
            Sleep(size_t(begin + 500 - end));
        }
        next.pop_back();
        size_t clear;
        int add_point = 0;
        if(result.target == nullptr)
        {
            clear = 0;
        }
        else
        {
            clear = result.target->attach(map);
        }
        if(clear > 0)
        {
            add_point = combo;
            if(result.target.type == search_tag::Search::TSpin)
            {
                add_point += clear * 6;
            }
            else
            {
                switch(clear)
                {
                case 1: add_point += 1; break;
                case 2: add_point += 3; break;
                case 3: add_point += 6; break;
                case 4: add_point += 12; break;
                }
            }
            ++combo;
        }
        else
        {
            combo = 0;
        }
        size_t clear2;
        int add_point2 = 0;
        if(result2.target == nullptr)
        {
            clear2 = 0;
        }
        else
        {
            clear2 = result2.target->attach(map2);
        }
        if(clear2 > 0)
        {
            add_point2 = combo2;
            if(result2.target.type == search_tag::Search::TSpin)
            {
                add_point2 += clear * 6;
            }
            else
            {
                switch(clear)
                {
                case 1: add_point2 += 1; break;
                case 2: add_point2 += 3; break;
                case 3: add_point2 += 6; break;
                case 4: add_point2 += 12; break;
                }
            }
            ++combo2;
        }
        else
        {
            combo2 = 0;
        }
        under_attack(map, ai, (add_point2 + point2 % 4) / 4, static_cast<int>(ege::mtdrand() * ai.context()->width()));
        under_attack(map2, ai2, (add_point + point % 4) / 4, static_cast<int>(ege::mtdrand() * ai2.context()->width()));
        point += add_point;
        point2 += add_point2;
        if(round % 20 == 0)
        {
            under_attack(map2, ai2, 1, -1);
            under_attack(map, ai, 1, -1);
        }
        //if(round % 3 == 0)
        //{
        //    int hold = static_cast<int>(ege::mtdrand() * ai.context()->width());
        //    under_attack(map2, ai2, 1, hold);
        //    under_attack(map, ai, 1, hold);
        //}
    }
}
#endif