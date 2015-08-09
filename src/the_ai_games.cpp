
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
            m_tetris::TetrisNode const *node1 = bot_1.get(status);
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
            ai_tag::the_ai_games::TetrisNodeEx target;
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
            if(target.type == ai_tag::the_ai_games::TSpinType::None)
            {
                output += "drop\n";
            }
            else
            {
                output.back() = '\n';
            }
            std::cout << output;
            return true;
        }
    },
};

#if !defined(RANK_MODE)

int main()
{
    ai_tag::the_ai_games::Config config =
    {
        /*map_low_width       = */128.000000 ,
        /*col_trans_width     = */170.000000 ,
        /*row_trans_width     = */128.000000 ,
        /*hold_count_width    = */80.000000  ,
        /*hold_focus_width    = */400.000000 ,
        /*well_depth_width    = */100.000000 ,
        /*hole_depth_width    = */40.000000  ,
        /*dig_clear_width     = */33.000000  ,
        /*line_clear_width    = */40.000000  ,
        /*tspin_clear_width   = */4096.000000,
        /*tetris_clear_width  = */4096.000000,
        /*tspin_build_width   = */4.000000   ,
        /*combo_add_width     = */56.000000  ,
        /*combo_break_minute  = */64.000000  ,
    };
    *bot_1.ai_config() = config;
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

#define NOMINMAX
#include <windows.h>
#include <fstream>
#include <mutex>
#include <random>
#include <atomic>
#include "sb_tree.h"

struct test_ai
{
    m_tetris::TetrisEngine<rule_tag::TetrisRule, ai_tag::the_ai_games, search_tag::Search> ai;
    m_tetris::TetrisEngine<rule_tag::TetrisRule, ai_tag::the_ai_games_enemy, search_tag::Search> t;
    m_tetris::TetrisMap map;
    int point = 0, combo = 0;
    int win = 0, add_point;
    int attack;
    ege::mtrandom r1, r2;
    std::vector<char> next;
    void init(uint32_t seed)
    {
        r1.reset(seed);
        r2.reset(seed);
        map = m_tetris::TetrisMap(10, 21);
        ai.prepare(10, 21);
        ai.status()->max_combo = 0;
        ai.status()->combo = 0;
        ai.status()->max_attack = 0;
        ai.status()->attack = 0;
        ai.status()->up = 0;
        ai.status()->land_point = 0;
        ai.status()->value = 0;
        t.prepare(10, 21);
        t.status()->combo = 0;
        t.status()->point = 0;
        t.ai_config()->point_ptr = &add_point;
    }
    void reset()
    {
        r2.reset(r1.rand());
        map = m_tetris::TetrisMap(10, 21);
        point = 0, combo = 0;
    }
    m_tetris::TetrisNode const *node() const
    {
        return ai.context()->generate(next.front());
    }
    bool prepare()
    {
        if(!next.empty())
        {
            next.erase(next.begin());
        }
        while(next.size() <= 1)
        {
            next.push_back(ai.context()->convert(static_cast<size_t>(r1.real() * 7)));
        }
        return !ai.context()->generate(next.front())->check(map) || map.roof >= map.height;
    }
    void run(int enemy_combo, int enemy_point, int round, m_tetris::TetrisMap const &enemy_map)
    {
        char current = next.front();
        add_point = 0;
        t.status()->combo = enemy_combo;
        next.push_back('?');
        t.run(enemy_map, t.context()->generate(current), next.data() + 1, next.size() - 1, 20);
        ai.status()->up = (enemy_point % 4 + add_point) / 4 + (round % 20 == 0 ? 1 : 0);
        ai.status()->combo = combo;
        auto result = ai.run(map, ai.context()->generate(current), next.data() + 1, next.size() - 1, 10000);
        next.pop_back();
        size_t clear;
        int new_point = 0;
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
            new_point = combo;
            if(result.target.type == search_tag::Search::TSpin)
            {
                new_point += clear * 6;
            }
            else
            {
                switch(clear)
                {
                case 1: new_point += 1; break;
                case 2: new_point += 3; break;
                case 3: new_point += 6; break;
                case 4: new_point += 12; break;
                }
            }
            ++combo;
        }
        else
        {
            combo = 0;
        }
        attack = (point % 4 + new_point) / 4;
        point += new_point;
    }
    void under_attack(int line, int hole)
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
        for(int y = full; y < line + full; ++y)
        {
            uint32_t new_line = ai.context()->full();
            if(hole != -1)
            {
                new_line &= ~(1 << static_cast<int>(r2.real() * ai.context()->width()));
            }
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
    }
};

