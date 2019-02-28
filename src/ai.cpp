
#define DECLSPEC_EXPORT __declspec(dllexport)
#define WINAPI __stdcall

#include <ctime>
#include "tetris_core.h"
#include "search_simple.h"
#include "search_path.h"
#include "search_simulate.h"
#include "search_cautious.h"
#include "search_tspin.h"
#include "ai_ax.h"
#include "ai_zzz.h"
#include "ai_tag.h"
#include "ai_farter.h"
#include "ai_misaka.h"
#include "rule_st.h"
#include "rule_qq.h"
#include "rule_srs.h"
#include "rule_toj.h"
#include "rule_c2.h"
#include "random.h"

m_tetris::TetrisEngine<rule_st::TetrisRule, ai_zzz::Dig, search_path::Search> tetris_ai;
//m_tetris::TetrisEngine<rule_st::TetrisRule, ai_ax::AI, search_simple::Search> tetris_ai;
//m_tetris::TetrisEngine<rule_st::TetrisRule, ai_farteryhr::AI, search_simple::Search> tetris_ai;

extern "C" void attach_init()
{
    ege::mtsrand(unsigned int(time(nullptr)));
}

//返回AI名字，会显示在界面上
extern "C" DECLSPEC_EXPORT char const *WINAPI Name()
{
    static std::string name = tetris_ai.ai_name();
    return name.c_str();
}

/*
 ***********************************************************************************************
 * 用于多next版本的ST...我自己MOD过的...参与比赛http://misakamm.com/blog/504请参照demo.cpp的AIPath
 ***********************************************************************************************
 * path 用于接收操作过程并返回，操作字符集：
 *      'l': 左移一格
 *      'r': 右移一格
 *      'd': 下移一格
 *      'L': 左移到头
 *      'R': 右移到头
 *      'D': 下移到底（但不粘上，可继续移动）
 *      'z': 逆时针旋转
 *      'c': 顺时针旋转
 * 字符串末尾要加'\0'，表示落地操作（或硬降落）
 *
 * 本函数支持任意路径操作，若不需要此函数只想使用上面一个的话，则删掉本函数即可
 *
 ***********************************************************************************************
 * 将此文件(ai.cpp)从工程排除,增加demo.cpp进来就可以了.如果直接使用标准的ST调用...会发生未定义的行为!
 ***********************************************************************************************
 */
extern "C" DECLSPEC_EXPORT int WINAPI AIPath(int boardW, int boardH, char board[], char curPiece, int curX, int curY, int curR, char const *nextPiece, char path[])
{
    if(!tetris_ai.prepare(boardW, boardH))
    {
        return 0;
    }
    m_tetris::TetrisMap map(boardW, boardH);
    for(int y = 0, add = 0; y < boardH; ++y, add += boardW)
    {
        for(int x = 0; x < boardW; ++x)
        {
            if(board[x + add] == '1')
            {
                map.top[x] = map.roof = y + 1;
                map.row[y] |= 1 << x;
                ++map.count;
            }
        }
    }
    m_tetris::TetrisBlockStatus status(curPiece, curX - 1, curY - 1, curR - 1);
    std::string next(nextPiece);
    m_tetris::TetrisNode const *node = tetris_ai.get(status);
    auto target = tetris_ai.run(map, node, next.data(), next.size(), 49).target;
    if(target != nullptr)
    {
        std::vector<char> ai_path = tetris_ai.make_path(node, target, map);
        std::memcpy(path, ai_path.data(), ai_path.size());
        path[ai_path.size()] = '\0';
    }
    return 0;
}

m_tetris::TetrisEngine<rule_toj::TetrisRule, ai_zzz::TOJ, search_tspin::Search> srs_ai;

extern "C" DECLSPEC_EXPORT int __cdecl AIDllVersion()
{
    return 2;
}

extern "C" DECLSPEC_EXPORT char *__cdecl AIName(int level)
{
    static char name[200];
    strcpy_s(name, srs_ai.ai_name().c_str());
    return name;
}

