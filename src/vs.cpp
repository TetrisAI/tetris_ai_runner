
#ifdef _WIN32
#  define DECLSPEC_EXPORT __declspec(dllexport)
#  define WINAPI __stdcall
#  ifndef WINVER
#    define WINVER 0x0500
#  endif
#  ifndef _WIN32_WINNT
#    define _WIN32_WINNT 0x0501
#  endif
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#else
#  define DECLSPEC_EXPORT __attribute__((visibility("default")))
#  define WINAPI
#  include <dlfcn.h>
#  include <unistd.h>
#  include <cstdio>
// POSIX 兼容声明
using HMODULE = void *;
inline HMODULE LoadLibraryW(wchar_t const *path)
{
    // wchar_t -> char 转换（假设 ASCII 路径）
    char buf[4096] = {};
    size_t i = 0;
    for (; path[i] != L'\0' && i + 1 < sizeof buf; ++i)
    {
        buf[i] = static_cast<char>(path[i]);
    }
    buf[i] = '\0';
    return dlopen(buf, RTLD_LAZY);
}
inline HMODULE LoadLibraryA(char const *path) { return dlopen(path, RTLD_LAZY); }
inline HMODULE LoadLibrary(wchar_t const *path) { return LoadLibraryW(path); }
inline void FreeLibrary(HMODULE h) { dlclose(h); }
inline void *GetProcAddress(HMODULE h, char const *sym) { return dlsym(h, sym); }
#endif

#include <ctime>
#include <cstdlib>
#include <cstring>
#include <cwchar>
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
#include "movegen_sim.h"

extern "C" void attach_init()
{
    ege::mtsrand(static_cast<unsigned int>(time(nullptr)));
}