void match(test_ai &ai1, test_ai &ai2, std::function<void(test_ai const &, test_ai const &)> out_put)
{
    int round = 0;
    for(; ; )
    {
        ++round;
        if(ai1.prepare())
        {
            ++ai2.win;
            round = 0;
        }
        if(ai2.prepare())
        {
            ++ai1.win;
            round = 0;
        }
        if(round == 0)
        {
            return;
        }
        if(out_put)
        {
            out_put(ai1, ai2);
        }

        m_tetris::TetrisMap map_copy1 = ai1.map;
        m_tetris::TetrisMap map_copy2 = ai2.map;
        int point1 = ai1.point, combo1 = ai1.combo;
        int point2 = ai2.point, combo2 = ai2.combo;

        ai1.run(combo2, point2, round, map_copy2);
        ai2.run(combo1, point1, round, map_copy2);

        ai1.under_attack(ai2.attack, 0);
        ai2.under_attack(ai1.attack, 0);
        if(round % 20 == 0)
        {
            ai1.under_attack(1, -1);
            ai2.under_attack(1, -1);
        }
    }
}

double elo_init()
{
    return 1500;
}
double elo_rate(double const &self_score, double const &other_score)
{
    return 1 / (1 + std::pow(10, -(self_score - other_score) / 400));
}
double elo_get_k()
{
    return 4;
}
double elo_calc(double const &self_score, double const &other_score, double const &win)
{
    return self_score + elo_get_k() * (win - elo_rate(self_score, other_score));
}
double elo_calc(double const &self_score, double const *other_score_array, size_t length, double const &win)
{
    double rate = 0;
    for(size_t i = 0; i < length; ++i)
    {
        rate += elo_rate(self_score, other_score_array[i]);
    }
    return self_score + elo_get_k() * (win - rate) / length;
}


struct BaseNode
{
    BaseNode *parent, *left, *right;
    size_t size : sizeof(size_t) * 8 - 1;
    size_t is_nil : 1;
};

struct NodeData
{
    char name[64];
    double score;
    size_t match;
    ai_tag::the_ai_games::Config config;
};

struct Node : public BaseNode
{
    Node(NodeData const &d) : data(d)
    {
    }
    NodeData data;
};

struct SBTreeInterface
{
    typedef double key_t;
    typedef BaseNode node_t;
    typedef Node value_node_t;
    static key_t const &get_key(Node *node)
    {
        return node->data.score;
    }
    static bool is_nil(BaseNode *node)
    {
        return node->is_nil;
    }
    static void set_nil(BaseNode *node, bool nil)
    {
        node->is_nil = nil;
    }
    static BaseNode *get_parent(BaseNode *node)
    {
        return node->parent;
    }
    static void set_parent(BaseNode *node, BaseNode *parent)
    {
        node->parent = parent;
    }
    static BaseNode *get_left(BaseNode *node)
    {
        return node->left;
    }
    static void set_left(BaseNode *node, BaseNode *left)
    {
        node->left = left;
    }
    static BaseNode *get_right(BaseNode *node)
    {
        return node->right;
    }
    static void set_right(BaseNode *node, BaseNode *right)
    {
        node->right = right;
    }
    static size_t get_size(BaseNode *node)
    {
        return node->size;
    }
    static void set_size(BaseNode *node, size_t size)
    {
        node->size = size;
    }
    static bool predicate(key_t const &left, key_t const &right)
    {
        return left > right;
    }
};

