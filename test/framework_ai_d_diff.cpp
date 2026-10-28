// framework_ai_d_diff.cpp — shard D (misaka / ai_ax / ai_farteryhr) 迁移数值对拍。
//
// 【范围声明】本驱动是 UNIT 级 direct eval/get 逐字段对拍：直接对单个 AI 对象调用
//   旧四参 eval/get 与新契约 eval(map)/GetArgs-get，逐字段打印 canonical 输出并要求
//   OLD/NEW 逐字节相等。它【不是】完整 engine 端到端——没有跑搜索树、hold 轮换、
//   多盘连续 gameplay；farter 的 "eval 空返回" 仅指其新 eval() 为空 Result，get 仍独立
//   从 src_map+Landing 重算（非 engine 0 层树搜索）。完整 tree/hold 由后续 engine 测试覆盖。
//
// 同一驱动源码编译两遍：
//   OLD:  -DAGENT_OLD -I<research/.../snapshot/src>  链接冻结旧 AI（旧四参 eval / 旧 get）。
//   NEW:  -DAGENT_NEW -I<src>                       链接迁移后 AI（新 eval(map) / 新 get）。
// 两遍对同一组确定性 case（多盘、全 piece 全旋转、多 landing x/y、多 clear、多父 status、
// 多 Config 权重）调用各自 API。OLD 与 NEW 输出必须逐字节相等。
//
// 不是文本 diff：真实调用编译期实例化的 eval/get，并以同一 (T,R,x,y,spin) 由 shape 几何
// 同时构造旧 TetrisNode 与新 GetLanding，保证着陆几何一致。
//
// 运行/对拍命令见 test/framework_ai_d_diff_compare.sh（自包含、相对路径、失配非零退出，
// 可被 ctest 直接调用；不进 CMake，硬编码 artifact 路径）。

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <string>
#include <utility>
#include <vector>

#include "tetris_core.h"
#include "tetris_map.h"
#include "tetris_node.h"
#include "tetris_shape.h"
#include "rule_srs.h"
#include "search_tspin.h"

#include "ai_misaka.h"
#include "ai_ax.h"
#include "ai_farter.h"

using Spec40 = rule_srs::detail::SrsRule; // 10x40 全 piece 几何（ai_ax / farter）

// misaka 必须 10x22（static_assert + VirtualRow 21-index）。复用 SRS ops 构造一个 10x22
// RuleSpec：piece 几何与 board 高度正交，ops 不含出生高度依赖；默认 spawn(3,21) 即可。
using Spec22 = m_tetris2::RuleSpec<10, 22, 4,
    rule_srs::detail::op_O1,
    rule_srs::detail::op_I1, rule_srs::detail::op_I2, rule_srs::detail::op_I3, rule_srs::detail::op_I4,
    rule_srs::detail::op_S1, rule_srs::detail::op_S2, rule_srs::detail::op_S3, rule_srs::detail::op_S4,
    rule_srs::detail::op_Z1, rule_srs::detail::op_Z2, rule_srs::detail::op_Z3, rule_srs::detail::op_Z4,
    rule_srs::detail::op_L1, rule_srs::detail::op_L2, rule_srs::detail::op_L3, rule_srs::detail::op_L4,
    rule_srs::detail::op_J1, rule_srs::detail::op_J2, rule_srs::detail::op_J3, rule_srs::detail::op_J4,
    rule_srs::detail::op_T1, rule_srs::detail::op_T2, rule_srs::detail::op_T3, rule_srs::detail::op_T4>;

// ---------------------------------------------------------------------------
// canonical printer
// ---------------------------------------------------------------------------
static std::string g_out;
static void pr(const std::string &s) { g_out += s; g_out.push_back('\n'); }
template<class... A> void prn(const char *fmt, A... a)
{
    char buf[256];
    std::snprintf(buf, sizeof(buf), fmt, a...);
    pr(buf);
}

// ---------------------------------------------------------------------------
// board builders
// ---------------------------------------------------------------------------
template<int W, int H>
static m_tetris2::TetrisMap<W, H> board_from(std::initializer_list<std::pair<int, uint32_t>> rows)
{
    using Map = m_tetris2::TetrisMap<W, H>;
    Map m;
    for (auto [y, bits] : rows)
        m.set_row(y, static_cast<typename Map::row_t>(Map::row_full ^ static_cast<typename Map::row_t>(bits)));
    return m;
}

