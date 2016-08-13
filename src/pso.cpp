

#include <ctime>
#include <mutex>
#include <fstream>
#include <thread>
#include <array>
#include <random>
#include <sstream>
#include <iostream>
#include <chrono>

#include "tetris_core.h"
#include "search_cautious.h"
#include "ai_zzz.h"
#include "rule_c2.h"

struct pso_dimension
{
    double x;
    double p;
    double v;
};
struct pso_data
{
    size_t index;
    size_t count;
    double score;
    double best;
    std::array<pso_dimension, 64> data;
};

struct pso_config_element
{
    double x_min, x_max, v_max;
};
struct pso_config
{
    std::vector<pso_config_element> config;
    double c1, c2, w;
};

typedef std::vector<pso_data> pso_vector;

void pso_init(pso_config const &config, pso_vector &data, std::mt19937 &mt, size_t count)
{
    for(size_t c = 0; c < count; ++c)
    {
        pso_data item;
        item.index = c + 1;
        item.count = 0;
        item.score = item.best = std::numeric_limits<double>::quiet_NaN();
        for(size_t i = 0; i < config.config.size(); ++i)
        {
            auto &cfg = config.config[i];
            auto &d = item.data[i];
            d.x = std::uniform_real_distribution<double>(cfg.x_min, cfg.x_max)(mt);
            d.v = std::uniform_real_distribution<double>(-cfg.v_max, cfg.v_max)(mt);
            item.best = item.score = d.p = std::numeric_limits<double>::quiet_NaN();
        }
        data.emplace_back(item);
    }
}
void pso_logic(pso_config const &config, pso_data const &best, pso_data &data, std::mt19937 &mt)
{
    assert(config.config.size() <= data.data.size());
    for(size_t i = 0; i < config.config.size(); ++i)
    {
        auto &cfg = config.config[i];
        auto &item = data.data[i];
        item.x += item.v;
        item.v = item.v * config.w
            + std::uniform_real_distribution<double>(0, config.c1)(mt) * (item.p - item.x)
            + std::uniform_real_distribution<double>(0, config.c2)(mt) * (best.data[i].p - item.x)
            ;
        if(item.v > cfg.v_max)
        {
            item.v = cfg.v_max;
        }
        if(item.v < -cfg.v_max)
        {
            item.v = -cfg.v_max;
        }
    }
}








void save(std::string const &file, pso_vector &data)
{
    std::ofstream ofs(file, std::ios::out | std::ios::binary);
    for(auto &pair : data)
    {
        ofs.write(reinterpret_cast<char const *>(&pair), sizeof pair);
    }
    ofs.flush();
    ofs.close();
};
bool load(std::string const &file, pso_vector &data)
{
    std::ifstream ifs(file, std::ios::in | std::ios::binary);
    if(ifs.good())
    {
        pso_vector::value_type item;
        while(ifs.read(reinterpret_cast<char *>(&item), sizeof item).gcount() == sizeof item)
        {
            data.emplace_back(item);
        }
        ifs.close();
    }
    return !data.empty();
};

struct test_ai
{
    m_tetris::TetrisEngine<rule_c2::TetrisRule, ai_zzz::Dig, search_cautious::Search> ai;
    m_tetris::TetrisMap map;
    std::mt19937 r1, r2;
    std::vector<char> next;
    void init(pso_data const &data, pso_config const &config)
    {
        map = m_tetris::TetrisMap(10, 40);
        ai.prepare(10, 21);
        *ai.status() = 0;
        ai.search_config()->fast_move_down = true;
        for(size_t i = 0; i < config.config.size(); ++i)
        {
            ai.ai_config()->p[i] = data.data[i].x;
        }
        next.clear();
    }
    m_tetris::TetrisNode const *node() const
    {
        return ai.context()->generate(next.front());
    }
    bool prepare(size_t next_length)
    {
        if(!next.empty())
        {
            next.erase(next.begin());
        }
        while(next.size() <= next_length)
        {
            next.push_back(ai.context()->convert(std::uniform_int_distribution<size_t>(0, 6)(r1)));
        }
        return true;
    }
    bool run(size_t piece, size_t &up, double up_round)
    {
        char current = next.front();
        auto result = ai.run(map, ai.context()->generate(current), next.data() + 1, next.size() - 1, 10000);
        if(result.target == nullptr)
        {
            return false;
        }
        else
        {
            result.target->attach(map);
            if(piece >= up * up_round)
            {
                ++up;
                under_attack(1);
            }
            return true;
        }
    }
    void under_attack(int line)
    {
        if(line == 0)
        {
            return;
        }
        for(int y = map.height - 1; y >= line; --y)
        {
            map.row[y] = map.row[y - line];
        }
        for(int y = 0; y < line; ++y)
        {
            uint32_t row;
//            do
//            {
//                row = std::uniform_int_distribution<uint32_t>(0, ai.context()->full())(r2);
//            }
//            while(row == ai.context()->full());
            row = ai.context()->full() & ~(1 << std::uniform_int_distribution<uint32_t>(0, ai.context()->width() - 1)(r2));
            map.row[y] = row;
        }
        map.count = 0;
        for(int my = 0; my < map.height; ++my)
        {
            for(int mx = 0; mx < map.width; ++mx)
            {
                if(map.full(mx, my))
                {
                    map.top[mx] = map.roof = my + 1;
                    ++map.count;
                }
            }
        }
    }

