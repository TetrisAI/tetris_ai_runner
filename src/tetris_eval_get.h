#pragma once

//==========================================================================
// tetris_eval_get.h — Config-aware eval(map) cache + GetArgs type-list get.
//
//   1. type_list + args::* 参数标签：AI 声明 GetArgs，框架按类型表按需萃取。
//   2. GetLanding<SpinT>：get 侧落块几何视图（运行期 POD，child 创建时预计算）。
//   3. EvalCache<MapT, Result>：每 Context 一张固定容量直接映射缓存，
//      hash 定位 + 全 raw map 相等，miss 当场 eval 覆盖；generation 作失效纪元。
//==========================================================================

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <stdexcept>
#include <cstdint>
#include <type_traits>
#include <utility>
#include <vector>
#include <variant> // std::monostate

namespace m_tetris2
{
    constexpr std::uint64_t hash_combine(std::uint64_t seed, std::uint64_t value) noexcept;

    // type_list<P...>：AI::GetArgs 的唯一事实来源。
    template<class... Ts>
    struct type_list
    {
    };

    namespace args
    {
        template<class E> struct Eval {};       // → E const&（节点结果，必要时 lookup 刷新）
        template<class S> struct Parent {};     // → S const&（parent->status.get_raw()）
        struct Depth {};                        // → size_t（parent->level，根子=0）
        struct Clear {};                         // → int（attach 消行数）
        struct MapBefore {};              // → map_t const&（parent->map；引擎定 map_t，get 可 template<MapT>）
        struct MapAfter {};               // → map_t const&（node->map）
        template<class L> struct Landing {};     // → L const&（GetLanding<SpinT>）
        struct NextView {};                      // → char const*（展开队列；唯一需要 next_c 视图）
        struct NextLength {};                    // → size_t（队列剩余长度，随机访问 O(1)）
        struct NextNode {};                      // → char（当前块 type，= lp_type(identity)，O(1)）
        struct Hold {};                          // → char
        struct IsHold {};                        // → bool
        struct Env {};                           // → AIEnv（完整环境，按需）
    } // namespace args

    // GetLanding<SpinT>：运行期落块几何 POD。
    // h = shape::piece_h 原 NxN box 高；land_height = row + h；y_master_plus1 = node.y + 1。
    template<class SpinT = std::monostate>
    struct GetLanding
    {
        static constexpr int kMaxLines = 8; // 安全上限；填充处 static_assert piece_h<=kMaxLines
        char piece = 0;
        std::uint8_t rotation = 0;
        std::int8_t xb = 0;
        std::int8_t yb = 0;
        SpinT spin = {};
        std::int8_t w = 0;
        std::int8_t h = 0;
        int y_master_plus1 = 0;
        std::uint64_t lines[kMaxLines] = {};

        int row() const { return static_cast<int>(yb); }
        int col() const { return static_cast<int>(xb); }
        int land_height() const { return row() + static_cast<int>(h); }
        int y_plus1() const { return y_master_plus1; }
        int width() const { return static_cast<int>(w); }
        int piece_height() const { return static_cast<int>(h); }
        std::uint64_t line(int i) const { return (i >= 0 && i < kMaxLines) ? lines[i] : std::uint64_t(0); }
    };

    // EvalCache<MapT, Result>：固定容量、直接映射。
    //   - prepare 锁定 capacity = buckets_.size()（真实分配），lookup 一律按真实 size 索引，
    //     options 运行期改 bucket_count 不影响索引（preprepare 仅在 prepare 前读取）。
    //   - 允许单桶（capacity=1，mask=0 恒落桶 0）；命中仍需 written_gen==纪元 && hash && 全 raw map 相等。
    //   - Config 变更：++generation；溢出到 0 时清 written_gen、返回 overflowed，由引擎 era++ 全树失效。
    template<class MapT, class Result>
    class EvalCache
    {
    public:
        struct Stats
        {
            std::uint64_t requests = 0;
            std::uint64_t hits = 0;
            std::uint64_t misses = 0;
            std::uint64_t evals = 0;
            std::uint64_t replacements = 0;
            std::uint64_t generation = 0;
            std::size_t bucket_count = 0;
            std::size_t result_size = 0;
            std::size_t entry_bytes = 0;
            std::size_t total_bytes = 0;
        };
        struct Options
        {
            std::size_t bucket_count = std::size_t(1) << 16; // 默认 65536（preprepare 读取）
            bool disabled = false;                            // 测试：旁路 cache
            std::uint64_t hash_salt = 0;                    // 测试：异或扰动
            std::uint64_t hash_override = 0;                 // 测试：非 0 直接作桶 hash，强制同桶
        };

