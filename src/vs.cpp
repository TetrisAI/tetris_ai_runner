
#define DECLSPEC_EXPORT __declspec(dllexport)
#define WINAPI __stdcall

#include <ctime>
#include "tetris_core.h"
#include "search_simple.h"
#include "search_path.h"
#include "search_simulate.h"
#include "ai_ax.h"
#include "ai_zzz.h"
#include "rule_st.h"
#include "rule_qq.h"
#include "rule_srs.h"
#include "random.h"
#include "tetris_engine2.h"
#include "bb_sim.h"

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
    typedef int (*ai_run_t)(int boardW, int boardH, int board[], char nextPiece[], int curX, int curY, int curR, int level, int mode, char path[], size_t limit);
    m_tetris2::TetrisEngine2<rule_qq::TetrisRule, ai_zzz::qq::Attack, search_path::Search> tetris_ai;
    ege::mtrandom random;
    m_tetris2::TetrisMap map;
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
        for (size_t y = h - 1; y >= line; --y)
        {
            map.row[y] = map.row[y - line];
        }
        for (size_t y = 0; y < line; ++y)
        {
            map.row[y] = 0;
            for (size_t x = 0; x < w; ++x)
            {
                if ((x + y) % 2 != 0)
                {
                    map.row[y] |= 1 << x;
                }
            }
        }
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

    bool init(size_t w, size_t h, size_t next, std::wstring dll)
    {
        next_length = next;
        if (hDll != nullptr)
        {
            FreeLibrary(hDll);
        }
        hDll = LoadLibrary(dll.c_str());
        if (hDll == nullptr)
        {
            return 0;
        }
        ai = GetProcAddress(hDll, "_QQTetrisAI@44");
        if (ai == NULL)
        {
            ai = GetProcAddress(hDll, "QQTetrisAI@44");
        }
        if (ai == NULL)
        {
            ai = GetProcAddress(hDll, "QQTetrisAI");
        }
        tetris_ai.prepare(w, h);
        return 1;
    }

    int run(size_t limit)
    {
        using H = m_tetris2::bb::Helpers<rule_qq::TetrisRule::rule_spec>;
        fill_next();
        auto sp = rule_qq::TetrisRule::rule_spec::spawn(next.front(), tetris_ai.width(), tetris_ai.height());
        auto status = m_tetris2::TetrisBlockStatus(next.front(), static_cast<int8_t>(sp.first), static_cast<int8_t>(sp.second), 0);
        // æ„å»ºä½æ¿åˆå§‹çŠ¶æ€
        std::array<H::map_t, H::kMaxR> usable_arr{};
        auto board = m_tetris2::bb::build_board_for_search<rule_qq::TetrisRule::rule_spec>(map);
        H::build_usable_for_piece(status.t, board, usable_arr);
        auto cs = H::state_from_status(status.t, status.r, status.x, status.y);
        // game over æ£€æµ‹ï¼šé¡¶è¡Œæœ‰æ–¹å— æˆ– åˆå§‹ä½ç½®ä¸åˆæ³•
        if (map.row[tetris_ai.height() - 1] != 0 || !H::usable_at_bb(cs.r, cs.xb, cs.yb, usable_arr))
        {
            return -1;
        }
        std::memset(path, 0, sizeof path);
        next.push_back(0);
        ((ai_run_t)ai)(tetris_ai.width(), tetris_ai.height(), reinterpret_cast<int *>(map.row), next.data(), status.x, status.y, (4 - status.r) % 4, 10, 0, path, limit);
        char *move = path, *move_end = path + sizeof path;
        next.pop_back();
        next.erase(next.begin());
        // çº¯ä½æ¿è·¯å¾„æ¨¡æ‹Ÿ
        m_tetris2::bb::sim_path_bb<H>(cs, usable_arr, move, move_end);
        // ç¡¬é™è½åœ°
        if (auto landed = H::drop_bb_state(cs, usable_arr))
            cs = *landed;
        // attachï¼šç”¨ ad-hoc lp wrapper æ»¡è¶³ engine.attach çš„ lp.state è¦æ±‚
        struct LpWrapper { m_tetris2::bb::BBState state; };
        LpWrapper lp{cs};
        size_t clear = tetris_ai.attach(lp, map);
        if (clear >= 3)
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
            if (next_index < 7)
            {
                next.push_back(tetris_ai.convert(static_cast<size_t>(next_index)));
            }
        } while (next.size() <= next_length);
    }

    void new_game(size_t seed)
    {
        random.reset(seed);
        map.width = tetris_ai.width();
        map.height = tetris_ai.height();
        map.count = 0;
        map.roof = 0;
        std::memset(map.top, 0, sizeof map.top);
        std::memset(map.row, 0, sizeof map.row);
        next.clear();
        fill_next();
    }
};

