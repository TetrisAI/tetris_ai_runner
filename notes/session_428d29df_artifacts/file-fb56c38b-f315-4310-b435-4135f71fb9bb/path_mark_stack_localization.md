# PathMark 栈化可行性 — 调研

> 触发: 用户问 "以后要支持并发, land_point_cache 可能会随着 TetrisContext 移除
> 而消灭, PathMark 有必要放到 ctx 内么? PathMark 直接栈变量行不行?"
>
> 结论 TL;DR: **能栈化 3/4**, `t_mark_` 因 oracle stale-data 语义不能栈化,
> 但可以脱离 ctx, 改成 thread_local static.

## 1. 当前 ctx 内 PathMark 字段清单

| 字段 | 类型 | 持有方 | 入口 clear? | 跨调用 data 残留? |
|------|------|--------|------------|------------------|
| `search_mark_` | PathMarkBit | tag::ExtrasMixin | ✅ memset | ❌ 无 (1 bit) |
| `make_path_mark_` | PathMark | tag::ExtrasMixin | ✅ ++version_ | ❌ 无 (clear 复位语义) |
| `t_mark_` | PathMark | tag::ExtrasMixin | ✅ ++version_ | **✅ 必须保留 prev/op** |
| `path_mark_` | PathMark | path::PathMarkMixin / simulate::PathMarkMixin | ✅ ++version_ | ❌ 无 |

## 2. 栈大小评估

PathMark = `cell_ver_[kR][kCells](8B) + cell_prev_[kR][kCells](3B,可能pad到4) + cell_op_[kR][kCells](1B)`,
SRS-7 + 10x40 板下 kR=4, kCells=400: **约 19-20KB / 实例**.

PathMarkBit: kBits=4*400=1600, kWords=25, **约 200B / 实例**.

栈预算: Linux 默认 8MB 主线程栈, pthread 默认 8MB. 单帧 ~20KB 是 0.25%, 完全可承受
(搜索栈深度本来就浅: search() / make_path() 各自单层不嵌套, T-spin emit 也是
循环不递归).

## 3. 性能影响 — 关键: PathMark 是 POD, 没有动态分配

PathMark 是 `int8/uint64/char` 数组, **没有任何 vector / heap allocation**.
"放在 ctx 重用" 跟 "栈对象每次重新构造" 在内存分配上完全等价 — 栈版没有
malloc, ctx 版本也没有. 唯一差异:

- ctx 版: clear() = `++version_` (1 store, ~1 ns).
- 栈版: ctor zero-init = `memset(~20KB)` (~5-10 ns), 再 ctor 即 zero state,
  无需 clear().

**PathMark 的 ctor zero-init 比 ++version_ 慢一个量级**, 但绝对值还是纳秒级,
search() 一次调用千百次入口 clear() 都不到 us 量级. **没意义**.

PathMarkBit 200B: ctor zero-init = `memset(200)` ≈ 2-3 cache line write,
跟当前 clear() 行为完全相同 (PathMarkBit 已经是 memset 实现).

→ **栈化 vs ctx 化, 性能差异可忽略**. 决策点在语义和架构, 不在性能.

## 4. byte-equal 约束 — t_mark_ 必须保留跨调用 data 残留

详见 `tag_t_path_mark_merge_v2.md` §3.4-3.7. 简述:

oracle TetrisNodeMark::clear() 只 `++version_`, 不动 `data_`.
search_t() 入口 mark(entry) 把 entry version 提到本轮, **本轮 BFS 不会再写
entry 自己的 data**. emit 阶段 get(entry) 读到的 prev/op = "prior 同 piece T
的某次 search_t BFS 中, 把 entry 当 child 时最后一次写下的 (parent, op)".

位板侧 t_mark_ 复刻此行为依赖两点:
1. `mark_bbox` 不写 `cell_prev_/cell_op_` (✅ 已实现);
2. **跨 search_t_native 调用的 cell_prev_/cell_op_ 必须持久** (= 不能栈化).

**栈化 t_mark_ 的后果**:
- 每次 search_t_native 入口 ctor 新 PathMark → cell_prev_=`{0,0,0}`, cell_op_=`'\0'`;
- entry mark 后 get → 命中, 读默认值;
- `last_node = state_to_node(PrevKey{0,0,0}+sunk_state.t)`, 反查到 (r=0,xb=0,yb=0)
  位置的某 master node — 与 oracle 的 prior-call-data 不一致;
- tag_node_diff 第 N 次调用 (N≥2) 会失败.

→ **t_mark_ 不能栈化**.

但 t_mark_ 可以**脱离 ctx**:
- 改成 `static thread_local PathMark t_mark_;` 局部于 search_t_native;
- thread_local 自带跨调用持久化 + 线程隔离, 完美适配并发场景;
- 从 ctx 字段移除, 与 "TetrisContext 移除" 路线一致.