struct tetris_game
{
    typedef int (*ai_run_t)(int boardW, int boardH, int board[], char nextPiece[], int curX, int curY, int curR, int level, int mode, char path[], size_t limit);
    m_tetris2::TetrisEngine<rule_qq::TetrisRule, ai_zzz::qq::Attack, path::Search> tetris_ai;
    ege::mtrandom random;
    decltype(tetris_ai)::map_t map;
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
        constexpr int w = decltype(map)::width;
        constexpr int h = decltype(map)::height;
        for (int y = h - 1; y >= static_cast<int>(line); --y)
        {
            map.set_row(y, map.row(y - static_cast<int>(line)));
        }
        for (int y = 0; y < static_cast<int>(line); ++y)
        {
            map.set_row(y, 0);
            for (int x = 0; x < w; ++x)
            {
                if ((x + y) % 2 != 0)
                {
                    map.set(x, y);
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
        // 构建位板初始状态
        std::array<H::map_t, H::kMaxR> usable_arr{};
        auto board = m_tetris2::bb::build_board_for_search<rule_qq::TetrisRule::rule_spec>(map);
        H::build_usable_for_piece(status.t, board, usable_arr);
        auto cs = H::state_from_status(status.t, status.r, status.x, status.y);
        // game over 检测：顶行有方块 或 初始位置不合法
        if (map.row(tetris_ai.height() - 1) != 0 || !H::usable_at(cs.r, cs.xb, cs.yb, usable_arr))
        {
            return -1;
        }
        std::memset(path, 0, sizeof path);
        next.push_back(0);
        {
            // 旧版 QQ AI DLL 期望 int board[h*w]，逐格展开
            constexpr int bw = decltype(map)::width;
            constexpr int bh = decltype(map)::height;
            int board_buf[bh * bw] = {};
            for (int my = 0; my < bh; ++my)
                for (int mx = 0; mx < bw; ++mx)
                    board_buf[my * bw + mx] = map.get(mx, my) ? 1 : 0;
            ((ai_run_t)ai)(tetris_ai.width(), tetris_ai.height(), board_buf, next.data(), status.x, status.y, (4 - status.r) % 4, 10, 0, path, limit);
        }
        char *move = path, *move_end = path + sizeof path;
        next.pop_back();
        next.erase(next.begin());
        // 纯位板路径模拟
        m_tetris2::bb::sim_path<H>(cs, usable_arr, move, move_end);
        // 硬降落地
        if (auto landed = H::drop(cs, usable_arr))
            cs = *landed;
        // attach：用 ad-hoc lp wrapper 满足 engine.attach 的 lp.state 要求
        struct LpWrapper { m_tetris2::bb::State state; };
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
        map = decltype(map){};
        next.clear();
        fill_next();
    }
};

#ifdef _WIN32
int speed_test(unsigned int argc, wchar_t *argv[], wchar_t *eve[]);
#else
int speed_test(int argc, char *argv[]);
#endif

//创建一个读取两个dll对战数据的控制台，先调用AIPath，再尝试调用旧版AI
#ifdef _WIN32
int wmain(unsigned int argc, wchar_t *argv[], wchar_t *eve[])
#else
int main(int argc, char *argv[])
#endif
{
    ege::mtsrand(static_cast<unsigned int>(time(nullptr)));
    if (argc < 3)
    {
#ifdef _WIN32
        return speed_test(argc, argv, eve);
#else
        return speed_test(argc, argv);
#endif
    }
    tetris_game game[2];
#ifdef _WIN32
    game[0].init(12, 21, 13, argv[1]);
    game[1].init(12, 21, 13, argv[2]);
#else
    auto to_wstr = [](char const *s) {
        std::wstring w(std::strlen(s), L'\0');
        std::mbstowcs(w.data(), s, w.size());
        return w;
    };
    game[0].init(12, 21, 13, to_wstr(argc > 1 ? argv[1] : ""));
    game[1].init(12, 21, 13, to_wstr(argc > 2 ? argv[2] : ""));
#endif
    size_t new_seed = ege::mtirand();
    game[0].new_game(new_seed);
    game[1].new_game(new_seed);
    size_t win[2] = {};
    double total = 0;
    unsigned long long attack[2];

    char out[81920] = "";
    char const *box_0 = "□";
    char const *box_1 = "■";

#ifdef _WIN32
    CONSOLE_CURSOR_INFO cursorInfo = {1, FALSE};
    SetConsoleCursorInfo(GetStdHandle(STD_OUTPUT_HANDLE), &cursorInfo);
#else
    fputs("\033[?25l", stdout); // 隐藏光标
#endif

    while (true)
    {
#ifdef _WIN32
        static COORD cd;
        cd.X = 0;
        cd.Y = 0;
        SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), cd);
#else
        fputs("\033[0;0H", stdout); // 移光标到左上角
#endif
        out[0] = '\0';
        decltype(game[0].tetris_ai)::map_t map[2] = {game[0].map, game[1].map};
        auto sp = rule_qq::TetrisRule::rule_spec::spawn(game[0].next.front(), game[0].tetris_ai.width(), game[0].tetris_ai.height());
        auto cs = m_tetris2::bb::Helpers<rule_qq::TetrisRule::rule_spec>::state_from_status(game[0].next.front(), 0, sp.first, sp.second);
        auto lp = m_tetris2::BBLandPoint(cs);
        game[0].tetris_ai.attach(lp, map[0]);
        game[0].tetris_ai.attach(lp, map[1]);
        for (int y = 20; y >= 0; --y)
        {
            for (int x = 0; x < 12; ++x)
            {
                std::strcat(out, map[0].get(x, y) ? box_1 : box_0);
            }
            std::strcat(out, "           ");
            for (int x = 0; x < 12; ++x)
            {
                std::strcat(out, map[1].get(x, y) ? box_1 : box_0);
            }
            std::strcat(out, "\r\n");
        }
        std::strcat(out, "\r\n");
        printf(out);

        int attack_0 = game[0].run(166);
        int attack_1 = game[1].run(166);
        if (attack_0 == -1 || attack_1 == -1)
        {
            if (attack_0 == -1)
                ++win[1];
            if (attack_1 == -1)
                ++win[0];
#ifdef _WIN32
#  pragma warning(push)
#  pragma warning(disable : 4996)
            {
                wchar_t BUFFER[1024];
                std::swprintf(BUFFER, 1024, L"[%s][比分%d:%d][效率%f:%f]",
                    (attack_0 == -1 ? (attack_1 == -1 ? L"平局" : argv[2]) : argv[1]),
                    (int)win[0], (int)win[1], attack[0] / total, attack[1] / total);
                SetWindowText(GetConsoleWindow(), BUFFER);
            }
#  pragma warning(pop)
#else
            {
                char BUFFER[1024];
                const char *winner = (attack_0 == -1 ? (attack_1 == -1 ? "平局" : "AI-2") : "AI-1");
                std::snprintf(BUFFER, sizeof BUFFER, "[%s][比分%zu:%zu][效率%f:%f]",
                    winner, win[0], win[1], attack[0] / total, attack[1] / total);
                std::printf("\033]0;%s\007", BUFFER); // 设置终端窗口标题
            }
#endif
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

m_tetris2::TetrisEngine<rule_st::TetrisRule, ai_zzz::Dig, simple::Search> tetris_ai;

//创建一个读取两个dll对战数据的控制台，先调用AIPath，再尝试调用旧版AI
#ifdef _WIN32
int speed_test(unsigned int argc, wchar_t *argv[], wchar_t *eve[])
#else
int speed_test(int argc, char *argv[])
#endif
{
    attach_init();
    if (argc < 2)
    {
        return 0;
    }
#ifdef _WIN32
    HMODULE hDll = LoadLibrary(argv[1]);
#else
    HMODULE hDll = LoadLibraryA(argv[1]);
#endif
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
#ifdef _WIN32
    SetWindowTextA(GetConsoleWindow(), ((char const *(*)())name)());
#else
    std::printf("\033]0;%s\007", ((char const *(*)())name)());
#endif
    int w = 10, h = 20;
    decltype(tetris_ai)::map_t map;
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
    // st LpWrapper，供 engine.attach 桥接
    struct LpWrapper { m_tetris2::bb::State state; };
    ege::mtrandom st_random;

    while (true)
    {
        char cur_t = tetris_ai.convert(static_cast<size_t>(st_random.rand() % tetris_ai.type_max()));
        auto sp = rule_st::TetrisRule::rule_spec::spawn(cur_t, w, h);
        m_tetris2::TetrisBlockStatus status(cur_t, static_cast<int8_t>(sp.first), static_cast<int8_t>(sp.second), 0);
        // 构建位板初始状态
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
        // game over 检测：初始位置不合法
        if (!H::usable_at(cs.r, cs.xb, cs.yb, usable_arr))
        {
            total_lines += this_lines;
            if (this_lines > max_line)
            {
                max_line = this_lines;
            }
            ++game_count;
            printf("{\"avg\":%.2lf,\"max\":%lld,\"count\":%lld,\"current\":%lld}\n", game_count == 0 ? 0. : double(total_lines) / game_count, max_line, game_count, this_lines);
            this_lines = 0;
            map = decltype(map){};
            // 地图清空后重建 usable_arr
            board = m_tetris2::bb::build_board_for_search<rule_st::TetrisRule::rule_spec>(map);
            H::build_usable_for_piece(status.t, board, usable_arr);
        }
        for (int y = 0; y < h; ++y)
        {
            int row = y * w;
            for (int x = 0; x < w; ++x)
            {
                param_map[x + row] = map.get(x, y) ? '1' : '0';
            }
        }
        if (version == 0)
        {
            std::memset(path, 0, 1024);
            typedef int(WINAPI * ai_run_t)(int boardW, int boardH, char board[], char curPiece, int curX, int curY, int curR, char *nextPiece, char path[]);
            char next[] = {'\0'};
            ((ai_run_t)ai[version])(w, h, param_map, status.t, status.x + 1, status.y + 1, status.r + 1, next, path);
            char *move = path, *move_end = path + 1024;
            // 纯位板路径模拟
            m_tetris2::bb::sim_path<H>(cs, usable_arr, move, move_end);
        }
        else
        {
            typedef int(WINAPI * ai_run_t)(int boardW, int boardH, char board[], char curPiece, int curX, int curY, int curR, char nextPiece, int *bestX, int *bestRotation);
            int best_x = status.x + 1, best_r = status.r + 1;
            ((ai_run_t)ai[version])(w, h, param_map, status.t, best_x, status.y + 1, best_r, ' ', &best_x, &best_r);
            --best_x;
            --best_r;
            // 旋转到目标 r：优先 ccw，不够再 cw
            int cur_r = static_cast<int>(cs.r);
            while (best_r > cur_r)
            {
                if (auto kicked = H::first_kick(static_cast<char>(cs.t), m_tetris2::bb::KickDir::Ccw, cs, usable_arr))
                    cs = *kicked;
                else
                    break;
                cur_r = static_cast<int>(cs.r);
            }
            while (best_r < cur_r)
            {
                if (auto kicked = H::first_kick(static_cast<char>(cs.t), m_tetris2::bb::KickDir::Cw, cs, usable_arr))
                    cs = *kicked;
                else
                    break;
                cur_r = static_cast<int>(cs.r);
            }
            // 平移到目标 x（master 坐标）
            auto [cur_mx, cur_my] = H::master_xy_from_state(cs);
            while (best_x > cur_mx && H::usable_at(cs.r, static_cast<int>(cs.xb) + 1, static_cast<int>(cs.yb), usable_arr))
            {
                cs.xb++;
                cur_mx++;
            }
            while (best_x < cur_mx && H::usable_at(cs.r, static_cast<int>(cs.xb) - 1, static_cast<int>(cs.yb), usable_arr))
            {
                cs.xb--;
                cur_mx--;
            }
        }
        // 硬降落地
        if (auto landed = H::drop(cs, usable_arr))
            cs = *landed;
        LpWrapper lp{cs};
        int clear = static_cast<int>(tetris_ai.attach(lp, map));
        this_lines += clear;
        log_rows += clear;
        ++log_piece;
    }
}