// 一组 22 高盘面（misaka）：空 / 平堆 / 带洞 / 井 / 接近顶 / 满底
static m_tetris2::TetrisMap<10, 22> board22(int which)
{
    switch (which)
    {
    case 0: return board_from<10, 22>({}); // 空盘
    case 1: return board_from<10, 22>({{0, 0x3ff}, {1, 0x3ff}}); // 满底两行
    case 2: return board_from<10, 22>({{0, 0x3ff}, {1, 0x3e0}, {2, 0x3ff}}); // 带一个洞(列5空)
    case 3: return board_from<10, 22>({{0, 0x3ff}, {1, 0x001}, {2, 0x003}, {3, 0x007}, {4, 0x00f}}); // 左井
    case 4: return board_from<10, 22>({{18, 0x3ff}, {19, 0x2aa}, {20, 0x155}, {21, 0x3ff}}); // 接近顶
    case 5: return board_from<10, 22>({{0, 0x2aa}, {1, 0x155}, {2, 0x3ff}, {3, 0x100}, {4, 0x080}, {5, 0x040}});
    }
    return board_from<10, 22>({});
}

// 一组 40 高盘面（ai_ax / farter）
static m_tetris2::TetrisMap<10, 40> board40(int which)
{
    switch (which)
    {
    case 0: return board_from<10, 40>({});
    case 1: return board_from<10, 40>({{0, 0x3ff}, {1, 0x3ff}, {2, 0x3ff}});
    case 2: return board_from<10, 40>({{0, 0x3ff}, {1, 0x3e0}, {2, 0x3ff}, {3, 0x200}}); // 洞+高柱
    case 3: return board_from<10, 40>({{0, 0x001}, {1, 0x003}, {2, 0x007}, {3, 0x00f}, {4, 0x01f}}); // 左井
    case 4: return board_from<10, 40>({{36, 0x3ff}, {37, 0x2aa}, {38, 0x155}, {39, 0x3ff}}); // 接近顶
    case 5: return board_from<10, 40>({{0, 0x2aa}, {1, 0x155}, {5, 0x3ff}, {10, 0x001}, {11, 0x003}});
    case 6: return board_from<10, 40>({{0, 0x3ff}, {1, 0x3ff}, {39, 0x3ff}}); // 底满+顶满
    case 7: return board_from<10, 40>({{0, 0x1ff}, {1, 0x3ff}, {2, 0x3ff}, {3, 0x3ff}}); // 左缺一列
    }
    return board_from<10, 40>({});
}

// ---------------------------------------------------------------------------
// misaka Config（多变体，扰动权重覆盖权重敏感路径）
// ---------------------------------------------------------------------------
static int g_table[8] = {0, 0, 1, 2, 4, 8, 16, 32};
static ai_misaka::misaka::Config make_misaka_config(int variant)
{
    ai_misaka::misaka::Config c{};
    c.miny_factor = 100; c.hole = 80; c.open_hole = 40; c.v_transitions = 20; c.tspin3 = 60;
    c.clear_efficient = 50; c.upcomeAtt = 25; c.h_factor = 15; c.hole_dis_factor2 = 30;
    c.hole_dis = 10; c.hole_dis_factor = 12; c.tspin = 45; c.hold_t = 70; c.hold_i = 60;
    c.clear_useless_factor = 35; c.dif_factor = 8; c.strategy_4w = 1;
    c.table = g_table; c.table_max = 8;
    if (variant == 1)
    {
        c.miny_factor = 250; c.hole = 5; c.open_hole = 90; c.v_transitions = 77; c.tspin3 = 0;
        c.upcomeAtt = -40; c.h_factor = 99; c.tspin = 1; c.strategy_4w = 0;
    }
    else if (variant == 2)
    {
        c.miny_factor = 1; c.hole = 200; c.h_factor = 0; c.clear_efficient = 13; c.hole_dis = 7;
        c.hold_t = 0; c.hold_i = 0; c.dif_factor = 50;
    }
    return c;
}
struct MisakaSpec
{
    static constexpr int width = 10;
    static constexpr int height = 22;
};

static m_tetris2::AIEnv make_env(char hold, const char *next, size_t len, char node, bool is_hold)
{
    m_tetris2::AIEnv e{};
    e.next = next;
    e.length = len;
    e.node = node;
    e.hold = hold;
    e.is_hold = is_hold;
    return e;
}

