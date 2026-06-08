#ifdef _WIN32
#define DECLSPEC_EXPORT __declspec(dllexport)
#define WINAPI __stdcall
#else
#define DECLSPEC_EXPORT __attribute__((visibility("default")))
#define WINAPI
#define __cdecl
#endif

#include <ctime>
#include "tetris_core.h"
#include "search_tspin.h"
#include "search_aspin.h"
#include "search_path.h"
#include "search_simple.h"
#include "search_simulate.h"
#include "search_cautious.h"
#include "movegen_hook.h"
#include "movegen_searcher.h"
#include "tetris_engine2.h"
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
#include "rule_botris.h"
#include "random.h"
#include "bb_sim.h"

m_tetris2::TetrisEngine2<rule_st::TetrisRule, ai_zzz::Dig, path::Search> tetris_ai;

extern "C" void attach_init()
{
    ege::mtsrand((unsigned int)(time(nullptr)));
}

//����AI���֣�����ʾ�ڽ�����
extern "C" DECLSPEC_EXPORT char const *WINAPI Name()
{
    static std::string name = tetris_ai.ai_name();
    return name.c_str();
}

/*
 ***********************************************************************************************
 * ���ڶ�next�汾��ST...���Լ�MOD����...�������http://misakamm.com/blog/504�����demo.cpp��AIPath
 ***********************************************************************************************
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
 *
 ***********************************************************************************************
 * �����ļ�(ai.cpp)�ӹ����ų�,����demo.cpp�����Ϳ�����.���ֱ��ʹ�ñ�׼��ST����...�ᷢ��δ�������Ϊ!
 ***********************************************************************************************
 */
extern "C" DECLSPEC_EXPORT int WINAPI AIPath(int boardW, int boardH, char board[], char curPiece, int curX, int curY, int curR, char const *nextPiece, char path[])
{
    if (!tetris_ai.prepare(boardW, boardH))
    {
        return 0;
    }
    m_tetris2::TetrisMap map(boardW, boardH);
    for (int y = 0, add = 0; y < boardH; ++y, add += boardW)
    {
        for (int x = 0; x < boardW; ++x)
        {
            if (board[x + add] == '1')
            {
                map.row[y] |= 1 << x;
            }
        }
    }
    map.prepare();
    m_tetris2::TetrisBlockStatus status(curPiece, curX - 1, curY - 1, curR - 1);
    std::string next(nextPiece);
    auto target = tetris_ai.run(map, status, next.data(), next.size(), 49).target;
    if (target != nullptr)
    {
        std::vector<char> ai_path = tetris_ai.make_path(status, target, map);
        std::memcpy(path, ai_path.data(), ai_path.size());
        path[ai_path.size()] = '\0';
    }
    return 0;
}
#define USE_V08 1
#define USE_THREAD 1
#define USE_PC 1

#if !USE_V08
#if USE_THREAD
m_tetris2::TetrisThreadEngine2<rule_toj::TetrisRule, ai_zzz::TOJ, tspin::Search> srs_ai;
#else
m_tetris2::TetrisEngine2<rule_toj::TetrisRule, ai_zzz::TOJ, tspin::Search> srs_ai;
#endif
#else
#if USE_THREAD
m_tetris2::TetrisThreadEngine2<rule_toj::TetrisRule, ai_zzz::TOJ_v08, tspin::Search> srs_ai;
#else
m_tetris2::TetrisEngine2<rule_toj::TetrisRule, ai_zzz::TOJ_v08, tspin::Search> srs_ai;
#endif
#endif
#if USE_PC
std::unique_ptr<m_tetris2::TetrisThreadEngine2<rule_toj::TetrisRule, ai_zzz::TOJ_PC, tspin::Search>> srs_pc;
#endif
std::mutex srs_ai_lock;

extern "C" DECLSPEC_EXPORT int __cdecl AIDllVersion()
{
    return 2;
}

