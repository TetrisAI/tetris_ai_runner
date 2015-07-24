
#define DECLSPEC_EXPORT __declspec(dllexport)
#define WINAPI __stdcall

#include <ctime>
#include "tetris_core.h"
#include "search_simple.h"
#include "search_path.h"
#include "search_simulate.h"
#include "search_cautious.h"
#include "ai_ax.h"
#include "ai_zzz.h"
#include "rule_st.h"
#include "rule_qq.h"
#include "rule_srs.h"
#include "random.h"

extern "C" void attach_init()
{
    ege::mtsrand(unsigned int(time(nullptr)));
}

#ifndef WINVER
#define WINVER 0x0500
#endif

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0501
#endif
#define WIN32_LEAN_AND_MEAN

#include <windows.h>

struct tetris_game
{
    typedef int(*ai_run_t)(int boardW, int boardH, int board[], char nextPiece[], int curX, int curY, int curR, int level, int mode, char path[], size_t limit);
    m_tetris::TetrisEngine<rule_qq::TetrisRule, ai_zzz::qq::Attack, search_path::Search> tetris_ai;
    ege::mtrandom random;
    m_tetris::TetrisMap map;
    std::vector<char> next;
    size_t next_length;
    HMODULE hDll;
    void *ai;
    int version;
    char path[2048];

    tetris_game() : hDll(), version(-1)
    {
    }
    ~tetris_game()
    {
    }

