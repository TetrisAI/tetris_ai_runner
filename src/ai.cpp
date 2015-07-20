
#define DECLSPEC_EXPORT __declspec(dllexport)
#define WINAPI __stdcall

#include <ctime>
#include "tetris_core.h"
#include "land_point_search_simple.h"
#include "land_point_search_path.h"
#include "land_point_search_simulate.h"
#include "land_point_search_cautious.h"
#include "land_point_search_tspin.h"
#include "ai_ax.h"
#include "ai_zzz.h"
#include "ai_tag.h"
#include "ai_farteryhr.h"
#include "rule_st.h"
#include "rule_qq.h"
#include "rule_srs.h"
#include "rule_toj.h"
#include "rule_c2.h"
#include "random.h"

m_tetris::TetrisEngine<rule_st::TetrisRuleSet, ai_tag::the_ai_games, land_point_search_path::Search> tetris_ai;
//m_tetris::TetrisEngine<rule_st::TetrisRuleSet, ai_zzz::qq::Attack, land_point_search_path::Search> tetris_ai;
//m_tetris::TetrisEngine<rule_st::TetrisRuleSet, ai_ax::AI, land_point_search_simple::Search> tetris_ai;
//m_tetris::TetrisEngine<rule_st::TetrisRuleSet, ai_farteryhr::AI, land_point_search_simple::Search> tetris_ai;

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
    m_tetris::TetrisMap map =
    {
        boardW, boardH
    };
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
    m_tetris::TetrisBlockStatus status =
    {
        curPiece, curX - 1, curY - 1, curR - 1
    };
    size_t next_length = std::strlen(nextPiece);
    /////////////////////////////////////////////////
    tetris_ai.param()->combo = 0;
    tetris_ai.param()->up = 0;
    tetris_ai.param()->length = next_length + 1;
    tetris_ai.param()->virtual_length = next_length > 0 ? 1 : 0;
    tetris_ai.param()->search = std::bind(&decltype(tetris_ai)::search<m_tetris::TetrisNode const *>, &tetris_ai, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
    //tetris_ai.param()->level = 10;
    //tetris_ai.param()->mode = 0;
    //tetris_ai.param()->next_length = next_length;
    /////////////////////////////////////////////////
    m_tetris::TetrisNode const *node = tetris_ai.get(status);
    auto target = tetris_ai.run(map, node, reinterpret_cast<unsigned char const *>(nextPiece), next_length, 999).target;
    if(target != nullptr)
    {
        std::vector<char> ai_path = tetris_ai.make_path(node, target, map);
        memcpy(path, ai_path.data(), ai_path.size());
        path[ai_path.size()] = '\0';
    }
    return 0;
}

m_tetris::TetrisEngine<rule_toj::TetrisRuleSet, ai_zzz::TOJ, land_point_search_tspin::Search> srs_ai;

extern "C" DECLSPEC_EXPORT int AIDllVersion()
{
    return 2;
}

extern "C" DECLSPEC_EXPORT char *AIName(int level)
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
extern "C" DECLSPEC_EXPORT char *TetrisAI(int overfield[], int field[], int field_w, int field_h, int b2b, int combo, char next[], char hold, bool curCanHold, char active, int x, int y, int spin, bool canhold, bool can180spin, int upcomeAtt, int comboTable[], int maxDepth, int level, int player)
{
    static char result_buffer[8][1024];
    char *result = result_buffer[player];

    if(field_w != 10 || field_h != 22 || !srs_ai.prepare(10, 40))
    {
        *result = '\0';
        return result;
    }
    m_tetris::TetrisMap map =
    {
        10, 40
    };
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
    srs_ai.status()->allow_180 = can180spin;
    srs_ai.param()->combo = combo;
    srs_ai.param()->under_attack = upcomeAtt;
    srs_ai.param()->b2b = !!b2b;
    srs_ai.param()->table = comboTable;
    srs_ai.param()->table_max = [comboTable]()->size_t
    {
        size_t max = 0;
        while(comboTable[max++] != -1)
            ;
        return max - 1;
    }();
    m_tetris::TetrisBlockStatus status =
    {
        active, x, 22 - y, (4 - spin) % 4
    };
    m_tetris::TetrisNode const *node = srs_ai.get(status);
    if(canhold)
    {
        auto run_result = srs_ai.run_hold(map, node, hold, curCanHold, reinterpret_cast<unsigned char const *>(next), maxDepth, level * 5 + 1);
        if(run_result.change_hold)
        {
            result++[0] = 'v';
            if(run_result.target != nullptr)
            {
                std::vector<char> ai_path = srs_ai.make_path(srs_ai.context()->generate(run_result.target->status.t), run_result.target, map);
                memcpy(result, ai_path.data(), ai_path.size());
                result += ai_path.size();
            }
        }
        else
        {
            if(run_result.target != nullptr)
            {
                std::vector<char> ai_path = srs_ai.make_path(node, run_result.target, map);
                memcpy(result, ai_path.data(), ai_path.size());
                result += ai_path.size();
            }
        }
    }
    else
    {
        auto target = srs_ai.run(map, node, reinterpret_cast<unsigned char *>(next), maxDepth).target;
        if(target != nullptr)
        {
            std::vector<char> ai_path = srs_ai.make_path(node, target, map);
            memcpy(result, ai_path.data(), ai_path.size());
            result += ai_path.size();
        }
    }
    result[0] = 'V';
    result[1] = '\0';
    return result_buffer[player];
}