extern "C" DECLSPEC_EXPORT char *__cdecl AIName(int level)
{
    static char name[200];
    strcpy(name, srs_ai.ai_name().c_str());
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
    std::unique_lock<std::mutex> lock(srs_ai_lock);

    if (field_w != 10 || field_h != 22 || !srs_ai.prepare(10, 40))
    {
        *result = '\0';
        return result;
    }
#if USE_PC
    if (!srs_pc || srs_pc->context() != srs_ai.context())
    {
        srs_pc.reset(new m_tetris2::TetrisThreadEngine2<rule_toj::TetrisRule, ai_zzz::TOJ_PC, tspin::Search>(srs_ai.context()));
        memset(srs_pc->status(), 0, sizeof *srs_pc->status());
    }
#endif
    m_tetris2::TetrisMap map(10, 40);
    for (size_t d = 0, s = 22; d < 23; ++d, --s)
    {
        map.row[d] = field[s];
    }
    for (size_t d = 23, s = 0; s < 8; ++d, ++s)
    {
        map.row[d] = overfield[s];
    }
    map.prepare();
    srs_ai.search_config()->allow_rotate_move = false;
    srs_ai.search_config()->allow_180 = can180spin;
    srs_ai.search_config()->allow_d = true;
    srs_ai.search_config()->is_20g = false;
    srs_ai.search_config()->last_rotate = false;
#if USE_PC
    *srs_pc->search_config() = *srs_ai.search_config();
#endif
    struct ComboTable
    {
        int table[24] = {0};
        int table_max = 0;
    };
    static ComboTable table;
    if (table.table_max == 0)
    {
        size_t max = 0;
        while (comboTable[max] != -1)
        {
            table.table[max] = comboTable[max];
            ++max;
        }
        table.table_max = max - 1;
    }
    srs_ai.ai_config()->table = table.table;
    srs_ai.ai_config()->table_max = table.table_max;
#if USE_PC
    srs_pc->ai_config()->table = table.table;
    srs_pc->ai_config()->table_max = table.table_max;
#endif
    srs_ai.memory_limit(256ull << 20);
#if !USE_V08
    srs_ai.ai_config()->safe = srs_ai.ai()->get_safe(map, active);
    // srs_ai.ai_config()->param = { 36.118271157, 202.203495764, 200.737909778, 170.781301529, 277.040476787, 247.783175303, 3.729165582, -55.949272093, -30.745551429, 11.519702458, 3.400517468, 112.960485307, 171.678755503, -0.004778355, -0.111297405, -22.246305463, -7.869832591, -56.390368723, -70.581632887, -63.004355360, -1.839383519, 1.285416709, -0.143928932, -3.284161895, 5.967192336, 3.808250892, 3.238022919, 83.284536559, 0.309568618 };
    srs_ai.ai_config()->param = {10.507166148, 7.539860726, 13.048099725, 13.388476179, 6.728747539, 9.476881786, 0.258534525, -0.108269503, 4.394241496, -4.892359035, 0.049148374, 1.586714505, 8.885878229, -0.006001836, -0.004336234, -2.021765056, -0.951446468, -1.145468832, -1.515758227, -0.612910192, -0.476031978, 0.009596827, -0.399212013, -0.855819915, -0.418779377, -0.454784178, -1.417493065, 1.050941751, 0.756272086};
    // srs_ai.ai_config()->param = { 9.751914367, 6.771584511, 13.984778367, 20.368342456, 6.585961649, 20.332921226, 0.356373036, -0.386894461, -3.709699649, -1.675111576, 0.023687142, 10.297308519, 8.682478710, 0.001170853, 0.001022283, -1.241649334, -0.733500168, -1.611358148, -1.038454063, -0.445952144, -0.207823940, 0.000092385, -0.797795083, -6.153751596, -1.118142645, -0.641180767, -0.193865538, 0.505326328, 3.467200242 };
    srs_ai.status()->death = 0;
    srs_ai.status()->combo = combo;
    if (srs_ai.status()->under_attack != upcomeAtt)
    {
        srs_ai.update();
    }
    srs_ai.status()->under_attack = upcomeAtt;
    srs_ai.status()->map_rise = 0;
    srs_ai.status()->b2b = !!b2b;
    srs_ai.status()->acc_value = 0;
    srs_ai.status()->like = 0;
    srs_ai.status()->value = 0;
    ai_zzz::TOJ::Status::init_t_value(map, srs_ai.status()->t2_value, srs_ai.status()->t3_value);
#else
    srs_ai.status()->max_combo = 0;
    srs_ai.status()->max_attack = 0;
    srs_ai.status()->death = 0;
    srs_ai.status()->combo = combo;
    srs_ai.status()->attack = 0;
    if (srs_ai.status()->under_attack != upcomeAtt)
    {
        srs_ai.update();
    }
    srs_ai.status()->under_attack = upcomeAtt;
    srs_ai.status()->map_rise = 0;
    srs_ai.status()->b2b = !!b2b;
    srs_ai.status()->like = 0;
    srs_ai.status()->value = 0;
#endif
#if USE_PC
    srs_pc->memory_limit(768ull << 20);
    srs_pc->status()->attack = 0;
    srs_pc->status()->b2b = !!b2b;
    srs_pc->status()->combo = combo;
    srs_pc->status()->like = 0;
    srs_pc->status()->pc = false;
    srs_pc->status()->recv_attack = 0;
    if (srs_pc->status()->under_attack != upcomeAtt)
    {
        srs_pc->update();
    }
    srs_pc->status()->under_attack = upcomeAtt;
    srs_pc->status()->value = 0;
#endif

    m_tetris2::TetrisBlockStatus status(active, x, 22 - y, (4 - spin) % 4);
    static double const base_time = std::pow(100, 1.0 / 8);
    if (canhold)
    {
#if USE_PC
        srs_pc->run_hold(map, status, hold, curCanHold, next, maxDepth, time_t(0));
#endif
        auto run_result = srs_ai.run_hold(map, status, hold, curCanHold, next, maxDepth, time_t(std::pow(base_time, level)));
#if USE_PC
        auto pc_result = srs_pc->run_hold(map, status, hold, curCanHold, next, maxDepth, time_t(0));
        if (pc_result.status.pc)
        {
            run_result.change_hold = pc_result.change_hold;
            run_result.target = pc_result.target;
        }
#endif
        if (run_result.change_hold)
        {
            result++[0] = 'v';
            if (run_result.target != nullptr)
            {
                auto sp = rule_srs::TetrisRule::rule_spec::spawn(static_cast<char>(run_result.target.state.t), srs_ai.width(), srs_ai.height());
                m_tetris2::TetrisBlockStatus spawn_status(static_cast<char>(run_result.target.state.t), static_cast<int8_t>(sp.first), static_cast<int8_t>(sp.second), 0);
                std::vector<char> ai_path = srs_ai.make_path(spawn_status, run_result.target, map);
                std::memcpy(result, ai_path.data(), ai_path.size());
                result += ai_path.size();
            }
        }
        else
        {
            if (run_result.target != nullptr)
            {
                std::vector<char> ai_path = srs_ai.make_path(status, run_result.target, map);
                std::memcpy(result, ai_path.data(), ai_path.size());
                result += ai_path.size();
            }
        }
    }
    else
    {
#if USE_PC
        srs_pc->run(map, status, next, maxDepth, time_t(0));
#endif
        auto run_result = srs_ai.run(map, status, next, maxDepth, time_t(std::pow(base_time, level)));
#if USE_PC
        auto pc_result = srs_pc->run(map, status, next, maxDepth, time_t(0));
        if (pc_result.status.pc)
        {
            run_result.change_hold = pc_result.change_hold;
            run_result.target = pc_result.target;
        }
#endif
        if (run_result.target != nullptr)
        {
            std::vector<char> ai_path = srs_ai.make_path(status, run_result.target, map);
            std::memcpy(result, ai_path.data(), ai_path.size());
            result += ai_path.size();
        }
    }
    result++[0] = 'V';
    result[0] = '\0';
    return result_buffer[player];
}

