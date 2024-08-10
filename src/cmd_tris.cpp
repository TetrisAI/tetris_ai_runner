
#define _CRT_SECURE_NO_WARNINGS
#include <ctime>
#include <mutex>
#include <fstream>
#include <thread>
#include <array>
#include <random>
#include <sstream>
#include <iostream>
#include <chrono>
#include <queue>
#include <atomic>

#include "tetris_core.h"
#include "search_aspin.h"
#include "ai_zzz.h"
#include "rule_botris.h"
#include "sb_tree.h"
#include "integer_utils.h"

#if _MSC_VER
#define NOMINMAX
#include <windows.h>
#else
#include <unistd.h>
#endif


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

using Engine = m_tetris::TetrisEngine<rule_botris::TetrisRule, ai_zzz::Botris, search_aspin::Search>;

struct test_ai
{
    Engine ai;
    m_tetris::TetrisMap map;
    m_tetris::TetrisNode const *curr;
    std::mt19937 r_next, r_garbage;
    size_t next_length;
    size_t run_ms;
    int const *combo_table;
    int combo_table_max;
    size_t round;
    std::vector<char> next;
    std::deque<int> recv_attack;
    int send_attack;
    int combo;
    char hold;
    bool b2b;
    bool dead;
    int total_block;
    int total_clear;
    int total_attack;
    int total_receive;

    test_ai(Engine &global_ai, int const *_combo_table, int _combo_table_max)
        : ai(global_ai.context())
        , combo_table(_combo_table)
        , combo_table_max(_combo_table_max)
    {
    }