    void under_attack(size_t line)
    {
        size_t w = map.width, h = map.height;
        for(size_t y = h - 1; y >= line; --y)
        {
            map.row[y] = map.row[y - line];
        }
        for(size_t y = 0; y < line; ++y)
        {
            map.row[y] = 0;
            for(size_t x = 0; x < w; ++x)
            {
                if((x + y) % 2 != 0)
                {
                    map.row[y] |= 1 << x;
                }
            }
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

    bool init(size_t w, size_t h, size_t next, std::wstring dll)
    {
        next_length = next;
        if(hDll != nullptr)
        {
            FreeLibrary(hDll);
        }
        hDll = LoadLibrary(dll.c_str());
        if(hDll == nullptr)
        {
            return 0;
        }
        ai = GetProcAddress(hDll, "_QQTetrisAI@44");
        if(ai == NULL)
        {
            ai = GetProcAddress(hDll, "QQTetrisAI@44");
        }
        if(ai == NULL)
        {
            ai = GetProcAddress(hDll, "QQTetrisAI");
        }
        tetris_ai.prepare(w, h);
        return 1;
    }

    int run(size_t limit)
    {
        fill_next();
        m_tetris::TetrisNode const *node = tetris_ai.context()->generate(next.front());
        if(map.row[tetris_ai.context()->height() - 1] != 0 || !node->check(map))
        {
            return -1;
        }
        memset(path, 0, sizeof path);
        next.push_back(0);
        ((ai_run_t)ai)(tetris_ai.context()->width(), tetris_ai.context()->height(), reinterpret_cast<int *>(map.row), next.data(), node->status.x, node->status.y, (4 - node->status.r) % 4, 10, 0, path, limit);
        char *move = path, *move_end = path + sizeof path;
        next.pop_back();
        next.erase(next.begin());
        //printf("%c->%s\n", node->status.t, path);
        while(move != move_end && *move != '\0')
        {
            switch(*move++)
            {
            case 'l':
                if(node->move_left && (node->row >= map.roof || node->move_left->check(map)))
                {
                    node = node->move_left;
                }
                break;
            case 'r':
                if(node->move_right && (node->row >= map.roof || node->move_right->check(map)))
                {
                    node = node->move_right;
                }
                break;
            case 'd':
                if(node->move_down && (node->row > map.roof || node->move_down->check(map)))
                {
                    node = node->move_down;
                }
                break;
            case 'L':
                while(node->move_left && (node->row >= map.roof || node->move_left->check(map)))
                {
                    node = node->move_left;
                }
                break;
            case 'R':
                while(node->move_right && (node->row >= map.roof || node->move_right->check(map)))
                {
                    node = node->move_right;
                }
                break;
            case 'D':
                node = node->drop(map);
                break;
            case 'z':
                if(node->rotate_counterclockwise && node->rotate_counterclockwise->check(map))
                {
                    node = node->rotate_counterclockwise;
                }
                break;
            case 'c':
                if(node->rotate_clockwise && node->rotate_clockwise->check(map))
                {
                    node = node->rotate_clockwise;
                }
                break;
            case 'x':
                if(node->rotate_opposite && node->rotate_opposite->check(map))
                {
                    node = node->rotate_opposite;
                }
                break;
            default:
                move = move_end;
                break;
            }
        }
        node = node->drop(map);
        size_t clear = node->attach(map);
        if(clear >= 3)
        {
            return clear - 1;
        }
        else
        {
            return 0;
        }
    }

    void fill_next()
    {
        do
        {
            size_t next_index = random.rand() & 7;
            if(next_index < 7)
            {
                next.push_back(tetris_ai.context()->generate(next_index)->status.t);
            }
        } while(next.size() <= next_length);
    }

    void new_game(size_t seed)
    {
        random.reset(seed);
        map.width = tetris_ai.context()->width();
        map.height = tetris_ai.context()->height();
        map.count = 0;
        map.roof = 0;
        memset(map.top, 0, sizeof map.top);
        memset(map.row, 0, sizeof map.row);
        next.clear();
        fill_next();
    }
};

int speed_test(unsigned int argc, wchar_t *argv[], wchar_t *eve[]);

//这是一个加载dll测试数据的控制台,优先调用AIPath,找不到则调用AI
int wmain(unsigned int argc, wchar_t *argv[], wchar_t *eve[])
{
    ege::mtsrand(unsigned int(time(nullptr)));
    if(argc < 3)
    {
        return speed_test(argc, argv, eve);
    }
    tetris_game game[2];
    game[0].init(12, 21, 13, argv[1]);
    game[1].init(12, 21, 13, argv[2]);
    size_t new_seed = ege::mtirand();
    game[0].new_game(new_seed);
    game[1].new_game(new_seed);
    size_t win[2] = {};
    double total = 0;
    unsigned long long attack[2];

    char out[81920] = "";
    char box_0[3] = "□";
    char box_1[3] = "■";

    CONSOLE_CURSOR_INFO cursorInfo = {1, FALSE};  // 光标信息
    SetConsoleCursorInfo(GetStdHandle(STD_OUTPUT_HANDLE), &cursorInfo);  // 设置光标隐藏
    
    while(true)
    {
        static COORD cd;
        cd.X = 0;
        cd.Y = 0;
        SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), cd);
        out[0] = '\0';
        m_tetris::TetrisMap map[2] = {game[0].map, game[1].map};
        m_tetris::TetrisNode const *node = game[0].tetris_ai.context()->generate(game[0].next.front());
        node->attach(map[0]);
        node->attach(map[1]);
        for(int y = 20; y >= 0; --y)
        {
            for(int x = 0; x < 12; ++x)
            {
                strcat_s(out, map[0].full(x, y) ? box_1 : box_0);
            }
            strcat_s(out, "           ");
            for(int x = 0; x < 12; ++x)
            {
                strcat_s(out, map[1].full(x, y) ? box_1 : box_0);
            }
            strcat_s(out, "\r\n");
        }
        strcat_s(out, "\r\n");
        printf(out);

        int attack_0 = game[0].run(166);
        int attack_1 = game[1].run(166);
        if(attack_0 == -1 || attack_1 == -1)
        {
            if(attack_0 == -1) ++win[1];
            if(attack_1 == -1) ++win[0];
#pragma warning(push)
#pragma warning(disable:4996)
            wchar_t BUFFER[1024];
            std::swprintf(BUFFER, L"[%s][比分%d:%d][效率%f:%f]", (attack_0 == -1 ? attack_1 == -1 ? TEXT("平局") : argv[2] : argv[1]), win[0], win[1], attack[0] / total, attack[1] / total);
            SetWindowText(GetConsoleWindow(), BUFFER);
#pragma warning(pop)
            new_seed = ege::mtirand();
            game[0].new_game(new_seed);
            game[1].new_game(new_seed);
        }
        else
        {
            total += 4;
            if(attack_0 > 0)
            {
                attack[0] += (attack_0 + 1) * 12;
                game[1].under_attack(attack_0);
            }
            if(attack_1 > 0)
            {
                attack[1] += (attack_1 + 1) * 12;
                game[0].under_attack(attack_1);
            }
        }

    }
}

