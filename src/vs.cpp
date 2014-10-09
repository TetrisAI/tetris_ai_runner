
#define DECLSPEC_EXPORT __declspec(dllexport)
#define WINAPI __stdcall

#include <ctime>
#include "tetris_core.h"
#include "land_point_search_simple.h"
#include "land_point_search_path.h"
#include "land_point_search_simulate.h"
#include "land_point_search_cautious.h"
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
    m_tetris::TetrisEngine<rule_qq::TetrisRuleSet, ai_zzz::qq::Attack, land_point_search_path::Search, ai_zzz::qq::Attack::Param> tetris_ai;
    ege::mtrandom random;
    m_tetris::TetrisMap map;
    std::vector<unsigned char> next;
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
        ((ai_run_t)ai)(tetris_ai.context()->width(), tetris_ai.context()->height(), map.row, reinterpret_cast<char *>(next.data()), node->status.x, node->status.y, (4 - node->status.r) % 4, 10, 0, path, limit);
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

//这是一个加载dll测试数据的控制台,优先调用AIPath,找不到则调用AI
int wmain(unsigned int argc, wchar_t *argv[], wchar_t *eve[])
{
    ege::mtsrand(unsigned int(time(nullptr)));
    if(argc < 3)
    {
        return 0;
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