m_tetris2::TetrisThreadEngine2<rule_botris::TetrisRule, ai_zzz::Botris, aspin::Search> botris_ai;
std::unique_ptr<m_tetris2::TetrisThreadEngine2<rule_botris::TetrisRule, ai_zzz::Botris_PC, aspin::Search>> botris_pc;

extern "C" DECLSPEC_EXPORT char *__cdecl BotrisAI3(int field[], int field_w, int field_h, int b2b, int combo, char next[], char hold, bool curCanHold, char active, int x, int y, int spin, bool canhold, bool can180spin, int upcomeAtt, int comboTable[], int maxDepth, int duration)
{
    static char result_buffer[1024];
    char *result = result_buffer;
    std::unique_lock<std::mutex> lock(srs_ai_lock);

    if (field_w != 10 || field_h != 22 || !botris_ai.prepare(10, 40))
    {
        *result = '\0';
        return result;
    }
    if (!botris_pc || botris_pc->context() != botris_ai.context())
    {
        botris_pc.reset(new m_tetris2::TetrisThreadEngine2<rule_botris::TetrisRule, ai_zzz::Botris_PC, aspin::Search>(botris_ai.context()));
        memset(botris_pc->status(), 0, sizeof *botris_pc->status());
    }
    m_tetris2::TetrisMap map(10, 40);
    for (size_t d = 0; d < 40; ++d)
    {
        map.row[d] = field[d];
    }
    map.prepare();
    botris_ai.search_config()->allow_rotate_move = false;
    botris_ai.search_config()->allow_180 = can180spin;
    botris_ai.search_config()->allow_d = true;
    botris_ai.search_config()->is_20g = false;
    *botris_pc->search_config() = *botris_ai.search_config();
    struct ComboTable
    {
        int table[24] = {0};
        int table_max = 0;
    };
    static ComboTable table;
    if (table.table_max == 0)
    {
        size_t max = 0;
        while (comboTable[max] != -1)
        {
            table.table[max] = comboTable[max];
            ++max;
        }
        table.table_max = max - 1;
    }
    botris_ai.ai_config()->table = table.table;
    botris_ai.ai_config()->table_max = table.table_max;
    botris_pc->ai_config()->table = table.table;
    botris_pc->ai_config()->table_max = table.table_max;
    botris_ai.memory_limit(1024ull << 20);
    botris_ai.status()->max_combo = 0;
    botris_ai.status()->max_attack = 0;
    botris_ai.status()->death = 0;
    botris_ai.status()->combo = combo;
    botris_ai.status()->attack = 0;
    botris_ai.status()->clear = 0;
    if (botris_ai.status()->under_attack != upcomeAtt)
    {
        botris_ai.update();
    }
    botris_ai.status()->under_attack = upcomeAtt;
    botris_ai.status()->map_rise = 0;
    botris_ai.status()->b2b = !!b2b;
    botris_ai.status()->like = 0;
    botris_ai.status()->value = 0;
    botris_pc->memory_limit(768ull << 20);
    botris_pc->status()->attack = 0;
    botris_pc->status()->b2b = !!b2b;
    botris_pc->status()->combo = combo;
    botris_pc->status()->like = 0;
    botris_pc->status()->pc = false;
    botris_pc->status()->recv_attack = 0;
    if (botris_pc->status()->under_attack != upcomeAtt)
    {
        botris_pc->update();
    }
    botris_pc->status()->under_attack = upcomeAtt;
    botris_pc->status()->value = 0;

    m_tetris2::TetrisBlockStatus status(active, x, 22 - y, (4 - spin) % 4);
    if (canhold)
    {
        botris_pc->run_hold(map, status, hold, curCanHold, next, maxDepth, time_t(0));
        auto run_result = botris_ai.run_hold(map, status, hold, curCanHold, next, maxDepth, time_t(duration));
        auto pc_result = botris_pc->run_hold(map, status, hold, curCanHold, next, maxDepth, time_t(0));
        if (pc_result.status.pc && pc_result.status.attack > run_result.status.attack)
        {
            run_result.change_hold = pc_result.change_hold;
            run_result.target = pc_result.target;
        }
        if (run_result.change_hold)
        {
            result++[0] = 'v';
            if (run_result.target != nullptr)
            {
                auto sp = rule_botris::TetrisRule::rule_spec::spawn(static_cast<char>(run_result.target.state.t), botris_ai.width(), botris_ai.height());
                m_tetris2::TetrisBlockStatus spawn_status(static_cast<char>(run_result.target.state.t), static_cast<int8_t>(sp.first), static_cast<int8_t>(sp.second), 0);
                std::vector<char> ai_path = botris_ai.make_path(spawn_status, run_result.target, map);
                std::memcpy(result, ai_path.data(), ai_path.size());
                result += ai_path.size();
            }
        }
        else
        {
            if (run_result.target != nullptr)
            {
                std::vector<char> ai_path = botris_ai.make_path(status, run_result.target, map);
                std::memcpy(result, ai_path.data(), ai_path.size());
                result += ai_path.size();
            }
        }
    }
    else
    {
        botris_pc->run(map, status, next, maxDepth, time_t(0));
        auto run_result = botris_ai.run(map, status, next, maxDepth, time_t(duration));
        auto pc_result = botris_pc->run(map, status, next, maxDepth, time_t(0));
        if (pc_result.status.pc)
        {
            run_result.change_hold = pc_result.change_hold;
            run_result.target = pc_result.target;
        }
        if (run_result.target != nullptr)
        {
            std::vector<char> ai_path = botris_ai.make_path(status, run_result.target, map);
            std::memcpy(result, ai_path.data(), ai_path.size());
            result += ai_path.size();
        }
    }
    result++[0] = 'V';
    result[0] = '\0';
    return result_buffer;
}

