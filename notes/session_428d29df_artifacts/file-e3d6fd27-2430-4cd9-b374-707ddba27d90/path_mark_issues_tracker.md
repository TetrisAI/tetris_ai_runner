# PathMark 全景问题追踪

> 目的: 把当前已识别的所有 PathMark 相关设计问题/优化机会汇总, 避免散落在
> 多个 commit notes / handoff 文档里被遗忘. 每条都包含: 现状 / 问题 / 收益 /
> 状态.
>
> 上下文: bitboard 化的 PathMark 是 oracle TetrisNodeMark 的位板等价物, 当前
> 在 path/simulate/tag 三个 strategy 共用. simple 不用 PathMark.
> tag 的 t_path_mark_/path_mark_ 拆分 + PathMarkBit 引入是最近的演进起点.

---

## P0 — 已完成

### P0-1. PathMarkBit 用一维 bitset + memset, 不再走 version_
**现状**: `bb::Helpers<RuleSpec>::PathMarkBit` 已改成 `std::uint64_t bits_[kWords]`
+ `memset` clear. 仅承载 1 bit/cell 的"已访问"信息.
**位置**: `src/bb_state.h:298-332`.
**收益**: 内存 ~12.5KB → ~200B (64x), 完整 fit L1 cache; clear 还是 O(1) 级.
**状态**: ✅ 已落地.

### P0-2. tag::t_mark_ 不需要按 T piece 分桶
**现状**: `t_mark_` 类型 = `bb::PathMark` (单桶), `search_t_native` 入口直接
`t_mark_.clear()`. 不再有 `PathMarkPathT` 这种 kPieceCount 倍内存的设计.
**位置**: `src/search_tag.h:88` + 入口 clear.
**理由**: `search_t_native` 仅对 `SpinHook::active_for_piece<T> == true` 的
piece 实例化 (TSpinHook 下只对 'T'), 物理上单 piece 写入. oracle 的"跨调用
同 piece 残留可见"语义靠 `mark_bbox` 不写 prev/op 的 stale 行为单独保证,
跟 piece 分桶正交.
**状态**: ✅ 已落地.

### P0-3. tag PathMark 双档化 (PathMarkBit + PathMark)
**现状**: tag::ExtrasMixin 三个字段:
- `search_mark_` (PathMarkBit, L1) — 1g/20g 非 T BFS;
- `t_mark_` (PathMark, L3 + entry-stale) — search_t_native;
- `make_path_mark_` (PathMark, L3) — make_path_none.

PathMarkMixin 已从 tag::Context 移除.
**状态**: ✅ 已落地, **但还未对拍验证** (P1 待办的 build/diff 任务).

---

## P1 — 接下来落地

### P1-1. PathMark 容量按"实际可能 piece 集合"收紧
**现状**: 所有档位 `kR = bb::Helpers<RuleSpec>::kMaxR` (全 piece max).
**问题**: 字段类型上声明的 r 范围比实际承载的 piece 集合宽. 例如 tag::t_mark_
仅承载 active_for_piece<T>=true 的 piece (TSpinHook 下只 'T'), search_mark_
只承载非 T-spin piece, 用全集 max 是过度表达.
**收益**: 类型自洽, 编译期挡住误用 (例如把非 T BFS 状态喂给 t_mark_); 加新
规则 (5-cell, SZ-spin) 时容量自动随集合收缩. SRS-7 + TSpinHook 下 byte 数
**没有节省** (因为 L/J/T 的 rcount 都是 4, 取 max 还是 4).
**实施**: bb_state.h 给 PathMark/PathMarkBit 加 `template<int RBound = kMaxR>`,
旧名字保留为默认实例; 加 `MaxRotationIf<Spec, ops, Pred>` 编译期工具; tag
ExtrasMixin 用 SpinPred/NonSpinPred 算 kSearchR/kTSearchR.
**详细方案**: `path_mark_per_piece_capacity.md`.
**状态**: ⏳ 调研完, 等 P0 对拍通过后开工.

### P1-2. tag PathMark 双档化的对拍验证
**现状**: 代码已改, 未跑 tag_node_diff.
**方法**: build/ 重编 + 跑 oracle vs bitboard byte-equal 对拍.
**状态**: ⏳ 进行中.

---

## P2 — PathMark 多档化全局推广 (跨 strategy)

