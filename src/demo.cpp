
#define DECLSPEC_EXPORT __declspec(dllexport)
#define WINAPI __stdcall

#include <ctime>
#include "tetris_core.h"
#include "random.h"
#include "land_point_search_simple.h"
#include "ai_easy.h"
#include "rule_st.h"

m_tetris::TetrisEngine<rule_st::TetrisRuleSet, ai_easy::AI, land_point_search_simple::Search> tetris_ai;

extern "C" void attach_init()
{
    ege::mtsrand(unsigned int(time(nullptr)));
}

//����AI���֣�����ʾ�ڽ�����
extern "C" DECLSPEC_EXPORT char const *WINAPI Name()
{
    static std::string name = "ai demo (random)";
    return name.c_str();
}

namespace demo
{
    using namespace m_tetris;

    double eval(TetrisNode const *node, TetrisMap const &map, TetrisMap const &src_map, size_t clear)
    {
        return clear * 100 + ege::mtdrand();
    }
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
    if(!tetris_ai.prepare(boardW, boardH))
    {
        return 0;
    }
    tetris_ai.param()->eval_func = demo::eval;
    tetris_ai.param()->bad_value = -1;
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
    size_t next_length = nextPiece == ' ' ? 0 : 1;
    m_tetris::TetrisNode const *node = tetris_ai.get(status);
    auto target = tetris_ai.run(map, node, reinterpret_cast<unsigned char const *>(&nextPiece), next_length, 49).target;
    if(target != nullptr)
    {
        std::vector<char> ai_path = tetris_ai.make_path(node, target, map);
        memcpy(path, ai_path.data(), ai_path.size());
        path[ai_path.size()] = '\0';
    }
    return 0;
}
