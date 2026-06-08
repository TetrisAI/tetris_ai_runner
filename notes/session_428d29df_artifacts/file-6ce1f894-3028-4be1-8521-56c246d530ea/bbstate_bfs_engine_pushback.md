# 位板框架 BFS 抽象可行性: 参考 bfs_engine 改造?

> 时间: 2026-05-30  会话: flip-bits-clean (head=6fce037)
> 用户提议: "参考 bfs_engine 的方式，对位板框架进行调整，可行？"
> 我的判断: **可行但不推荐做整体抽象, 给一个折中方案**

## 1. 先把"参考对象"看清楚: bfs_engine.h 抽出来的是什么

`bfs_engine.h` 的成功是**指针图 BFS 的 95% 重复消除**:

```
Node            = TetrisNode const *           (4-byte 指针, 不变量)
NeighborProvider= OneGravityNeighbors / TwentyGravityNeighbors / FullActionNeighbors
DedupPolicy     = IndexedDedup / ParentTrackingDedup
Visitor         = OneGravityCollector / TwentyGravityCollector / TargetHitVisitor
```

适用场景: `search_path_node / search_simulate_node / search_tag_node` 三个独立 search,
**主循环结构 100% 一致** — 邻居 provider 9 行差异, dedup 共用, visitor 是 land-point collector。
所以 4 模板 + 几个现成 provider 就能消掉三处 90+% 的 BFS 重复。

## 2. 位板框架三处 BFS 看起来像复用对象, 实际共享度远低于预期

`MoveGenSearch` 现有三个 BFS 主循环:

| 位置 | 节点形态 | 邻居生成 | 命中谓词 | mark 语义 | drop 时机 |
|---|---|---|---|---|---|
| run_piece_20g | BBState | l/r/d + kick 链 (x/z/c) | "出队 drop 后 yb-1 越界 → emit" | **cover_if** (kick 升级 shift mark, 让 spin tag 正确) | pop 后 drop |
| make_path 1g | BBState | x/z/c + l/r + L/R + d/D + 'X/Z/C' (shift 后再旋转复合邻居) | `cells_key == target` | first visit wins | 不 drop, 仅末段 D / 重放 wall-kick |
| make_path 20g | BBState | l/r/d + kick 链 (x/z/c) | `cells_key_for_state(post_drop) == target` | first visit wins | pop 后再 drop, drop 后入 mark |

共享的部分实际只剩:
- `std::vector<BBState> queue`
- mark 数组按 `(piece_index?, r, xb, yb)` 线性化
- `usable_at_bb / drop_bb_state / first_passing_kick_bb / cells_key_for_state` 这些 helper

差异点比共享点多, 而且差异点是真差异 — `cover_if` / 复合邻居 / drop 时机, 不是参数化几行能搞定的。

## 3. 套 BfsEngine 风格的代价

如果硬抽 `BBStateBfsEngine<RuleSpec, T, NeighborProvider, DedupPolicy, Visitor>`:

### 代价 1: NeighborProvider 不再 freestanding

bfs_engine.h 那边 `OneGravityNeighbors::for_each(node, map, sink)` 是无状态静态函数,
邻居只查 `node->move_*->check(map)`。

位板侧邻居要查 `usable_arr / shape::wk_* / RuleSpec::ops` 一票编译期表 +
`first_passing_kick_bb` 的 piece-aware 派发, **NeighborProvider 必须模板化到 `<RuleSpec, T>`**, 还要持 `usable_arr const &` 引用。
这就退化为"穿马甲的 lambda", 抽象收益≈0。

### 代价 2: DedupPolicy 接口装不下 cover_if

bfs_engine.h `DedupPolicy::on_enqueue(node, from, action) -> bool` 表达"首次访问就接收, 否则丢弃"。

`run_piece_20g` 的 `try_cover_with_parent` 是**三态**:
- 未访问 → 标 visited + 写 (parent, action), 入队
- 已访问且 mark 是 shift (' ') 且当前是 kick (x/z/c) → **覆盖** parent / action, **不入队**
- 其它已访问 → 拒绝

这是"重写元数据但不改图"的 mark 升级语义。BfsEngine 的 bool 返回值表达不了"覆盖但不入队"。
要塞进去, 接口得变成 `on_enqueue -> EnqueueDecision { Skip, MarkOnly, MarkAndEnqueue }` —
本来 bfs_engine.h 的 search_*_node 用例完全不需要这个枚举, **新接口反向污染清爽的指针 BFS 引擎**。

### 代价 3: 1g make_path 的复合邻居 (X/Z/C)

