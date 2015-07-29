
//by ZouZhiZhang

#include "tetris_core.h"
#include "integer_utils.h"
#include "ai_misaka.h"
#include <cstdint>

using namespace m_tetris;
using namespace zzz;


namespace ai_misaka
{

    bool misaka::Status::operator < (Status const &other) const
    {
        auto &m = other;
#if 0
    {
        if(max_combo > (combo - 1) * 32 && m.max_combo > (m.combo - 1) * 32)
        {
            if(att > 8 || m.att > 8)
            {
                if(abs(first.score - m.first.score) < 400)
                {
                    if(att != m.att)
                        return att < m.att;
                }
                else
                {
                    return first < m.first;
                }
            }
        }
        if((max_combo > 6 * 32 || m.max_combo > 6 * 32))
        {
            if(max_combo != m.max_combo)
            {
                return max_combo < m.max_combo;
            }
        }
        if(ai_settings[player].strategy_4w)
            if((combo > 3 * 32 || m.combo > 3 * 32))
            {
                if(combo != m.combo)
                {
                    return combo < m.combo;
                }
            }
    }
    //if (0)
    if((pool_last.combo > 3 * 32 || m.pool_last.combo > 3 * 32) && pool_last.combo != m.pool_last.combo)
    {
        return pool_last.combo < m.pool_last.combo;
    }
#else
        //if ( abs(first.score - m.first.score) >= 900 ) {
        //    return first < m.first;
        //}
        //if ( (max_att >= 6 || m.max_att >= 6) && abs(max_att - m.max_att) >= 2 ) {
        //    return max_att < m.max_att;
        //}
        //else
        if(strategy_4w)
        {
            if((max_combo > 6 * 32 || m.max_combo > 6 * 32))
            {
                if(max_combo != m.max_combo)
                {
                    return max_combo < m.max_combo;
                }
            }
            if((combo >= 32 * 3 || m.combo >= 32 * 3) && combo != m.combo)
            {
                return combo < m.combo;
            }
        }
        else
        {
            if(true/*ai_settings[player].combo*/)
            {
                if((max_combo > 6 * 32 || m.max_combo > 6 * 32))
                {
                    if(max_combo != m.max_combo)
                    {
                        return max_combo < m.max_combo;
                    }
                }
                if(max_combo > combo && m.max_combo > m.combo && (m.max_combo > 4 * 32 || max_combo > 4 * 32))
                {
                    if((combo <= 2 * 32 && m.combo <= 2 * 32))
                    {
                        if(abs(score - m.score) < 1000)
                        {
                            if(att != m.att)
                                return att < m.att;
                        }
                        else
                        {
                            return score > m.score;
                        }
                    }
                }
                ////if ( ai_settings[player].strategy_4w ) {
                //    if ( ( combo > 3 * 32 || m.combo > 3 * 32 ) ) {
                //        if ( combo != m.combo ) {
                //            return combo < m.combo;
                //        }
                //    }
                //}
            }
            //if (0)
            //if ( (pool_last.combo > 32 || m.pool_last.combo > 32 ) )
            //{
            //    int m1 = (max_combo!=pool_last.combo ? std::max(max_combo - 32 * 2, 0) * 2 : 0 ) + pool_last.combo;
            //    int m2 = (m.max_combo!=m.pool_last.combo ? std::max(m.max_combo - 32 * 2, 0) * 2 : 0 ) + m.pool_last.combo;
            //    if ( m1 != m2 ) {
            //        return m1 < m2;
            //    }
            //}
            if((combo > 32 * 2 || m.combo > 32 * 2) && combo != m.combo)
            {
                return combo < m.combo;
            }
        }
        //if ( (pool_last.combo > 1 || m.pool_last.combo > 1) && pool_last.combo != m.pool_last.combo) {
        //    return pool_last.combo < m.pool_last.combo;
        //}
#endif
        return score > m.score;

    }

    void misaka::init(m_tetris::TetrisContext const *context, Config const *config)
    {
        context_ = context;
        config_ = config;
    }

    std::string misaka::ai_name() const
    {
        return "Misakamm v0.1";
    }

    misaka::Result misaka::eval(TetrisNodeEx &node, TetrisMap const &map, TetrisMap const &src_map, size_t clear) const
    {
        if(clear > 0 && node.is_check && node.is_last_rotate)
        {
            if(clear == 1 && node.is_mini_ready)
            {
                node.type = TSpinType::TSpinMini;
            }
            else if(node.is_ready)
            {
                node.type = TSpinType::TSpin;
            }
            else
            {
                node.type = TSpinType::None;
            }
        }
        return
        {
            node, &map, &src_map, clear, node.type
        };
    }