### P2-1. PathStrategy 1g make_path PathMark 是否过度配置
**现状**: `PathStrategy::Context` 用 `PathMarkMixin` (= `bb::PathMark`, L3 全档),
1g make_path 走 `MakePath1gDedup` (set + cover_if 都不调, 只 set/get).
**问题**: `MakePath1gDedup::try_admit` 只调 `set_bbox` 写 prev+op, build_path
反向链 `get_bbox` 同时读 prev+op. 似乎都需要, 不冗余. **需要 subagent
盘点确认**: 1g neighbors 是否真要 prev+op, 还是只 prev (op 仅末段写一次)?
**收益**: 若 op 字段不需要 per-cell, 内存可降 ~15KB.
**状态**: ❓ 待确认.

### P2-2. PathStrategy 20g make_path PathMark 是否过度配置
**现状**: `MakePath20gDedup` 同样写 prev+op. 但 20g neighbor 入队前已 drop,
build_path 反向链是否需要 op?
**位置**: `src/search_path.h:620-650, 944-960`.
**状态**: ❓ 待确认.

### P2-3. PathStrategy run_piece_20g 自有 Run20gMarkSlot 是另一种 PathMark 变体
**现状**: `Run20gMarkSlot` 直接 vector<{visited, op, parent_r, parent_xb, parent_yb}>,
不走 `bb::PathMark` 体系.
**位置**: `src/search_path.h:239-300`.
**问题**: 这是一个手写的 L3 等价实现 (visited+op+prev), 与 `bb::PathMark` 重复.
应该统一到 PathMark 多档体系, 减少代码面.
**收益**: 类型一致 + 利用 PathMark 的 version_ 墓碑 O(1) clear (当前 vector
是 ctor-zero, 每次调用 `Run20gMarkSlot{0,...}` 重建 vector ~12.5KB allocation).
**状态**: ❓ 待评估迁移.

### P2-4. SimulateStrategy PathMark 档位
**现状**: `SimulateStrategy::Context` 用 `PathMarkMixin` (L3 全档), 1g/20g
make_path 都走 `MakePath1gDedup` (set+get).
**问题**: 同 P2-1, 需要确认 op 字段是否真需要.
**位置**: `src/search_simulate.h:100-103, 449-509`.
**状态**: ❓ 待确认.

### P2-5. SimpleStrategy 不用 PathMark, 是否需要?
**现状**: simple 用栈数组 + 循环做 rotation BFS, 不走 PathMark.
**位置**: `src/search_simple.h:325-380`.
**评估**: simple 只关心 rotation 维度 (BFS 内只 4-7 个 r 节点), 不需要
per-cell 标记. 当前设计合理, **不动**.
**状态**: ✅ 不动.

### P2-6. SimulateStrategy 20g neighbor 入队前不 drop, 但 dedup 用 1g 同款
**现状**: `MakePath1gDedup` 被 simulate 1g 和 20g 共用; 20g 不 drop, 与 path
20g (drop 后入队) 行为不同.
**问题**: 这条不属于 PathMark 档位本身, 但属于"PathMark 接口契约"的边界
情况, 值得在档位划分时一并审视 — 20g simulate 是否应该用专属 dedup?
**状态**: ❓ 待确认.

---

## P3 — 长期边界 / 健壮性

### P3-1. NoSpinHook 下 t_mark_ 是死字段 (浪费 ~50B)
**现状**: TagStrategy 当前唯一 SpinHook 是 TSpinHook, 但模板上允许 NoSpinHook.
NoSpinHook 下 `active_for_piece<T>` 全 false, t_mark_ 永不被 search_t_native
读写, 但仍占 ~50B (Pred 兜底 kR=1).
**问题**: 非 P0, 但 P1-1 落地后值得用 `if constexpr` + 空类型替代死字段.
**状态**: 🟢 备查.

### P3-2. PathMark::clear() 中 version_ 溢出回 1 路径未触达
**现状**: `version_` 是 uint64, 实际不会溢出 (一秒生成数千万次 BFS 也得跑
~5800 年). 但溢出回 1 + 全表清零的 fallback 占 ~5 行代码.
**问题**: 是否应当用 `[[unlikely]]` + 注释明确"never reachable in practice"?
或干脆删掉, 让 wrap-around 自然撞到旧 version 后由 cell_ver_=0 兜底 (但
0 是初始值, 会撞).
**状态**: 🟢 备查 (低优先级).

