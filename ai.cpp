
#define DECLSPEC_EXPORT __declspec(dllexport)
#define WINAPI __stdcall

#include "tetris_core.h"


//����AI���֣�����ʾ�ڽ�����
extern "C" DECLSPEC_EXPORT char const *WINAPI Name()
{
    static std::string name = ai_name();
    return name.c_str();
}

//����ת��
void build_map(char board[], int w, int h, TetrisMap &map)
{
    memset(&map, 0, sizeof map);
    map.width = w;
    map.height = h;
    for(int y = 0, add = 0; y < h; ++y, add += w)
    {
        for(int x = 0; x < w; ++x)
        {
            if(board[x + add] == '1')
            {
                map.top[x] = map.roof = y + 1;
                map.row[y] |= 1 << x;
                ++map.count;
            }
        }
    }
}

/*
 * board��һ��boardW*boardH������01��ɵ��ַ�����ԭ�������½ǣ����к��С�
 * ����8*3�ĳ���ʵ����״��
 * 00000000
 * 00011001
 * 01111111
 * �����board������Ϊ��"011111110001100100000000"��
 *
 * Piece����ʹ���ַ� OISZLJT ���ո��ʾ������״��
 * nextPieceΪ' 'ʱ��ʾ��Ԥ����
 * curR�Ƿ�����1�ǳ�ʼ����2����ʱ��90�ȣ�3��180�ȣ�4��˳ʱ��90�ȡ�
 * curX,curY�����꣬���Ե�ǰ��4*4����������ڶ��У������ڶ���Ϊ��׼��
 *     ���½�Ϊx=1,y=1�����½�Ϊx=boardW,y=1�����Ͻ�Ϊx=1,y=boardH
 *     ���巽����״������һ��Ŀ¼�µ�pieces_orientations.jpg
 *
 * bestX,bestRotation ���ڷ�������λ�ã���curX,curR�Ĺ�����ͬ��
 *
 * ע�⣺�����������涨Ϊ����ת����ƽ�ƣ������¡�
 *       ���м����赲��AI����û���ж���ᵼ�´���ڷš�
 *       �ú����ڳ����·����ʱ�򱻵��ã�һ���������һ�Ρ�
 */
extern "C" DECLSPEC_EXPORT int WINAPI AI(int boardW, int boardH, char board[], char curPiece, int curX, int curY, int curR, char nextPiece, int *bestX, int *bestRotation)
{
    if(!init_ai(boardW, boardH))
    {
        return 0;
    }
    TetrisMap map;
    TetrisBlockStatus status =
    {
        curPiece, curX - 1, curY - 1, curR
    };
    int next_count = 0;
    unsigned char next;
    if(nextPiece != ' ')
    {
        next = nextPiece;
        next_count = 1;
    }
    build_map(board, boardW, boardH, map);
    auto result = ai_simple::do_ai(map, get(status), &next, next_count).first;

    if(result != nullptr)
    {
        *bestX = result->status.x + 1;
        *bestRotation = result->status.r;
    }
    return 0;
}

/*
 * path ���ڽ��ղ������̲����أ������ַ�����
 *      'l': ����һ��
 *      'r': ����һ��
 *      'd': ����һ��
 *      'L': ���Ƶ�ͷ
 *      'R': ���Ƶ�ͷ
 *      'D': ���Ƶ��ף�����ճ�ϣ��ɼ����ƶ���
 *      'z': ��ʱ����ת
 *      'c': ˳ʱ����ת
 * �ַ���ĩβҪ��'\0'����ʾ��ز�������Ӳ���䣩
 *
 * ������֧������·��������������Ҫ�˺���ֻ��ʹ������һ���Ļ�����ɾ������������
 */
extern "C" DECLSPEC_EXPORT int WINAPI AIPath(int boardW, int boardH, char board[], char curPiece, int curX, int curY, int curR, char nextPiece, char path[])
{
    if(!init_ai(boardW, boardH))
    {
        return 0;
    }
    TetrisMap map;
    TetrisBlockStatus status =
    {
        curPiece, curX - 1, curY - 1, curR
    };
    size_t next_length = 0;
    unsigned char next[] = {nextPiece, ' '};
    build_map(board, boardW, boardH, map);
    /////////////////////////////////////////////////
    //���������з�����,���ƱȽϲ��vp����
    int free_block = 0;
    for(int x = 0; x < map.width; ++x)
    {
        free_block += map.height - map.top[x];
    }
    if(free_block < map.width * 6)
    {
        next_length = 2;
    }
    else if(nextPiece != ' ' || free_block < map.width * 10)
    {
        next_length = 1;
    }
    /////////////////////////////////////////////////
    TetrisNode const *node = get(status);
    auto result = ai_path::do_ai(map, node, next, next_length);

    if(result.first != nullptr)
    {
        std::vector<char> ai_path = ai_path::make_path(node, result.first, map);
        memcpy(path, ai_path.data(), ai_path.size());
    }
    return 0;
}

#ifndef WINVER
#define WINVER 0x0500
#endif

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0501
#endif
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <ctime>

//����һ������dll�������ݵĿ���̨,���ȵ���AIPath,�Ҳ��������AI
int wmain(unsigned int argc, wchar_t *argv[], wchar_t *eve[])
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
    TetrisMap map =
    {
        {}, {}, w, h
    };
    char *param_map = new char[w * h];
    char *path = new char[1024];
    init_ai(w, h);
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
        TetrisNode const *node = generate(map);
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
            typedef int(__stdcall *ai_run_t)(int boardW, int boardH, char board[], char curPiece, int curX, int curY, int curR, char nextPiece, char path[]);
            ((ai_run_t)ai[version])(w, h, param_map, node->status.t, node->status.x + 1, node->status.y + 1, node->status.r, ' ', path);
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
            int best_x = node->status.x + 1, best_r = node->status.r;
            ((ai_run_t)ai[version])(w, h, param_map, node->status.t, best_x, node->status.y + 1, best_r, ' ', &best_x, &best_r);
            --best_x;
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