extern "C" DECLSPEC_EXPORT char *__cdecl BotrisAI2(int field[], int field_w, int field_h, int b2b, int combo, char next[], char hold, bool curCanHold, char active, int x, int y, int spin, bool canhold, bool can180spin, int upcomeAtt, int comboTable[], int maxDepth, int level, int player)
{
    static double const base_time = std::pow(100, 1.0 / 8);
    return BotrisAI3(field, field_w, field_h, b2b, combo, next, hold, curCanHold, active, x, y, spin, canhold, can180spin, upcomeAtt, comboTable, maxDepth, int(std::pow(base_time, level)));
}

extern "C" DECLSPEC_EXPORT char *__cdecl BotrisAI(int overfield[], int field[], int field_w, int field_h, int b2b, int combo, char next[], char hold, bool curCanHold, char active, int x, int y, int spin, bool canhold, bool can180spin, int upcomeAtt, int comboTable[], int maxDepth, int level, int player)
{
    int row[40];
    std::memset(row, 0, sizeof row);
    for (size_t d = 0, s = 22; d < 23; ++d, --s)
    {
        row[d] = field[s];
    }
    for (size_t d = 23, s = 0; s < 8; ++d, ++s)
    {
        row[d] = overfield[s];
    }
    return BotrisAI2(row, field_w, field_h, b2b, combo, next, hold, curCanHold, active, x, y, spin, canhold, can180spin, upcomeAtt, comboTable, maxDepth, level, player);
}