        // 前置契约：bucket_count 必须是非零 2 的幂（n==0 或 !(n&(n-1)) 为 2 幂）。
        // 非法输入一律拒绝（不静默归一）；容量合法但 bytes 预算越界（n*sizeof(Entry)
        // 超过 vector 可分配 max_size）也拒绝，不尝试天量分配。失败时清表、保持未准备态。
        bool prepare(std::size_t bucket_count)
        {
            prepared_ = false;
            buckets_.clear();
            if (bucket_count == 0 || (bucket_count & (bucket_count - 1)) != 0)
                return false;                                  // 非法容量：非零 2 幂契约
            if (bucket_count > max_buckets())
                return false;                                  // bytes 预算 / max_size 越界
            try
            {
                buckets_.assign(bucket_count, Entry{});
            }
            catch (std::bad_alloc const&)
            {
                buckets_.clear();                              // 分配失败
                return false;
            }
            catch (std::length_error const&)
            {
                buckets_.clear();                              // 容量越界
                return false;
            }
            options_.bucket_count = bucket_count;              // 容量 prepare 后固定
            generation_ = 1;
            stats_ = Stats{};
            prepared_ = true;
            return true;
        }
        static std::size_t max_buckets()
        {
            // 元素数上界：min(vector 可分配元素数, SIZE_MAX/sizeof(Entry))，显式 bytes 预算。
            std::size_t vec = std::vector<Entry>{}.max_size();
            std::size_t by_bytes = std::size_t(-1) / sizeof(Entry);
            return vec < by_bytes ? vec : by_bytes;
        }
        bool prepared() const { return prepared_; }
        Options& options() { return options_; }
        Options const& options() const { return options_; }
        std::uint64_t generation() const { return generation_; }
        // test-only seam：直接设定 generation 以验证溢出全失效（生产路径不调用）。
        void debug_set_generation(std::uint64_t g) { generation_ = g; }
        // 返回 true 表示本次发生溢出（引擎应 era++ 全树失效）。
        bool on_config_changed()
        {
            ++generation_;
            if (generation_ == 0)
            {
                for (auto& e : buckets_)
                    e.written_gen = 0;
                generation_ = 1;
                return true;
            }
            return false;
        }

        // true = out 已填有效结果（命中 或 现场 eval 的 miss）；false = 未准备/非法态（out 不赋值）。
        // 不抛异常、不静默给零分；命中/miss 分别计入 stats。
        template<class AI>
        bool try_lookup(AI const& ai, MapT const& map, Result& out)
        {
            if (!prepared_)
                return false;
            ++stats_.requests;
            std::uint64_t h = options_.hash_override != 0
                                  ? options_.hash_override
                                  : (hash_map(map) ^ options_.hash_salt);
            const std::size_t cap = buckets_.size();
            const std::size_t idx = static_cast<std::size_t>(h & (cap - 1));
            Entry& b = buckets_[idx];
            if (!options_.disabled && b.written_gen == generation_ && b.hash == h && b.map_key == map)
            {
                ++stats_.hits;
                out = b.result;
                return true;
            }
            ++stats_.misses;
            out = ai.eval(map);
            ++stats_.evals;
            b.written_gen = generation_;
            b.hash = h;
            b.map_key = map;
            b.result = out;
            ++stats_.replacements;
            return true;
        }
        // 正常已准备接口值语义不变；未 prepare 调用是清晰 fatal guard（运行期生效，
        // 不被 NDEBUG 静默成零分）。
        template<class AI>
        Result lookup(AI const& ai, MapT const& map)
        {
            if (!prepared_)
                fatal_unprepared_lookup();
            Result r{};
            (void)try_lookup(ai, map, r);
            return r;
        }

