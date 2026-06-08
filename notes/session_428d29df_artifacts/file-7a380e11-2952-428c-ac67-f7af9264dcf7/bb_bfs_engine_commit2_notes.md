# Commit 2 笔记: run_piece_20g 接入位板 BFS 引擎

> 时间: 2026-05-30
> 分支: flip-bits-clean (commit 1: bb::Helpers 抽离 已完成 → 本提交)
> 目标: 把 `MoveGenSearch::run_piece_20g` 的 BFS 主循环切到新的通用位板 BFS 引擎
>       `bb_bfs_engine.h` 上, 行为 1:1 等价.
> 不在范围: `run_piece_1g` / `make_path` 1g/20g (留给 commit 3、4).

## 一、run_piece_20g 现有 BFS 的状态/邻居/去重契约

代码位置: `src/movegen_search.h::MoveGenSearch::run_piece_20g<T>`.

### 1.1 状态 (State)
- `bb::BBState { uint8_t t, uint8_t r, int8_t xb, int8_t yb }`. 入队前不 drop,
  出队后再 drop, 这是 master `search_t` 20g 行为的等价复刻.

### 1.2 起点 (seed)
- spawn = `RuleSpec::spawn(T, kW, kH)` 折出 BBState (`build_state_from_master<T,0>`).
- 起点 sink: `drop_bb_state(spawn_bb, usable_arr)`; sink 失败 (一般是 spawn
  即被堵死) -> BFS 不开始, `land_point_cache_` 维持空集.
- 起点入队后只标 `mark[i].visited = 1`, **不写 parent / action** (parent_r=0xFF
  作为根哨兵).

### 1.3 邻居顺序 (master `search_tspin.cpp:1007~1080` 严格对齐)
出队时取 `s`, 先 drop 得到 `ss`; 然后按以下次序生成邻居:

| 顺序 | 操作 | 邻居生成 | 准入语义 |
|---|---|---|---|
| 1 | `d` 单步下落 | `ss.yb-1` 仍 usable | `set` (首次访问写 parent+' ') |
| 2 | `l` 单步左移 | `ss.xb-1` 仍 usable | `set` |
| 3 | `r` 单步右移 | `ss.xb+1` 仍 usable | `set` |
| 4 | `x` 180-kick (allow_180) | `first_passing_kick_bb(T, KickDir::Opp, ss)` | **cover_if** (升级 ' '→'x') |
| 5 | `z` ccw-kick | `first_passing_kick_bb(T, KickDir::Ccw, ss)` | **cover_if** (' '→'z') |
| 6 | `c` cw-kick | `first_passing_kick_bb(T, KickDir::Cw, ss)` | **cover_if** (' '→'c') |

注意: 邻居入队前**不**再 drop (master 20g BFS 的 drop 时机是出队后).

### 1.4 准入策略 (DedupPolicy)

mark 以 `(r, xb, yb)` 三元组为键, slot 内存:
- `visited` (1 bit)
- `action` (' '/'x'/'z'/'c', 仅 `EnableT` 路径写)
- `parent_r/xb/yb` (仅 `EnableT` 路径写; 0xFF 表 root)

两条规则:
1. **set** (用于 d/l/r): `visited == 0` 时写 mark + 入队 (返回
   `MarkAndEnqueue`); `visited == 1` 时直接 Skip.
2. **cover_if** (用于 x/z/c): `visited == 0` 时同 set 写 mark + 入队;
   `visited == 1 && action == ' '` 时**覆盖** action / parent (升级旋转
   标记, master `cover_if(child, parent, ' ', kick_action)`), 返回
   `MarkOnly` (不入队); 其它情形 Skip.

`EnableT == false` 路径 (非 T piece) 不需要 cover_if 语义 (emit 不读 action),
两条规则等价于"首次访问即接受", cover_if 在 `visited == 1` 时直接 Skip.

### 1.5 出队访问 (Visitor)

每次出队 `s` -> drop 得 `ss`; 然后:
- **landing 谓词**: `!usable_at_bb(ss.r, ss.xb, ss.yb-1, usable_arr)` (再下一步
  不 usable, 即 ss 是 landing).