    void init(size_t round_ms)
    {
        map = m_tetris::TetrisMap(10, 40);
        r_next.seed(std::random_device()());
        r_garbage.seed(r_next());
        next.clear();
        round = 0;
        recv_attack.clear();
        send_attack = 0;
        combo = 0;
        hold = ' ';
        b2b = false;
        dead = false;
        next_length = 6;
        run_ms = round_ms;
        total_block = 0;
        total_clear = 0;
        total_attack = 0;
        total_receive = 0;
        curr = nullptr;
    }
    m_tetris::TetrisNode const *node() const
    {
        return curr;
    }
    void prepare()
    {
        while(next.size() <= next_length)
        {
            for (size_t i = 0; i < ai.context()->type_max(); ++i)
            {
                next.push_back(ai.context()->convert(i));
            }
            std::shuffle(next.end() - ai.context()->type_max(), next.end(), r_next);
        }
    }
    void run(std::function<void(test_ai&)> view_func)
    {
        ai.search_config()->allow_rotate_move = true;
        ai.search_config()->allow_180 = true;
        ai.search_config()->allow_d = true;
        ai.search_config()->is_20g = false;
        ai.ai_config()->table = combo_table;
        ai.ai_config()->table_max = combo_table_max;
        size_t upcomeAtt = std::accumulate(recv_attack.begin(), recv_attack.end(), 0);
        ai.status()->max_combo = 0;
        ai.status()->max_attack = 0;
        ai.status()->death = 0;
        ai.status()->combo = combo;
        ai.status()->attack = 0;
        ai.status()->clear = 0;
        if (ai.status()->under_attack != upcomeAtt)
        {
            ai.update();
        }
        ai.status()->under_attack = upcomeAtt;
        ai.status()->map_rise = 0;
        ai.status()->b2b = !!b2b;
        ai.status()->like = 0;
        ai.status()->value = 0;

        ++round;
        char current = next.front();
        if (curr == nullptr)
        {
            curr = ai.context()->generate(current);
        }
        view_func(*this);
        char c;
        do
        {
            c = getchar();
        } while (c == EOF || c == '\n');
        int attack = 0;
        auto get_combo_attack = [&](int c)
        {
            return combo_table[std::min(combo_table_max - 1, c)];
        };
        static m_tetris::TetrisNode const *n = nullptr;
        switch (c)
        {
        default:
            break;
        case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8':
            under_attack(c - '0');
            break;
        case 'L':
            while (curr->move_left != nullptr && curr->move_left->check(map))
            {
                curr = curr->move_left;
            }
            break;
        case 'R':
            while (curr->move_right != nullptr && curr->move_right->check(map))
            {
                curr = curr->move_right;
            }
          break;
        case 'd':
            if (curr->move_down != nullptr && curr->move_down->check(map))
            {
                curr = curr->move_down;
            }
            break;
        case 'D':
            curr = curr->drop(map);
            break;
        case 'l':
            if (curr->move_left != nullptr && curr->move_left->check(map))
            {
                curr = curr->move_left;
            }
            break;
        case 'r':
            if (curr->move_right != nullptr && curr->move_right->check(map))
            {
                curr = curr->move_right;
            }
            break;
        case 'z': case 'Z':
            for (auto wall_kick_node : curr->wall_kick_counterclockwise)
            {
                if (wall_kick_node)
                {
                    if (wall_kick_node->check(map))
                    {
                        curr = wall_kick_node;
                        break;
                    }
                }
                else
                {
                    break;
                }
            }
            break;
        case 'x': case 'X':
            for (auto wall_kick_node : curr->wall_kick_opposite)
            {
                if (wall_kick_node)
                {
                    if (wall_kick_node->check(map))
                    {
                        curr = wall_kick_node;
                        break;
                    }
                }
                else
                {
                    break;
                }
            }
            break;
        case 'c': case 'C':
            for (auto wall_kick_node : curr->wall_kick_clockwise)
            {
              if (wall_kick_node)
              {
                  if (wall_kick_node->check(map))
                  {
                      curr = wall_kick_node;
                      break;
                  }
              }
              else
              {
                  break;
              }
            }
            break;
        case 'v':
            if (hold == ' ')
            {
                next.erase(next.begin());
                hold = current;
                current = next.front();
                curr = ai.context()->generate(current);
                prepare();
            }
            else
            {
                next.front() = hold;
                hold = current;
                curr = ai.context()->generate(current);
            }
            break;
        // case 'o':
        //     static int k = 0;
        //     n = curr;
        //     k = 0;
        //     break;
        // case 'k':
        //     if (n->wall_kick_counterclockwise[k] != nullptr)
        //     {
        //         curr = n->wall_kick_counterclockwise[k];
        //     }
        //     else
        //     {
        //         k = 0;
        //         curr = n->wall_kick_counterclockwise[k];
        //     }
        //     ++k;
        //     break;
        // case 'm':
        //     curr = n;
        //     break;
        case 'V':
            curr = curr->drop(map);
            next.erase(next.begin());
            prepare();
            int clear = curr->attach(ai.context().get(), map);
            total_clear += clear;
            auto& m = map;
            n = curr;
            bool immovable = (!n->move_up || !n->move_up->check(m)) && (!n->move_down || !n->move_down->check(m)) && (!n->move_left || !n->move_left->check(m)) && (!n->move_right || !n->move_right->check(m));
            switch (clear)
            {
            case 0:
                combo = 0;
                break;
            case 1:
                if (immovable)
                {
                    attack += 2 + b2b;
                    b2b = 1;
                }
                else
                {
                    b2b = 0;
                }
                attack += get_combo_attack(++combo);
                break;
            case 2:
                if (immovable)
                {
                    attack += 4 + b2b;
                    b2b = 1;
                }
                else
                {
                    attack += 1;
                    b2b = 0;
                }
                attack += get_combo_attack(++combo);
                break;
            case 3:
                if (immovable)
                {
                    attack += 6 + b2b;
                    b2b = 1;
                }
                else
                {
                    attack += 2;
                    b2b = 0;
                }
                attack += get_combo_attack(++combo);
                break;
            case 4:
                attack += get_combo_attack(++combo) + 4 + b2b;
                b2b = 1;
                break;
            }
            if (map.count == 0)
            {
                attack += 6;
            }
            ++total_block;
            total_attack += attack;
            send_attack = attack;
            while (!recv_attack.empty())
            {
                if (send_attack > 0)
                {
                    if (recv_attack.front() <= send_attack)
                    {
                        send_attack -= recv_attack.front();
                        recv_attack.pop_front();
                        continue;
                    }
                    else
                    {
                        recv_attack.front() -= send_attack;
                        send_attack = 0;
                    }
                }
                if (send_attack > 0 || combo > 0)
                {
                    break;
                }
                int line = recv_attack.front();
                total_receive += line;
                recv_attack.pop_front();
                for (int y = map.height - 1; y >= line; --y)
                {
                    map.row[y] = map.row[y - line];
                }
                uint32_t row = ai.context()->full() & ~(1 << std::uniform_int_distribution<uint32_t>(0, ai.context()->width() - 1)(r_garbage));
                for (int y = 0; y < line; ++y)
                {
                    map.row[y] = row;
                }
                map.roof = 0;
                map.count = 0;
                for (int my = 0; my < map.height; ++my)
                {
                    for (int mx = 0; mx < map.width; ++mx)
                    {
                        if (map.full(mx, my))
                        {
                            map.top[mx] = map.roof = my + 1;
                            ++map.count;
                        }
                    }
                }
            }
            curr = nullptr;
            break;
        }
    }
    void under_attack(int line)
    {
        if(line > 0)
        {
            recv_attack.emplace_back(line);
        }
    }
};


double elo_init()
{
    return 1500;
}
double elo_rate(double const &self_score, double const &other_score)
{
    return 1 / (1 + std::pow(10, -(self_score - other_score) / 400));
}
double elo_get_k(int curr, int max)
{
    return 20 * (max - curr) / max + 4;
}
double elo_calc(double const &self_score, double const &other_score, double const &win, int curr, int max)
{
    return self_score + elo_get_k(curr, max) * (win - elo_rate(self_score, other_score));
}


struct BaseNode
{
    BaseNode *parent, *left, *right;
    size_t size : sizeof(size_t) * 8 - 1;
    size_t is_nil : 1;
};

