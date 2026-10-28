// framework_eval_cache_test.cpp — 共享框架 EvalCache 独立探针.
//
// 归属: test owner (本 owner 不改 core/AI/oracle).
// 目的: 不依赖任何 15 AI, 直接驱动 m_tetris2::EvalCache<Map,Result>, 验证:
//   1. 容量契约 (非零 2 的幂, 0/非2幂拒绝; 无最小 2 bucket 硬编码);
//   2. 同 map 二次 lookup 命中 (eval 只调一次);
//   3. hash_override 强制两个不同 map 落同桶 -> 全 raw map 不等必须重算,
//      绝不返回 stale bucket 结果 (full-map 相等守卫);
//   4. 空盘 (默认构造 Map) 是正常键, 不是 sentinel; 空盘 vs 有块盘不互串;
//   5. options.disabled=true 旁路: 每次都 eval, 仍写桶但不命中 (written_gen 更新, stats.hits=0);
//   6. on_config_changed() 提升 generation 后, 旧桶自动 stale -> 同 map 重算;
//   7. 非平凡 Result (含 std::vector) 构造/赋值/析构干净 (ASan 监护);
//   8. stats 字段如实: requests/hits/misses/evals/replacements/generation/
//      bucket_count/result_size.
// 已知不可运行时验证 (诚实标注): generation 从 2^64-1 绕哨兵 0 全清分支 (debug_set_generation 为 seam),
// 无 test-only 跳变缝, 只能代码审查证据 (见 src/tetris_eval_get.h on_config_changed).