/*
all 'char' type is using the characters in ' ITLJZSO'

field data like this:
00........   -> 0x3
00.0......   -> 0xb
00000.....   -> 0x1f

b2b: the count of special attack, the first one set b2b=1, but no extra attack. Have extra attacks when b2b>=2
combo: first clear set combo=1, so the comboTable in toj rule is [0, 0, 0, 1, 1, 2, 2, 3, ...]
next: array size is 'maxDepth'
x, y, spin: the active piece's x/y/orientation,
x/y is the up-left corner's position of the active piece.
see tetris_gem.cpp for the bitmaps.
curCanHold: indicates whether you can use hold on current move.
might be caused by re-think after a hold move.
canhold: false if hold is completely disabled.
comboTable: -1 is the end of the table.
*/
extern "C" DECLSPEC_EXPORT char *__cdecl TetrisAI(int overfield[], int field[], int field_w, int field_h, int b2b, int combo, char next[], char hold, bool curCanHold, char active, int x, int y, int spin, bool canhold, bool can180spin, int upcomeAtt, int comboTable[], int maxDepth, int level, int player)
{
    static char result_buffer[8][1024];
    char *result = result_buffer[player];

    if(field_w != 10 || field_h != 22 || !srs_ai.prepare(10, 40))
    {
        *result = '\0';
        return result;
    }
    m_tetris::TetrisMap map(10, 40);
    for(size_t d = 0, s = 22; d < 23; ++d, --s)
    {
        map.row[d] = field[s];
    }
    for(size_t d = 23, s = 0; s < 8; ++d, ++s)
    {
        map.row[d] = overfield[s];
    }
    for(int my = 0; my < map.height; ++my)
    {
        for(int mx = 0; mx < map.width; ++mx)
        {
            if(map.full(mx, my))
            {
                map.top[mx] = map.roof = my + 1;
                map.row[my] |= 1 << mx;
                ++map.count;
            }
        }
    }
    srs_ai.update();
    srs_ai.search_config()->allow_180 = can180spin;
    srs_ai.search_config()->last_rotate = false;
    srs_ai.ai_config()->p =
    {
        2.21814, 0.391674, -0.0358847, 0.678461, 3.39133, 0.859429, -0.201051, 110.951, -0.572156, 111.773, -3.35123, 113.878, -0.633104, 47.1717, -1.54499, 396.14, -4.83616, 95.5258, -1.62344, 63.8952, 1.95443, 45990.6, 34.1561, 0.257397,
    };
    srs_ai.ai_config()->p_rate = 1;
    srs_ai.ai_config()->table = comboTable;
    srs_ai.ai_config()->table_max = [comboTable]()->size_t
    {
        size_t max = 0;
        while(comboTable[max++] != -1)
            ;
        return max - 1;
    }();
    srs_ai.status()->total_attack = combo;
    srs_ai.status()->death = 0;
    srs_ai.status()->combo = combo;
    srs_ai.status()->attack = 0;
    srs_ai.status()->under_attack = upcomeAtt;
    srs_ai.status()->map_rise = 0;
    srs_ai.status()->b2b = !!b2b;
    srs_ai.status()->like = 0;
    srs_ai.status()->value = 0;
    m_tetris::TetrisBlockStatus status(active, x, 22 - y, (4 - spin) % 4);
    m_tetris::TetrisNode const *node = srs_ai.get(status);
    static double const base_time = std::pow(100, 1.0 / 8);
    if(canhold)
    {
        auto run_result = srs_ai.run_hold(map, node, hold, curCanHold, next, maxDepth, time_t(std::pow(base_time, level)));
        if(run_result.change_hold)
        {
            result++[0] = 'v';
            if(run_result.target != nullptr)
            {
                std::vector<char> ai_path = srs_ai.make_path(srs_ai.context()->generate(run_result.target->status.t), run_result.target, map);
                std::memcpy(result, ai_path.data(), ai_path.size());
                result += ai_path.size();
                result++[0] = 'V';
            }
        }
        else
        {
            if(run_result.target != nullptr)
            {
                std::vector<char> ai_path = srs_ai.make_path(node, run_result.target, map);
                std::memcpy(result, ai_path.data(), ai_path.size());
                result += ai_path.size();
            }
            result++[0] = 'V';
        }
    }
    else
    {
        auto target = srs_ai.run(map, node, next, maxDepth, time_t(std::pow(base_time, level))).target;
        if(target != nullptr)
        {
            std::vector<char> ai_path = srs_ai.make_path(node, target, map);
            std::memcpy(result, ai_path.data(), ai_path.size());
            result += ai_path.size();
        }
        result++[0] = 'V';
    }
    result[1] = '\0';
    return result_buffer[player];
}

