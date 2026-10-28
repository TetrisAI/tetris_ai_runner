
#ifdef _WIN32
#  define DECLSPEC_EXPORT __declspec(dllexport)
#  define WINAPI __stdcall
#else
#  define DECLSPEC_EXPORT __attribute__((visibility("default")))
#  define WINAPI
#endif

#include <ctime>
#include <cstring>
#include "random.h"
#include "search_simple.h"
#include "rule_st.h"
#include "tetris_eval_get.h"

//for https://misakamm.com/blog/504

// DemoAI: 入门级演示 AI，eval 完全随机（dummy stub，新契约迁移）。
namespace demo
{
    using namespace m_tetris2;

    class AI
    {
    public:
        using Result = double;
        using Status = double;  // dummy：Status=Result
        using GetArgs = type_list<>;  // 无 Eval：eval 不参与 cache 正确性
        template<class Spec>
        void init(void const * /*cfg*/ = nullptr) {}
        std::string ai_name() const
        {
            return "Demo Random AI";
        }

        // get()：每步随机（dummy stub，记录为新契约行为，非旧语义 100% 相同）
        double get() const
        {
            return ege::mtdrand() * 100;
        }

        // eval(map)：确定性常数，可 cache
        template<class MapT>
        double eval(MapT const & /*map*/) const
        {
            return 0.0;
        }
    };
}

m_tetris2::TetrisEngine<rule_st::TetrisRule, demo::AI, simple::Search> tetris_ai;

extern "C" void attach_init()
{
    ege::mtsrand(static_cast<unsigned int>(time(nullptr)));
}

//输出AI名字，将显示在界面上
extern "C" DECLSPEC_EXPORT char const *WINAPI Name()
{
    static std::string name = "Demo Random AI";
    return name.c_str();
}

/*
 * path 是接收路径字符串的字符数组，字符含义：
 *      'l': 向左一格
 *      'r': 向右一格
 *      'd': 向下一格
 *      'L': 移到最左
 *      'R': 移到最右
 *      'D': 移到最下（软降到底，即可移动距离）
 *      'z': 逆时针旋转
 *      'c': 顺时针旋转
 * 字符串末尾要加'\0'，表示路径结束（硬降落）
 *
 * 如果不支持路径输出可忽略该参数，如果只使用这一个函数的话可删掉下面两个函数
 */
extern "C" DECLSPEC_EXPORT int WINAPI AIPath(int boardW, int boardH, char board[], char curPiece, int curX, int curY, int curR, char nextPiece, char path[])
{
    if (!tetris_ai.prepare(boardW, boardH))
    {
        return 0;
    }
    decltype(tetris_ai)::map_t map;
    for (int y = 0, add = 0; y < boardH; ++y, add += boardW)
    {
        for (int x = 0; x < boardW; ++x)
        {
            if (board[x + add] == '1')
            {
                map.set(x, y);
            }
        }
    }
    m_tetris2::TetrisBlockStatus status(curPiece, curX - 1, curY - 1, curR - 1);
    std::string next;
    if (nextPiece != ' ')
    {
        next += nextPiece;
    }
    auto target = tetris_ai.run(map, status, next.data(), next.size(), 99).target;
    if (target != nullptr)
    {
        std::vector<char> ai_path = tetris_ai.make_path(status, target, map);
        std::memcpy(path, ai_path.data(), ai_path.size());
        path[ai_path.size()] = '\0';
    }
    return 0;
}
