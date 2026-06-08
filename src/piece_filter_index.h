#ifndef TETRIS_AI_RUNNER_PIECE_FILTER_INDEX_H_
#define TETRIS_AI_RUNNER_PIECE_FILTER_INDEX_H_

//==========================================================================
// piece_filter_index: cell-footprint 等价类索引.
//
// 给定 (Spec, T): 把所有 (r, xb, yb) 三元组按 "piece_cells 平移到 (xb, yb) 后
// 落到棋盘上的 cell 集合" 分组, 同组分配同一 filtered_idx.
//
// 与 master `tetris_core.cpp` 的 IndexFilter 等价: 后者用 (node.data[0..3],
// node.row) 在 std::map 里作 memcmp key, 几何上等同于 piece 在 board 上 *实际
// 占据的 cells* 集合相同 + base row 相同; 我们在 (xb, yb) 维度直接按"绝对 cell
// 集合"分组, 不用 row 单独维度 (cell.y 已包含 yb), 等价.
//
// 与 commit 5 之前 `kCanonicalR + same_geometry` 的差异:
//   * `same_geometry` 只比较 piece 在 pivot 归一化下的 cells 数组, 与 (xb, yb)
//     无关 -> 把跨 (xb, yb) 的等价类全部塞进同一 canonical r.
//   * 本表按 (r, xb, yb) 三元组的 *绝对* footprint 分组, 完全不依赖 SRS-7
//     pivot 归一化巧合.
//
// 编译期 (constexpr) 一次 group-by:
//   1. 枚举 (r, xb, yb), 计算 sorted footprint 并打包成 FootprintKey
//      (= std::array<uint64_t, ceil(MaxCells*16/64)>); cell 升序填入低位起,
//      不足 MaxCells 的高位 16-bit slot 补 0xFFFF; cell 越界 -> array 全填
//      0xFFFFFFFFFFFFFFFF 作 invalid sentinel (filtered_idx = kFilteredIndexInvalid).
//   2. open-addressing hash table (linear probing) 做 group-by: hash(key) ->
//      probe; 命中复用 idx, 未命中分配新 idx + 写桶. BUCKETS = next_pow2(N*2)
//      保证 load factor <= 0.5, 平均探测 ~1, 把编译期 ops 控制在 default
//      `-fconstexpr-ops-limit=33M` 内 (commit 6 早先线性扫到 ~5G ops 才能编译).
//   3. 输出 idx[(r * W + xb) * H + yb] 与等价类总数 count.
//==========================================================================

#include "tetris_map.h"
#include "tetris_rule_spec.h"
#include "tetris_shape.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>

namespace m_tetris2
{
    namespace movegen
    {
        //invalid sentinel: 该 (r, xb, yb) 至少有一个 cell 落到棋盘外, 不参与
        //  等价类分配; 运行时 emit 路径不会触达 (BFS 已被 usable / landable
        //  限制在棋盘内), 留 sentinel 仅作 defensive fallback.
        inline constexpr std::uint16_t kFilteredIndexInvalid = 0xFFFFu;

        namespace filter_index_detail
        {
            //=== 编译期常量推导 ===
            //
            //piece_max_cells<Spec, T>: 在 R = 0..R_count-1 上取 piece_cells.count
            //  的最大值. SRS / Botris 4-cell 系恒为 4; pentomino 5; 通用 N-cell
            //  自动适配, 不再硬编码 4 上限.
            template<class Spec, char T, std::size_t... Rs>
            constexpr std::size_t piece_max_cells_impl(std::index_sequence<Rs...>)
            {
                std::size_t m = 0;
                ((m = (shape::piece_cells<Spec, T, static_cast<std::uint8_t>(Rs)>.count > m
                           ? shape::piece_cells<Spec, T, static_cast<std::uint8_t>(Rs)>.count
                           : m)),
                 ...);
                return m;
            }

            template<class Spec, char T>
            inline constexpr std::size_t piece_max_cells =
                piece_max_cells_impl<Spec, T>(
                    std::make_index_sequence<shape::rotation_count<Spec, T>>{});

            //FootprintKey: 每个 cell 占 16 bits (足够容纳 abs_y * W + abs_x; 即便
            //  W * H = 65535 也只刚到 16-bit 上界), 共 MaxCells 个 slot, 打包成
            //  uint64 array. 4-cell piece -> array<uint64, 1>; 5-cell pentomino
            //  -> array<uint64, 2>.
            template<class Spec, char T>
            inline constexpr std::size_t piece_max_cell_words =
                (piece_max_cells<Spec, T> * 16 + 63) / 64;