    void test(std::string const &log_file, pso_data &data, pso_config const &config)
    {
        int const round = 10000;
        double const up_round = 5;
        int const next_length = 0;
        double total = 0;
        for(int i = 0; i < round; ++i)
        {
            init(data, config);
            size_t piece = 0, up = 0;
            while(prepare(next_length) && run(piece, up, up_round))
            {
                ++piece;
            }
            total += piece;
        }
        data.score = total / round;
        ++data.count;
        if(std::isnan(data.best) || data.best < data.score)
        {
            data.best = data.score;
            for(size_t i = 0; i < config.config.size(); ++i)
            {
                data.data[i].p = data.data[i].x;
            }
        }
        std::stringstream ss;
        ss
            << "$ index : " << data.index
            << " piece : " << data.score
            << std::endl;
        std::cout << ss.str();
        for(size_t i = 0; i < config.config.size(); ++i)
        {
            ss << data.data[i].x << ", ";
        }
        ss << std::endl;
        std::ofstream ofs(log_file, std::ios::out | std::ios::binary | std::ios::app);
        ofs << ss.str();
    }
};


int main(int argc, char const *argv[])
{
    pso_vector data;
    std::mt19937 mt;
    pso_config config =
    {
        {}, 2, 2, 1,
    };
    auto v = [&config](double v, double r, double s)
    {
        pso_config_element d =
        {
            v - r, v + r, s
        };
        config.config.emplace_back(d);
    };
    v(0, 1, 1); v(1, 1, 0.1);
    v(0, 1, 1); v(1, 1, 0.1);
    v(0, 1, 1); v(1, 1, 0.1);
    v(0, 1, 1); v(96, 6, 5);
    v(0, 1, 1); v(160, 10, 5);
    v(0, 1, 1); v(128, 8, 5);
    v(0, 1, 1); v(60, 5, 5);
    v(0, 1, 1); v(380, 10, 5);
    v(0, 1, 1); v(100, 5, 5);
    v(0, 1, 1); v(40, 5, 5);
    v(0, 1, 1); v(50000, 10000, 100);
    v(32, 2, 1); v(0.25, 0.1, 0.05);

    std::mutex m;
    auto data_file = argc > 1 ? argv[1] : "./file.data";
    auto log_file = argc > 2 ? argv[2] : "./log.txt";
    if(!load(data_file, data))
    {
        pso_init(config, data, mt, 20);
    }
    std::vector<std::mutex *> status;
    while(status.size() < data.size())
    {
        status.emplace_back(new std::mutex());
    }
    std::chrono::high_resolution_clock::time_point next_save = std::chrono::high_resolution_clock::now();
    auto f = [&mt, &m, &data_file, &log_file, &data, &status, &config, &next_save]()
    {
        test_ai ai;
        for(; ; )
        {
            size_t index;
            {
                std::lock_guard<std::mutex> _l(m);
                index = std::uniform_int_distribution<size_t>(0, status.size() - 1)(mt);
                if(!status[index]->try_lock())
                {
                    continue;
                }
            }
            pso_data &current = data[index];
            ai.init(current, config);
            ai.test(log_file, current, config);
            {
                std::lock_guard<std::mutex> _l(m);
                pso_data const *best = nullptr;
                for(auto &d : data)
                {
                    if(best == nullptr || std::isnan(best->best) || d.best > best->best)
                    {
                        best = &d;
                    }
                }
                pso_logic(config, *best, current, mt);
                if(std::chrono::high_resolution_clock::now() > next_save)
                {
                    next_save += std::chrono::seconds(10);
                    save(data_file, data);
                    std::stringstream ss;
                    std::map<double, pso_data, std::greater<double>> check;
                    for(auto &d : data)
                    {
                        if(!std::isnan(d.best))
                        {
                            check.emplace(d.best, d);
                            if(check.size() > 10)
                            {
                                check.erase(std::prev(check.end()));
                            }
                        }
                    }
                    ss << "$ current : " << std::endl;
                    for(auto pair : check)
                    {
                        ss << "index : " << pair.second.index << " best : " << pair.second.best << " current : " << pair.second.score << std::endl;
                    }
                    std::cout << ss.str();
                }
            }
            status[index]->unlock();
        }
    };

    for(size_t i = 0; i < std::thread::hardware_concurrency(); ++i)
    {
        auto t = std::thread(f);
        t.detach();
    }
    f();
}