// 父 Status 多变体：覆盖 get 中分支/累积路径（含负值、零值、极值）。
static ai_misaka::misaka::Status make_misaka_parent(int v)
{
    ai_misaka::misaka::Status p{};
    if (v == 0)
    {
        p.upcomeAtt = 15; p.combo = 2; p.b2b = 1; p.total_clear_att = 10;
        p.total_clears = 3; p.clearScore = -5; p.att = 4; p.max_att = 9;
        p.max_combo = 6; p.strategy_4w = 1; p.score = 120;
    }
    else
    {
        p.upcomeAtt = 0; p.combo = 0; p.b2b = 0; p.total_clear_att = 0;
        p.total_clears = 0; p.clearScore = 0; p.att = 0; p.max_att = 0;
        p.max_combo = 0; p.strategy_4w = 0; p.score = -1000;
    }
    return p;
}

// ---------------------------------------------------------------------------
// landing <-> node bridge (NEW only: 旧框架无 GetLanding)
// ---------------------------------------------------------------------------
#ifdef AGENT_NEW
template<class Det>
static m_tetris2::GetLanding<search_tspin::Search::TSpinType>
bridge_misaka_landing(m_tetris2::TetrisNode<Det, search_tspin::Search::TSpinType> const &node)
{
    using SpinT = search_tspin::Search::TSpinType;
    m_tetris2::GetLanding<SpinT> L;
    L.piece = Det::t_value;
    L.rotation = Det::r_value;
    L.xb = static_cast<int8_t>(node.col());
    L.yb = static_cast<int8_t>(node.row());
    L.spin = node.spin;
    L.w = static_cast<int8_t>(m_tetris2::TetrisNodeBase<Det>::kWidth);
    L.h = static_cast<int8_t>(m_tetris2::TetrisNodeBase<Det>::kHeight);
    L.y_master_plus1 = node.y + 1;
    for (int i = 0; i < L.h; ++i)
        L.lines[i] = node.line(i);
    return L;
}

template<class Det>
static m_tetris2::GetLanding<std::monostate>
bridge_monostate_landing(m_tetris2::TetrisNode<Det, std::monostate> const &node)
{
    m_tetris2::GetLanding<std::monostate> L;
    L.piece = Det::t_value;
    L.rotation = Det::r_value;
    L.xb = static_cast<int8_t>(node.col());
    L.yb = static_cast<int8_t>(node.row());
    L.w = static_cast<int8_t>(m_tetris2::TetrisNodeBase<Det>::kWidth);
    L.h = static_cast<int8_t>(m_tetris2::TetrisNodeBase<Det>::kHeight);
    L.y_master_plus1 = node.y + 1;
    for (int i = 0; i < L.h; ++i)
        L.lines[i] = node.line(i);
    return L;
}
#endif // AGENT_NEW

// ===========================================================================
// MISAKA
// ===========================================================================
template<class Det>
static void run_misaka(int board, int x, int y, int clear, search_tspin::Search::TSpinType spin,
                       const ai_misaka::misaka::Config &cfg, int pvar)
{
    using SpinT = search_tspin::Search::TSpinType;
    auto map = board22(board);

    ai_misaka::misaka ai;
    ai.init<MisakaSpec>(&cfg);

    // 旧节点（编译期几何）
    m_tetris2::TetrisNode<Det, SpinT> node;
    node.x = static_cast<int8_t>(x);
    node.y = static_cast<int8_t>(y);
    node.spin = spin;

    auto parent = make_misaka_parent(pvar);

    const char *next = "TISZLJ";
    auto env = make_env('I', next, 6, Det::t_value, false);

#ifdef AGENT_OLD
    auto r = ai.eval(node, map, map, static_cast<size_t>(clear));
    auto st = ai.get(r, static_cast<size_t>(7), parent, env);
#else
    auto r = ai.eval(map);
    auto L = bridge_misaka_landing<Det>(node);
    auto st = ai.get(r, clear, L, parent, env);
#endif

    char tag[80];
    std::snprintf(tag, sizeof(tag), "MIS b%d x%d y%d cl%d sp%d cfg%d pv%d", board, x, y, clear, (int)spin, cfg.miny_factor, pvar);
    prn("== %s", tag);
    for (int i = 0; i < 22; ++i)
        prn("  row[%02d]=0x%04x", i, (unsigned)r.map_rows[i]);
    prn("  map_count=%d", r.map_count);
    prn("  st.upcomeAtt=%d st.combo=%d st.b2b=%d st.total_clear_att=%d st.total_clears=%d st.clearScore=%d st.att=%d st.max_att=%d st.max_combo=%d st.strategy_4w=%d st.score=%d",
        st.upcomeAtt, st.combo, (int)st.b2b, st.total_clear_att, st.total_clears, st.clearScore,
        st.att, st.max_att, st.max_combo, st.strategy_4w, st.score);
}