注意 thread_local 静态对象的初始化时机: C++ 标准是"首次进入函数时初始化",
zero-init PathMark 的成本一次性 (~20KB memset), 之后每次入口 clear() = 
`++version_`, 行为与当前 ctx 版本完全一致.

## 5. 其他 3 个字段栈化方案

### 5.1 search_mark_ (PathMarkBit, ~200B)
- 入口 clear (memset(0)) → 栈化后 ctor 直接 zero-init, 行为等价.
- 仅 `run_piece_1g_native` / `run_piece_20g_native` 内使用, 函数局部.
- **完全可以栈化**, 0 风险, 可顺手完成.

### 5.2 make_path_mark_ (PathMark, ~20KB)
- 入口 clear, BFS 写 prev/op, build_path 反向链读 prev/op.
- 跨调用 data 残留 **不被消费** (clear 后 version 不命中, 任何 get 返默认).
- 仅 `make_path_none` 内使用.
- **可以栈化**. 顺手把 stack 帧拉大 20KB.

### 5.3 path/simulate 的 path_mark_ (PathMark, ~20KB)
- 入口 clear, 同 make_path_mark_, 跨调用 data 残留不被消费.
- 用法在 `make_path_1g_native`, `make_path_20g_native`, `make_path_simulate_*`.
- **可以栈化**.

## 6. 并发收益评估

用户场景: "以后要支持并发, land_point_cache 可能会随着 TetrisContext 移除".

栈化 PathMark 的并发收益:
- ✅ 天然 per-call, 无共享状态;
- ✅ 跟 land_point_cache 的去 ctx 化路线一致;
- ✅ Context 字段大幅瘦身 (单 strategy 实例减少 ~40-60KB);
- ⚠️ 唯一"共享/持久"诉求是 t_mark_, 用 thread_local 解决.

**不栈化**则需要 Context per-thread 复制, 即多线程各持一份 ctx. 也能并发,
但 ctx 体积大, 多份开销可观.

## 7. 推荐落地顺序

1. **A 步 — 栈化三档 (低风险)**
   - tag::ExtrasMixin 砍掉 search_mark_, make_path_mark_ 字段, 改栈局部.
   - path/simulate 砍掉 PathMarkMixin (path_mark_), 改栈局部.
   - PathMarkMixin 类型本身可以删除.
   - 跑 4 个 diff, byte-equal 应保持.

2. **B 步 — t_mark_ 改 thread_local static**
   - tag::ExtrasMixin 砍掉 t_mark_ 字段, search_t_native 内
     `static thread_local PathMark t_mark_;` 替代.
   - 跑 tag_node_diff 验证 stale-data 语义保持.

3. **B' 备选 — 维持 ctx.t_mark_, 但加注释明确"持久化语义"**
   - 如果暂不打算改 thread_local (例如未来并发架构尚未敲定),
     维持现状但在字段注释加 "MUST be persistent across calls (oracle
     stale-data semantics)" 警示.

A 跟 B 是独立的可分别提交的两个 commit.

## 8. 已知风险点

- **栈帧大小**: 三档同时栈化, 单 strategy 调用栈帧增加 ~60KB. Linux 8MB 栈下
  仍 < 1%, 无压力. WebAssembly / 嵌入式平台栈预算可能更紧, 需要确认编译目标.
- **PathMark ctor zero-init 成本**: ~20KB memset, 单次调用纳秒级. 但若
  search() 在 hot loop 内被高频调用 (例如 perft 单线程 100k+ qps), 每次
  栈帧 ctor 多花 ~5ns × 多档可能累积. 需要 benchmark 而非靠估算.
- **thread_local 初始化**: ABI 上首次访问需要一次原子 init guard, 后续访问
  零开销. 不是热点.

## 9. 与"TetrisContext 移除"路线的关系

PathMark 栈化是 ctx 瘦身的子任务之一. 砍掉 PathMarkMixin / search_mark_ /
make_path_mark_ / t_mark_ 后, ctx 上的 PathMark 体系从 "持久化 mixin" 退化
为 "0 字段". 与 land_point_cache_ 的去 ctx 化是一致方向.

未涉及的其他 ctx 字段:
- `node_search_path_` (BfsQueueMixin, std::vector<BBState>): 这个**真正有
  buffer 复用收益** (vector 重用 capacity 避免重复 allocate). 栈化反而损失.
- `state_node_lut_` (StateNodeLutMixin, std::array): 是预计算 LUT, 在 init()
  阶段一次性填充, 跨 search 调用读取. **不能栈化**.
- `land_point_cache_` (std::vector): emit 输出, 跨 search/make_path 一对调用
  共用. 用户已点出未来要去 ctx 化, 方案需另议.
