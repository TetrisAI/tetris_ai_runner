# Tag PathMark 分档与 t_path_mark_ 清理 — 任务计划

## 决策定性
1. **砍掉 `t_path_mark_` 是定性目标** — 不是冗余的"备用桶", 是设计错误.
2. **PathMark 不应是单一实现**, 而应按"实际去重需求"分档, 编译期选档.
3. 当前任务范围: **只在 tag 内部** 把 tag 需要的 PathMark 各档实现到位,
   tag 的所有 dedup 都使用"刚好够用"的档位, 不付额外代价.
4. **下一个任务** (本计划之外): 检查 path / simulate / simple 三个 search,
   把它们各自的 dedup 切到对应档位, 完成"PathMark 多档化"全局推广.

## tag 内部各档 PathMark 真实需求 (subagent 已盘点)

| dedup 名 | 需要档 | 写入 API | 读取 API | piece 分桶 |
|---------|-------|---------|---------|-----------|
| TagSearch1gDedup (search 阶段, non-T 1g BFS) | **L1**: 1 bit/cell | `mark(r,xb,yb)` | (BFS 内不读) | 单 piece BFS, 单桶, clear 复位 |
| TagSearch20gDedup (search 阶段, non-T 20g BFS) | **L1**: 1 bit/cell | `mark(r,xb,yb)` | (BFS 内不读) | 单 piece BFS, 单桶, clear 复位 |
| TagSearchTDedup (search_t_native, T BFS) | **L3-stale**: PrevKey + op + entry mark 不写 data 保留 stale | `set` / `cover_if` (常规) + `mark` (entry 仅 bump version) | `get` (emit 阶段读 last/op) | **跨 search() 同 piece 残留可见, 跨 piece 残留隔离**: 必须按 piece 分桶 |
| TagMakePath1gDedup (make_path 阶段) | **L3**: PrevKey + op | `set` | `get` (build_path 反向链表) | 单 piece BFS, 单桶, clear 复位 |
| TagMakePath20gDedup (make_path 阶段, 走 PathStrategy 同款 set 二态) | **L3**: PrevKey + op | `set` | `get` (build_path 反向链表) | 单 piece BFS, 单桶, clear 复位 |

注: TagMakePath20g 在 search_tag.h 中是 None 路径回溯, 走 set 二态 + visitor
on_admit 命中. 与 1g 同档.

### 关键观察
- **L1 (search 阶段非 T)**: 不需要 piece 分桶 — 单次 search() 调用 1 个 piece,
  入口 clear 即可. 内存 1 bit/cell. 但实际上 cell_ver_ 用 64-bit version
  比 1 bit 更省 clear 成本; 性能优化方向是 "去掉 cell_prev_/cell_op_ 数组".
- **L3-stale (T-search)**: piece 分桶必要. 同 piece 跨调用残留对当前 search 可见
  (oracle 语义), 跨 piece 残留隔离 (同 (r,xb,yb) 不同 piece 不撞).
- **L3 (make_path)**: 不需要 piece 分桶 — make_path 单次调用单 piece, 入口
  clear 即可.

## tag 实施步骤 (本任务)

### Step 1: 三档 PathMark 类型在 bb 命名空间
新文件 `src/bb_path_mark.h` 或扩展 `src/bb_state.h`:
- `bb::PathMarkBit<RuleSpec>` (L1): kR×kCells 的 cell_ver_, 仅 mark/clear.
- `bb::PathMarkPathT<RuleSpec>` (L3-stale): kR×kCells 的 cell_ver_/cell_prev_/cell_op_,
  **+ piece 分桶** (内部按 kP 复制): set_active_piece(t) / clear() 仅清当前桶 /
  set/cover_if/mark/get 隐式用 active 桶.
- `bb::PathMarkPath<RuleSpec>` (L3): kR×kCells 的 cell_ver_/cell_prev_/cell_op_,
  无 piece 分桶, set/cover_if/get/clear.