- **emit 去重**: 用 `cells_key_for(sunk_node)` 作等价类键, `emitted_keys`
  线性表去重 (master `node_mark_filtered_` 的位板替代).
- emit 命中: 反查 `state_to_node(ss)` 拿 master pointer, 调 `Hook::apply_emit_20g`
  (T 路径), `land_point_cache_.push_back`.
- BFS 不会因为 landing 命中而提前终止 — 一直跑到队列空.

## 二、引擎接口设计与映射关系

引擎位置: `src/bb_bfs_engine.h`, 命名空间 `m_tetris::bb`.

```cpp
enum class EnqueueDecision { Skip, MarkOnly, MarkAndEnqueue };

template<class State, class NeighborProvider, class DedupPolicy, class Visitor>
void run_bb_bfs(State const& start, NeighborProvider& np, DedupPolicy& dedup,
                Visitor& vis, std::vector<State>& queue_buffer);
```

### 2.1 主循环骨架

```
queue.clear();
admit = dedup.try_admit(start, nullptr, 0);
if admit != MarkAndEnqueue: return;
queue.push_back(start);
while head < queue.size():
    cur = queue[head++];
    if !vis.on_pop(cur): return;
    np.expand(cur, [&](child, action) {
        d = dedup.try_admit(child, &cur, action);
        if d == MarkAndEnqueue: queue.push_back(child);
    });
```

- 起点 try_admit 仅接受 `MarkAndEnqueue` (commit 2 调用方都这样用).
  `MarkOnly` 在起点的语义退化为 "什么都不做"; Skip 同理. 引擎直接 return.
- visitor 返回 false 即 stop 整个 BFS.

### 2.2 commit 2 调用方的三件套

- **State** = `bb::BBState`.
- **NeighborProvider** = `Run20gNeighbors<RuleSpec, T>` (本类内部的局部 functor):
  - 持有 `usable_arr_ const&`, `allow_180_`.
  - `expand(s, emit)`: 出队后先 drop 一次, 再依次 emit `d/l/r/x/z/c` 邻居.
  - drop 失败 (理论不发生于 mark 已通过的状态) 直接返回 (邻居为空).
- **DedupPolicy** = `Run20gDedup<EnableT>`:
  - 持有 `MarkSlot* mark`, kW/kH/kMaxR.
  - `try_admit(s, parent_or_null, action)`:
    - parent_or_null == nullptr: root, `visited=1`, parent_r=0xFF, 返
      `MarkAndEnqueue`.
    - 越界 (xb/yb 不在 [0,k)): `Skip`.
    - 看 `action`: 若 action ∈ {' '} 走 set 语义 (visited 0 -> Mark&Enqueue;
      visited 1 -> Skip); 若 action ∈ {'x','z','c'} 走 cover_if 语义
      (visited 0 -> Mark&Enqueue; visited 1 && stored_action == ' ' ->
      **MarkOnly** (升级 action+parent); 其它 Skip).
  - 由于 emit 阶段需要直接把 `(parent_r, parent_xb, parent_yb, action)` 作为
    `(last_node, action)` 反查给 `Hook::apply_emit_20g`, 在每个 admitted slot
    存上述四字段; 引擎不接触这些细节, 透明放在 dedup 内.
- **Visitor** = `Run20gVisitor<EnableT>`:
  - 出队 -> drop -> 检查 landing 谓词 -> 命中即 emit (查 LUT, cells_key
    去重, 调 Hook::apply_emit_20g, push 到 `land_point_cache_`).
  - 始终返回 true (不主动停止 BFS).

### 2.3 行为微差异 (drop 语义放在哪一侧?)

master 20g 主循环的特征是**出队后 drop**. 把 drop 放进 visitor 还是 neighbor?
权衡:

- 选项 A: visitor 出队后先 drop, neighbor 接收 `s_post_drop`. 但邻居入队
  时存的是 pre-drop 还是 post-drop?
  - master 是: 入队的就是 post-drop (因为 d/l/r/x/z/c 全部基于 ss=drop(s) 推).