class QQTetrisSearch
{

public:
    enum Config
    {
        Simple, Simulate, Path
    };
    void init(m_tetris::TetrisContext const *context, Config const *config)
    {
        simple_.init(context);
        simulate_.init(context);
        path_.init(context);
        config_ptr = config;
    }
    std::vector<char> make_path(m_tetris::TetrisNode const *node, m_tetris::TetrisNode const *land_point, m_tetris::TetrisMap const &map)
    {
        switch(*config_ptr)
        {
        case Simple:
            return simple_.make_path(node, land_point, map);
        case Simulate:
            return simulate_.make_path(node, land_point, map);
        case Path:
            return path_.make_path(node, land_point, map);
        default:
            return std::vector<char>();
        }
    }
    std::vector<m_tetris::TetrisNode const *> const *search(m_tetris::TetrisMap const &map, m_tetris::TetrisNode const *node, size_t depth)
    {
        switch(*config_ptr)
        {
        case Simple:
            return simple_.search(map, node, depth);
        case Simulate:
            return simulate_.search(map, node, depth);
        case Path:
            return path_.search(map, node, depth);
        default:
            empty_.resize(1);
            empty_.front() = node->drop(map);
            return &empty_;
        }
    }
private:
    Config const *config_ptr;
    search_simple::Search simple_;
    search_simulate::Search simulate_;
    search_path::Search path_;
    std::vector<m_tetris::TetrisNode const *> empty_;
};
m_tetris::TetrisEngine<rule_qq::TetrisRule, ai_zzz::qq::Attack, QQTetrisSearch> qq_ai;

extern "C" DECLSPEC_EXPORT int __cdecl QQTetrisAI(int boardW, int boardH, int board[], char nextPiece[], int curX, int curY, int curR, int level, int mode, char path[], size_t limit)
{
    if(!qq_ai.prepare(boardW, boardH))
    {
        *path = '\0';
        return 0;
    }
    m_tetris::TetrisMap map(boardW, boardH);
    std::memcpy(map.row, board, boardH * sizeof(int));
    for(int my = 0; my < map.height; ++my)
    {
        for(int mx = 0; mx < map.width; ++mx)
        {
            if(map.full(mx, my))
            {
                map.top[mx] = map.roof = my + 1;
                map.row[my] |= 1 << mx;
                ++map.count;
            }
        }
    }
    m_tetris::TetrisBlockStatus status(nextPiece[0], curX, curY, (4 - curR) % 4);
    size_t next_length = std::strlen(nextPiece) - 1;
    if(level < 10)
    {
        next_length = std::min<size_t>(level, next_length);
    }
    std::string next_str(nextPiece + 1, nextPiece + 1 + next_length);
    if(next_length <= 2)
    {
        std::string next_new = "?";
        for(auto c : next_str)
        {
            next_new += c;
            next_new += '?';
        }
        next_str.swap(next_new);
    }
    if(level == 10)
    {
        *qq_ai.search_config() = QQTetrisSearch::Path;
    }
    else if(mode == 0 || map.count <= boardW * 2)
    {
        *qq_ai.search_config() = QQTetrisSearch::Simulate;
    }
    else
    {
        *qq_ai.search_config() = QQTetrisSearch::Simple;
    }
    qq_ai.ai_config()->level = level;
    qq_ai.ai_config()->mode = mode;
    qq_ai.status()->land_point = 0;
    qq_ai.status()->attack = 0;
    qq_ai.status()->rubbish = 0;
    qq_ai.status()->value = 0;
    m_tetris::TetrisNode const *node = qq_ai.get(status);
    while(node == nullptr && status.y > 0)
    {
        --status.y;
        node = qq_ai.get(status);
    }
    auto target = qq_ai.run(map, node, next_str.data(), next_str.length(), 60).target;
    std::vector<char> ai_path;
    if(target != nullptr)
    {
        ai_path = qq_ai.make_path(node, target, map);
        std::memcpy(path, ai_path.data(), ai_path.size());
    }
    path[ai_path.size()] = 'V';
    path[ai_path.size() + 1] = '\0';
    return 0;
}