class QQTetrisSearch
{
public:
    using rule_spec = rule_qq::TetrisRule::rule_spec;
    using LandPoint = m_tetris2::BBLandPoint;
    enum Config
    {
        Simple,
        Simulate,
        Path
    };
    void init(Config const *config)
    {
        simple_.init(nullptr);
        simulate_.init(nullptr);
        path_.init(nullptr);
        config_ptr = config;
    }
    std::vector<char> make_path(m_tetris2::bb::BBState const &spawn,
                                m_tetris2::BBLandPoint const &land_point,
                                m_tetris2::Map<rule_spec::width, rule_spec::height> const &board)
    {
        switch (*config_ptr)
        {
        case Simple:
            return simple_.make_path(spawn, land_point, board);
        case Simulate:
            return simulate_.make_path(spawn, land_point, board);
        case Path:
            return path_.make_path(spawn, land_point, board);
        default:
            return std::vector<char>();
        }
    }
    std::vector<m_tetris2::BBLandPoint> const *search(m_tetris2::TetrisMap const &map, m_tetris2::TetrisBlockStatus const &status, size_t depth)
    {
        switch (*config_ptr)
        {
        case Simple:
        {
            auto src = simple_.search(map, status, depth);
            flatten_landpoint_.clear();
            if (src != nullptr)
            {
                flatten_landpoint_.reserve(src->size());
                for (auto const &lp : *src)
                {
                    flatten_landpoint_.push_back(lp);
                }
            }
            return &flatten_landpoint_;
        }
        case Simulate:
        {
            auto src = simulate_.search(map, status, depth);
            flatten_landpoint_.clear();
            if (src != nullptr)
            {
                flatten_landpoint_.reserve(src->size());
                for (auto const &lp : *src)
                {
                    flatten_landpoint_.push_back(lp);
                }
            }
            return &flatten_landpoint_;
        }
        case Path:
        {
            auto src = path_.search(map, status, depth);
            flatten_landpoint_.clear();
            if (src != nullptr)
            {
                flatten_landpoint_.reserve(src->size());
                for (auto const &lp : *src)
                {
                    flatten_landpoint_.push_back(lp);
                }
            }
            return &flatten_landpoint_;
        }
        default:
            empty_.clear();
            return &empty_;
        }
    }
    template<class EvalCallback>
    void search_eval(m_tetris2::Map<rule_qq::TetrisRule::rule_spec::width,
                                    rule_qq::TetrisRule::rule_spec::height> const &board,
                     m_tetris2::bb::BBState const &spawn, size_t depth, EvalCallback &cb)
    {
        switch (*config_ptr)
        {
        case Simple:
            simple_.search_eval(board, spawn, depth, cb);
            break;
        case Simulate:
            simulate_.search_eval(board, spawn, depth, cb);
            break;
        case Path:
            path_.search_eval(board, spawn, depth, cb);
            break;
        default:
            break;
        }
    }

private:
    Config const *config_ptr;
    m_tetris2::movegen::Searcher<m_tetris2::SimpleStrategy, m_tetris2::NoSpinHook, rule_qq::TetrisRule::rule_spec> simple_;
    m_tetris2::movegen::Searcher<m_tetris2::SimulateStrategy, m_tetris2::NoSpinHook, rule_qq::TetrisRule::rule_spec> simulate_;
    m_tetris2::movegen::Searcher<m_tetris2::PathStrategy, m_tetris2::NoSpinHook, rule_qq::TetrisRule::rule_spec> path_;
    std::vector<m_tetris2::BBLandPoint> flatten_landpoint_;
    std::vector<m_tetris2::BBLandPoint> empty_;
};
m_tetris2::TetrisEngine2<rule_qq::TetrisRule, ai_zzz::qq::Attack, QQTetrisSearch> qq_ai;