注: 当前的 bb::PathMark 等价于无 piece 分桶的 L3. 暂保留向后兼容,
本任务不动 path/simulate/simple. 下一任务统一改造.

### Step 2: tag ExtrasMixin 引入三档实例 (替代当前 path_mark_/t_path_mark_)
```cpp
struct tag::ExtrasMixin {
  ...
  bb::PathMarkBit<RuleSpec>   tag_search_mark_;       // 1g/20g BFS
  bb::PathMarkPathT<RuleSpec> tag_t_mark_;            // search_t_native
  bb::PathMarkPath<RuleSpec>  tag_make_path_mark_;    // make_path
};
```
当前 PathMarkMixin 提供的 path_mark_ 在 tag 中**不再使用**. tag Context 把
PathMarkMixin 从 mixin 列表删掉; 上面三个加入 ExtrasMixin.

### Step 3: 改造 tag 的五个 dedup 类
- TagSearch1gDedup: 改用 PathMarkBit. try_admit 用 mark + usable check 二态
  (无 prev/op, 无 cover_if).
- TagSearch20gDedup: 同上.
- TagSearchTDedup: 改用 PathMarkPathT. set_active_piece(T) 在 search_t_native
  入口被调用 (不是 dedup 自己调). entry 写入仍是 mark (保 stale 语义).
- TagMakePath1gDedup (make_path 阶段): 改用 PathMarkPath. (虽然结构与
  现有 set 二态一致, 但分类清晰, 不依赖外层 PathMarkMixin.)
- TagMakePath20gDedup: 改用 PathMarkPath.

### Step 4: search_t_native 入口
```cpp
ctx.tag_t_mark_.set_active_piece(T);
ctx.tag_t_mark_.clear();   // 清当前 piece 桶
```
跨 piece 自动隔离, 不再依赖 t_path_mark_ 字段名.

### Step 5: build/ 编译, 跑 tag_node_diff 对拍.
**预期**: byte-equal 通过.

### Step 6: clang-format + 单 commit.
Commit 标题: `refactor(movegen): split tag PathMark into per-stage variants`
描述 (中文): 砍掉冗余的 t_path_mark_; 引入 PathMarkBit/PathMarkPathT/PathMarkPath
三档实现, tag 各 dedup 选用刚好够用的档位; PathMarkPathT 内部按 piece 分桶
保 oracle stale 语义.

## 风险点
- PathMarkPathT 的 piece 分桶需要 kPieceCount 副本. 内存 = kP × kR × kCells ×
  (8B version + 1B prev_pack + 1B op_pack) ≈ 7×4×400×16 = ~180KB. 可接受.
- PathMarkBit 实际用 8B version_ per cell (与 cell_ver_ 同款), 无内存节省 vs
  当前 PathMark (因为 PathMark 也是 cell_ver_ 主存). 真正"省"在 1) 不再
  分配 cell_prev_/cell_op_ 数组; 2) clear 仍 O(1).
- tag 的非 T 路径 (1g/20g) 切到 L1 后, dedup 三态 (Skip/MarkOnly/MarkAndEnqueue)
  从原来的 set_bbox 三态退化成 mark 二态 + usable_at_bb 三态. 行为等价,
  但需对照 oracle search_tag.cpp:228-258 仔细比对 mark/check 顺序.

## 下一任务 (本计划外, 占位)
- 检查 path 的 search/make_path 是否有过度配置: 当前 Path Search Run20gDedup
  实际写 PrevKey + op, 但 visitor 只读 prev (build_path 反向). 是否真需要 op?
- Simple Search 当前过度配置 (subagent 标 ✅), 切到 L1.
- Simulate Search 已是 L1 (bitset, 不走 PathMark), 不动.
- 全局推广完成后, 把 bb::PathMark 删掉, 各 strategy 全部用 PathMarkBit/Path/PathT.