### P3-3. PathMarkBit kBits=0 兜底
**现状**: 当 kR=0 时 kBits=0, kWords=0, `bits_[0]` 是 zero-size array, 编译期
错误. 已用 `MaxRotationIf` 兜底成 1, 但 PathMarkBit 自己内部没有同等防御.
**问题**: 若有人直接 `PathMarkBitT<0>` 实例化会爆炸. 加 static_assert 或者
内部用 `(kBits + 63 + 1) / 64` 强制 ≥ 1 word.
**状态**: 🟢 备查 (P1-1 落地时一并加 static_assert).

---

### P1-3. PathMark 栈化 + search_t 入口语义修正 (并发预备 + oracle bug fix)
**触发**:
1. 未来 TetrisContext 移除 + 并发支持, ctx 内 PathMark 字段需要清理.
2. 用户点出 entry-grounded TSpin 触发漏判: T 摆下去直接消行的实战场景下,
   entry 几何已满足 3-角 corners 但 spawn 不算 last_rotate, 需要"原地 rotate
   一下再转回来"才能算 TSpin. oracle 当前完全靠 prior-call 残留 op='z'/'c'
   触发, 没有残留就漏判, 这是 oracle 设计漏洞.

**当前实现的语义陷阱 (修正版)**:
clear 后 cell version 不命中, cover_if 第一行 `version 命中 && data.second != ck`
因短路 false 直接进入写入分支. 真正的拒绝来源是 **入口 `mark(entry)`** 把
entry version 抢先提到本轮 — 后续 BFS rotate 回 entry 时 cover_if 走 "version
命中, data.second != ck" 分支被拒绝, 升级路径完全死掉.

**正确设计 (修正版)**:
直接**去掉入口 `mark(entry)`**, entry 作为 BFS 起点由调用方手动 push 入队
(不走 dedup try_admit). BFS 邻居 rotate 回 entry 时, version 不命中 →
cover_if 第一次访问直接写入 (parent, 'z'/'c'), 升级路径来自本轮真实探索,
与 prior-call 完全解耦. 这比"入口 set 占位 ' '"方案改动更小.

`mark(entry)` 在功能上是冗余的: 起点本来就是手动入队, 不需要预占 version
阻止入队; 后续访问由 cover_if/set 自身的 version 检查正确拒绝重复写入.

**副作用**:
- 栈化 t_mark_ 不再有 byte-equal 顾虑 — 新设计不依赖 stale, 栈对象 ctor
  zero-init 完全够用. ctx 内 4 个 PathMark 字段全部可栈化.
- tag_node_diff 在 entry-grounded 棋面会与 oracle (未修) 出现差异, 差异方向
  是"oracle 漏判 TSpin, 新版正确判定". 需决定 oracle 是否同步修.
- BFS 探索量基本不变 ("rotate 回 entry" 路径本来也走, 只是被 cover_if 拒绝
  写入; 改 set 后写入 + 入队, 多 1 次 entry 重访, 量级可忽略).

**实施顺序 (建议)**:
1. **commit A**: oracle TetrisNodeMark::set(entry,...) 替换 mark(entry) (修
   漏判) + 位板 t_mark_ 同步修正. 跑 tag_node_diff 仍 byte-equal.
2. **commit B**: ctx 瘦身. tag::ExtrasMixin 砍 search_mark_/t_mark_/
   make_path_mark_; path/simulate::Context 砍 PathMarkMixin. 4 个 search/
   make_path 入口接收 mark 参数, 调用方栈创建. PathMarkMixin 类型从
   movegen_context.h 删除.

**待确认**:
- Q1: 入口语义 mark → set 是否落地?
- Q2: oracle 是否同步修?
- Q3: A/B 是否合并为单 commit?

**详细分析**: `path_mark_stack_localization.md` (栈化部分) +
本条备注 (entry-grounded 修正部分).
**状态**: ⏳ 调研完, 等用户拍板 Q1/Q2/Q3.

---

## 当前推进顺序
1. **P1-2 对拍** ✅ 已完成. P0 系列 single commit `4fef7c3` 落地, 未 push.
2. **P1-1 容量收紧** — byte 数 0 节省, 类型自洽收益.
3. **P1-3 栈化 + thread_local** — 真正的 ctx 瘦身, 与 P1-1 独立.
4. **P2 系列** — 全局推广. 由 subagent 盘点 P2-1/P2-2/P2-4 后批量改造.
5. **P3 系列** — 顺手做或留 TODO 注释.