extern "C" DECLSPEC_EXPORT int __cdecl QQTetrisAI(int boardW, int boardH, int board[], char nextPiece[], int curX, int curY, int curR, int level, int mode, char path[], size_t limit)
{
    if (!qq_ai.prepare(boardW, boardH))
    {
        *path = '\0';
        return 0;
    }
    m_tetris2::TetrisMap map(boardW, boardH);
    std::memcpy(map.row, board, boardH * sizeof(int));
    map.prepare();
    m_tetris2::TetrisBlockStatus status(nextPiece[0], curX, curY, (4 - curR) % 4);
    size_t next_length = std::strlen(nextPiece) - 1;
    if (level < 10)
    {
        next_length = std::min<size_t>(level, next_length);
    }
    std::string next_str(nextPiece + 1, nextPiece + 1 + next_length);
    if (next_length <= 2)
    {
        std::string next_new = "?";
        for (auto c : next_str)
        {
            next_new += c;
            next_new += '?';
        }
        next_str.swap(next_new);
    }
    if (level == 10)
    {
        *qq_ai.search_config() = QQTetrisSearch::Path;
    }
    else if (mode == 0 || map.count <= boardW * 2)
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
    using QQSpecHelpers = m_tetris2::bb::Helpers<rule_qq::TetrisRule::rule_spec>;
    std::array<QQSpecHelpers::map_t, QQSpecHelpers::kMaxR> qq_usable_arr{};
    auto qq_board = m_tetris2::bb::build_board_for_search<rule_qq::TetrisRule::rule_spec>(map);
    QQSpecHelpers::build_usable_for_piece(status.t, qq_board, qq_usable_arr);
    while (!QQSpecHelpers::check_T(status.t, status.x, status.y, static_cast<std::uint8_t>(status.r), qq_usable_arr) && status.y > 0)
    {
        --status.y;
    }
    auto target = qq_ai.run(map, status, next_str.data(), next_str.length(), 60).target;
    std::vector<char> ai_path;
    if (target != nullptr)
    {
        ai_path = qq_ai.make_path(status, target, map);
        std::memcpy(path, ai_path.data(), ai_path.size());
    }
    path[ai_path.size()] = 'V';
    path[ai_path.size() + 1] = '\0';
    return 0;
}