int speed_test(unsigned int argc, wchar_t *argv[], wchar_t *eve[]);

//ÕâÊÇÒ»¸ö¼ÓÔØdll²âÊÔÊı¾İµÄ¿ØÖÆÌ¨,ÓÅÏÈµ÷ÓÃAIPath,ÕÒ²»µ½Ôòµ÷ÓÃAI
int wmain(unsigned int argc, wchar_t *argv[], wchar_t *eve[])
{
    ege::mtsrand(unsigned int(time(nullptr)));
    if (argc < 3)
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
    char box_0[3] = "¡õ";
    char box_1[3] = "¡ö";

    CONSOLE_CURSOR_INFO cursorInfo = {1, FALSE}; // ¹â±êĞÅÏ¢
    SetConsoleCursorInfo(GetStdHandle(STD_OUTPUT_HANDLE), &cursorInfo); // ÉèÖÃ¹â±êÒş²Ø

    while (true)
    {
        static COORD cd;
        cd.X = 0;
        cd.Y = 0;
        SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), cd);
        out[0] = '\0';
        m_tetris2::TetrisMap map[2] = {game[0].map, game[1].map};
        auto sp = rule_qq::TetrisRule::rule_spec::spawn(game[0].next.front(), game[0].tetris_ai.width(), game[0].tetris_ai.height());
        auto cs = m_tetris2::bb::Helpers<rule_qq::TetrisRule::rule_spec>::state_from_status(game[0].next.front(), 0, sp.first, sp.second);
        auto lp = m_tetris2::BBLandPoint(cs);
        game[0].tetris_ai.attach(lp, map[0]);
        game[0].tetris_ai.attach(lp, map[1]);
        for (int y = 20; y >= 0; --y)
        {
            for (int x = 0; x < 12; ++x)
            {
                strcat_s(out, map[0].full(x, y) ? box_1 : box_0);
            }
            strcat_s(out, "           ");
            for (int x = 0; x < 12; ++x)
            {
                strcat_s(out, map[1].full(x, y) ? box_1 : box_0);
            }
            strcat_s(out, "\r\n");
        }
        strcat_s(out, "\r\n");
        printf(out);

        int attack_0 = game[0].run(166);
        int attack_1 = game[1].run(166);
        if (attack_0 == -1 || attack_1 == -1)
        {
            if (attack_0 == -1)
                ++win[1];
            if (attack_1 == -1)
                ++win[0];
#pragma warning(push)
#pragma warning(disable : 4996)
            wchar_t BUFFER[1024];
            std::swprintf(BUFFER, L"[%s][±È·Ö%d:%d][Ğ§ÂÊ%f:%f]", (attack_0 == -1 ? attack_1 == -1 ? TEXT("Æ½¾Ö") : argv[2] : argv[1]), win[0], win[1], attack[0] / total, attack[1] / total);
            SetWindowText(GetConsoleWindow(), BUFFER);
#pragma warning(pop)
            new_seed = ege::mtirand();
            game[0].new_game(new_seed);
            game[1].new_game(new_seed);
        }
        else
        {
            total += 4;
            if (attack_0 > 0)
            {
                attack[0] += (attack_0 + 1) * 12;
                game[1].under_attack(attack_0);
            }
            if (attack_1 > 0)
            {
                attack[1] += (attack_1 + 1) * 12;
                game[0].under_attack(attack_1);
            }
        }
    }
}