        Stats stats() const
        {
            Stats s = stats_;
            s.generation = generation_;
            s.bucket_count = buckets_.size();
            s.result_size = sizeof(Result);
            s.entry_bytes = sizeof(Entry);
            s.total_bytes = buckets_.size() * sizeof(Entry);
            return s;
        }

    private:
        struct Entry
        {
            std::uint64_t written_gen = 0;
            std::uint64_t hash = 0;
            MapT map_key{};
            Result result{};
        };
        [[noreturn]] static void fatal_unprepared_lookup()
        {
            std::fputs("tetris eval cache: lookup before prepare()\n", stderr);
            std::abort();
        }
        static std::uint64_t hash_map(MapT const& map)
        {
            std::uint64_t h = std::uint64_t(0x9e3779b97f4a7c15ull);
            for (auto lane : map.data.data)
                h = hash_combine(h, static_cast<std::uint64_t>(lane));
            return h;
        }
        std::vector<Entry> buckets_;
        Options options_;
        std::uint64_t generation_ = 1;
        mutable Stats stats_;
        bool prepared_ = false;
    };

    // LandingSpinOf<AI>：AI::EvalSpinType，缺省 std::monostate。
    template<class AI, class = void>
    struct LandingSpinOf
    {
        using type = std::monostate;
    };
    template<class AI>
    struct LandingSpinOf<AI, std::void_t<typename AI::EvalSpinType>>
    {
        using type = typename AI::EvalSpinType;
    };

    namespace detail
    {
        template<class T> struct is_eval_tag : std::false_type {};
        template<class E> struct is_eval_tag<args::Eval<E>> : std::true_type {};
        template<class T> struct is_parent_tag : std::false_type {};
        template<class S> struct is_parent_tag<args::Parent<S>> : std::true_type {};
        template<class T> struct is_landing_tag : std::false_type {};
        template<class L> struct is_landing_tag<args::Landing<L>> : std::true_type {};

        template<class P> constexpr bool tag_is_clear_v = std::is_same_v<P, args::Clear>;
        template<class P> constexpr bool tag_is_depth_v = std::is_same_v<P, args::Depth>;
        template<class P> constexpr bool tag_is_map_before_v = std::is_same_v<P, args::MapBefore>;
        template<class P> constexpr bool tag_is_map_after_v = std::is_same_v<P, args::MapAfter>;
        template<class P> constexpr bool tag_is_env_v = std::is_same_v<P, args::Env>;
        template<class P> constexpr bool tag_is_next_view_v = std::is_same_v<P, args::NextView>;
        template<class P> constexpr bool tag_is_next_length_v = std::is_same_v<P, args::NextLength>;
        template<class P> constexpr bool tag_is_next_node_v = std::is_same_v<P, args::NextNode>;
        template<class P> constexpr bool tag_is_hold_v = std::is_same_v<P, args::Hold>;
        template<class P> constexpr bool tag_is_is_hold_v = std::is_same_v<P, args::IsHold>;

