
#include <chrono>
#include <thread>
#include <string>
#include <iostream>
#include "tetris_core.h"
#include "land_point_search_path.h"
#include "ai_ax.h"
#include "rule_tag.h"
#include "random.h"

m_tetris::TetrisEngine<rule_tag::TetrisRuleSet, ai_ax::AI, land_point_search_path::Search> bot_1;

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
int row_points = 0, combo = 0;
std::vector<int> field;
std::string my_name;

std::map<std::string, std::function<void(std::vector<std::string> const &)>> command_map =
{
    {
        "settings", [](std::vector<std::string> const &params)
        {
            if(params.size() < 3)
            {
                return;
            }
            if(params[1] == "field_width")
            {
                field_width = std::atoi(params[2].c_str());
            }
            else if(params[1] == "field_height")
            {
                field_height = std::atoi(params[2].c_str());
            }
            else if(params[1] == "your_bot")
            {
                my_name = params[2];
            }
        }
    },
    {
        "update", [](std::vector<std::string> const &params)
        {
            if(params.size() < 4)
            {
                return;
            }
            if(params[1] != my_name && params[1] != "game")
            {
                return;
            }
            if(params[2] == "this_piece_type")
            {
                this_piece = params[3].front();
            }
            else if(params[2] == "next_piece_type")
            {
                next_piece = params[3].front();
            }
            else if(params[2] == "this_piece_position")
            {
                std::vector<std::string> token;
                zzz::split(token, params[3], ",");
                if(token.size() < 2)
                {
                    return;
                }
                this_piece_pos_x = std::atoi(token[0].c_str());
                this_piece_pos_y = std::atoi(token[1].c_str());
            }
            else if(params[2] == "row_points")
            {
                row_points = std::atoi(params[3].c_str());
            }
            else if(params[2] == "combo")
            {
                combo = std::atoi(params[3].c_str());
            }
            else if(params[2] == "field")
            {
                std::vector<std::string> token;
                token.reserve(field_width * field_height);
                zzz::split(token, params[3], ",;");
                if(token.size() < size_t(field_width * field_height))
                {
                    return;
                }
                field.resize(token.size());
                for(size_t i = 0; i < token.size(); ++i)
                {
                    field[i] = (token[i].length() == 1 && token[i].front() == '0') ? 0 : 1;
                }
            }
        }
    },
    {
        "action", [](std::vector<std::string> const &params)
        {
            std::string output;
            if(!bot_1.prepare(field_width, field_height + 1))
            {
                output += "no_moves\n";
                std::cout << output;
                return;
            }
            m_tetris::TetrisMap map(field_width, field_height + 1);
            m_tetris::TetrisBlockStatus status =
            {
                this_piece, this_piece_pos_x - 1, this_piece_pos_y + map.height, 0
            };
            m_tetris::TetrisNode const *node = bot_1.get(status), *target = nullptr;
            if(node == nullptr)
            {
                output += "no_moves\n";
                std::cout << output;
                return;
            }
            for(int my = 0; my < field_height; ++my)
            {
                for(int mx = 0; mx < field_width; ++mx)
                {
                    if(field[mx + field_width * (field_height - my - 1)] != 0)
                    {
                        if(my < node->row || my >= node->row + node->height || ((node->data[my - node->row] >> mx) & 1) == 0)
                        {
                            map.top[mx] = map.roof = my + 1;
                            map.row[my] |= 1 << mx;
                            ++map.count;
                        }
                    }
                }
            }
            unsigned char next_arr[] = {next_piece};
            std::vector<char> ai_path;
            if(node != nullptr)
            {
                target = bot_1.run(map, node, next_arr, 1, 49).target;
            }
            if(target != nullptr)
            {
                ai_path = bot_1.make_path(node, target, map);
            }
            for(auto c : ai_path)
            {
                switch(c)
                {
                case 'l':
                    output += "left,";
                    node = node->move_left;
                    break;
                case 'r':
                    output += "right,";
                    node = node->move_right;
                    break;
                case 'd':
                    output += "down,";
                    node = node->move_down;
                    break;
                case 'L':
                    while(node->move_left && node->move_left->check(map))
                    {
                        output += "left,";
                        node = node->move_left;
                    }
                    break;
                case 'R':
                    while(node->move_right && node->move_right->check(map))
                    {
                        output += "right,";
                        node = node->move_right;
                    }
                    break;
                case 'D':
                    while(node->move_down && node->move_down->check(map))
                    {
                        output += "down,";
                        node = node->move_down;
                    }
                    break;
                case 'z':
                    output += "turnleft,";
                    node = node->rotate_counterclockwise;
                    break;
                case 'c':
                    output += "turnright,";
                    node = node->rotate_clockwise;
                    break;
                }
            }
            output += "drop\n";
            std::cout << output;
        }
    },
};

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
        find->second(token);
    }
}