    misaka::Status misaka::get(Result const &eval_result, size_t depth, Status const &status, TetrisContext::Env const &env) const
    {
#pragma warning(push)
#pragma warning(disable:4244 4554)
#define XP_RELEASE
#define USE4W 1
        struct VirtualRow
        {
            VirtualRow(uint32_t const *row, int max)
                : row_(row), max_(max)
            {
            }
            uint32_t const *row_;
            int max_;
            uint32_t operator[](int index) const
            {
                index = 21 - index;
                if(index < 0 || index >= max_)
                {
                    return uint32_t(-1);
                }
                return row_[index];
            }
        };
        struct VirtualPool
        {
            VirtualPool(TetrisContext const *context, Result const &eval_result, size_t depth, char hold, Status const &status)
                : context_(context), eval_result_(eval_result), depth_(depth), hold_(hold), status_(status)
                , row(eval_result.map->row, context->height())
                , m_hold(hold)
                , m_w_mask(context->full())
                , combo(status.combo)
                , b2b(status.b2b)
            {
            }
            TetrisContext const *context_;
            Result const &eval_result_;
            size_t depth_;
            char hold_;
            Status const &status_;

            VirtualRow row;
            char m_hold;
            uint32_t m_w_mask;
            int16_t combo;
            int8_t b2b;

            int width()
            {
                return 10;
            }
            int height()
            {
                return 22;
            }
            int getPCAttack()
            {
                return 6;
            }

        } pool(context_, eval_result, depth, env.hold, status);
        Config const &ai_param = *config_;
        char const GEMTYPE_T = 'T';
        char const GEMTYPE_I = 'I';
        auto softdropEnable = []()
        {
            return true;
        };
        const int gem_beg_x = 3;
        const int m_pc_att = 6;
        const int combo_step_max = 32;
        int curdepth = depth;
        int clears = eval_result.clear;
        char cur_num = eval_result.node->status.t;
        int total_clear_att = status.total_clear_att;
        int total_clears = status.total_clears;
        int clear_att = status.att;
        int upcomeAtt = status.upcomeAtt;
        int8_t wallkick_spin = eval_result.t_spin != TSpinType::None ? 2 : 0;
        int t_dis = [=]()->int
        {
            if(env.hold == GEMTYPE_T)
            {
                return 0;
            }
            for(size_t i = 0; i < env.length; ++i)
            {
                if(env.next[i] == GEMTYPE_T)
                {
                    return i;
                }
            }
            return 14;
        }();

        Status result = status;
        int &clearScore = result.clearScore;


        int score = 0;
        // 测高度
        //int last_min_y[32] = {0};
        int min_y[32] = {0};
        int emptys[32] = {0};
        int maxy_index = 31, maxy_cnt = 0;
        int maxy_flat_cnt = 0; // 最长平台
        int miny_val = 31;
        int total_hole = 0;
        int beg_y = -5;
        const int pool_w = pool.width(), pool_h = pool.height();
        //last_min_y[31] = -1;
        min_y[31] = -1;
        {
            //while ( last_pool.row[beg_y] == 0 ) ++beg_y;
            //for ( int x = 0; x < pool_w; ++x) {
            //    for ( int y = beg_y, ey = pool_h + 1; y <= ey; ++y) { // 要有底行保护（pool.h），否则会挂
            //        if ( last_pool.row[y] & ( 1 << x ) ) {
            //            last_min_y[x] = y;
            //            break;
            //        }
            //    }
            //}
            beg_y = -5;
            while(pool.row[beg_y] == 0) ++beg_y;
            for(int x = 0; x < pool_w; ++x)
            {
                for(int y = beg_y, ey = pool_h + 1; y <= ey; ++y)
                { // 要有底行保护（pool.h），否则会挂
                    if(pool.row[y] & (1 << x))
                    {
                        min_y[x] = y;
                        miny_val = std::min(miny_val, y);
                        if(y > min_y[maxy_index])
                        {
                            maxy_index = x;
                            maxy_cnt = 0;
                        }
                        else if(y == min_y[maxy_index])
                        {
                            ++maxy_cnt;
                        }
                        break;
                    }
                }
            }
            int transitions = 0;
            for(int y = beg_y; y <= pool_h; ++y)
            {
                int last = 1; //pool.row[y] & 1;
                if(pool.row[y] > 0)
                {
                    for(int x = 0; x < pool_w; ++x)
                    {
                        if(pool.row[y] & (1 << x))
                        {
                            if(last == 0) ++transitions;
                            last = 1;
                        }
                        else
                        {
                            if(last == 1) ++transitions;
                            last = 0;
                            if(y >= 0) ++emptys[y];
                        }
                    }
                }
                else
                {
                    emptys[y] = pool_w;
                }
                transitions += !last;
            }
            score += ai_param.v_transitions * transitions / 10;
        }
        min_y[pool_w] = min_y[pool_w - 2];
        //last_min_y[pool_w] = last_min_y[pool_w-2];

        if(pool.m_hold == GEMTYPE_I)
        {
            score -= ai_param.hold_I;
        }
        if(pool.m_hold == GEMTYPE_T)
        {
            score -= ai_param.hold_T;
        }
        if(maxy_cnt > 0)
        {
            int ybeg = min_y[maxy_index];
            unsigned rowdata = pool.row[ybeg - 1];
            int empty = ~rowdata & pool.m_w_mask;
            for(int b = maxy_index; b < pool_w; ++b)
            {
                if(ybeg != min_y[b]) continue;
                int cnt = 1;
                for(int b1 = b + 1; empty & (1 << b1); ++b1) ++cnt;
                if(maxy_flat_cnt < cnt)
                {
                    maxy_flat_cnt = cnt;
                    maxy_index = b;
                }
            }
        }
        // 洞的数量
        int x_holes[32] = {0}; // 水平方向洞的数量
        int y_holes[32] = {0}; // 垂直方向洞的数量
        int x_op_holes[32] = {0}; // 水平方向洞的数量
                                  //int last_pool_hole_score;
        int pool_hole_score;
        int pool_total_cell = 0;
        //{   // last_pool
        //    int x_holes[32] = {0}; // 水平方向洞的数量
        //    int x_op_holes[32] = {0}; // 水平方向洞的数量
        //    int first_hole_y[32] = {0}; // 垂直方向最近的洞的y
        //    int hole_score = 0;
        //    const GameField& _pool = last_pool;
        //    for ( int x = 0; x < pool_w; ++x) {
        //        int last = 0, next;
        //        first_hole_y[x] = pool_h + 1;
        //        for ( int y = last_min_y[x] + 1; y <= pool_h; ++y, last = next) {
        //            if ( ( _pool.row[y] & ( 1 << x ) ) == 0) {
        //                next = 1;
        //                if ( x > 1 ) {
        //                    if (last_min_y[x-1] > y && last_min_y[x-2] > y) {
        //                        if ( last == 0) {
        //                            hole_score += ai_param.open_hole;
        //                            if ( y >= 0 ) ++x_op_holes[y];
        //                            continue;
        //                        }
        //                    }
        //                }
        //                if ( x < pool_w - 2 ) {
        //                    if (last_min_y[x+1] > y && last_min_y[x+2] > y) {
        //                        if ( last == 0) {
        //                            hole_score += ai_param.open_hole;
        //                            if ( y >= 0 ) ++x_op_holes[y];
        //                            continue;
        //                        }
        //                    }
        //                }
        //                if ( y >= 0 ) ++x_holes[y];
        //                if ( first_hole_y[x] > pool_h ) {
        //                    first_hole_y[x] = y;
        //                }
        //                if ( last ) {
        //                    hole_score += ai_param.hole / 4;
        //                } else {
        //                    //score += (y - min_y[x]) * ai_param.hole_dis;
        //                    //if ( x_holes[y] > 2 ) {
        //                    //    hole_score += ai_param.hole / 4;
        //                    //} else
        //                    if ( x_holes[y] >= 2 ) {
        //                        hole_score += ai_param.hole * x_holes[y];
        //                    } else {
        //                        hole_score += ai_param.hole * 2;
        //                    }
        //                }
        //            } else {
        //                next = 0;
        //            }
        //        }
        //    }
        //    //if(1)
        //    for ( int y = 0; y <= pool_h; ++y) {
        //        if ( x_holes[y] > 0 ) {
        //            hole_score += ai_param.hole_dis * (pool_h - y + 1);
        //            //int min_dis = pool_h;
        //            //for ( int x = 0; x < pool_w; ++x) {
        //            //    if ( first_hole_y[x] == y ) {
        //            //        min_dis = std::min(min_dis, y - min_y[x]);
        //            //    }
        //            //}
        //            //if ( min_dis == 1 ) {
        //            //    bool fill = true;
        //            //    for ( int i = y - min_dis; i < y ; ++i ) {
        //            //        int empty = ~pool.row[i] & pool.m_w_mask;
        //            //        if ( empty & ( empty - 1) ) {
        //            //            fill = false;
        //            //            break;
        //            //        }
        //            //    }
        //            //    if ( fill ) {
        //            //        score -= ai_param.hole_dis;
        //            //    }
        //            //}
        //            break;
        //        }
        //    }
        //    //for ( int y = 0; y <= pool_h; ++y) {
        //    //    if ( x_holes[y] ) score += ai_param.has_hole_row;
        //    //}
        //    last_pool_hole_score = hole_score;
        //}
        {   // pool
            int first_hole_y[32] = {0}; // 垂直方向最近的洞的y
            int x_renholes[32] = {0}; // 垂直连续洞的数量
            double hole_score = 0;
            const auto& _pool = pool;
            for(int x = 0; x < pool_w; ++x)
            {
                for(int y = min_y[x]; y <= pool_h; ++y)
                {
                    if((_pool.row[y] & (1 << x)) == 0)
                    {
                        pool_total_cell++;
                    }
                }
            }
            for(int x = 0; x < pool_w; ++x)
            {
                int last = 0, next;
                first_hole_y[x] = pool_h + 1;
                for(int y = std::min(min_y[x] + 1, std::max(min_y[x - 1] + 6, min_y[x + 1] + 6)); y <= pool_h; ++y, last = next)
                {
                    if((_pool.row[y] & (1 << x)) == 0)
                    { //_pool.row[y] && 
                        double factor = (y < 20 ? (1 + (20 - y) / 20.0 * 2) : 1.0);
                        y_holes[x]++;
                        next = 1;
                        if(softdropEnable())
                        {
                            if(x > 1)
                            {
                                if(min_y[x - 1] > y && min_y[x - 2] > y)
                                {
                                    //if ( last == 0) {
                                    hole_score += ai_param.open_hole * factor;
                                    if(y >= 0) ++x_op_holes[y];
                                    continue;
                                    //}
                                }
                            }
                            if(x < pool_w - 2)
                            {
                                if(min_y[x + 1] > y && min_y[x + 2] > y)
                                {
                                    //if ( last == 0) {
                                    hole_score += ai_param.open_hole * factor;
                                    if(y >= 0) ++x_op_holes[y];
                                    continue;
                                    //}
                                }
                            }
                        }
                        if(y >= 0) ++x_holes[y];
                        if(first_hole_y[x] > pool_h)
                        {
                            first_hole_y[x] = y;
                        }
                        int hs = 0;
                        if(last)
                        {
                            hs += ai_param.hole / 2;
                            if(y >= 0) ++x_renholes[y];
                        }
                        else
                        {
                            hs += ai_param.hole * 2;
                        }
                        {
                            //if ( x_holes[y] == 2 ) {
                            //    hs -= ai_param.hole;
                            //} else if ( x_holes[y] >= 3 ){
                            //    hs -= ai_param.hole * 2;
                            //}
                            ++total_hole;
                        }
                        hole_score += hs * factor;
                    }
                    else
                    {
                        next = 0;
                    }
                }
            }
            //for ( int y = 0; y <= pool_h; ++y) {
            //    if ( x_holes[y] > 1 ) {
            //        int n = x_holes[y] - x_renholes[y];
            //        int hs = 0;
            //        if ( n == 2 )
            //            hs = ai_param.hole + x_renholes[y] * ai_param.hole / 2;
            //        else if ( n > 2 )
            //            hs = (n - 2) * ai_param.hole * 2 + x_renholes[y] * ai_param.hole / 2;
            //        hole_score -= hs * ( y < 10 ? ( 1 + (10 - y) / 10.0 * 2 ) : 1.0);
            //        score -= ai_param.v_transitions * x_holes[y] / 10;
            //    }
            //}
            for(int y = 0; y <= pool_h; ++y)
            {
                if(x_holes[y] > 0)
                {
                    score += ai_param.hole_dis * (pool_h - y + 1);
                    break;
                }
            }
            if(1)
                if(ai_param.hole_dis_factor)
                {
                    for(int y = 0, cnt = 5, index = -1; y <= pool_h; ++y)
                    {
                        if(x_holes[y] > 0)
                        {
                            if(cnt > 0) --cnt, ++index;
                            else break;
                            for(int x = 0; x <= pool_w; ++x)
                            {
                                if((_pool.row[y] & (1 << x)) == 0)
                                {
                                    int h = y - min_y[x];
                                    if(h > 4 - index) h = 4 + (h - (4 - index)) * cnt / 4;
                                    //if ( h > 4 ) h = 4;
                                    if(h > 0)
                                    {
                                        if((_pool.row[y - 1] & (1 << x)) != 0)
                                        {
                                            score += ai_param.hole_dis_factor * h * cnt / 5 / 2;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            if(1)
                if(ai_param.hole_dis_factor2)
                {
                    int miny = pool_h;
                    for(int y = 0, cnt = 0; y <= pool_h; ++y)
                    {
                        if(x_holes[y] > 0)
                        {
                            if(cnt < 4) ++cnt;
                            else break;
                            for(int x = 0; x <= pool_w; ++x)
                            {
                                if((_pool.row[y] & (1 << x)) == 0)
                                {
                                    int vy = min_y[x] + cnt * 1;
                                    if(vy < miny) miny = vy;
                                }
                            }
                        }
                    }
                    for(int x = 0; x <= pool_w; ++x)
                    {
                        int vy = min_y[x] + 6;
                        if(vy < miny) miny = vy;
                    }
                    double total_emptys = 0;
                    for(int y = miny; y <= pool_h; ++y)
                    {
                        total_emptys += emptys[y] * (y < 10 ? (10 + 10 - y) / 10.0 : 1);
                    }
                    score += ai_param.hole_dis_factor2 * total_emptys / 4;
                    //score += ai_param.hole_dis_factor2 * (pool_h - miny);
                    //int h = 0;
                    //h = min_y[maxy_index] - miny - 1;
                    //if ( h > 0 )
                    //    score += ai_param.hole_dis_factor2 * h;
                    //for ( int x = 0; x <= pool_w; ++x) {
                    //    h += min_y[x] - miny;
                    //}
                    //if ( h > 0 )
                    //    score += ai_param.hole_dis_factor2 * h * 2 / pool_w;
                }
            //for ( int y = 0; y <= pool_h; ++y) {
            //    if ( x_holes[y] ) score += ai_param.has_hole_row;
            //}
            pool_hole_score = hole_score;
        }
        score += pool_hole_score;
#ifdef XP_RELEASE
        // 全消
        if(0) //&& pool.getPCAttack() > 8 )
        {
            if(beg_y > pool_h)
            {
                //score -= 1000;
                clearScore -= (2000 * pool.combo - curdepth * 50) * 1.0;
            }
            else if(pool_total_cell % 2 == 0 && beg_y > pool_h - 10 && total_hole < 2)
            {
                int h_hole[32] = {0};
                auto const & _pool = pool;
                int top_y = beg_y, hole_add = 0, height;
                if(pool_total_cell % 4) top_y -= 1;
                //if ( x_holes[beg_y+1] > 0 ) top_y -= 2;
                //if ( top_y > pool_h - 2 ) top_y -= 2;
                height = pool_h + 1 - top_y;
                if(height < 3) height += 2;
                for(int x = 0; x < pool_w; ++x)
                {
                    h_hole[x] = min_y[x] - top_y + y_holes[x];
                }

                int last_cnt = 0, pc = 1, finish = 0, h4 = 0, cnt1 = 0, total_cnt = 0;
                for(int x = 0; x < pool_w; ++x)
                {
                    total_cnt += h_hole[x];
                }
                for(int x = 0; x < pool_w; ++x)
                {
                    if(h_hole[x] == 0)
                    {
                        if(last_cnt % 4 != 0)
                        {
                            //pc = 0;
                            top_y -= 2;
                            height += 2;
                            x = -1;
                            last_cnt = 0, finish = 0, h4 = 0, cnt1 = 0, total_cnt += pool_w * 2;
                            {
                                for(int x = 0; x < pool_w; ++x)
                                {
                                    h_hole[x] += 2;
                                }
                            }
                        }
                        else if(last_cnt <= 0 || last_cnt >= total_cnt - 0) finish++;
                        else ++h4;
                    }
                    else
                    {
                        if(min_y[x] == top_y + 1)
                        {
                            ++cnt1;
                        }
                        last_cnt += h_hole[x];
                    }
                }
                //if ( pc && ( height <= 4 || last_cnt < height * 6 ) ) {
                if(pc && ((total_cnt) / 4 * 0.8 + total_clear_att < pool.getPCAttack()))
                { //&& last_cnt < 30 ) {
                  //if ( beg_y > pool_h - 4 ) score -= 500 * 2;
                  //int s = 1000 * 5 + finish * 400 - h4 * 10 - (height + total_clears) * 800 - cnt1 * 20;
                  //int s = (50 + finish * 10 + h4 * 1 - (height + total_clears) * 10 - cnt1 * 2) * 2.0;
                    int s = (30 + finish * 2 + h4 * 1 - (height /*+ total_clears * 2*/)* 3 /*- cnt1 * 2 */) * 1.0;
                    //int s = 100 * 2 + finish * 3 - h4 * 1 - (height + total_clears) * 10 - cnt1 * 2;
                    //score -= 1000 * 4 + finish * 100 - last_cnt * 30 - cnt1 * 50;
                    //score -= 500 * 4 + finish * 100 - last_cnt * 30 - cnt1 * 50 - curdepth * 100;
                    if(s > 0)
                        score -= s;
                }
            }
        }
#endif
        // 高度差
        {
            //int n_maxy_index = maxy_index;
            //if ( maxy_cnt != 0 ) n_maxy_index = -9;

            int last = min_y[1];
            for(int x = 0; x <= pool_w; last = min_y[x], ++x)
            {
                int v = min_y[x] - last;
                if(x == maxy_index)
                {
                    x += maxy_flat_cnt;
                    continue;
                }
                int absv = abs(v);
                if(x + 1 == maxy_index && v > 0 || x - 2 == maxy_index && v < 0); //
                else score += absv * ai_param.h_factor;
            }
        }
        // 平地
        /*
        {
        int last = -1, len = 0;
        for ( int x = 0; x <= pool_w; ++x) {
        if ( last == min_y[x] ) {
        ++len;
        } else {
        if ( len > 0 && len < 4) {
        score -= ((len - 1) / 3 + 1) * ai_param.flat_factor;
        }
        len = 0;
        last = min_y[x];
        }
        }
        if ( len > 0 && len < 4) {
        score -= ((len - 1) / 3 + 1) * ai_param.flat_factor;
        }
        }
        */
        int center = 10; // 摆楼警戒线
        double warning_factor = 1;
        int h_variance_score = 0;
        // 算方差
        {
            int avg = 0;
            {
                int sum = 0;
                int sample_cnt = 0;
                for(int x = 0; x < pool_w; ++x)
                {
                    avg += min_y[x];
                }
                if(0)
                {
                    double h = pool_h - (double)avg / pool_w;
                    score += int(ai_param.miny_factor * h * h / pool_h);
                }
                else
                {
                    int h = std::min(std::min(min_y[gem_beg_x], min_y[gem_beg_x + 1]), std::min(min_y[gem_beg_x + 2], min_y[gem_beg_x + 3]));
                    if(h < 8)
                    {
                        score += int(ai_param.miny_factor * (8 - h) * 2);
                    }
                }
                if(1)
                {
                    if(avg < pool_w * center)
                    {
                        warning_factor = 0.0 + (double)avg / pool_w / center / 1;
                    }
                }
                // 偏差值
                {
                    int dif_sum = 0;
                    for(int x = 0; x < pool_w; ++x)
                    {
                        dif_sum += abs(min_y[x] * pool_w - avg);
                    }
                    score += ai_param.dif_factor * dif_sum / pool_w / pool_w;
                }
            }
            // 攻击计算
            {
                int s = 0;
                int t_att = total_clear_att;
                double t_clear = total_clears; //+ total_clears / 4.0;
                if(pool.b2b) s -= 5; // b2b score
                if(t_clear > 0)
                {
                    s -= int(((ai_param.clear_efficient) * (t_att)));
                }
                {
                    //if ( t_clear > t_att ) {
                    //int warning_factor = 0.5 + (double)avg / pool_w / center / 2;
                    s += int(warning_factor * t_clear * ai_param.clear_useless_factor);
                    //}
                }
                int cs = 0;
                if(cur_num == GEMTYPE_T && wallkick_spin && clears > 0 && ai_param.tspin > 0)
                { // T消附加分，要比T1/T2形状基本分大一
                    s -= ai_param.hold_T;
                    if(clears >= 3)
                    {
                        if(clear_att >= clears * 2)
                        { // T3
                            cs -= int(warning_factor * (ai_param.tspin3 * 8) + ai_param.hole * 2);
                        }
                    }
                    else if(clears >= 2)
                    {
                        if(clear_att >= clears * 2)
                        { // T2
                            cs -= int(warning_factor * (ai_param.tspin * 5 + ai_param.open_hole / 2));
                        }
                    }
                    else if(wallkick_spin == 1)
                    { // T1
                        cs -= int(warning_factor * (ai_param.tspin * 1 + ai_param.open_hole / 2));
                    }
                    else if(wallkick_spin == 2)
                    { // Tmini
                        cs -= int(warning_factor * (ai_param.tspin / 2));
                    }
                }
                clearScore += cs;
#ifdef XP_RELEASE
                if(1)
                    if(clears > 0 && upcomeAtt >= 4 && ai_param.upcomeAtt > 0)
                    {
                        int cur_s = 0;
                        cur_s -= int(((ai_param.clear_efficient) * (clear_att)))
                            - int(warning_factor * clears * ai_param.clear_useless_factor);
                        if(avg - (12 + upcomeAtt) * pool_w > 0 && cur_s + cs < 0)
                            s -= (cur_s + cs) * (avg - (12 + upcomeAtt) * pool_w) * ai_param.upcomeAtt / pool_w / 10 / 20;
                        //if ( upcomeAtt >= 4 ) {
                        //    if ( total_hole < 4 && avg - upcomeAtt * pool_w >= 8 * pool_w ) {
                        //        s = s - s * ( 4 - total_hole ) * ai_param.upcomeAtt / 40;
                        //    }
                        //}
                    }
#endif
                score += s;
            }
            //if ( clears ) {
            //    int center = 10; // 摆楼警戒线
            //    double factor = 1;
            //    if ( avg < pool_w * center ) {
            //        factor = (double)avg / pool_w / center;
            //    }
            //    int s = 0;
            //    if ( pool_hole_score < last_pool_hole_score ) {
            //        s -= int( factor * (ai_param.clear_efficient * ( clear_att ) * ( clear_att ) / clears) );
            //        //s -= ai_param.open_hole;
            //        if ( clear_att >= 4 ) {
            //            if ( clear_att >= clears * 2 ) { // T2/T3
            //                clearScore -= int( factor * (ai_param.tspin * 4 + ai_param.open_hole + ai_param.clear_efficient * ( clear_att ) ) );
            //                s -= ai_param.hold_T;
            //            }
            //        }
            //        if ( clears > clear_att ) {
            //            s += int( factor * (ai_param.clear_efficient * ( clears - clear_att ) / 2 ) );
            //        }
            //    } else if ( pool_hole_score == last_pool_hole_score ) {
            //        s -= int( factor * (ai_param.clear_efficient * ( clear_att ) * ( clear_att ) / clears) );
            //        if ( clear_att >= 4 ) {
            //            if ( clear_att >= clears * 2 ) { // T2/T3
            //                clearScore -= int( factor * (ai_param.tspin * 4 + ai_param.open_hole + ai_param.clear_efficient * ( clear_att ) ) );
            //                s -= ai_param.hold_T;
            //            }
            //        } else if ( clear_att >= clears ) {
            //            if ( clear_att >= clears * 2 ) {
            //                if ( clears == 1 ) { // T1
            //                    //s += int( factor * (ai_param.clear_efficient * ( clear_att ) / clears) );
            //                }
            //            }
            //        } else if ( avg < 8 * pool_w ) {
            //            //s += int(ai_param.hole * ( clears - clear_att ) * factor / 2 );
            //            if ( clears > clear_att ) {
            //                s += int( factor * (ai_param.clear_efficient * ( clears - clear_att ) / 2 ) );
            //            }
            //        } else if ( total_hole >= 1 || min_y[maxy_index] < pool_h - 4 ) {
            //            if ( clears > clear_att ) {
            //                s += int( factor * (ai_param.clear_efficient * ( clears - clear_att ) * 2 ) );
            //            }
            //            //if ( clear_att == 0 ) {
            //            //    s += int( factor * (ai_param.hole * ( clears - clear_att ) ) / 3 );
            //            //}
            //        } else {
            //            if ( clears > clear_att ) {
            //                s += int( factor * (ai_param.clear_efficient * ( clears - clear_att ) * 4) );
            //            }
            //            //if ( clear_att == 0 ) {
            //            //    s += int( factor * (ai_param.hole * ( clears - clear_att ) ) / 3 );
            //            //}
            //        }
            //    } else {
            //        s -= int( factor * (ai_param.clear_efficient * ( clear_att ) / clears) );
            //        if ( clears > clear_att ) {
            //            s += int( factor * (ai_param.clear_efficient * ( clears - clear_att ) * 4 ) );
            //        }
            //    }
            //    if ( pool.combo > 2 )
            //    {
            //        int combo = pool.combo - 2;
            //        //clearScore -= combo * combo * ai_param.combo_factor;
            //    }
            //    score += s;
            //}
        }

        // 特殊形状判定

        // 计算可攻击（Tetris和T2）
        //int t2_x[32] = {0};
        if(maxy_cnt == 0)
        {
            //if ( maxy_index == 0 || maxy_index == pool_w - 1 ) {
            //    score += ai_param.att_col_sel_side;
            //}
            int ybeg = 0;
            if(softdropEnable() && maxy_index > 0 && maxy_index < pool_w - 1 && ai_param.tspin > 0)
            { // T1/T2基本形状分
                ybeg = std::max(min_y[maxy_index - 1], min_y[maxy_index + 1]);
                if(min_y[maxy_index - 1] == min_y[maxy_index + 1]
                   && x_holes[ybeg] == 0 && x_holes[ybeg - 1] == 0
                   && x_op_holes[ybeg] == 0 && x_op_holes[ybeg - 1] == 0
                   )
                { // T准备
                    int cnt = 0;
                    if(maxy_index > 1 && min_y[maxy_index - 2] >= min_y[maxy_index - 1] - 2) ++cnt;
                    if(maxy_index < pool_w - 2 && min_y[maxy_index + 2] >= min_y[maxy_index + 1] - 2) ++cnt;
                    if(cnt > 0)
                    {
                        score -= int(warning_factor * ai_param.tspin);
                        if((~pool.row[ybeg] & pool.m_w_mask) == (1 << maxy_index))
                        { // T1基础
                            score -= int(warning_factor * ai_param.tspin);
                            if((~pool.row[ybeg - 1] & pool.m_w_mask) == (7 << (maxy_index - 1)))
                            { // 可T2完美坑
                                score -= int(warning_factor * (ai_param.tspin * cnt));
                            }
                        }
                    }
                }
                else if(ybeg <= 6 && ybeg - t_dis > 1 || ybeg > 6)
                {
                    int row_data = pool.row[ybeg - 1];
                    if((row_data & (1 << (maxy_index - 1))) == 0 && (row_data & (1 << (maxy_index + 1))) == 0 // 坑的左右为空
                       && x_holes[ybeg] == 0 && x_holes[ybeg - 1] == 0 // 其它位置无洞
                       && x_op_holes[ybeg] == 0 && x_op_holes[ybeg - 1] <= 1
                       )
                    {
                        // T坑形状
                        if((pool.row[ybeg] & (1 << (maxy_index - 1))) && (pool.row[ybeg] & (1 << (maxy_index + 1))))
                        { // 坑的下面两块存在
                            if(!!(pool.row[ybeg - 2] & (1 << (maxy_index - 1))) + !!(pool.row[ybeg - 2] & (1 << (maxy_index + 1))) == 1)
                            { // 坑的上面的块存在
                                double s = 0;
                                //t2_x[maxy_index] = ybeg;
                                double factor = ybeg > 6 ? 0.5 : 1 - t_dis / 6.0 * 0.5;
                                if(warning_factor < 1)
                                    factor = ybeg > 6 ? 1.0 / 5 : 1 / (1 + t_dis / 3.0);
                                s += ai_param.open_hole;
                                if((~pool.row[ybeg] & pool.m_w_mask) == (1 << maxy_index))
                                { // 可T1
                                    s += ai_param.tspin + ai_param.tspin * 1 * factor;
                                    if((~row_data & pool.m_w_mask) == (7 << (maxy_index - 1)))
                                    { // 可T2完美坑
                                        s += ai_param.tspin * 3 * factor;
                                        // s -= ai_param.tspin * 3 / factor / 1;
                                    }
                                }
                                else
                                {
                                    s += ai_param.tspin * 1 + ai_param.tspin * 2 * factor / 2;
                                }
                                score -= int(warning_factor * s);
                            }
                        }
                    }
                }
            }
            else
            {
                if(maxy_index == 0)
                {
                    ybeg = min_y[maxy_index + 1];
                }
                else
                {
                    ybeg = min_y[maxy_index - 1];
                }
            }
            int readatt = 0;
            int last = pool.row[ybeg];
            for(int y = ybeg; y <= pool_h; ++y)
            {
                if(last != pool.row[y]) break;
                int row_data = ~pool.row[y] & pool.m_w_mask;
                if((row_data & (row_data - 1)) != 0) break;
                ++readatt;
            }
            if(readatt > 4) readatt = 4;
            //score -= readatt * ai_param.readyatt;

        }
        // T3 形状判定
        //3001
        //2000
        // 1101
        // 1x01
        // 1101
        //
        // 1003
        // 0002
        //1011 
        //10x1
        //1011
        if(softdropEnable() && ai_param.tspin3 > 0)
        {
            for(int y = 3; y < pool_h; ++y)
            {
                if(x_holes[y] == 0) continue;
                for(int x = 1; x < pool_w - 1; ++x)
                {
                    if((pool.row[y + 1] & (1 << x)) == 0 || (pool.row[y + 1] & (1 << x)) == 0)
                    {
                        continue; // 上下无洞
                    }
                    int row_y[5];
                    for(int i = 0; i < 5; ++i)
                    {
                        row_y[i] = ((pool.row[y - 3 + i] | (3 << pool_w)) << 2) | 3;
                    }
                    if(((row_y[3] >> (x + 1)) & (7)) == 1 /*100*/)
                    { // 上图情况
                        if(x == pool_w - 2) continue;
                        //if ( t2_x[x+1] == y ) continue; // 排除T2坑
                        // 所有空的地方先匹配
                        if(((row_y[2] >> (x + 1)) & (7)) != 3 /*110*/
                                                              //|| ( (row_y[4] >> (x + 1)) & ( 15 ) ) != 11 /*1101*/
                           || ((row_y[4] >> (x + 1)) & (13)) != 9 /*1011mask=1001*/
                           || ((row_y[1] >> (x + 1)) & (7)) != 0 /*000*/
                                                                 //|| ( (row_y[0] >> (x + 1)) & ( 3 ) ) != 0 /*00*/
                           )
                        {
                            continue;
                        }
                        if(min_y[x] != y - 1 || min_y[x - 1] != y - 1)
                        {
                            continue;
                        }
                        if((row_y[0] & (1 << (x))) == 0 && (row_y[1] & (1 << (x))))
                        {
                            continue; // 高处转角
                        }
                        if(min_y[x + 1] > y)
                        { // 洞判定
                            if(x_holes[y - 1] > 0 || x_holes[y + 1] > 0 || x_holes[y] > 1
                               || x_op_holes[y - 1] > 0 || x_op_holes[y + 1] > 0 || x_op_holes[y] > 0)
                            {
                                continue;
                            }
                        }
                        else
                        {
                            if(x_holes[y - 1] > 1 || x_holes[y + 1] > 1 || x_holes[y] > 2
                               || x_op_holes[y - 1] > 0 || x_op_holes[y + 1] > 0 || x_op_holes[y] > 0)
                            {
                                continue;
                            }
                        }
                        if(((row_y[0] >> (x + 3)) & (1)) == 0 && y - min_y[x + 2] > 3) continue;
                        int s = 0;
                        //tp3 * 1
                        s -= int(warning_factor * ai_param.tspin3);// + int( warning_factor * ( ai_param.tspin * 4 + ai_param.open_hole ) );
                        score += s;
                        if(y <= pool_h - 3 && (pool.row[y + 3] & (1 << x)) == 0)
                        {
                            int r = ~pool.row[y + 3] & pool.m_w_mask;
                            if((r & (r - 1)) == 0)
                            {
                                score -= int(warning_factor * (ai_param.tspin * 4 + ai_param.open_hole));
                            }
                        }
                        //int full = 0;
                        {
                            int e = ~(pool.row[y + 1] | (1 << x)) & pool.m_w_mask;
                            e &= (e - 1);
                            if(e == 0)
                            { // 最底只剩一空
                              //++full;
                            }
                            else
                            {
                                score -= s;
                                continue;
                            }
                        }
                        {
                            int e = ~(pool.row[y] | (1 << (x + 2))) & pool.m_w_mask;
                            e &= (e - 1);
                            if((e & (e - 1)) == 0)
                            { // 底二只剩两空
                              //++full;
                            }
                            else
                            {
                                if((pool.row[y] & (1 << (x + 2))) == 0)
                                {
                                    score -= int(warning_factor * ai_param.tspin3 * 3);
                                }
                                score -= s;
                                score -= int(warning_factor * ai_param.tspin3 / 3);
                                continue;
                            }
                        }
                        {
                            int e = ~pool.row[y - 1] & pool.m_w_mask;
                            e &= (e - 1);
                            if(e == 0)
                            { // 底三只剩一空
                              //++full;
                            }
                            else
                            {
                                score -= s;
                                score -= int(warning_factor * ai_param.tspin3);
                                continue;
                            }
                        }
                        score -= int(warning_factor * ai_param.tspin3 * 3);
                        if(pool.row[y - 3] & (1 << (x + 1)))
                        {
                            if(t_dis < 7)
                            {
                                score -= int(warning_factor * (ai_param.tspin3 * 1) + ai_param.hole * 2);
                                score -= int(warning_factor * ai_param.tspin3 * 3 / (t_dis + 1));
                            }
                        }
                    }
                    else if(((row_y[3] >> (x + 1)) & (7)) == 4 /*001*/)
                    { // 镜像情况
                        if(x == 1) continue;
                        //if ( t2_x[x-1] == y ) continue; // 排除T2坑
                        // 所有空的地方先匹配
                        if(((row_y[2] >> (x + 1)) & (7)) != 6 /*011*/
                                                              //|| ( (row_y[4] >> (x)) & ( 15 ) ) != 13 /*1011*/
                           || ((row_y[4] >> (x)) & (11)) != 9 /*1101mask=1001*/
                           || ((row_y[1] >> (x + 1)) & (7)) != 0 /*000*/
                                                                 //|| ( (row_y[0] >> (x + 1)) & ( 3 ) ) != 0 /*00*/
                           )
                        {
                            continue;
                        }
                        if(min_y[x] != y - 1 || min_y[x + 1] != y - 1)
                        {
                            continue;
                        }
                        if((row_y[0] & (1 << (x + 4))) == 0 && (row_y[1] & (1 << (x + 4))))
                        {
                            continue; // 高处转角
                        }
                        if(min_y[x - 1] > y)
                        { // 洞判定
                            if(x_holes[y - 1] > 0 || x_holes[y + 1] > 0 || x_holes[y] > 1
                               || x_op_holes[y - 1] > 0 || x_op_holes[y + 1] > 0 || x_op_holes[y] > 0)
                            {
                                continue;
                            }
                        }
                        else
                        {
                            if(x_holes[y - 1] > 1 || x_holes[y + 1] > 1 || x_holes[y] > 2
                               || x_op_holes[y - 1] > 0 || x_op_holes[y + 1] > 0 || x_op_holes[y] > 0)
                            {
                                continue;
                            }
                        }
                        if(((row_y[0] >> (x + 1)) & (1)) == 0 && y - min_y[x - 2] > 3) continue;
                        int s = 0;
                        // tp3 * 1
                        s -= int(warning_factor * ai_param.tspin3);// + int( warning_factor * ( ai_param.tspin * 4 + ai_param.open_hole ) );
                        score += s;
                        if(y <= pool_h - 3 && (pool.row[y + 3] & (1 << x)) == 0)
                        {
                            int r = ~pool.row[y + 3] & pool.m_w_mask;
                            if((r & (r - 1)) == 0)
                            {
                                score -= int(warning_factor * (ai_param.tspin * 4 + ai_param.open_hole));
                            }
                        }
                        //int full = 0;
                        {
                            int e = ~(pool.row[y + 1] | (1 << x)) & pool.m_w_mask;
                            e &= (e - 1);
                            if(e == 0)
                            { // 最底只剩一空
                              //++full;
                            }
                            else
                            {
                                score -= s;
                                continue;
                            }
                        }
                        {
                            int e = ~(pool.row[y] | (1 << x - 2)) & pool.m_w_mask;
                            e &= (e - 1);
                            if((e & (e - 1)) == 0)
                            { // 底二只剩两空
                              //++full;
                            }
                            else
                            {
                                if((pool.row[y] & (1 << (x - 2))) == 0)
                                {
                                    score -= int(warning_factor * ai_param.tspin3 * 3);
                                }
                                score -= s;
                                score -= int(warning_factor * ai_param.tspin3 / 3);
                                continue;
                            }
                        }
                        {
                            int e = ~pool.row[y - 1] & pool.m_w_mask;
                            e &= (e - 1);
                            if(e == 0)
                            { // 底三只剩一空
                              //++full;
                            }
                            else
                            {
                                score -= s;
                                score -= int(warning_factor * ai_param.tspin3);
                                continue;
                            }
                        }
                        score -= int(warning_factor * ai_param.tspin3 * 3);
                        if(pool.row[y - 3] & (1 << (x - 1)))
                        {
                            if(t_dis < 7)
                            {
                                score -= int(warning_factor * (ai_param.tspin3 * 1) + ai_param.hole * 2);
                                score -= int(warning_factor * ai_param.tspin3 * 3 / (t_dis + 1));
                            }
                        }
                    }
                }
            }
        }
        // 4W形状判定
        if(USE4W)
            if(ai_param.strategy_4w > 0 && total_clears < 1) //&& lastCombo < 1 && pool.combo < 1 )
            {
                int maxy_4w = min_y[3];
                maxy_4w = std::max(maxy_4w, min_y[4]);
                maxy_4w = std::max(maxy_4w, min_y[5]);
                maxy_4w = std::max(maxy_4w, min_y[6]);
                int maxy_4w_combo = min_y[0];
                maxy_4w_combo = std::max(maxy_4w_combo, min_y[1]);
                maxy_4w_combo = std::max(maxy_4w_combo, min_y[2]);
                maxy_4w_combo = std::max(maxy_4w_combo, min_y[pool_w - 3]);
                maxy_4w_combo = std::max(maxy_4w_combo, min_y[pool_w - 2]);
                maxy_4w_combo = std::max(maxy_4w_combo, min_y[pool_w - 1]);
                if((min_y[4] < min_y[3] && min_y[4] <= min_y[5])
                   || (min_y[5] < min_y[6] && min_y[5] <= min_y[4]))
                {
                    maxy_4w = -10;
                }
                else
                    for(int x = 0; x < pool_w; ++x)
                    {
                        if(min_y[x] > maxy_4w)
                        {
                            maxy_4w = -10;
                            break;
                        }
                    }
                while(maxy_4w > 0)
                {
                    //if ( abs( min_y[0] - min_y[1] ) > 4 ) { maxy_4w = -10; break; }
                    //if ( abs( min_y[1] - min_y[2] ) > 4 ) { maxy_4w = -10; break; }
                    //if ( abs( min_y[pool_w-1] - min_y[pool_w-2] ) > 4 ) { maxy_4w = -10; break; }
                    //if ( abs( min_y[pool_w-2] - min_y[pool_w-3] ) > 4 ) { maxy_4w = -10; break; }
                    //if ( abs( min_y[2] - min_y[pool_w-3] ) > 7 ) { maxy_4w = -10; break; }
                    //int avg = (min_y[0] + min_y[1] + min_y[2] + min_y[pool_w-1] + min_y[pool_w-2] + min_y[pool_w-3]) / 6;
                    if((pool_h - maxy_4w) * 2 >= maxy_4w - maxy_4w_combo)
                    {
                        maxy_4w = -10;
                        break;
                    }
                    break;
                }
                if(maxy_4w <= pool_h - 4)
                { // 如果有超过4攻击行就不搭
                    maxy_4w = -10;
                }
                //if ( maxy_4w - maxy_4w_combo > 15 ) { // 如果有超过10预备行就不搭
                //    maxy_4w = -10;
                //}
                if(maxy_4w - maxy_4w_combo < 9 && pool_hole_score > ai_param.hole * (maxy_4w - maxy_4w_combo) / 2)
                {
                    maxy_4w = -10;
                }

                if(maxy_4w > 8)
                {
                    bool has_hole = false;
                    for(int y = maxy_4w - 1; y >= 0; --y)
                    {
                        if(x_holes[y] || x_op_holes[y])
                        {
                            has_hole = true;
                            break;
                        }
                    }
                    if(!has_hole && maxy_4w < pool_h)
                    {
                        if(x_holes[maxy_4w]>1 || x_op_holes[maxy_4w]>1)
                        {
                            has_hole = true;
                        }
                    }

                    if(!has_hole)
                    {
                        int sum = maxy_4w - min_y[3];
                        sum += maxy_4w - min_y[4];
                        sum += maxy_4w - min_y[5];
                        sum += maxy_4w - min_y[6];
                        int s = 0;
                        if(sum == 3 || sum == 0 || sum == 4) //{ // - (pool_h - maxy_4w) - clears * lastCombo * 2
                        {
                            int hv = (maxy_4w - maxy_4w_combo + 1) * 1 + pool.combo;
                            s += ai_param.strategy_4w * (hv)+(ai_param.hole * 2 + ai_param.tspin * 4);
                            if(sum > 0)
                            {
                                s -= ai_param.strategy_4w / 3;
                            }
                        }
                        if(s > 0)
                        {
                            score -= s;
                        }
                        //if ( pool_h * 4 + 4 + x_holes[pool_h] + x_op_holes[pool_h] - min_y[0] - min_y[1] - min_y[2] - min_y[3] <= 4 ) {
                        //    score -= 800 + (ai_param.hole * 2 + ai_param.tspin * 4);
                        //} else if ( pool_h * 4 + 4 + x_holes[pool_h] + x_op_holes[pool_h] - min_y[pool_w - 4] - min_y[pool_w - 3] - min_y[pool_w - 2] - min_y[pool_w - 1] <= 4 ) {
                        //    score -= 800 + (ai_param.hole * 2 + ai_param.tspin * 4);
                        //}
                    }
                }
            }
        // 累积分
#undef USE4W
#undef XP_RELEASE
#pragma warning(pop)
        score += clearScore;

        int att = 0;
        switch(eval_result.clear)
        {
        case 1:
            if(eval_result.t_spin == TSpinType::TSpinMini)
            {
                att += status.b2b ? 2 : 1;
            }
            else if(eval_result.t_spin == TSpinType::TSpin)
            {
                att += status.b2b ? 3 : 2;
            }
            att += config_->table[std::min(config_->table_max - 1, ++result.combo)];
            result.b2b = eval_result.t_spin != TSpinType::None;
            break;
        case 2:
            if(eval_result.t_spin != TSpinType::None)
            {
                att += status.b2b ? 5 : 4;
            }
            att += config_->table[std::min(config_->table_max - 1, ++result.combo)];
            result.b2b = eval_result.t_spin != TSpinType::None;
            break;
        case 3:
            if(eval_result.t_spin != TSpinType::None)
            {
                att += status.b2b ? 8 : 6;
            }
            att += config_->table[std::min(config_->table_max - 1, ++result.combo)] + 2;
            result.b2b = eval_result.t_spin != TSpinType::None;
            break;
        case 4:
            att += config_->table[std::min(config_->table_max - 1, ++result.combo)] + (status.b2b ? 5 : 4);
            result.b2b = true;
            break;
        }

        if(eval_result.clear > 0)
        {
            result.combo = status.combo + combo_step_max + 1 - eval_result.clear;
            if(status.upcomeAtt > 0)
                result.upcomeAtt = std::max(0, status.upcomeAtt - att);
        }
        else
        {
            result.combo = 0;
            if(status.upcomeAtt > 0)
            {
                result.upcomeAtt = -status.upcomeAtt;
            }
        }
        if(eval_result.map->count == 0 && result.upcomeAtt >= 0)
        {
            att += m_pc_att;
        }
        result.total_clear_att += att;
        result.total_clears += eval_result.clear;
        result.att = att;
        result.max_att = std::max(status.max_att, status.att + att);
        result.max_combo = std::max(status.max_combo, result.combo);
        result.score = score;
        result.strategy_4w = config_->strategy_4w;
        return result;
    }


}