# Tag PathMark 分档与 t_path_mark_ 清理 — 任务计划 (final)

## 决策定性
1. **砍掉 `t_path_mark_` 是定性目标** — 不是冗余的"备用桶", 是设计错误.
2. **PathMark 不应是单一实现**, 而应按"实际去重需求"分档, 编译期选档.
3. 当前任务范围: **只在 tag 内部** 把 tag 需要的 PathMark 各档实现到位,
   tag 的所有 dedup 都使用"刚好够用"的档位, 不付额外代价.
4. **下一个任务** (本计划之外): 检查 path / simulate / simple 三个 search,
   把它们各自的 dedup 切到对应档位, 完成"PathMark 多档化"全局推广.

## 最终结论 — 双档方案 (而非三档)

| dedup 名 | 档位 | 写入 API | 读取 API | piece 分桶 |
|---------|------|---------|---------|-----------|
| TagSearch1gDedup (search 1g BFS, 非 T) | **PathMarkBit** (L1) | `mark_bbox` | (BFS 不读) | **不需要** |
| TagSearch20gDedup (search 20g BFS, 非 T) | **PathMarkBit** (L1) | `mark_bbox` | (BFS 不读) | **不需要** |
| TagSearchTDedup (search_t_native, T BFS) | **PathMark** (L3 + entry-stale) | `set_bbox` / `cover_if_bbox` (常规) + `mark_bbox` (entry 仅 bump) | `get_bbox` | **不需要** (见下) |
| TagMakePath1gDedup / TagMakePath20gDedup | **PathMark** (L3) | `set_bbox` | `get_bbox` (build_path 反向链) | **不需要** |

最终落地三个 mark 字段:
- `search_mark_` (PathMarkBit) — search 阶段 1g/20g BFS.
- `t_mark_` (PathMark) — search_t_native.
- `make_path_mark_` (PathMark) — make_path.

## 关键修正: t_mark_ 不需要 piece 分桶

最初设想的 `PathMarkPathT` 内部按 kPieceCount 分桶, 是基于"oracle stale 语义
要求跨 piece 隔离, 跨 search() 同 piece 累积"的对应. 但实测物理结构后发现:

- `t_mark_` 仅在 `search_t_native` 中被读写;
- `search_t_native` 仅对 `SpinHook::active_for_piece<T> == true` 的 piece 实例化
  (RuleSpec hook), 在标准规则下只对 piece 'T' 实例化;
- 因此 `t_mark_` **物理上就只被 piece T 的 search() 调用写入和读取**,
  不存在跨 piece 写入冲突;
- "跨调用同 piece 残留可见" 由 `mark_bbox` 不写 `cell_prev_/cell_op_` 的
  stale 语义直接保证 (oracle TetrisNodeMark::mark 的等价行为).

结论: 没必要付出 kPieceCount 倍内存 (~180KB) 来做"逻辑 piece 分桶".
直接用 `bb::PathMark` (单桶) + 物理字段隔离即可, 与 oracle 行为完全一致.

`PathMarkPathT` 类型从 `bb_state.h` 中移除.

## 关键修正: PathMarkBit 用一维 bitset + memset

最初 `PathMarkBit` 沿用 `version_` + `cell_ver_[kR][kCells]` 设计 (与
`PathMark` 同款), clear 操作只 `++version_`. 性能上 clear 是 O(1), 但:

- 单个 cell 占 8B (version_ 是 uint64), 内存 = kR×kCells×8 ≈ kR×400×8B,
  四旋转下 ~12.5KB, 大量 cache line 浪费;
- `mark_bbox` 一次访问需要先读 `cell_ver_[r][i]` (8B load) 再比较, cache miss
  概率高;
- 不需要 prev/op, 完全可以压缩到每 cell 1 bit.