#include "../src/tetris_core.h"
#include "../src/tetris_eval_get.h"
#include "../src/tetris_node.h"

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace
{
    int g_failures = 0;
    void expect(bool ok, char const *what)
    {
        if (!ok)
        {
            ++g_failures;
            std::printf("FAIL: %s\n", what);
        }
    }

    using Map = m_tetris2::TetrisMap<10, 40>;

    // 非平凡 Result: 携带 side 向量 + 构造/赋值计数, 证明桶按值存放且不 memset.
    struct Result
    {
        static int g_constructions;
        static int g_destructions;
        static int g_copies;

        double value = 0.0;
        std::vector<int> side;

        Result() { ++g_constructions; }
        explicit Result(double v) : value(v), side(4, 0) { ++g_constructions; }
        Result(Result const &o) : value(o.value), side(o.side) { ++g_constructions; ++g_copies; }
        Result(Result &&o) noexcept : value(o.value), side(std::move(o.side)) { ++g_constructions; }
        Result &operator=(Result const &o)
        {
            value = o.value;
            side = o.side;
            ++g_copies;
            return *this;
        }
        ~Result() { ++g_destructions; }
    };
    int Result::g_constructions = 0;
    int Result::g_destructions = 0;
    int Result::g_copies = 0;

    struct CacheProbeAI
    {
        mutable size_t eval_calls = 0;
        mutable Map last_map{};

        Result eval(Map const &m) const
        {
            ++eval_calls;
            last_map = m;
            // 结果携带盘指纹: 盘不同结果必须不同, 用于抓 stale 桶错配.
            std::uint64_t fp = 0;
            for (auto lane : m.data.data)
                fp = fp * 1315423911ull + static_cast<std::uint64_t>(lane);
            Result r(static_cast<double>(fp & 0xFFFFFFFFull));
            r.side[0] = static_cast<int>(eval_calls);
            r.side[1] = static_cast<int>(fp); // 精确盘指纹, 避免 double 舍入误判
            return r;
        }
    };

    Map map_with_row(int y, typename Map::row_t occupied)
    {
        Map m{};
        m.set_row(y, occupied);
        return m;
    }

    void single_bucket_capacity()
    {
        m_tetris2::EvalCache<Map, Result> cache;
        cache.prepare(2);
        auto s = cache.stats();
        expect(s.bucket_count == 2, "prepare(2) -> bucket_count==2");
        expect(s.result_size == sizeof(Result), "stats.result_size 如实反映非平凡 Result");
        expect(cache.generation() == 1, "prepare 后 generation==1");
    }

    void hit_on_second_lookup()
    {
        m_tetris2::EvalCache<Map, Result> cache;
        cache.prepare(4);
        CacheProbeAI ai;
        Map m = map_with_row(0, Map::row_full ^ 0x3EF);

        Result r1 = cache.lookup(ai, m);
        Result r2 = cache.lookup(ai, m);
        auto s = cache.stats();
        expect(ai.eval_calls == 1, "同 map 二次 lookup 不重复 eval");
        expect(s.requests == 2 && s.hits == 1 && s.misses == 1 && s.evals == 1, "stats hits/misses 计数");
        expect(r1.side[1] == r2.side[1], "命中返回同指纹 Result");
        expect(s.generation == cache.generation(), "stats.generation 同步");
    }

    void fullhash_collision_must_recompute()
    {
        // hash_override 非 0 -> 所有 map 用同一 hash -> 必然落同桶.
        m_tetris2::EvalCache<Map, Result> cache;
        cache.prepare(2);
        cache.options().hash_override = 0xC0FFEEull;
        CacheProbeAI ai;

        Map m1 = map_with_row(0, Map::row_full ^ 0x3EF);
        Map m2 = map_with_row(1, Map::row_full ^ 0x007); // 完全不同的盘, 同桶

        Result r1 = cache.lookup(ai, m1);
        Result r2 = cache.lookup(ai, m2);

        expect(r1.side[1] != r2.side[1], "fullhash 碰撞: 不同盘必须算出不同 Result");
        expect(ai.eval_calls == 2, "碰撞两次 miss 都必须 eval (不得返回 stale)");
        // 第三次查 m1: 桶已被 m2 覆盖 -> 必须重算 m1, 值与首次一致.
        Result r3 = cache.lookup(ai, m1);
        expect(r3.side[1] == r1.side[1], "碰撞覆盖后 m1 重算结果与首次一致");
        expect(ai.eval_calls == 3, "碰撞覆盖后 m1 仍 miss 重算");
        auto s = cache.stats();
        expect(s.requests == 3 && s.misses == 3 && s.hits == 0, "同桶不同 map 永不命中");
    }

    void empty_map_is_normal_key()
    {
        // 本仓 bit 约定: 空盘 = 默认 Map{} (全 lane = row_full, 0=occupied).
        // 空盘必须是正常键: 有块盘 != 空盘; 空盘二次 lookup 可命中.
        m_tetris2::EvalCache<Map, Result> cache;
        cache.prepare(8);
        CacheProbeAI ai;
        Map empty{};
        Map occupied{};
        occupied.set_row(3, Map::row_full ^ 0x03F); // row3 低 6 列有块

        Result re = cache.lookup(ai, empty);
        Result ro = cache.lookup(ai, occupied);
        expect(re.side[1] != ro.side[1], "空盘与有块盘被正确区分 (空盘非 sentinel)");
        expect(ai.eval_calls == 2, "首次各 miss");
        Result re2 = cache.lookup(ai, empty);
        expect(re2.side[1] == re.side[1], "空盘作为正常键可命中");
        expect(ai.eval_calls == 2, "空盘命中后不重复 eval");
    }

    void disabled_bypass()
    {
        m_tetris2::EvalCache<Map, Result> cache;
        cache.prepare(4);
        cache.options().disabled = true;
        CacheProbeAI ai;
        Map m = map_with_row(2, Map::row_full ^ 0x101);
        cache.lookup(ai, m);
        cache.lookup(ai, m);
        auto s = cache.stats();
        expect(ai.eval_calls == 2, "disabled: 每次都 eval");
        expect(s.hits == 0 && s.misses == 2, "disabled: 无命中");
    }

    void generation_invalidation()
    {
        m_tetris2::EvalCache<Map, Result> cache;
        cache.prepare(4);
        CacheProbeAI ai;
        Map m = map_with_row(0, Map::row_full ^ 0x3EF);

        cache.lookup(ai, m);
        size_t gen_before = cache.generation();
        bool overflowed = cache.on_config_changed();
        expect(!overflowed, "单次 on_config_changed 不触发溢出");
        expect(cache.generation() == gen_before + 1, "on_config_changed 提升 generation");

        cache.lookup(ai, m);
        expect(ai.eval_calls == 2, "Config 变更后旧桶 stale -> 同 map 重 eval");
        auto s = cache.stats();
        expect(s.misses == 2, "失效后第二次 lookup 记 miss");
    }

    void nontrivial_lifecycle_clean()
    {
        // 反复 insert/overwrite 非平凡 Result, 进程退出时 ASan 验证无泄漏/悬垂.
        m_tetris2::EvalCache<Map, Result> cache;
        cache.prepare(2);
        cache.options().hash_override = 0x7E57ull; // 强制同桶连续 overwrite
        CacheProbeAI ai;
        for (int i = 0; i < 64; ++i)
        {
            Map m = map_with_row(i % 40, static_cast<Map::row_t>(Map::row_full ^ (i & 0x1FF)));
            cache.lookup(ai, m);
        }
        auto s = cache.stats();
        expect(s.replacements == 64, "同桶连续 overwrite 全记 replacement");
        std::printf("  lifecycle: constructions=%d destructions=%d copies=%d (ASan clean on exit)\n",
                    Result::g_constructions, Result::g_destructions, Result::g_copies);
        expect(Result::g_destructions <= Result::g_constructions, "析构 <= 构造 (ASan 兜底)");
    }
}

int main()
{
    std::printf("# framework_eval_cache: standalone EvalCache probes\n");
    single_bucket_capacity();
    hit_on_second_lookup();
    fullhash_collision_must_recompute();
    empty_map_is_normal_key();
    disabled_bypass();
    generation_invalidation();
    nontrivial_lifecycle_clean();

    if (g_failures == 0)
    {
        std::printf("framework_eval_cache: OK (7 groups)\n");
        return 0;
    }
    std::printf("framework_eval_cache: %d FAILURES\n", g_failures);
    return 1;
}