struct NodeData
{
    NodeData()
    {
        score = elo_init();
        best = std::numeric_limits<double>::quiet_NaN();
        match = 0;
        gen = 0;
    }
    char name[64];
    double score;
    double best;
    uint32_t match;
    uint32_t gen;
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
        return *reinterpret_cast<double const *>(&node->data.score);
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

int main(int argc, char const *argv[])
{
    size_t elo_max_match = 256;
    size_t elo_min_match = 128;

    std::mt19937 mt;

    int combo_table[] = { 0, 0, 1, 1, 1, 2, 2, 3, 3, 4 };
    int combo_table_max = 10;
    Engine global_ai;
    global_ai.prepare(10, 40);

    for (; ; )
    {
        test_ai ai1(global_ai, combo_table, combo_table_max);
        for (; ; )
        {
#if _MSC_VER
            auto view_func = [](test_ai const &ai1)
            {
                COORD coordScreen = { 0, 0 };
                DWORD cCharsWritten;
                CONSOLE_SCREEN_BUFFER_INFO csbi;
                DWORD dwConSize;
                HANDLE hConsole;
                if (view && view_index == 0)
                {
                    rank_table_lock.lock();
                    if (view && view_index == 0)
                    {
                        view_index = index;
                        hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
                        GetConsoleScreenBufferInfo(hConsole, &csbi);
                        dwConSize = csbi.dwSize.X * csbi.dwSize.Y;
                        FillConsoleOutputCharacterA(hConsole, ' ', dwConSize, coordScreen, &dwConSize);
                    }
                    rank_table_lock.unlock();
                }
                if (index != view_index)
                {
                    return;
                }

                hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

                GetConsoleScreenBufferInfo(hConsole, &csbi);
                dwConSize = csbi.dwSize.X * 2;
                FillConsoleOutputCharacterA(hConsole, ' ', dwConSize, coordScreen, &dwConSize);
                GetConsoleScreenBufferInfo(hConsole, &csbi);
                FillConsoleOutputAttribute(hConsole, csbi.wAttributes, dwConSize, coordScreen, &cCharsWritten);
                SetConsoleCursorPosition(hConsole, coordScreen);

                char out[81920] = "";
                char box_0[3] = "[]";
                char box_1[3] = "  ";

                out[0] = '\0';
                int up1 = std::accumulate(ai1.recv_attack.begin(), ai1.recv_attack.end(), 0);
                snprintf(out, sizeof out, "HOLD = %c NEXT = %c%c%c%c%c%c COMBO =%2d B2B = %d APP = %1.2f UP = %2d\n",
                     ai1.hold, ai1.next[1], ai1.next[2], ai1.next[3], ai1.next[4], ai1.next[5], ai1.next[6], ai1.combo, ai1.b2b, 1. * ai1.total_attack / ai1.round, up1);
                m_tetris::TetrisMap map_copy1 = ai1.map;
                if (ai1.node() != nullptr)
                {
                    ai1.node()->attach(ai1.ai.context().get(), map_copy1);
                }
                for (int y = 21; y >= 0; --y)
                {
                    strcat_s(out, "##");
                    for (int x = 0; x < 10; ++x)
                    {
                        strcat_s(out, map_copy1.full(x, y) ? box_1 : box_0);
                    }
                    strcat_s(out, "##\r\n");
                }
                strcat_s(out, "########################\r\n");
                WriteConsoleA(hConsole, out, strlen(out), nullptr, nullptr);
            };
#else
            auto view_func = [](test_ai const &ai1)
            {
                char out[81920] = "";
                char box_0[3] = "  ";
                char box_1[3] = "[]";

                out[0] = '\0';
                int up1 = std::accumulate(ai1.recv_attack.begin(), ai1.recv_attack.end(), 0);
                snprintf(out, sizeof out, "HOLD = %c NEXT = %c%c%c%c%c%c COMBO =%2d B2B = %d APP = %1.2f UP = %2d\n",
                       ai1.hold, ai1.next[1], ai1.next[2], ai1.next[3], ai1.next[4], ai1.next[5], ai1.next[6], ai1.combo, ai1.b2b, 1. * ai1.total_attack / ai1.round, up1);
                m_tetris::TetrisMap map_copy1 = ai1.map;
                if (ai1.node() != nullptr)
                {
                    ai1.node()->attach(ai1.ai.context().get(), map_copy1);
                }
                for (int y = 21; y >= 0; --y)
                {
                    strcat(out, "##");
                    for (int x = 0; x < 10; ++x)
                    {
                        strcat(out, map_copy1.full(x, y) ? box_1 : box_0);
                    }
                    strcat(out, "##\r\n");
                }
                strcat(out, "########################\r\n");
                printf("%s", out);
            };
#endif
            ai1.init(0);
            ai1.prepare();
            for (; ; )
            {
                ai1.run(view_func);

                if (ai1.dead)
                {
                    break;
                }
            }
        }
    }
}