m_tetris2::TetrisEngine2<rule_st::TetrisRule, ai_zzz::Dig, search_simple::Search> tetris_ai;

//ÕâÊÇÒ»¸ö¼ÓÔØdll²âÊÔÊı¾İµÄ¿ØÖÆÌ¨,ÓÅÏÈµ÷ÓÃAIPath,ÕÒ²»µ½Ôòµ÷ÓÃAI
int speed_test(unsigned int argc, wchar_t *argv[], wchar_t *eve[])
{
    attach_init();
    if (argc < 2)
    {
        return 0;
    }
    HMODULE hDll = LoadLibrary(argv[1]);
    if (hDll == nullptr)
    {
        return 0;
    }
    void *name = nullptr;
    name = GetProcAddress(hDll, "_Name@0");
    if (name == nullptr)
    {
        name = GetProcAddress(hDll, "Name@0");
    }
    if (name == nullptr)
    {
        name = GetProcAddress(hDll, "Name");
    }
    void *ai[2] = {};
    ai[0] = GetProcAddress(hDll, "_AIPath@36");
    if (ai[0] == NULL)
    {
        ai[0] = GetProcAddress(hDll, "AIPath@36");
    }
    if (ai[0] == NULL)
    {
        ai[0] = GetProcAddress(hDll, "AIPath");
    }
    ai[1] = GetProcAddress(hDll, "_AI@40");
    if (ai[1] == NULL)
    {
        ai[1] = GetProcAddress(hDll, "AI@40");
    }
    if (ai[1] == NULL)
    {
        ai[1] = GetProcAddress(hDll, "AI");
    }

    if (name == nullptr)
    {
        return 0;
    }
    int version = -1;
    for (int i = 0; i < sizeof ai / sizeof ai[0]; ++i)
    {
        if (ai[i] != nullptr)
        {
            version = i;
            break;
        }
    }
    if (version == -1)
    {
        return 0;
    }
    SetWindowTextA(GetConsoleWindow(), ((char const *(*)())name)());
    int w = 10, h = 20;
    m_tetris2::TetrisMap map(w, h);
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

    using H = m_tetris2::bb::Helpers<rule_st::TetrisRule::rule_spec>;
    // st LpWrapperï¼Œä¾› engine.attach æ¡¥æ¥
    struct LpWrapper { m_tetris2::bb::BBState state; };
    ege::mtrandom st_random;

    while (true)
    {
        char cur_t = tetris_ai.convert(static_cast<size_t>(st_random.rand() % tetris_ai.type_max()));
        auto sp = rule_st::TetrisRule::rule_spec::spawn(cur_t, w, h);
        m_tetris2::TetrisBlockStatus status(cur_t, static_cast<int8_t>(sp.first), static_cast<int8_t>(sp.second), 0);
        // æ„å»ºä½æ¿åˆå§‹çŠ¶æ€
        std::array<H::map_t, H::kMaxR> usable_arr{};
        auto board = m_tetris2::bb::build_board_for_search<rule_st::TetrisRule::rule_spec>(map);
        H::build_usable_for_piece(status.t, board, usable_arr);
        auto cs = H::state_from_status(status.t, status.r, status.x, status.y);
        log_new_time = clock();
        if (log_new_time - log_time > log_interval)
        {
            printf("{\"time\":%.2lf,\"current\":%lld,\"rows_ps\":%lld,\"piece_ps\":%lld}\n", (log_new_time - log_start) / 1000., this_lines, log_rows * 1000 / log_interval, log_piece * 1000 / log_interval);
            log_time += log_interval;
            log_rows = 0;
            log_piece = 0;
        }
        // game over æ£€æµ‹ï¼šåˆå§‹ä½ç½®ä¸åˆæ³•
        if (!H::usable_at_bb(cs.r, cs.xb, cs.yb, usable_arr))
        {
            total_lines += this_lines;
            if (this_lines > max_line)
            {
                max_line = this_lines;
            }
            ++game_count;
            printf("{\"avg\":%.2lf,\"max\":%lld,\"count\":%lld,\"current\":%lld}\n", game_count == 0 ? 0. : double(total_lines) / game_count, max_line, game_count, this_lines);
            this_lines = 0;
            map.count = 0;
            map.roof = 0;
            std::memset(map.top, 0, sizeof map.top);
            std::memset(map.row, 0, sizeof map.row);
            // åœ°å›¾æ¸…ç©ºåé‡å»º usable_arr
            board = m_tetris2::bb::build_board_for_search<rule_st::TetrisRule::rule_spec>(map);
            H::build_usable_for_piece(status.t, board, usable_arr);
        }
        for (int y = 0; y < h; ++y)
        {
            int row = y * w;
            for (int x = 0; x < w; ++x)
            {
                param_map[x + row] = map.full(x, y) ? '1' : '0';
            }
        }
        if (version == 0)
        {
            std::memset(path, 0, 1024);
            typedef int(__stdcall * ai_run_t)(int boardW, int boardH, char board[], char curPiece, int curX, int curY, int curR, char *nextPiece, char path[]);
            char next[] = {'\0'};
            ((ai_run_t)ai[version])(w, h, param_map, status.t, status.x + 1, status.y + 1, status.r + 1, next, path);
            char *move = path, *move_end = path + 1024;
            // çº¯ä½æ¿è·¯å¾„æ¨¡æ‹Ÿ
            m_tetris2::bb::sim_path_bb<H>(cs, usable_arr, move, move_end);
        }
        else
        {
            typedef int(__stdcall * ai_run_t)(int boardW, int boardH, char board[], char curPiece, int curX, int curY, int curR, char nextPiece, int *bestX, int *bestRotation);
            int best_x = status.x + 1, best_r = status.r + 1;
            ((ai_run_t)ai[version])(w, h, param_map, status.t, best_x, status.y + 1, best_r, ' ', &best_x, &best_r);
            --best_x;
            --best_r;
            // æ—‹è½¬åˆ°ç›®æ ‡ rï¼šä¼˜å…ˆ ccwï¼Œä¸å¤Ÿå† cw
            int cur_r = static_cast<int>(cs.r);
            while (best_r > cur_r)
            {
                if (auto kicked = H::first_passing_kick_bb(static_cast<char>(cs.t), m_tetris2::bb::KickDir::Ccw, cs, usable_arr))
                    cs = *kicked;
                else
                    break;
                cur_r = static_cast<int>(cs.r);
            }
            while (best_r < cur_r)
            {
                if (auto kicked = H::first_passing_kick_bb(static_cast<char>(cs.t), m_tetris2::bb::KickDir::Cw, cs, usable_arr))
                    cs = *kicked;
                else
                    break;
                cur_r = static_cast<int>(cs.r);
            }
            // å¹³ç§»åˆ°ç›®æ ‡ xï¼ˆmaster åæ ‡ï¼‰
            auto [cur_mx, cur_my] = H::master_xy_from_state(cs);
            while (best_x > cur_mx && H::usable_at_bb(cs.r, static_cast<int>(cs.xb) + 1, static_cast<int>(cs.yb), usable_arr))
            {
                cs.xb++;
                cur_mx++;
            }
            while (best_x < cur_mx && H::usable_at_bb(cs.r, static_cast<int>(cs.xb) - 1, static_cast<int>(cs.yb), usable_arr))
            {
                cs.xb--;
                cur_mx--;
            }
        }
        // ç¡¬é™è½åœ°
        if (auto landed = H::drop_bb_state(cs, usable_arr))
            cs = *landed;
        LpWrapper lp{cs};
        int clear = static_cast<int>(tetris_ai.attach(lp, map));
        this_lines += clear;
        log_rows += clear;
        ++log_piece;
    }
}