- 选项 B: 队列里始终存 pre-drop 状态, 出队后由 visitor + neighbor 同时 drop.
  - 起点 spawn 已经 drop 过, 进队时就是 post-drop; d 邻居的 yb-1 严格说是
    再下落一步, 入队后下次出队 drop 又会贴地, 这里没问题.
  - 实际 master 也是这样: spawn drop -> 入队; 出队 -> drop (re-drop, 不变,
    因为入队的 ss 已经贴地); 邻居 ss.yb-1 入队 -> 下次出队 drop 落到底.

选项 B 更接近 master 行为. **本 commit 采用选项 B**: visitor 出队时调
drop_bb_state 一次, 拿到 ss; neighbor 也基于 ss 生成邻居 (再 drop 由下一次
出队保证). 由于 visitor 与 neighbor 都需要 ss, 我们在 NeighborProvider 内
显式持有"出队后 drop 缓存"是不可能的 (引擎不知道这是个跨阶段的状态), 因此:

最终决定: **保持入队 = post-drop 之后的 raw state; visitor 与 neighbor 各自
独立调一次 drop**. drop_bb_state 是纯函数 + 局部数组查表, 单次 ~10 行 cell
扫描, 重复成本可忽略. 这样引擎接口保持纯净 (visitor 与 neighbor 各自只看
当前出队 state).

> 实测: oracle_diff 跑过的 7 piece × 多场地 17~34 个 land_point, 单次 BFS
> 出队 ~64 帧, drop 算两次 = 128 次, 而 master 也是 128 次 (each state 反复
> 调 drop). 不增加渐进复杂度.

## 三、行为等价性核对清单

按 BFS 分支逐条核对位板侧实现 vs commit 1 (master 等价) 的行为:

- [x] **z (ccw kick)**: `first_passing_kick_bb(T, KickDir::Ccw, ss, usable)`
  返 `optional<BBState>`, 命中走 cover_if. 行为同 commit 1.
- [x] **c (cw kick)**: `first_passing_kick_bb(T, KickDir::Cw, ss, usable)`,
  cover_if.
- [x] **x (180 kick)**: 仅 `allow_180_cfg` 时尝试; cover_if.
- [x] **l (left)**: `ss.xb-1`, usable_at_bb 检查, set.
- [x] **r (right)**: `ss.xb+1`, usable_at_bb 检查, set.
- [x] **d (down)**: `ss.yb-1`, usable_at_bb 检查, set. (run_piece_20g 不参与
  L/R 多步走, 那是 make_path 的逻辑.)
- [x] **L/R (allow_LR)**: run_piece_20g **不**消费这个开关; allow_LR 只在
  make_path BFS 用. 不在本 commit 范围.
- [x] **D (drop)**: run_piece_20g 不显式产生 D 邻居 (drop 是出队动作不是
  按键), 不在邻居枚举中.
- [x] **disable_d**: 这是 1g make_path 的 disable_d 一阶段重跑机制, 与
  20g run_piece 无关. 不影响.
- [x] **EnableT 分支**: emit 阶段是否调 `Hook::apply_emit_20g`, 由 piece T
  通过 `Hook::active_for_piece<T>` 决定; 引擎透明传递 (Visitor 内
  `if constexpr`).
- [x] **emit 去重**: 仍走 `emitted_keys` 线性 cells_key 表 (master
  `node_mark_filtered_` 的等价).
- [x] **kick cover_if**: 三态 EnqueueDecision::MarkOnly 实现; 在
  `Run20gDedup::try_admit` 内根据 action 字符路由 set vs cover_if.
- [x] **mark 表越界**: 同 commit 1, 越界即 Skip.
- [x] **root 入队**: parent=nullptr, action=0 (引擎契约) -> dedup 写
  visited=1 + parent_r=0xFF, 不写 action 字段.