            template<class Spec, char T>
            using FootprintKey = std::array<std::uint64_t, piece_max_cell_words<Spec, T>>;

            //invalid key sentinel: 全 0xFFFFFFFFFFFFFFFF. 任意 valid 打包不撞,
            //  因为 valid 高位 16-bit slot 至多填 0xFFFF 单元, 单元间 cell 必然
            //  互不相等 (piece 自身不重叠), 总有一个 slot < 0xFFFF.
            template<class Spec, char T>
            constexpr FootprintKey<Spec, T> make_invalid_key()
            {
                FootprintKey<Spec, T> k{};
                for (std::size_t i = 0; i < k.size(); ++i)
                    k[i] = 0xFFFFFFFFFFFFFFFFull;
                return k;
            }

            //计算 (r, xb, yb) 打包 footprint key. 任意 cell 越界 -> invalid sentinel.
            //  cells 数组先按绝对索引 (yb + cy)*W + (xb + cx) 升序排序 (n ≤ MaxCells,
            //  朴素 bubble), 然后按 16-bit 切片填入 array<uint64, Words>: slot i 对应
            //  word (i * 16 / 64), 位偏移 (i * 16 % 64). 不足 MaxCells 个的尾部 slot
            //  保持初值 0xFFFF, 保证同 piece 不同 cell 数 (理论上 piece 跨 R 的 cell
            //  数恒等, 此处仅作接口稳健) 对齐 key 不冲突.
            template<class Spec, char T, std::uint8_t R>
            constexpr FootprintKey<Spec, T> compute_key(int xb, int yb)
            {
                int W = static_cast<int>(Spec::width);
                int H = static_cast<int>(Spec::height);
                auto const &cells = shape::piece_cells<Spec, T, R>;
                std::size_t n = cells.count;
                constexpr std::size_t MaxCells = piece_max_cells<Spec, T>;
                std::array<std::uint16_t, MaxCells> arr{};
                for (std::size_t i = 0; i < MaxCells; ++i)
                    arr[i] = 0xFFFFu;
                for (std::size_t i = 0; i < n; ++i)
                {
                    int abs_x = xb + static_cast<int>(cells.cells[i].x);
                    int abs_y = yb + static_cast<int>(cells.cells[i].y);
                    if (abs_x < 0 || abs_x >= W || abs_y < 0 || abs_y >= H)
                        return make_invalid_key<Spec, T>();
                    arr[i] = static_cast<std::uint16_t>(abs_y * W + abs_x);
                }
                //bubble sort 升序 (n <= MaxCells, 一般 4 / 5, O(n^2) 可接受).
                for (std::size_t i = 0; i < n; ++i)
                    for (std::size_t j = i + 1; j < n; ++j)
                        if (arr[j] < arr[i])
                        {
                            std::uint16_t tmp = arr[i];
                            arr[i] = arr[j];
                            arr[j] = tmp;
                        }
                FootprintKey<Spec, T> key{};
                for (std::size_t i = 0; i < MaxCells; ++i)
                {
                    std::size_t word = (i * 16) / 64;
                    std::size_t bit = (i * 16) % 64;
                    key[word] |= static_cast<std::uint64_t>(arr[i]) << bit;
                }
                return key;
            }

            //=== 编译期 hash 与 group-by ===
            //
            //next_pow2: 取 >= n 的最小 2 的幂 (constexpr, n 上界由 R_count*W*H 决定).
            constexpr std::size_t next_pow2(std::size_t n)
            {
                std::size_t p = 1;
                while (p < n)
                    p <<= 1;
                return p;
            }

            //splitmix64 风格折叠: 对 array<uint64, M> 逐 word 异或/混合, 简单稳定,
            //  不追求加密强度. 仅用作 open-addressing hash table 的 bucket 选择.
            template<class Spec, char T>
            constexpr std::uint64_t hash_key(FootprintKey<Spec, T> const &key)
            {
                std::uint64_t h = 0xCBF29CE484222325ull; //FNV offset basis 作初值, 任意非零常量即可
                for (std::size_t i = 0; i < key.size(); ++i)
                {
                    h ^= key[i];
                    h *= 0x9E3779B97F4A7C15ull;
                    h ^= h >> 32;
                }
                return h;
            }