```cpp
nL = cur.shift_left();
if (allow_rotate_move) {
    rn = rotate_no_kick_bb(piece_t, KickDir::Cw, nL);  // shift 完不再 kick 的 cw 旋转
    sink(rn, 'C');
}
```

这是"先 shift 再 rotate"的双步邻居, neighbor provider 内部要查另一个 helper。
封装到 `MakePath1gNeighbors` 里需要传 `allow_rotate_move / allow_180 / allow_LR / allow_d / allow_D / disable_d` 五个 config 标志。
NeighborProvider 状态机化 → **lambda 比 provider 更短**。

### 代价 4: 多阶段控制 (disable_d 切换)

1g make_path 是 "disable_d=true 跑一遍 → 失败再 disable_d=false 跑一遍" 的两阶段 BFS。
BfsEngine 单次 `run` 表达不了"两次跑, 中间清 mark 改邻居", 需要外层 driver。
这部分逻辑放在 MoveGenSearch 比放在 engine 调用方更内聚。

### 代价 5: 性能模板深度 vs 内联

当前 `run_piece_20g<T>` 是 `template<char T>` 完整特化 + lambda 全内联。
mark slot 索引 / RuleSpec::ops dispatch / kick 链查表都在编译期展开, GCC -O2 / MSVC /O2
能把整段 BFS 折成 jump table。

抽成 `BBStateBfsEngine<RuleSpec, T, NeighborProviderT, ...>` 之后,
`Sink` / `Visitor` 多一层 indirection, 编译器是否还内联取决于 LTO 打开与否。
位板路径是 search 的热点, **为抽象付一定的性能不确定性**, 收益很小。

## 4. 反过来看, 真正能抽的"小工具"

不是 BFS 引擎本身, 是 BFS 用到的 `位板 helper` 集合, 现在全部嵌在 `MoveGenSearch::private` 里。
如果以后真要做 search_tag_node 接 MoveGenSearch / search_aspin 接 MoveGenSearch,
共享的会是:

| 工具 | 当前位置 | 抽出后用途 |
|---|---|---|
| `struct BBState` | MoveGenSearch private | 共享值类型 |
| `state_to_node[piece*r*h*w]` LUT | MoveGenSearch member | 多 search 共建 |
| `usable_at_bb / drop_bb_state` | MoveGenSearch private static | piece-aware helper |
| `cells_key_for_state` | MoveGenSearch private static | mark/dedup 键 |
| `first_passing_kick_bb` | MoveGenSearch private static | wall-kick 重放 |
| `BBStateLinearMark<kPieceCount, kMaxR, kH, kW>` | 待抽 | (r, xb, yb) → MarkSlot 线性表 |

抽出方向: 一个 `bb_state.h` (或合并进 `bfs_engine.h` 的 bb 命名空间) 持纯数据结构 + 静态 helper。
**不抽 Engine + 4 模板**, 三处 BFS 的主循环留在 MoveGenSearch.

## 5. 我的判断

- (A) **整体套 BfsEngine 风格**: 可行但不推荐。NeighborProvider/DedupPolicy 接口装不下
  位板 BFS 的真实差异 (cover_if / 复合邻居 / drop 时机), 套上去要新增枚举返回 / lambda
  捕获 / 模板嵌套, 抽象收益 < 复杂度上升 + 性能不确定性。**这是 over-abstraction 的镜像版本**。

- (B) **抽小工具**: 可行且有价值。BBState / state_to_node / usable_at_bb / drop_bb_state /
  cells_key 等是无状态静态 helper, 抽到 `bb_state.h` 后 MoveGenSearch 瘦身且可被未来其它
  位板消费者复用。但**当前还没有第二个位板消费者**, 抽出来短期内是空仓库, 真正动机不够强。

## 6. 想跟你确认的痛点

跟上次一样, 提案动机决定该不该动:

- **(α) 看到三个 BFS 主循环结构相似, 觉得"应该共享"** → 我的反驳是"看似相似, 差异点装不进 BfsEngine 接口", 不动整体。
- **(β) 后面要接 search_tag_node / search_aspin → MoveGenSearch, 担心位板 helper 不能复用** → 我的提议是抽"小工具"到 `bb_state.h`, 不抽 Engine。
- **(γ) 觉得 movegen_search.h 2200 行太大, 想拆文件** → 抽 helper 到 bb_state.h 能减 ~300 行, 但主循环留下还是 1900 行, 拆得不彻底, 收益有限。
- **(δ) 别的具体目标 (你说)**

针对 (α) 我反对; (β) 我支持但建议等真有第二消费者再动 (lazy abstraction); (γ) 拆 helper 收益小;
(δ) 看具体场景。

**先共识再动手, 此时不做代码变更。**