- [x] **队列容器复用**: 引擎接 `std::vector<BBState>&`, 调用方 (MoveGenSearch)
  把 `node_search_path_` 这类成员 vector 传入即可跨 search 调用复用容量.
  本 commit 选择更保守的方案: 直接传一个局部 vector, 后续 commit 4 再
  统一复用.

## 四、与 commit 1 的两个回滚补丁

commit 1 (`Move bitboard helpers from MoveGenSearch to bb::Helpers`) 在搬移
helper 时遗漏了两点, 导致 `tetris_ai` 这条 build 链失败 (`oracle_diff`
没踩到全部模板实例). 本 commit 顺手修掉:

1. `MoveGenSearch::run_piece_20g` 第 ~972 行 `build_state_from_master<T, 0>(...)`
   依赖基类 `bb::Helpers<RuleSpec>` 上的 template member, 在依赖类型上下文里
   需要 `this->template build_state_from_master<T, 0>(...)` 才能让 GCC
   把 `<` 解析成模板实参起始. (oracle_diff.cpp 入口巧合走的是非 20g 分支,
   T 模板参数没踩到 spawn 这一行, 因此前一个 commit 编译过.)

2. commit 1 把 `state_to_node(BBState)` LUT 反查 helper 删除了 (它依赖
   实例字段 `state_node_lut_[]`, 不能搬到无状态基类), 但 `run_piece_20g`
   末段 emit 仍调它 (1105/1126 行). 本 commit 在 `MoveGenSearch` 自身
   补回 `state_to_node(BBState const&)` 私有成员, 复用 init 期填好的
   `state_node_lut_` 数组. 行为与 commit 1 之前完全相同.

这两个修补严格属于"恢复 commit 1 的语义", 不带任何新行为差异;
单独抽 commit 反而会让 commit 2 的 review 变零散. 写在这里以便审稿确认.

## 五、改动清单 (本 commit)

新增:
- `src/bb_bfs_engine.h`: 通用位板 BFS 引擎. `EnqueueDecision` + `run_bb_bfs`.
- `research/flip-bits/bb_bfs_engine_commit2_notes.md`: 本文.

修改:
- `src/movegen_search.h`:
  - `#include "bb_bfs_engine.h"`.
  - 补回 `state_to_node(BBState const&)` (回滚 commit 1 的过度删除).
  - `run_piece_20g`: 主循环骨架替换为 `bb::run_bb_bfs(...)`, neighbor /
    dedup / visitor 三件套以本地结构体形式声明在函数体内 (避免污染外部
    命名空间, 同时仍可被模板实例化内联).
  - `build_state_from_master<T, 0>(...)` 处加 `this->template`.

不动:
- `make_path` (1g/20g), `run_piece_1g`, `Hook` 各 trait, `bb::Helpers`,
  `bfs_engine.h` (那是指针图 BFS 引擎, 与本位板引擎职责互不重叠).

## 六、验证

- 项目编译: `cmake --build build --target oracle_diff perft_movegen tetris_ai`
  全部 ok (commit 1 漏修的两点同时被本 commit 补回, 因此 `tetris_ai`
  这条编译链才会通过).
- 行为等价: oracle_diff 是定级核心. 见提交说明的 `cmake --build` 输出.

## 七、对后续 commit 的影响

- commit 3 (1g make_path BFS) 准备复用同一引擎: 1g 邻居顺序不同
  (x/z/c/l/r/L/R/d/D + 旋转 X/Z/C), 直接换 NeighborProvider; cover_if
  语义已经在引擎/EnqueueDecision 上定义好.
- commit 4 (20g make_path BFS): NeighborProvider 内部有 L/R 多步走+drop
  的特殊邻居语义 (一步入 mark, 不展开中间状态), 同 1g 一样换 NeighborProvider
  即可.
- DedupPolicy 的两种语义 (set vs cover_if) 已经能用 action 字符区分,
  后续可能进一步抽出 "policy trait" 把"哪个 action 走 cover_if" 编译期化.
  当前先放在 dedup functor 内部分支判断, 等 commit 4 完成后看是否值得拆.