            //compile-time 表数据. count = 等价类总数, idx[i] = 该三元组的
            //  filtered_idx (invalid -> kFilteredIndexInvalid).
            template<std::size_t N>
            struct FilterTable
            {
                std::array<std::uint16_t, N> idx;
                std::uint16_t count;
            };

            //CCW visit chain: 第 k 步沿 rotate_counterclockwise 链到达的 r.
            //  (cur 是运行时变量, 不能直接 target_ccw<Spec, T, cur>; 用模板
            //  递归把 r ∈ [0, R_count) 的 target_ccw 全部串起来分发. 旧实现走
            //  if constexpr (R_count >= K) 链, 只支持 R_count <= 4 — 把
            //  非 SRS rule (e.g. extreme_rule piece 'J' 10 旋转) 喂进 movegen
            //  会把 nx 永远卡在 cur, CCW 链退化, filter index 与 master oracle
            //  不再对齐. 这里改成通用递归.
            template<class Spec, char T, std::size_t R_count, std::size_t I>
            constexpr std::uint8_t target_ccw_at(std::uint8_t cur)
            {
                if constexpr (I >= R_count)
                {
                    return cur;
                }
                else
                {
                    if (cur == static_cast<std::uint8_t>(I))
                        return shape::target_ccw<Spec, T, static_cast<std::uint8_t>(I)>;
                    return target_ccw_at<Spec, T, R_count, I + 1>(cur);
                }
            }

            template<class Spec, char T, std::size_t R_count>
            constexpr std::array<std::uint8_t, R_count> build_ccw_visit()
            {
                std::array<std::uint8_t, R_count> visit_r{};
                std::uint8_t cur = 0;
                visit_r[0] = 0;
                for (std::size_t k = 1; k < R_count; ++k)
                {
                    std::uint8_t nx = target_ccw_at<Spec, T, R_count, 0>(cur);
                    if (nx == kOpRotateNone)
                        nx = cur; //链断: 退化, 后续填同 r (实际不会触发).
                    cur = nx;
                    visit_r[k] = cur;
                }
                return visit_r;
            }