m_tetris::TetrisEngine<rule_c2::TetrisRule, ai_zzz::C2, search_cautious::Search> c2_ai;

struct c2_out_put
{
    char move;
    int8_t x;
    int8_t y;
    uint8_t r;
};

struct c2_param
{
    int boardW;
    int boardH;
    int const *board;
    char const *nextPiece;
    int curX, curY, curR;
    int safe;
    int combo;
    int combo_limit;
    int danger;
    c2_out_put *path;
    size_t limit;
    int mode;
    int vp;
    int soft_drop;
};


extern "C" DECLSPEC_EXPORT int __cdecl C2TetrisAI(c2_param *param)
{
    int const &boardW = param->boardW;
    int const &boardH = param->boardH;
    int const *board = param->board;
    char const *nextPiece = param->nextPiece;
    int const &curX = param->curX, &curY = param->curY, &curR = param->curR;
    int const &safe = param->safe;
    int const &combo = param->combo;
    int const &combo_limit = param->combo_limit;
    int const &danger = param->danger;
    c2_out_put *path = param->path;
    size_t const &limit = param->limit;
    int const &mode = param->mode;
    int const &vp = param->vp;
    int const &soft_drop = param->soft_drop;
    if(!c2_ai.prepare(boardW, boardH))
    {
        path[0] = {'\0'};
        return 0;
    }
    m_tetris::TetrisMap map(boardW, boardH);
    std::memcpy(map.row, board, boardH * sizeof(int));
    for(int my = 0; my < map.height; ++my)
    {
        for(int mx = 0; mx < map.width; ++mx)
        {
            if(map.full(mx, my))
            {
                map.top[mx] = map.roof = my + 1;
                map.row[my] |= 1 << mx;
                ++map.count;
            }
        }
    }
    c2_ai.search_config()->fast_move_down = true;
    c2_ai.ai_config()->p =
    {
        //-0.386388, 0.241602, -1.10822, 0.465469, 0.10297, 0.617807, 0.31237, 102.649, -0.377377, 144.472, -1.69091, 130.465, 0.430648, 52.6962, -0.278674, 372.157, -0.371747, 89.3903, -0.765743, 43.436, -0.336645, 56379.1, 32.0632, 0.165945,
        //-1.49234, 0.194204, -1.43921, 0.334867, -1.38877, 0.624287, 1.53609, 102.186, 0.583796, 145.918, -1.95346, 129.961, 0.398329, 54.7758, -0.0166411, 372.462, -0.804703, 93.4535, -0.605844, 36.112, 0.447049, 41668.3, 31.8163, 0.158805,
        //-0.184721, 0.142187, -1.17734, 0.545641, 0.512126, 0.662053, -0.182696, 101.324, -0.909635, 145.311, -1.82185, 129.38, -0.565261, 48.0936, 1.139, 373.169, -0.800861, 87.9299, 0.576996, 37.9227, -0.129241, 42659.7, 32.1907, 0.165502,
        //-2.41102, 0.166163, -1.05491, 0.726452, 0.549544, 0.850309, -0.38095, 112.008, 1.09258, 111.481, -1.25704, 120.945, -1.84823, 39.6381, -0.999686, 381.848, -2.46182, 97.2093, -1.59963, 57.4471, 0.788324, 46215.5, 33.1116, 0.196942,
        //1.40044, 0.422027, -0.597924, 0.762662, 1.28441, 0.799014, -0.821158, 110.245, -1.13942, 111.409, -3.91054, 120.97, -1.10519, 36.2403, 0.723563, 388.515, -2.308, 91.1307, -2.28869, 54.5053, 1.98468, 45673.7, 32.4859, 0.185176,
        //1.65199, 0.396285, -0.176574, 0.730439, 3.00091, 0.848699, -0.41829, 108.659, -1.31244, 105.971, -4.53815, 114.038, -0.497036, 38.709, -0.299433, 390.707, -3.41728, 94.7167, -2.01878, 64.355, 0.444409, 45809, 34.0261, 0.192355,
        //2.21814, 0.391674, -0.0358847, 0.678461, 3.39133, 0.859429, -0.201051, 110.951, -0.572156, 111.773, -3.35123, 113.878, -0.633104, 47.1717, -1.54499, 396.14, -4.83616, 95.5258, -1.62344, 63.8952, 1.95443, 45990.6, 34.1561, 0.257397,
        2.87224, 0.372169, 0.102604, 0.723501, 3.08721, 0.802789, -0.786174, 107.713, -0.540719, 109.116, -3.84305, 116.58, -1.00066, 49.7899, -1.23986, 391.808, -4.30493, 91.0623, -1.60608, 67.7934, 2.36365, 46016.9, 34.2515, 0.285739,
    };
    c2_ai.ai_config()->p_rate = 1;
    c2_ai.ai_config()->safe = safe;
    c2_ai.ai_config()->mode = mode;
    c2_ai.ai_config()->danger = danger;
    c2_ai.ai_config()->soft_drop = soft_drop;
    c2_ai.status()->combo = combo;
    c2_ai.status()->combo_limit = combo_limit;
    c2_ai.status()->value = 0;
    m_tetris::TetrisBlockStatus status(nextPiece[0], curX, curY, curR);
    size_t next_length = nextPiece[1] == ' ' ? 0 : 1;
    std::string next;
    if(vp)
    {
        next += '?';
    }
    for(char const *n = nextPiece + 1, *const ne = nextPiece + 1 + next_length; n != ne; ++n)
    {
        next += *n;
        if(vp)
        {
            next += '?';
        }
    }
    m_tetris::TetrisNode const *node = c2_ai.get(status);
    auto target = c2_ai.run(map, node, next.data(), next.size(), limit).target;
    std::vector<char> ai_path;
    size_t size = 0;
    if(target != nullptr)
    {
        ai_path = c2_ai.make_path(node, target, map);
        node->open(map);
        for(char c : ai_path)
        {
            switch(c)
            {
            case 'L':
                while(node->move_left != nullptr && node->move_left->check(map))
                {
                    node = node->move_left;
                }
                break;
            case 'R':
                while(node->move_right != nullptr && node->move_right->check(map))
                {
                    node = node->move_right;
                }
                break;
            case 'D':
                node = node->drop(map);
                break;
            case 'l':
                if(node->move_left != nullptr && node->move_left->check(map))
                {
                    node = node->move_left;
                }
                break;
            case 'r':
                if(node->move_right != nullptr && node->move_right->check(map))
                {
                    node = node->move_right;
                }
                break;

            case 'z':
                for(auto wall_kick_node : node->wall_kick_counterclockwise)
                {
                    if(wall_kick_node)
                    {
                        if(wall_kick_node->check(map))
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
            case 'x':
                for(auto wall_kick_node : node->wall_kick_opposite)
                {
                    if(wall_kick_node)
                    {
                        if(wall_kick_node->check(map))
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
            case 'c':
                for(auto wall_kick_node : node->wall_kick_clockwise)
                {
                    if(wall_kick_node)
                    {
                        if(wall_kick_node->check(map))
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
            path[size++] = {c, node->status.x, node->status.y, node->status.r};
        }
    }
    if(size == 0)
    {
        path[size++] = {'V', int8_t(curX), int8_t(curY), uint8_t(curR)};
    }
    else
    {
        path[size] = path[size - 1];
        path[size++].move = 'V';
    }
    path[size++] = {'\0'};
    return target == nullptr ? 0 : target->attach(map);
}