// ===========================================================================
// AI_AX
// ===========================================================================
template<class Det>
static void run_ax(int board, int x, int y, int clear, int pvar)
{
#ifdef PAIR2_SKIP_EMPTY_AX
    // HEAD 语义无 guard：空盘(roof==0)访问 occupied_row(-1) 本就越界/崩溃，
    // 该空盘路径由 worktree 版（带 guard）在 pair1 覆盖；HEAD 对拍跳过 board0。
    if (board == 0)
        return;
#endif
    auto map = board40(board);

    ai_ax::AI ai;
    ai.init<Spec40>();

    m_tetris2::TetrisNode<Det, std::monostate> node;
    node.x = static_cast<int8_t>(x);
    node.y = static_cast<int8_t>(y);

    ai_ax::AI::Status parent;
    if (pvar == 0)
    {
        parent.land_point = -123.5;
        parent.value = 45.25;
    }
    else
    {
        parent.land_point = 1000.0;
        parent.value = -300.5;
    }

    size_t depth = 5;

#ifdef AGENT_OLD
    auto r = ai.eval(node, map, map, static_cast<size_t>(clear));
    auto st = ai.get(r, depth, parent);
#else
    auto r = ai.eval(map);
    auto L = bridge_monostate_landing<Det>(node);
    auto st = ai.get(r, L, clear, parent, depth);
#endif

    char tag[80];
    std::snprintf(tag, sizeof(tag), "AX b%d x%d y%d cl%d pv%d", board, x, y, clear, pvar);
    prn("== %s", tag);
    prn("  result.map=%.17g", r.map);
    prn("  st.land_point=%.17g st.value=%.17g", st.land_point, st.value);
}

// ===========================================================================
// FARTER
// ===========================================================================
template<class Det>
static void run_farter(int board, int x, int y, int pvar)
{
    auto src = board40(board);

    ai_farteryhr::AI ai;
    ai.init<Spec40>();

    m_tetris2::TetrisNode<Det, std::monostate> node;
    node.x = static_cast<int8_t>(x);
    node.y = static_cast<int8_t>(y);

    ai_farteryhr::AI::Status parent;
    if (pvar == 0)
    {
        parent.eval = 500;
        parent.value = 100;
    }
    else
    {
        parent.eval = 0;
        parent.value = 999;
    }
    size_t depth = 4;

#ifdef AGENT_OLD
    int pts = ai.eval(node, src, src, static_cast<size_t>(0));
    (void)pts;
    auto st = ai.get(pts, depth, parent);
#else
    auto r = ai.eval(src);
    (void)r;
    auto L = bridge_monostate_landing<Det>(node);
    auto st = ai.get(src, L, parent, depth);
#endif

    char tag[80];
    std::snprintf(tag, sizeof(tag), "FARTER b%d x%d y%d pv%d", board, x, y, pvar);
    prn("== %s", tag);
    prn("  st.eval=%d st.value=%d", st.eval, st.value);
}