m_tetris2::TetrisThreadEngine2<rule_c2::TetrisRule, ai_zzz::C2, cautious::Search> c2_ai;

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
    size_t ai_width;
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
    if (!c2_ai.prepare(boardW, boardH))
    {
        path[0] = {'\0'};
        return 0;
    }
    m_tetris2::TetrisMap map(boardW, boardH);
    std::memcpy(map.row, board, boardH * sizeof(int));
    map.prepare();
    c2_ai.memory_limit(1ull << 30);
    c2_ai.search_config()->fast_move_down = true;
    c2_ai.ai_config()->p =
        {
            2.87224,
            0.372169,
            0.102604,
            0.723501,
            3.08721,
            0.802789,
            -0.786174,
            107.713,
            -0.540719,
            109.116,
            -3.84305,
            116.58,
            -1.00066,
            49.7899,
            -1.23986,
            391.808,
            -4.30493,
            91.0623,
            -1.60608,
            67.7934,
            2.36365,
            46016.9,
            34.2515,
            0.285739,
        };
    c2_ai.ai_config()->p_rate = 1;
    c2_ai.ai_config()->safe = safe;
    c2_ai.ai_config()->mode = mode;
    c2_ai.ai_config()->danger = danger;
    c2_ai.ai_config()->soft_drop = soft_drop;
    c2_ai.status()->combo = combo;
    c2_ai.status()->combo_limit = combo_limit;
    c2_ai.status()->value = 0;
    m_tetris2::TetrisBlockStatus status(nextPiece[0], curX, curY, curR);
    size_t next_length = nextPiece[1] == ' ' ? 0 : 1;
    std::string next;
    if (vp)
    {
        next += '?';
    }
    for (char const *n = nextPiece + 1, *const ne = nextPiece + 1 + next_length; n != ne; ++n)
    {
        next += *n;
        if (vp)
        {
            next += '?';
        }
    }
    using C2SpecHelpers = m_tetris2::bb::Helpers<rule_c2::TetrisRule::rule_spec>;
    std::array<C2SpecHelpers::map_t, C2SpecHelpers::kMaxR> c2_usable_arr{};
    auto c2_board = m_tetris2::bb::build_board_for_search<rule_c2::TetrisRule::rule_spec>(map);
    C2SpecHelpers::build_usable_for_piece(status.t, c2_board, c2_usable_arr);
    while (!C2SpecHelpers::check_T(status.t, status.x, status.y, static_cast<std::uint8_t>(status.r), c2_usable_arr) && status.y > 0)
    {
        --status.y;
    }
    auto target = c2_ai.run(map, status, next.data(), next.size(), limit).target;
    std::vector<char> ai_path;
    size_t size = 0;
    if (target != nullptr)
    {
        ai_path = c2_ai.make_path(status, target, map);
        // 纯位板路径模拟（替换 TetrisNode 指针遍历）
        using H = m_tetris2::bb::Helpers<rule_c2::TetrisRule::rule_spec>;
        std::array<H::map_t, H::kMaxR> usable_arr{};
        auto board = m_tetris2::bb::build_board_for_search<rule_c2::TetrisRule::rule_spec>(map);
        H::build_usable_for_piece(status.t, board, usable_arr);
        auto cs = H::state_from_status(status.t, status.r, status.x, status.y);
        for (char c : ai_path)
        {
            switch (c)
            {
            case 'L':
                while (H::usable_at_bb(cs.r, static_cast<int>(cs.xb) - 1, static_cast<int>(cs.yb), usable_arr))
                    --cs.xb;
                break;
            case 'R':
                while (H::usable_at_bb(cs.r, static_cast<int>(cs.xb) + 1, static_cast<int>(cs.yb), usable_arr))
                    ++cs.xb;
                break;
            case 'l':
                if (H::usable_at_bb(cs.r, static_cast<int>(cs.xb) - 1, static_cast<int>(cs.yb), usable_arr))
                    --cs.xb;
                break;
            case 'r':
                if (H::usable_at_bb(cs.r, static_cast<int>(cs.xb) + 1, static_cast<int>(cs.yb), usable_arr))
                    ++cs.xb;
                break;
            case 'd':
                if (H::usable_at_bb(cs.r, static_cast<int>(cs.xb), static_cast<int>(cs.yb) - 1, usable_arr))
                    --cs.yb;
                break;
            case 'D':
                if (auto dropped = H::drop_bb_state(cs, usable_arr))
                    cs = *dropped;
                break;
            case 'z':
                if (auto kicked = H::first_passing_kick_bb(static_cast<char>(cs.t), m_tetris2::bb::KickDir::Ccw, cs, usable_arr))
                    cs = *kicked;
                break;
            case 'c':
                if (auto kicked = H::first_passing_kick_bb(static_cast<char>(cs.t), m_tetris2::bb::KickDir::Cw, cs, usable_arr))
                    cs = *kicked;
                break;
            case 'x':
                if (auto kicked = H::first_passing_kick_bb(static_cast<char>(cs.t), m_tetris2::bb::KickDir::Opp, cs, usable_arr))
                    cs = *kicked;
                break;
            default:
                break;
            }
            auto [mx, my] = H::master_xy_from_state(cs);
            path[size++] = {c, static_cast<int8_t>(mx), static_cast<int8_t>(my), static_cast<uint8_t>(cs.r)};
        }
    }
    if (size == 0)
    {
        path[size++] = {'V', int8_t(curX), int8_t(curY), uint8_t(curR)};
    }
    else
    {
        path[size] = path[size - 1];
        path[size++].move = 'V';
    }
    path[size++] = {'\0'};
    return target == nullptr ? 0 : c2_ai.attach(target, map);
}