class QQTetrisSearch
{

public:
    enum Status
    {
        Simple, Simulate, Path
    };
    void init(m_tetris::TetrisContext const *context, Status const *status)
    {
        simple_.init(context);
        simulate_.init(context);
        path_.init(context);
        status_ptr_ = status;
    }
    std::vector<char> make_path(m_tetris::TetrisNode const *node, m_tetris::TetrisNode const *land_point, m_tetris::TetrisMap const &map)
    {
        switch(*status_ptr_)
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
    std::vector<m_tetris::TetrisNode const *> const *search(m_tetris::TetrisMap const &map, m_tetris::TetrisNode const *node)
    {
        switch(*status_ptr_)
        {
        case Simple:
            return simple_.search(map, node);
        case Simulate:
            return simulate_.search(map, node);
        case Path:
            return path_.search(map, node);
        default:
            empty_.resize(1);
            empty_.front() = node->drop(map);
            return &empty_;
        }
    }
private:
    Status const *status_ptr_;
    land_point_search_simple::Search simple_;
    land_point_search_simulate::Search simulate_;
    land_point_search_path::Search path_;
    std::vector<m_tetris::TetrisNode const *> empty_;
};
m_tetris::TetrisEngine<rule_qq::TetrisRuleSet, ai_zzz::qq::Attack, QQTetrisSearch> qq_ai;

extern "C" DECLSPEC_EXPORT int QQTetrisAI(int boardW, int boardH, int board[], char nextPiece[], int curX, int curY, int curR, int level, int mode, char path[], size_t limit)
{
    if(!qq_ai.prepare(boardW, boardH))
    {
        *path = '\0';
        return 0;
    }
    m_tetris::TetrisMap map =
    {
        boardW, boardH
    };
    memcpy(map.row, board, boardH * sizeof(int));
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
    m_tetris::TetrisBlockStatus status =
    {
        nextPiece[0], curX, curY, (4 - curR) % 4
    };
    size_t next_length = (std::strlen(nextPiece) - 1) * std::min(9, level) / 9;
    if(level == 10)
    {
        *qq_ai.status() = QQTetrisSearch::Path;
    }
    else if(mode == 0 || map.count <= boardW * 2)
    {
        *qq_ai.status() = QQTetrisSearch::Simulate;
    }
    else
    {
        *qq_ai.status() = QQTetrisSearch::Simple;
    }
    qq_ai.param()->next_length = next_length;
    qq_ai.param()->level = level;
    qq_ai.param()->mode = mode;
    m_tetris::TetrisNode const *node = qq_ai.get(status);
    while(node == nullptr && status.y > 0)
    {
        --status.y;
        node = qq_ai.get(status);
    }
    auto target = qq_ai.run(map, node, reinterpret_cast<unsigned char const *>(nextPiece + 1), next_length, 60).target;
    std::vector<char> ai_path;
    if(target != nullptr)
    {
        ai_path = qq_ai.make_path(node, target, map);
        memcpy(path, ai_path.data(), ai_path.size());
    }
    path[ai_path.size()] = 'V';
    path[ai_path.size() + 1] = '\0';
    return 0;
}

m_tetris::TetrisEngine<rule_c2::TetrisRuleSet, ai_zzz::C2, land_point_search_cautious::Search> c2_ai;

extern "C" DECLSPEC_EXPORT int C2TetrisAI(int boardW, int boardH, int board[], char nextPiece[], int curX, int curY, int curR, int mode, int safe, int combo, char path[], size_t limit)
{
    if(!c2_ai.prepare(boardW, boardH))
    {
        *path = '\0';
        return 0;
    }
    m_tetris::TetrisMap map =
    {
        boardW, boardH
    };
    memcpy(map.row, board, boardH * sizeof(int));
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
    c2_ai.status()->fast_move_down = true;
    c2_ai.param()->length = 2;
    c2_ai.param()->virtual_length = 1;
    c2_ai.param()->safe = safe;
    c2_ai.param()->mode = mode;
    c2_ai.param()->combo = combo;
    c2_ai.param()->search = std::bind(&decltype(c2_ai)::search<m_tetris::TetrisNode const *>, &c2_ai, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
    m_tetris::TetrisBlockStatus status =
    {
        nextPiece[0], curX, curY, curR
    };
    size_t next_length = nextPiece[1] == ' ' ? 0 : 1;
    m_tetris::TetrisNode const *node = c2_ai.get(status);
    auto target = c2_ai.run(map, node, reinterpret_cast<unsigned char const *>(nextPiece + 1), next_length, limit).target;
    std::vector<char> ai_path;
    if(target != nullptr)
    {
        ai_path = c2_ai.make_path(node, target, map);
        memcpy(path, ai_path.data(), ai_path.size());
    }
    path[ai_path.size()] = 'V';
    path[ai_path.size() + 1] = '\0';
    return target == nullptr ? 0 : target->attach(map);
}