        struct GetTraitsBase
        {
            static constexpr bool kWantEval = false;
            static constexpr bool kWantClear = false;
            static constexpr bool kWantLanding = false;
            static constexpr bool kWantDepth = false;
            static constexpr bool kWantParent = false;
            static constexpr bool kWantMapBefore = false;
            static constexpr bool kWantMapAfter = false;
            static constexpr bool kWantEnv = false;
            // 只有 NextView / Env 才需要展开 next_c 队列（C2 的 NextLength/NextNode 为 O(1)，不构队列）。
            static constexpr bool kWantNextView = false;
            static constexpr bool kWantNextLength = false;
            static constexpr bool kWantNextNode = false;
            static constexpr bool kWantHold = false;
            static constexpr bool kWantIsHold = false;
        };
        template<class List> struct GetTraitsFold;
        template<> struct GetTraitsFold<type_list<>> : GetTraitsBase {};
        template<class P, class... Rest>
        struct GetTraitsFold<type_list<P, Rest...>> : GetTraitsBase
        {
            using R = GetTraitsFold<type_list<Rest...>>;
            static constexpr bool kWantEval = is_eval_tag<P>::value || R::kWantEval;
            static constexpr bool kWantClear = tag_is_clear_v<P> || R::kWantClear;
            static constexpr bool kWantLanding = is_landing_tag<P>::value || R::kWantLanding;
            static constexpr bool kWantDepth = tag_is_depth_v<P> || R::kWantDepth;
            static constexpr bool kWantParent = is_parent_tag<P>::value || R::kWantParent;
            static constexpr bool kWantMapBefore = tag_is_map_before_v<P> || R::kWantMapBefore;
            static constexpr bool kWantMapAfter = tag_is_map_after_v<P> || R::kWantMapAfter;
            static constexpr bool kWantEnv = tag_is_env_v<P> || R::kWantEnv;
            static constexpr bool kWantNextView = tag_is_next_view_v<P> || R::kWantNextView;
            static constexpr bool kWantNextLength = tag_is_next_length_v<P> || R::kWantNextLength;
            static constexpr bool kWantNextNode = tag_is_next_node_v<P> || R::kWantNextNode;
            static constexpr bool kWantHold = tag_is_hold_v<P> || R::kWantHold;
            static constexpr bool kWantIsHold = tag_is_is_hold_v<P> || R::kWantIsHold;
        };
    } // namespace detail

    template<class AI, class = void>
    struct GetTraits : detail::GetTraitsBase
    {
    };
    template<class AI>
    struct GetTraits<AI, std::void_t<typename AI::GetArgs>> : detail::GetTraitsFold<typename AI::GetArgs>
    {
    };

    // ProviderCounters：gated 每标签计数（默认关，引擎 accessor 供测试读取）。
    struct ProviderCounters
    {
        bool enabled = false;
        std::uint64_t eval = 0, parent = 0, depth = 0, clear = 0, map_before = 0, map_after = 0,
                      landing = 0, next_view = 0, next_length = 0, next_node = 0, hold = 0, is_hold = 0, env = 0;
        std::uint64_t virtual_get = 0, real_get = 0;   // get<false> vs get<true> 调用计数
        void reset()
        {
            eval = parent = depth = clear = map_before = map_after = landing = next_view = next_length = next_node = hold = is_hold = env = 0;
            virtual_get = real_get = 0;
        }
    };

    //----------------------------------------------------------------------
    // unique_types<List>：去重 type_list（保留首次出现顺序）。
    // get 侧：每个唯一 provider 只计算一次 + 计数一次，重复标签在原声明位置复用同一值。
    //----------------------------------------------------------------------
    namespace detail
    {
        template<class List, class... Seen> struct unique_impl;
        template<class... Seen> struct unique_impl<type_list<>, Seen...>
        {
            using type = type_list<Seen...>;
        };
        template<class H, class... Tail, class... Seen>
        struct unique_impl<type_list<H, Tail...>, Seen...>
            : std::conditional_t<(std::is_same_v<H, Seen> || ...),
                                 unique_impl<type_list<Tail...>, Seen...>,
                                 unique_impl<type_list<Tail...>, Seen..., H>>
        {
        };
        template<class T, class List> struct index_of;
        template<class T, class... Rest>
        struct index_of<T, type_list<T, Rest...>>
        {
            static constexpr std::size_t value = 0;
        };
        template<class T, class H, class... Rest>
        struct index_of<T, type_list<H, Rest...>>
        {
            static constexpr std::size_t value = 1 + index_of<T, type_list<Rest...>>::value;
        };
    } // namespace detail

    template<class List> using unique_types_t = typename detail::unique_impl<List>::type;
    template<class T, class List>
    inline constexpr std::size_t index_of_v = detail::index_of<T, List>::value;

    // EvalSlot<Result>：节点条件 eval 存储（仅 kWantEval 时占用）。
    // eval_era 与 Context::eval_era 配对，缓存溢出 era++ 时全树失效。
    template<class R>
    struct EvalSlot
    {
        R result{};
        std::uint64_t eval_gen = 0;
        std::uint64_t eval_era = 0;
    };
} // namespace m_tetris2
