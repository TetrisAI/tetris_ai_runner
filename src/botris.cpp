
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

        //combo = 10;
        //hold = 'T';
        //char next_arr[] = " TTTTTTT";
        //next.assign(next_arr, next_arr + 8);
        //map.row[18] = 0b1111101100;
        //map.row[17] = 0b1111111100;
        //map.row[16] = 0b1111111100;
        //map.row[15] = 0b1111111100;
        //map.row[14] = 0b1111111100;
        //map.row[13] = 0b1111111000;
        //map.row[12] = 0b1111111101;
        //map.row[11] = 0b1111111111;
        map.prepare();
    }
    m_tetris::TetrisNode const *node() const
    {
        return ai.context()->generate(next.front());
    }
    void prepare()
    {
        if(!next.empty())
        {
            next.erase(next.begin());
        }
        while(next.size() <= next_length)
        {
            for (size_t i = 0; i < ai.context()->type_max(); ++i)
            {
                next.push_back(ai.context()->convert(i));
            }
            std::shuffle(next.end() - ai.context()->type_max(), next.end(), r_next);
        }
    }
    void run()
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
        auto node = ai.context()->generate(current);
        auto result = ai.run_hold(map, node, hold, true, next.data() + 1, next_length, run_ms);
        if(result.target == nullptr || result.target->low >= 20)
        {
            dead = true;
            return;
        }
        if (result.change_hold)
        {
            node = ai.context()->generate(result.target->status.t);
            if (hold == ' ')
            {
                next.erase(next.begin());
            }
            hold = current;
        }
        int attack = 0;
        auto get_combo_attack = [&](int c)
        {
            return combo_table[std::min(combo_table_max - 1, c)];
        };
        auto node_p = node;
        auto ai_path = ai.make_path(node, result.target, map);
        auto apply = [](m_tetris::TetrisNode const* node, m_tetris::TetrisMap const &map, std::vector<char> const& path) {
          for (char c : path)
          {
              switch (c)
              {
              case 'L':
                  while (node->move_left != nullptr && node->move_left->check(map))
                  {
                      node = node->move_left;
                  }
                  break;
              case 'R':
                  while (node->move_right != nullptr && node->move_right->check(map))
                  {
                      node = node->move_right;
                  }
                break;
              case 'd':
                  if (node->move_down != nullptr && node->move_down->check(map))
                  {
                      node = node->move_down;
                  }
                  break;
              case 'D':
                  node = node->drop(map);
                  break;
              case 'l':
                  if (node->move_left != nullptr && node->move_left->check(map))
                  {
                      node = node->move_left;
                  }
                  break;
              case 'r':
                  if (node->move_right != nullptr && node->move_right->check(map))
                  {
                      node = node->move_right;
                  }
                  break;
              case 'z': case 'Z':
                  for (auto wall_kick_node : node->wall_kick_counterclockwise)
                  {
                      if (wall_kick_node)
                      {
                          if (wall_kick_node->check(map))
                          {
                              node = wall_kick_node;
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
                for (auto wall_kick_node : node->wall_kick_opposite)
                {
                    if (wall_kick_node)
                    {
                        if (wall_kick_node->check(map))
                        {
                            node = wall_kick_node;
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
                  for (auto wall_kick_node : node->wall_kick_clockwise)
                  {
                    if (wall_kick_node)
                    {
                        if (wall_kick_node->check(map))
                        {
                            node = wall_kick_node;
                            break;
                        }
                    }
                    else
                    {
                        break;
                    }
                  }
                  break;
              default:
                  break;
              }
          }
          return node->drop(map);
        };
        node = apply(node, map, ai_path);
        ai_path.push_back('\0');
        if (node->index_filtered != result.target->index_filtered)
        {
            printf("PATH = INVALID %s\r\n", ai_path.data());
            std::vector<m_tetris::TetrisNode const*> lps;
            ai.search(node_p, map, lps);
            if (std::find(lps.begin(), lps.end(), result.target) == lps.end())
            {
                printf("???\r\n");
            }
            else
            {
                printf("!!!\r\n");
                while (true)
                {
                    ai.search(node_p, map, lps);
                    ai_path = ai.make_path(node_p, result.target, map);
                    node = apply(node_p, map, ai_path);
                }
            }
        }
        int clear = node->attach(ai.context().get(), map);
        total_clear += clear;
        switch (clear)
        {
        case 0:
            combo = 0;
            break;
        case 1:
            if (result.target.type == ai_zzz::Botris::ASpinType::ASpin)
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
            if (result.target.type != ai_zzz::Botris::ASpinType::None)
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
            if (result.target.type != ai_zzz::Botris::ASpinType::None)
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
            uint32_t row = 1 << std::uniform_int_distribution<uint32_t>(0, ai.context()->width() - 1)(r_garbage);
            for (int y = 0; y < line; ++y)
            {
                map.row[y] = row;
            }
            map.prepare_internal();
        }
    }
    void under_attack(int line)
    {
        if(line > 0)
        {
            recv_attack.emplace_back(line);
        }
    }

    static void match(test_ai& ai1, test_ai& ai2, std::function<void(test_ai const &, test_ai const &)> out_put, size_t match_round)
    {
        size_t round = 0;
        for (; ; )
        {
            ++round;
            ai1.prepare();
            ai2.prepare();
            if (out_put)
            {
                out_put(ai1, ai2);
            }

            ai1.run();
            ai2.run();

            if (ai1.dead || ai2.dead)
            {
                return;
            }
            if (round > match_round)
            {
                return;
            }

            ai1.under_attack(ai2.send_attack);
            ai2.under_attack(ai1.send_attack);
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
    std::atomic<uint32_t> count{std::max<uint32_t>(1, std::thread::hardware_concurrency() - 1)};
    std::string file = "data.bin";
    if (argc > 1)
    {
        uint32_t arg_count = std::stoul(argv[1], nullptr, 10);
        if (arg_count != 0)
        {
            count = arg_count;
        }
    }
    uint32_t node_count = count * 2;
    if (argc > 2)
    {
        uint32_t arg_count = std::stoul(argv[2], nullptr, 10);
        if (arg_count >= 2)
        {
            node_count = arg_count;
        }
    }
    std::atomic<bool> view{true};
    std::atomic<uint32_t> view_index{0};

    std::recursive_mutex rank_table_lock;
    zzz::sb_tree<SBTreeInterface> rank_table;

    size_t elo_max_match = 256;
    size_t elo_min_match = 128;

    std::mt19937 mt;

    {
        if (rank_table.empty())
        {
            NodeData init_node;

            strncpy(init_node.name, "*default", sizeof init_node.name);
            rank_table.insert(new Node(init_node));
        }
    }

    while (rank_table.size() < node_count)
    {
        NodeData init_node = rank_table.at(std::uniform_int_distribution<size_t>(0, rank_table.size() - 1)(mt))->data;

        strncpy(init_node.name, ("init_" + std::to_string(rank_table.size())).c_str(), sizeof init_node.name);
        rank_table.insert(new Node(init_node));
    }

    std::vector<std::thread> threads;
    int combo_table[] = { 0, 0, 1, 1, 1, 2, 2, 3, 3, 4 };
    int combo_table_max = 10;
    Engine global_ai;
    global_ai.prepare(10, 40);

    for (size_t i = 1; i <= count; ++i)
    {
        threads.emplace_back([&, i]()
        {
            uint32_t index = i + 1;
            auto rand_match = [&](auto &mt, size_t max)
            {
                std::pair<size_t, size_t> ret;
                ret.first = std::uniform_int_distribution<size_t>(0, max - 1)(mt);
                do
                {
                    ret.second = std::uniform_int_distribution<size_t>(0, max - 1)(mt);
                } while (ret.second == ret.first);
                return ret;
            };
            test_ai ai1(global_ai, combo_table, combo_table_max);
            test_ai ai2(global_ai, combo_table, combo_table_max);
            rank_table_lock.lock();
            rank_table_lock.unlock();
            for (; ; )
            {
                rank_table_lock.lock();
                auto m12 = rand_match(mt, rank_table.size());
                auto m1 = rank_table.at(m12.first);
                auto m2 = rank_table.at(m12.second);
#if _MSC_VER
                auto view_func = [m1, m2, index, &view, &view_index, &rank_table_lock](test_ai const &ai1, test_ai const &ai2)
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
                    int up2 = std::accumulate(ai2.recv_attack.begin(), ai2.recv_attack.end(), 0);
                    snprintf(out, sizeof out, "HOLD = %c NEXT = %c%c%c%c%c%c COMBO =%2d B2B = %d APP = %1.2f UP = %2d NAME = %s\n"
                                              "HOLD = %c NEXT = %c%c%c%c%c%c COMBO =%2d B2B = %d APP = %1.2f UP = %2d NAME = %s\n",
                         ai1.hold, ai1.next[1], ai1.next[2], ai1.next[3], ai1.next[4], ai1.next[5], ai1.next[6], ai1.combo, ai1.b2b, 1. * ai1.total_attack / ai1.round, up1, m1->data.name,
                         ai2.hold, ai2.next[1], ai2.next[2], ai2.next[3], ai2.next[4], ai2.next[5], ai2.next[6], ai2.combo, ai2.b2b, 1. * ai2.total_attack / ai2.round, up2, m2->data.name);
                    m_tetris::TetrisMap map_copy1 = ai1.map;
                    m_tetris::TetrisMap map_copy2 = ai2.map;
                    ai1.node()->attach(ai1.ai.context().get(), map_copy1);
                    ai2.node()->attach(ai2.ai.context().get(), map_copy2);
                    for (int y = 21; y >= 0; --y)
                    {
                        strcat_s(out, "##");
                        for (int x = 0; x < 10; ++x)
                        {
                            strcat_s(out, map_copy1.full(x, y) ? box_1 : box_0);
                        }
                        strcat_s(out, "##  ##");
                        for (int x = 0; x < 10; ++x)
                        {
                            strcat_s(out, map_copy2.full(x, y) ? box_1 : box_0);
                        }
                        strcat_s(out, "##");
                        strcat_s(out, "\r\n");
                    }
                    strcat_s(out, "########################  ########################\r\n");
                    WriteConsoleA(hConsole, out, strlen(out), nullptr, nullptr);
                    Sleep(333);
                };
#else
                auto view_func = [m1, m2, index, &view, &view_index, &rank_table_lock](test_ai const &ai1, test_ai const &ai2)
                {
                    if (view && view_index == 0)
                    {
                        rank_table_lock.lock();
                        if (view && view_index == 0)
                        {
                            view_index = index;
                        }
                        rank_table_lock.unlock();
                    }
                    if (index != view_index)
                    {
                        return;
                    }

                    char out[81920] = "";
                    char box_0[3] = "  ";
                    char box_1[3] = "[]";

                    out[0] = '\0';
                    int up1 = std::accumulate(ai1.recv_attack.begin(), ai1.recv_attack.end(), 0);
                    int up2 = std::accumulate(ai2.recv_attack.begin(), ai2.recv_attack.end(), 0);
                    snprintf(out, sizeof out, "HOLD = %c NEXT = %c%c%c%c%c%c COMBO =%2d B2B = %d APP = %1.2f UP = %2d NAME = %s\n"
                                              "HOLD = %c NEXT = %c%c%c%c%c%c COMBO =%2d B2B = %d APP = %1.2f UP = %2d NAME = %s\n",
                           ai1.hold, ai1.next[1], ai1.next[2], ai1.next[3], ai1.next[4], ai1.next[5], ai1.next[6], ai1.combo, ai1.b2b, 1. * ai1.total_attack / ai1.round, up1, m1->data.name,
                           ai2.hold, ai2.next[1], ai2.next[2], ai2.next[3], ai2.next[4], ai2.next[5], ai2.next[6], ai2.combo, ai2.b2b, 1. * ai2.total_attack / ai2.round, up2, m2->data.name);
                    m_tetris::TetrisMap map_copy1 = ai1.map;
                    m_tetris::TetrisMap map_copy2 = ai2.map;
                    ai1.node()->attach(ai1.ai.context().get(), map_copy1);
                    ai2.node()->attach(ai1.ai.context().get(), map_copy2);
                    for (int y = 21; y >= 0; --y)
                    {
                        strcat(out, "##");
                        for (int x = 0; x < 10; ++x)
                        {
                            strcat(out, map_copy1.full(x, y) ? box_1 : box_0);
                        }
                        strcat(out, "##  ##");
                        for (int x = 0; x < 10; ++x)
                        {
                            strcat(out, map_copy2.full(x, y) ? box_1 : box_0);
                        }
                        strcat(out, "##");
                        strcat(out, "\r\n");
                    }
                    strcat(out, "########################  ########################\r\n");
                    printf("%s", out);
                    usleep(333000);
                };
#endif
                // size_t round_ms_min = (45 + m1->data.match) / 3;
                // size_t round_ms_max = (45 + m2->data.match) / 3;
                // if (round_ms_min > round_ms_max)
                // {
                //     std::swap(round_ms_min, round_ms_max);
                // }
                // size_t round_ms = std::uniform_int_distribution<size_t>(round_ms_min, round_ms_max)(mt);
                size_t round_ms = 50;
                size_t round_count = 3600;
                ai1.init(round_ms);
                ai2.init(round_ms);
                rank_table_lock.unlock();
                test_ai::match(ai1, ai2, view_func, round_count);
                rank_table_lock.lock();

                rank_table.erase(m1);
                rank_table.erase(m2);
                bool handle_elo_1;
                bool handle_elo_2;
                if ((m1->data.match > elo_min_match) == (m2->data.match > elo_min_match))
                {
                    handle_elo_1 = true;
                    handle_elo_2 = true;
                }
                else
                {
                    handle_elo_1 = m2->data.match > elo_min_match;
                    handle_elo_2 = !handle_elo_1;
                }
                double m1s = m1->data.score;
                double m2s = m2->data.score;
                double ai1_apl = ai1.total_clear == 0 ? 0. : 1. * ai1.total_attack / ai1.total_clear;
                double ai2_apl = ai2.total_clear == 0 ? 0. : 1. * ai2.total_attack / ai2.total_clear;
                int ai1_win = ai2.dead * 2 + (ai1_apl > ai2_apl);
                int ai2_win = ai1.dead * 2 + (ai2_apl > ai1_apl);
                if (ai1_win == ai2_win)
                {
                    if (handle_elo_1)
                    {
                        m1->data.score = elo_calc(m1s, m2s, 0.5, m1->data.match, elo_max_match);
                    }
                    if (handle_elo_2)
                    {
                        m2->data.score = elo_calc(m2s, m1s, 0.5, m2->data.match, elo_max_match);
                    }
                }
                else if (ai1_win > ai2_win)
                {
                    if (handle_elo_1)
                    {
                        m1->data.score = elo_calc(m1s, m2s, 1, m1->data.match, elo_max_match);
                    }
                    if (handle_elo_2)
                    {
                        m2->data.score = elo_calc(m2s, m1s, 0, m2->data.match, elo_max_match);
                    }
                }
                else
                {
                    if (handle_elo_1)
                    {
                        m1->data.score = elo_calc(m1s, m2s, 0, m1->data.match, elo_max_match);
                    }
                    if (handle_elo_2)
                    {
                        m2->data.score = elo_calc(m2s, m1s, 1, m2->data.match, elo_max_match);
                    }
                }
                m1->data.match += handle_elo_1;
                m2->data.match += handle_elo_2;
                rank_table.insert(m1);
                rank_table.insert(m2);

                rank_table_lock.unlock();
            }
        });
    }

    std::map<std::string, std::function<bool(std::vector<std::string> const &)>> command_map;
    command_map.insert(std::make_pair("view", [&view](std::vector<std::string> const &token)
    {
        view = true;
        return true;
    }));
    command_map.insert(std::make_pair("exit", [&file, &rank_table, &rank_table_lock](std::vector<std::string> const &token)
    {
        rank_table_lock.lock();
        std::ofstream ofs(file, std::ios::out | std::ios::binary);
        for (size_t i = 0; i < rank_table.size(); ++i)
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
            "help                 - ...\n"
            "view                 - view a match (press enter to stop)\n"
            "exit                 - exit\n"
        );
        return true;
    }));
    std::string line, last;
    while (true)
    {
        std::getline(std::cin, line);
        if (view)
        {
            view = false;
            view_index = 0;
            continue;
        }
        std::vector<std::string> token;
        zzz::split(token, line, " ");
        if (token.empty())
        {
            line = last;
            zzz::split(token, line, " ");
        }
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
            printf("-------------------------------------------------------------\n");
            last = line;
            std::cout.flush();
        }
    }
}