最终方案:
```cpp
struct PathMarkBit {
    static constexpr int kCells = kW * kH;
    static constexpr int kR = kMaxR;
    static constexpr int kBits = kR * kCells;
    static constexpr int kWords = (kBits + 63) / 64;
    std::uint64_t bits_[kWords] = {};

    void clear() { std::memset(bits_, 0, sizeof(bits_)); }

    bool mark_bbox(int r, int x, int y) {
        if (r < 0 || r >= kR || x < 0 || x >= kW || y < 0 || y >= kH)
            return false;
        int idx = r * kCells + y * kW + x;
        std::uint64_t mask = std::uint64_t{1} << (idx & 63);
        std::uint64_t &w = bits_[idx >> 6];
        if (w & mask) return false;
        w |= mask;
        return true;
    }
};
```

收益:
- 内存 ~12.5KB → ~200B (64x 节省), 完整 fit 进 L1 cache;
- clear 是 `memset(~200B)`, 仍是 O(1) 级别 (实测一两条 cache line write);
- mark/check fast path 是单次 64-bit OR + 比较, 流水线友好.

`++version_` 优化只在 "数据结构本身不能 memset 清零" 时才有意义 (PathMark
里的 `cell_prev_/cell_op_` 不需要清零, 用 `cell_ver_` 做"墓碑"); 对纯 bitset
就是过度设计.

## tag 实施步骤 (已完成)

### Step 1: bb 命名空间双档类型 ✅
- `bb::Helpers<RuleSpec>::PathMark` (L3, 含 `mark_bbox` entry-stale 模式).
- `bb::Helpers<RuleSpec>::PathMarkBit` (L1, 一维 bitset + memset).

### Step 2: tag ExtrasMixin 字段 ✅
```cpp
typename bb::Helpers<RuleSpec>::PathMarkBit search_mark_{};
typename bb::Helpers<RuleSpec>::PathMark    t_mark_{};
typename bb::Helpers<RuleSpec>::PathMark    make_path_mark_{};
```
PathMarkMixin 从 tag::Context 移除.

### Step 3: tag 五个 dedup 类切档 ✅
- TagSearchDedup / TagSearch20gDedup → `PathMarkBit *path_mark`.
- TagSearchTDedup → `PathMark *path_mark` (常规 set/cover_if + entry mark).
- TagMakePathDedup → `PathMark *path_mark`.

### Step 4: search_t_native 入口 ✅
```cpp
ctx.t_mark_.clear();   // 单 piece 物理隔离, 直接 clear 即可.
```

### Step 5: 编译 + 对拍
**待办**: build/ 跑 tag_node_diff. 预期 byte-equal.

### Step 6: clang-format + 单 commit.
Commit 标题 (英): `refactor(movegen): split tag PathMark into per-stage variants`
描述 (中文): 砍掉冗余的 t_path_mark_; 引入 PathMarkBit (一维 bitset, memset
clear) 与 PathMark (L3 + entry-stale) 双档实现, tag 各 dedup 选用刚好够用的
档位; t_mark_ 物理上仅 search_t_native 单 piece 写入, 直接复用 PathMark
即可保持 oracle stale 语义, 不需要 piece 分桶.

## 风险点
- PathMarkBit memset 在每次 BFS 入口调用; 200B memset 性能可忽略.
- t_mark_ 单桶依赖 "search_t_native 仅对 active_for_piece<T> piece 实例化"
  的 RuleSpec 不变量. 若未来 RuleSpec 改成多 piece 走 search_t_native, 必须
  恢复 piece 分桶或拆字段. (留 TODO 注释在 search_t_native 入口.)

## 下一任务 (本计划外, 占位)
- 检查 path 的 search/make_path 是否有过度配置: 当前 Path Search Run20gDedup
  实际写 PrevKey + op, 但 visitor 只读 prev (build_path 反向). 是否真需要 op?
- Simple Search 当前过度配置, 切到 PathMarkBit.
- Simulate Search 已是 L1 (bitset, 不走 PathMark), 不动.
- 全局推广完成后, 把 bb::PathMark 删掉, 各 strategy 全部用 PathMarkBit/PathMark.