            template<class Spec, char T>
            constexpr auto compute_table()
            {
                int W = static_cast<int>(Spec::width);
                int H = static_cast<int>(Spec::height);
                constexpr std::size_t R_count = shape::rotation_count<Spec, T>;
                constexpr std::size_t N = R_count * static_cast<std::size_t>(Spec::width) *
                                          static_cast<std::size_t>(Spec::height);

                //先把每个 (r, xb, yb) 的 key 算出来, 之后过 hash table 去重.
                //  R 维度用 fold expression 静态展开 (compute_key 需 R 是 template
                //  param); xb / yb 在每个 R 内走运行时 for 双循环.
                std::array<FootprintKey<Spec, T>, N> keys{};
                [&]<std::size_t... Rs>(std::index_sequence<Rs...>)
                {
                    ((
                         [&]
                         {
                             constexpr std::uint8_t R = static_cast<std::uint8_t>(Rs);
                             for (int xb = 0; xb < W; ++xb)
                                 for (int yb = 0; yb < H; ++yb)
                                 {
                                     std::size_t i = ((static_cast<std::size_t>(R) *
                                                       static_cast<std::size_t>(W)) +
                                                      static_cast<std::size_t>(xb)) *
                                                         static_cast<std::size_t>(H) +
                                                     static_cast<std::size_t>(yb);
                                     keys[i] = compute_key<Spec, T, R>(xb, yb);
                                 }
                         }()),
                     ...);
                }(std::make_index_sequence<R_count>{});

                //CCW visit order: master tetris_core.cpp 注册阶段从 r=0 出发沿
                //  rotate_counterclockwise 链扩展节点, IndexFilter "先到先得"
                //  让 CCW 链上更早的 r 拿走 canonical idx. SRS 4 旋转 piece 链
                //  = [0, 3, 2, 1]. 为对齐 master, 我们按 CCW 链遍历分配 idx,
                //  让同一几何等价类里 CCW 序更靠前的 (r, xb, yb) 拿到先到 idx.
                //
                //TODO(post-oracle): 这是与 master oracle baseline 对拍兼容的临时
                //  手段 (master IndexFilter 注册顺序 -> tetris_core.cpp:296-322
                //  的 BFS rotate_counterclockwise 出列序). 待 oracle baseline
                //  按新框架重做后, 此处应回归 CW 顺序, 与 BFS 主循环 (旋转扩展
                //  默认 cw / ccw / opp 同时下发, 但 emit 阶段的 "先到先得" 自然
                //  从 r=0 沿 CW 顺序遍历) 方向自洽; 切换时同步删除运行时
                //  tetris_movegen.h 内的 kCcwVisit 构造与使用点.
                constexpr auto visit_r = build_ccw_visit<Spec, T, R_count>();

                FilterTable<N> table{};
                for (std::size_t i = 0; i < N; ++i)
                    table.idx[i] = kFilteredIndexInvalid;

                //=== open-addressing hash table ===
                //
                //  容量 BUCKETS = next_pow2(N*2) 保证 load factor <= 0.5;
                //  线性探测 + occupied[] 标志, 查找/插入摊销 O(1).
                //  hash 选 splitmix64 风格折叠 (hash_key); 冲突时 (b+1)&mask 步进.
                //  桶里同时记 (key, idx); 命中条件 = occupied && key == query.
                //  array<uint64, M> 默认 operator== 即逐 word 相等, 满足字典序
                //  相等判定.
                constexpr std::size_t BUCKETS = next_pow2(N * 2);
                static_assert(BUCKETS >= N * 2, "BUCKETS overflow");
                constexpr std::size_t MASK = BUCKETS - 1;
                std::array<FootprintKey<Spec, T>, BUCKETS> bucket_key{};
                std::array<std::uint16_t, BUCKETS> bucket_idx{};
                std::array<bool, BUCKETS> occupied{};
                std::uint16_t next_idx = 0;

                //外层按 CCW 顺序遍历 r, 内层 (xb, yb) 走线性序; 与 master
                //  注册阶段 BFS 入队顺序对齐.
                FootprintKey<Spec, T> invalid = make_invalid_key<Spec, T>();
                for (std::size_t k = 0; k < R_count; ++k)
                {
                    std::uint8_t r = visit_r[k];
                    for (int xb = 0; xb < W; ++xb)
                        for (int yb = 0; yb < H; ++yb)
                        {
                            std::size_t i = ((static_cast<std::size_t>(r) *
                                              static_cast<std::size_t>(W)) +
                                             static_cast<std::size_t>(xb)) *
                                                static_cast<std::size_t>(H) +
                                            static_cast<std::size_t>(yb);
                            if (keys[i] == invalid)
                                continue;
                            std::uint64_t h = hash_key<Spec, T>(keys[i]);
                            std::size_t b = static_cast<std::size_t>(h) & MASK;
                            //probe 上限 = BUCKETS, load factor <= 0.5 保证一定命中或
                            //  找到空桶; 越界即 hash table 实现 bug, 用 break 防死循环.
                            std::size_t probe = 0;
                            for (; probe < BUCKETS; ++probe)
                            {
                                if (!occupied[b])
                                {
                                    occupied[b] = true;
                                    bucket_key[b] = keys[i];
                                    bucket_idx[b] = next_idx;
                                    table.idx[i] = next_idx;
                                    ++next_idx;
                                    break;
                                }
                                if (bucket_key[b] == keys[i])
                                {
                                    table.idx[i] = bucket_idx[b];
                                    break;
                                }
                                b = (b + 1) & MASK;
                            }
                            //probe == BUCKETS 时表已满, 不应发生 (load factor <= 0.5);
                            //  保留这条 fallback 让 idx 留空, 运行时被 invalid 守
                            //  剔除, 避免 silent miscompare.
                        }
                }
                table.count = next_idx;
                return table;
            }
        }

        //=== 公开接口 ===
        //  kFilteredIndex<Spec, T> -> std::array<uint16_t, R_count*W*H>:
        //    线性下标 l = (r * W + xb) * H + yb -> filtered_idx.
        //  kFilteredCount<Spec, T> -> std::uint16_t: 等价类总数, BFS 期间用作
        //    bitset 的容量上界.
        template<class Spec, char T>
        inline constexpr auto kFilteredTable = filter_index_detail::compute_table<Spec, T>();

        template<class Spec, char T>
        inline constexpr auto const &kFilteredIndex = kFilteredTable<Spec, T>.idx;

        template<class Spec, char T>
        inline constexpr std::uint16_t kFilteredCount = kFilteredTable<Spec, T>.count;
    }
}

#endif // TETRIS_AI_RUNNER_PIECE_FILTER_INDEX_H_