// ---------------------------------------------------------------------------
// compile-time (piece,rotation) dispatch
// ---------------------------------------------------------------------------
template<char T, uint8_t R>
static void run_all_for(int bi)
{
    using Det22 = m_tetris2::TetrisNodeDetails<Spec22, T, R>;
    using Det40 = m_tetris2::TetrisNodeDetails<Spec40, T, R>;

    for (int cfgv = 0; cfgv < 3; ++cfgv)
    {
        auto cfg = make_misaka_config(cfgv);
        for (int board = 0; board < 6; ++board)
            for (int x : {1, 4, 7})
                for (int y : {4, 10, 18})
                    for (int clear : {0, 1, 2, 3, 4})
                        for (int spin = 0; spin <= 2; ++spin)
                            for (int pv = 0; pv < 2; ++pv)
                                run_misaka<Det22>(board, x, y, clear, (search_tspin::Search::TSpinType)spin, cfg, pv);
    }

    for (int board = 0; board < 8; ++board)
        for (int x : {1, 4, 7})
            for (int y : {5, 15, 30})
                for (int clear : {0, 1, 2, 3, 4})
                    for (int pv = 0; pv < 2; ++pv)
                        run_ax<Det40>(board, x, y, clear, pv);

    for (int board = 0; board < 8; ++board)
        for (int x : {1, 4, 7})
            for (int y : {5, 15, 30})
                for (int pv = 0; pv < 2; ++pv)
                    run_farter<Det40>(board, x, y, pv);

    (void)bi;
}

template<class List> struct Run;
template<char... Ts, uint8_t... Rs>
struct Run<std::tuple<std::tuple<std::integral_constant<char, Ts>, std::integral_constant<uint8_t, Rs>>...>>
{
    static void go(int bi)
    {
        (run_all_for<Ts, Rs>(bi), ...);
    }
};

int main()
{
    g_out.clear();
#ifdef AGENT_OLD
    pr("MODE=OLD");
#else
    pr("MODE=NEW");
#endif

    // 全 SRS 旋转覆盖：O0；I/S/Z/L/J/T 各 R0..R3（与 Spec ops 一一对应，共 25 组）。
    using Combos = std::tuple<
        std::tuple<std::integral_constant<char, 'O'>, std::integral_constant<uint8_t, 0>>,
        std::tuple<std::integral_constant<char, 'I'>, std::integral_constant<uint8_t, 0>>,
        std::tuple<std::integral_constant<char, 'I'>, std::integral_constant<uint8_t, 1>>,
        std::tuple<std::integral_constant<char, 'I'>, std::integral_constant<uint8_t, 2>>,
        std::tuple<std::integral_constant<char, 'I'>, std::integral_constant<uint8_t, 3>>,
        std::tuple<std::integral_constant<char, 'S'>, std::integral_constant<uint8_t, 0>>,
        std::tuple<std::integral_constant<char, 'S'>, std::integral_constant<uint8_t, 1>>,
        std::tuple<std::integral_constant<char, 'S'>, std::integral_constant<uint8_t, 2>>,
        std::tuple<std::integral_constant<char, 'S'>, std::integral_constant<uint8_t, 3>>,
        std::tuple<std::integral_constant<char, 'Z'>, std::integral_constant<uint8_t, 0>>,
        std::tuple<std::integral_constant<char, 'Z'>, std::integral_constant<uint8_t, 1>>,
        std::tuple<std::integral_constant<char, 'Z'>, std::integral_constant<uint8_t, 2>>,
        std::tuple<std::integral_constant<char, 'Z'>, std::integral_constant<uint8_t, 3>>,
        std::tuple<std::integral_constant<char, 'L'>, std::integral_constant<uint8_t, 0>>,
        std::tuple<std::integral_constant<char, 'L'>, std::integral_constant<uint8_t, 1>>,
        std::tuple<std::integral_constant<char, 'L'>, std::integral_constant<uint8_t, 2>>,
        std::tuple<std::integral_constant<char, 'L'>, std::integral_constant<uint8_t, 3>>,
        std::tuple<std::integral_constant<char, 'J'>, std::integral_constant<uint8_t, 0>>,
        std::tuple<std::integral_constant<char, 'J'>, std::integral_constant<uint8_t, 1>>,
        std::tuple<std::integral_constant<char, 'J'>, std::integral_constant<uint8_t, 2>>,
        std::tuple<std::integral_constant<char, 'J'>, std::integral_constant<uint8_t, 3>>,
        std::tuple<std::integral_constant<char, 'T'>, std::integral_constant<uint8_t, 0>>,
        std::tuple<std::integral_constant<char, 'T'>, std::integral_constant<uint8_t, 1>>,
        std::tuple<std::integral_constant<char, 'T'>, std::integral_constant<uint8_t, 2>>,
        std::tuple<std::integral_constant<char, 'T'>, std::integral_constant<uint8_t, 3>>>;

    Run<Combos>::go(0);

    std::fwrite(g_out.data(), 1, g_out.size(), stdout);
    return 0;
}