m_tetris::TetrisEngine<rule_st::TetrisRule, ai_zzz::Dig, search_simple::Search> tetris_ai;

//这是一个加载dll测试数据的控制台,优先调用AIPath,找不到则调用AI
int speed_test(unsigned int argc, wchar_t *argv[], wchar_t *eve[])
{
    attach_init();
    if(argc < 2)
    {
        return 0;
    }
    HMODULE hDll = LoadLibrary(argv[1]);
    if(hDll == nullptr)
    {
        return 0;
    }
    void *name = nullptr;
    name = GetProcAddress(hDll, "_Name@0");
    if(name == nullptr)
    {
        name = GetProcAddress(hDll, "Name@0");
    }
    if(name == nullptr)
    {
        name = GetProcAddress(hDll, "Name");
    }
    void *ai[2] = {};
    ai[0] = GetProcAddress(hDll, "_AIPath@36");
    if(ai[0] == NULL)
    {
        ai[0] = GetProcAddress(hDll, "AIPath@36");
    }
    if(ai[0] == NULL)
    {
        ai[0] = GetProcAddress(hDll, "AIPath");
    }
    ai[1] = GetProcAddress(hDll, "_AI@40");
    if(ai[1] == NULL)
    {
        ai[1] = GetProcAddress(hDll, "AI@40");
    }
    if(ai[1] == NULL)
    {
        ai[1] = GetProcAddress(hDll, "AI");
    }

    if(name == nullptr)
    {
        return 0;
    }
    int version = -1;
    for(int i = 0; i < sizeof ai / sizeof ai[0]; ++i)
    {
        if(ai[i] != nullptr)
        {
            version = i;
            break;
        }
    }
    if(version == -1)
    {
        return 0;
    }
    SetWindowTextA(GetConsoleWindow(), ((char const *(*)())name)());
    int w = 10, h = 20;
    m_tetris::TetrisMap map(w, h);
    char *param_map = new char[w * h];
    char *path = new char[1024];
    tetris_ai.prepare(w, h);
    clock_t log_start = clock();
    clock_t log_time = log_start;
    clock_t log_new_time;

    clock_t log_interval = 10000;
    long long log_rows = 0, log_piece = 0;

    long long total_lines = 0;
    long long this_lines = 0;
    long long max_line = 0;
    long long game_count = 0;

    while(true)
    {
        m_tetris::TetrisNode const *node = tetris_ai.context()->generate();
        log_new_time = clock();
        if(log_new_time - log_time > log_interval)
        {
            printf("{\"time\":%.2lf,\"current\":%lld,\"rows_ps\":%lld,\"piece_ps\":%lld}\n", (log_new_time - log_start) / 1000., this_lines, log_rows * 1000 / log_interval, log_piece * 1000 / log_interval);
            log_time += log_interval;
            log_rows = 0;
            log_piece = 0;
        }
        if(!node->check(map))
        {
            total_lines += this_lines;
            if(this_lines > max_line)
            {
                max_line = this_lines;
            }
            ++game_count;
            printf("{\"avg\":%.2lf,\"max\":%lld,\"count\":%lld,\"current\":%lld}\n", game_count == 0 ? 0. : double(total_lines) / game_count, max_line, game_count, this_lines);
            this_lines = 0;
            map.count = 0;
            map.roof = 0;
            memset(map.top, 0, sizeof map.top);
            memset(map.row, 0, sizeof map.row);
        }
        for(int y = 0; y < h; ++y)
        {
            int row = y * w;
            for(int x = 0; x < w; ++x)
            {
                param_map[x + row] = map.full(x, y) ? '1' : '0';
            }
        }
        if(version == 0)
        {
            memset(path, 0, 1024);
            typedef int(__stdcall *ai_run_t)(int boardW, int boardH, char board[], char curPiece, int curX, int curY, int curR, char *nextPiece, char path[]);
            char next[] = {'\0'};
            ((ai_run_t)ai[version])(w, h, param_map, node->status.t, node->status.x + 1, node->status.y + 1, node->status.r + 1, next, path);
            char *move = path, *move_end = path + 1024;
            //printf("%c->%s\n", node->status.t, path);
            while(move != move_end && *move != '\0')
            {
                switch(*move++)
                {
                case 'l':
                    if(node->move_left && (node->row >= map.roof || node->move_left->check(map)))
                    {
                        node = node->move_left;
                    }
                    break;
                case 'r':
                    if(node->move_right && (node->row >= map.roof || node->move_right->check(map)))
                    {
                        node = node->move_right;
                    }
                    break;
                case 'd':
                    if(node->move_down && (node->row > map.roof || node->move_down->check(map)))
                    {
                        node = node->move_down;
                    }
                    break;
                case 'L':
                    while(node->move_left && (node->row >= map.roof || node->move_left->check(map)))
                    {
                        node = node->move_left;
                    }
                    break;
                case 'R':
                    while(node->move_right && (node->row >= map.roof || node->move_right->check(map)))
                    {
                        node = node->move_right;
                    }
                    break;
                case 'D':
                    node = node->drop(map);
                    break;
                case 'z':
                    if(node->rotate_counterclockwise && node->rotate_counterclockwise->check(map))
                    {
                        node = node->rotate_counterclockwise;
                    }
                    break;
                case 'c':
                    if(node->rotate_clockwise && node->rotate_clockwise->check(map))
                    {
                        node = node->rotate_clockwise;
                    }
                    break;
                case 'x':
                    if(node->rotate_opposite && node->rotate_opposite->check(map))
                    {
                        node = node->rotate_opposite;
                    }
                    break;
                default:
                    move = move_end;
                    break;
                }
            }
        }
        else
        {
            typedef int(__stdcall *ai_run_t)(int boardW, int boardH, char board[], char curPiece, int curX, int curY, int curR, char nextPiece, int *bestX, int *bestRotation);
            int best_x = node->status.x + 1, best_r = node->status.r + 1;
            ((ai_run_t)ai[version])(w, h, param_map, node->status.t, best_x, node->status.y + 1, best_r, ' ', &best_x, &best_r);
            --best_x;
            --best_r;
            int r = node->status.r;
            while(best_r > r && node->rotate_counterclockwise && node->rotate_counterclockwise->check(map))
            {
                ++r;
                node = node->rotate_counterclockwise;
            }
            while(best_r < r && node->rotate_clockwise && node->rotate_clockwise->check(map))
            {
                --r;
                node = node->rotate_clockwise;
            }
            while(best_x > node->status.x && node->move_right && (node->row >= map.roof || node->move_right->check(map)))
            {
                node = node->move_right;
            }
            while(best_x < node->status.x && node->move_left && (node->row >= map.roof || node->move_left->check(map)))
            {
                node = node->move_left;
            }
        }
        node = node->drop(map);
        int clear = node->attach(map);
        this_lines += clear;
        log_rows += clear;
        ++log_piece;
    }
}