int wmain(int argc, wchar_t const *argv[])
{
    //ai_tag::the_ai_games a;
    //m_tetris::TetrisMap map(10, 21);
    //map.roof = 3;
    //map.row[0] = 0B1111111101;
    //map.row[1] = 0B1111111000;
    //map.row[2] = 0B0000000101;
    //a.map_for_tspin_(map, 1, 0);
    std::atomic_uint32_t count = std::max<uint32_t>(1, std::thread::hardware_concurrency() - 1);
    std::wstring file = L"data.bin";
    if(argc > 1)
    {
        uint32_t arg_count = std::wcstoul(argv[1], nullptr, 10);
        if(arg_count != 0)
        {
            count = arg_count;
        }
    }
    std::atomic_bool view = false;
    std::atomic_uint32_t view_index = 0;
    if(argc > 2)
    {
        file = argv[2];
    }
    std::recursive_mutex rank_table_lock;
    zzz::sb_tree<SBTreeInterface> rank_table;
    std::ifstream ifs(file, std::ios::in | std::ios::binary);
    if(ifs.good())
    {
        NodeData data;
        while(ifs.read(reinterpret_cast<char *>(&data), sizeof data).gcount() == sizeof data)
        {
            rank_table.insert(new Node(data));
        }
        ifs.close();
    }
    if(rank_table.empty())
    {
        NodeData default_node =
        {
            "default", elo_init(), 0,
            {
                /*map_low_width       = */128.000000 ,
                /*col_trans_width     = */170.000000 ,
                /*row_trans_width     = */128.000000 ,
                /*hold_count_width    = */80.000000  ,
                /*hold_focus_width    = */400.000000 ,
                /*well_depth_width    = */100.000000 ,
                /*hole_depth_width    = */40.000000  ,
                /*dig_clear_width     = */33.000000  ,
                /*line_clear_width    = */40.000000  ,
                /*tspin_clear_width   = */4096.000000,
                /*tetris_clear_width  = */4096.000000,
                /*tspin_build_width   = */4.000000   ,
                /*combo_add_width     = */56.000000  ,
                /*combo_break_minute  = */64.000000  ,
            }
        };
        rank_table.insert(new Node(default_node));
        rank_table.insert(new Node(default_node));
    }

    std::vector<std::thread *> threads;
    for(size_t i = 0; i < count; ++i)
    {
        std::thread *t = new std::thread([&rank_table, &rank_table_lock, &view, &view_index, i]()
        {
            std::mt19937 mt;
            uint32_t index = i + 1;
            auto rand_match = [&](size_t max)
            {
                std::pair<size_t, size_t> ret;
                ret.first = std::uniform_int_distribution<size_t>(0, max - 1)(mt);
                do
                {
                    ret.second = std::uniform_int_distribution<size_t>(0, max - 1)(mt);
                }
                while(ret.second == ret.first);
                return ret;
            };
            test_ai ai1;
            test_ai ai2;
            ai1.init(index);
            ai2.init(index + 1);
            for(; ; )
            {
                rank_table_lock.lock();
                auto m12 = rand_match(rank_table.size());
                auto m1 = rank_table.at(m12.first);
                auto m2 = rank_table.at(m12.second);
                double *pm1s = &m1->data.score;
                double *pm2s = &m2->data.score;
                double m1s = *pm1s;
                double m2s = *pm2s;
                auto view_func = [m1, m2, m1s, m2s, index, &view, &view_index, &rank_table_lock](test_ai const &ai1, test_ai const &ai2)
                {
                    COORD coordScreen = {0, 0};
                    DWORD cCharsWritten;
                    CONSOLE_SCREEN_BUFFER_INFO csbi;
                    DWORD dwConSize;
                    HANDLE hConsole;
                    if(view && view_index == 0)
                    {
                        rank_table_lock.lock();
                        if(view && view_index == 0)
                        {
                            view_index = index;
                            hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
                            GetConsoleScreenBufferInfo(hConsole, &csbi);
                            dwConSize = csbi.dwSize.X * csbi.dwSize.Y;
                            FillConsoleOutputCharacterA(hConsole, ' ', dwConSize, coordScreen, &dwConSize);
                        }
                        rank_table_lock.unlock();
                    }
                    if(index != view_index)
                    {
                        return;
                    }
                    Sleep(333);

                    hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

                    GetConsoleScreenBufferInfo(hConsole, &csbi);
                    dwConSize = csbi.dwSize.X * 2;
                    FillConsoleOutputCharacterA(hConsole, ' ', dwConSize, coordScreen, &dwConSize);
                    GetConsoleScreenBufferInfo(hConsole, &csbi);
                    FillConsoleOutputAttribute(hConsole, csbi.wAttributes, dwConSize, coordScreen, &cCharsWritten);
                    SetConsoleCursorPosition(hConsole, coordScreen);

                    char out[81920] = "";
                    char box_0[3] = "□";
                    char box_1[3] = "■";

                    out[0] = '\0';
                    snprintf(out, sizeof out, "score = %f point = %d combo = %d [%s]\nscore = %f point = %d combo = %d [%s]\n", m1s, ai1.point, ai1.combo, m1->data.name, m2s, ai2.point, ai2.combo, m2->data.name);
                    m_tetris::TetrisMap map_copy1 = ai1.map;
                    m_tetris::TetrisMap map_copy2 = ai2.map;
                    ai1.node()->attach(map_copy1);
                    ai2.node()->attach(map_copy2);
                    for(int y = 21; y >= 0; --y)
                    {
                        for(int x = 0; x < 10; ++x)
                        {
                            strcat_s(out, map_copy1.full(x, y) ? box_1 : box_0);
                        }
                        strcat_s(out, "  ");
                        for(int x = 0; x < 10; ++x)
                        {
                            strcat_s(out, map_copy2.full(x, y) ? box_1 : box_0);
                        }
                        strcat_s(out, "\r\n");
                    }
                    WriteConsoleA(hConsole, out, strlen(out), nullptr, nullptr);
                };
                rank_table_lock.unlock();
                *ai1.ai.ai_config() = m1->data.config;
                *ai2.ai.ai_config() = m2->data.config;
                ai1.win = 0;
                ai2.win = 0;
                match(ai1, ai2, view_func);
                ai1.reset();
                ai2.reset();
                rank_table_lock.lock();
                rank_table.erase(m1);
                rank_table.erase(m2);
                m1s = *pm1s;
                m2s = *pm2s;
                if(ai1.win == 1 && ai2.win == 1)
                {
                    m1->data.score = elo_calc(m1s, m2s, 0.5);
                    m2->data.score = elo_calc(m2s, m1s, 0.5);
                }
                else
                {
                    if(ai1.win == 1)
                    {
                        m1->data.score = elo_calc(m1s, m2s, 1);
                        m2->data.score = elo_calc(m2s, m1s, 0);
                    }
                    else
                    {
                        m1->data.score = elo_calc(m1s, m2s, 0);
                        m2->data.score = elo_calc(m2s, m1s, 1);
                    }
                }
                ++m1->data.match;
                ++m2->data.match;
                rank_table.insert(m1);
                rank_table.insert(m2);
                rank_table_lock.unlock();
            }
        });
    }
    Node *edit = nullptr;
    auto print_config = [&rank_table, &rank_table_lock](Node *node)
    {
        rank_table_lock.lock();
        printf(
            "[ 0]name                = %s\n"
            "[  ]rank                = %d\n"
            "[  ]score               = %f\n"
            "[  ]match               = %d\n"
            "[ 1]map_low_width       = %f\n"
            "[ 2]col_trans_width     = %f\n"
            "[ 3]row_trans_width     = %f\n"
            "[ 4]hold_count_width    = %f\n"
            "[ 5]hold_focus_width    = %f\n"
            "[ 6]well_depth_width    = %f\n"
            "[ 7]hole_depth_width    = %f\n"
            "[ 8]dig_clear_width     = %f\n"
            "[ 9]line_clear_width    = %f\n"
            "[10]tspin_clear_width   = %f\n"
            "[11]tetris_clear_width  = %f\n"
            "[12]tspin_build_width   = %f\n"
            "[13]combo_add_width     = %f\n"
            "[14]combo_break_minute  = %f\n"
            , node->data.name
            , rank_table.rank(node->data.score)
            , node->data.score
            , node->data.match
            , node->data.config.map_low_width
            , node->data.config.col_trans_width
            , node->data.config.row_trans_width
            , node->data.config.hold_count_width
            , node->data.config.hold_focus_width
            , node->data.config.well_depth_width
            , node->data.config.hole_depth_width
            , node->data.config.dig_clear_width
            , node->data.config.line_clear_width
            , node->data.config.tspin_clear_width
            , node->data.config.tetris_clear_width
            , node->data.config.tspin_build_width
            , node->data.config.combo_add_width
            , node->data.config.combo_break_minute
            );
        rank_table_lock.unlock();
    };
 
    command_map.clear();
    command_map.insert(std::make_pair("select", [&edit, &print_config, &rank_table, &rank_table_lock](std::vector<std::string> const &token)
    {
        if(token.size() == 2)
        {
            size_t index = std::atoi(token[1].c_str()) - 1;
            if(index < rank_table.size())
            {
                rank_table_lock.lock();
                edit = rank_table.at(index);
                print_config(edit);
                rank_table_lock.unlock();
            }
        }
        return true;
    }));
    command_map.insert(std::make_pair("set", [&edit, &print_config, &rank_table, &rank_table_lock](std::vector<std::string> const &token)
    {
        if(token.size() == 3 && edit != nullptr)
        {
            size_t index = std::atoi(token[1].c_str());
            if(index > 14 || (index == 0 && token[2].size() >= 64))
            {
                return true;
            }
            rank_table_lock.lock();
            if(index == 0)
            {
                memcpy(edit->data.name, token[2].c_str(), token[2].size() + 1);
            }
            else
            {
                double value = std::atof(token[2].c_str());
                double *ptr = reinterpret_cast<double *>(&edit->data.config);
                ptr[index - 1] = value;
            }
            print_config(edit);
            rank_table_lock.unlock();
        }
        return true;
    }));
    command_map.insert(std::make_pair("copy", [&edit, &print_config, &rank_table, &rank_table_lock](std::vector<std::string> const &token)
    {
        if(token.size() == 2 && token[1].size() < 64 && edit != nullptr)
        {
            rank_table_lock.lock();
            NodeData data = edit->data;
            memcpy(data.name, token[1].c_str(), token[1].size() + 1);
            data.match = 0;
            data.score = elo_init();
            Node *node = new Node(data);
            rank_table.insert(node);
            print_config(node);
            edit = node;
            rank_table_lock.unlock();
        }
        return true;
    }));
    command_map.insert(std::make_pair("rank", [&rank_table, &rank_table_lock](std::vector<std::string> const &token)
    {
        printf("-------------------------------------------------------------\n");
        rank_table_lock.lock();
        size_t begin = 0, end = rank_table.size();
        if(token.size() == 2)
        {
            begin = std::atoi(token[1].c_str()) - 1;
            end = begin + 1;
        }
        if(token.size() == 3)
        {
            begin = std::atoi(token[1].c_str()) - 1;
            end = begin + std::atoi(token[2].c_str());
        }
        for(size_t i = begin; i < end && i < rank_table.size(); ++i)
        {
            auto node = rank_table.at(i);
            printf("[%s]\nr=%d\ts=%f\tm=%d\n", node->data.name, i + 1, node->data.score, node->data.match);
        }
        rank_table_lock.unlock();
        printf("-------------------------------------------------------------\n");
        return true;
    }));
    command_map.insert(std::make_pair("view", [&view](std::vector<std::string> const &token)
    {
        view = true;
        return true;
    }));
    command_map.insert(std::make_pair("save", [&file, &rank_table, &rank_table_lock](std::vector<std::string> const &token)
    {
        rank_table_lock.lock();
        std::ofstream ofs(file, std::ios::out | std::ios::binary);
        for(size_t i = 0; i < rank_table.size(); ++i)
        {
            ofs.write(reinterpret_cast<char const *>(&rank_table.at(i)->data), sizeof rank_table.at(i)->data);
        }
        ofs.flush();
        ofs.close();
        printf("%d node(s) saved\n", rank_table.size());
        rank_table_lock.unlock();
        return true;
    }));
    command_map.insert(std::make_pair("exit", [&file, &rank_table, &rank_table_lock](std::vector<std::string> const &token)
    {
        rank_table_lock.lock();
        std::ofstream ofs(file, std::ios::out | std::ios::binary);
        for(size_t i = 0; i < rank_table.size(); ++i)
        {
            ofs.write(reinterpret_cast<char const *>(&rank_table.at(i)->data), sizeof rank_table.at(i)->data);
        }
        ofs.flush();
        ofs.close();
        rank_table_lock.unlock();
        exit(0);
        return true;
    }));
    command_map.insert(std::make_pair("help", [](std::vector<std::string> const &token)
    {
        printf(
            "-------------------------------------------------------------\n"
            "help                 - ...\n"
            "view                 - view a match (press enter to stop)\n"
            "rank                 - show all nodes\n"
            "rank [rank]          - show a node on the rank\n"
            "rank [rank] [length] - show nodes on the rank\n"
            "select [rank]        - select a node and view info\n"
            "set [index] [value]  - set node name or config which last selected\n"
            "copy [name]          - copy a new node which last selected\n"
            "save                 - ...\n"
            "exit                 - save & exit\n"
            "-------------------------------------------------------------\n"
            );
        return true;
    }));
    while(true)
    {
        std::string line;
        std::getline(std::cin, line);
        if(view)
        {
            view = false;
            view_index = 0;
            continue;
        }
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
#endif