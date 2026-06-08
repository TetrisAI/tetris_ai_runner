# Commit 4 笔记: make_path 20g 接入位板 BFS 引擎

> 时间: 2026-05-30
> 分支: flip-bits-clean (commit 3: make_path 1g 已切引擎)
> 目标: 把 `MoveGenSearch::make_path_20g_native` 中手写的 BFS 主循环
>       (`movegen_search.h:485-697`) 改造为 `bb::run_bb_bfs(...)` 驱动, 与
>       `Run20g*` / `MakePath1g*` 共用同一引擎; 行为 (路径输出 byte-for-byte) 与
>       commit 3 完全等价.
> 不在范围: `run_piece_20g` (commit 2 已落地), `make_path` 1g (commit 3 已落地),
>           `bb::Helpers` (commit 1 已稳定). 至此 `MoveGenSearch` 内三处 BFS
>           主循环全部由通用引擎驱动, 无任何手写 FIFO.

## 一、make_path 20g 现有 BFS 的状态/邻居/去重契约

代码位置: `src/movegen_search.h::MoveGenSearch::make_path_20g_native` (commit 6c
最后一次手写实现).

### 1.1 状态 (State)

- `bb::BBState { uint8_t t, r; int8_t xb, yb }`. 与 commit 2 的 run_piece_20g
  一致, **入队的都是 post-drop**, 每个邻居生成时都要 drop 一次. 这是 master
  `search_tspin::Search::make_path_20g` 的口径.

### 1.2 起点 (entry)

- `entry_state = state_from_node(node)` -> `s0 = drop_bb_state(entry_state)`.
  drop 失败 (起点已被堵死) -> 直接 return 空 path.
- 起点 mark: 写 PathMark root (PrevKey r=0xFF, op='\0').

### 1.3 起点 fast-path (与 1g 平行结构)

1. **landpoint_is_none + 起点 cells_key == land_point.node cells_key**: 直接
   return `[]`. master `make_path_20g` 的 sunk_spawn 命中即空路径.
2. **check_hit_state(s0)**: 起点 cells_key 已经命中 `index_key` (master 把
   `last` 节点 cells_key 当 index_key, 普通情况退化为 land_point.node) 或
   (`!landpoint_is_none && k == index_landpoint && config_last_rotate`).
   命中 -> 写 root mark + 调 build_path(s0). build_path 内的末段
   wall-kick 重放仍执行 (s0 的 cells_key 可能与 index_landpoint 不同, 由 last
   接末段 'x'/'z'/'c').

两条 fast-path 都保留在引擎之外, 与 commit 3 同结构, 让"起点即解"的两种
退化语义不绕一圈 BFS.

### 1.4 邻居顺序 (master `search_tspin.cpp` 20g 分支严格对齐)

每次出队 cur, 按以下次序生成邻居; **每个邻居在入队前都 drop 一次**:

| 顺序 | 操作 | 邻居生成 (pre-drop) | drop 后 | 准入语义 |
|---|---|---|---|---|
| 1 | `x` 180-kick (allow_180)  | `first_passing_kick_bb(t, Opp, cur)` | drop | set |
| 2 | `z` ccw-kick              | `first_passing_kick_bb(t, Ccw, cur)` | drop | set |
| 3 | `c` cw-kick               | `first_passing_kick_bb(t, Cw, cur)`  | drop | set |
| 4 | `l` 单步左移              | `(cur.r, cur.xb-1, cur.yb)` (前置 usable_at_bb) | drop | set |
| 5 | `r` 单步右移              | `(cur.r, cur.xb+1, cur.yb)` (同上) | drop | set |
| 6 | `L` (allow_LR)           | 沿 -x 一直走, 每步 usable + drop, 最末 terminal | (内置) | set |
| 7 | `R` (allow_LR)           | 沿 +x, 同 L                              | (内置) | set |

注:
- 没有 `d` (单步下落): 20g 模式下 drop 已隐含整列下落, 不需要单独 d 邻居.
  这是 1g 与 20g 邻居集的关键差异之一.
- 没有 `D` (硬下落): 同上, 已被 drop 内化.
- 没有 `rotate_move` (X/Z/C): 20g make_path 不消费 `allow_rotate_move` 配置.
- L/R 的 master 行为是 `while (move_left/right && check) cur = move_left/right -> drop`,
  位板侧用 `usable_at_bb` 替代 `move_left && check`, 每步显式 `drop_bb_state`.
  整段终态 (terminal) 整体作为一步 `'L'` / `'R'` 入 mark, 中间 sub-state
  不入队、不入 mark.

### 1.5 准入策略 (DedupPolicy) — 仅 set, 无 cover_if

- mark 用 `PathMark` (per-instance, kMaxR × kW × kH 维), key = (r, xb, yb).
- **set** 语义对所有邻居 (含 L/R 多步终态): version 检查首次写入即 `MarkAndEnqueue`,
  否则 `Skip`. 起点 (parent==nullptr) 写 PrevKey root (r=0xFF) + op=`'\0'`.
- 与 1g 的 `MarkOnly` 三态对比: 20g 没有 `cover_if` 升级语义, 也没有 `set+
  usable` 拆分 (邻居生成时已经经 `drop_bb_state` 隐式过 usable 检查), dedup
  侧只需要 set / Skip 二态. 但仍走同一引擎签名, `MarkOnly` 在 20g 下永不返回.

### 1.6 hit 检测时机

- 与 commit 3 1g 一致: 命中检测放在 `Visitor::on_admit` (dedup 准入即检).
- 命中条件:
  ```
  cells_key(child) == index_key
  || (!landpoint_is_none && cells_key(child) == index_landpoint && config_last_rotate)
  ```
  与原 `check_hit_state` 完全等价.
- 命中后 visitor.found = child, hit = true, 返回 false; 引擎立即停止.
- 起点 fast-path 已经在引擎外吃掉, 引擎内 on_admit 对起点也会再过一次 check_hit
  (永远返回 false 因为 fast-path 已先返回), 不会有副作用.

### 1.7 路径回放 (build_path)

终态 cur 的 PathMark 链回溯:

```
while True:
    (prev, op) = path_mark.get_bbox(cur.r, cur.xb, cur.yb)
    if !prev.has(): break
    path.push_back(op)
    cur = prev
reverse(path)
if cells_key(terminal) != index_landpoint:
    last_state = state_from_node(land_last)
    for dir in [Opp(if allow_180), Ccw, Cw]:
        hit = first_passing_kick_bb(t, dir, last_state)
        if hit and cells_key(drop_bb_state(*hit)) == index_landpoint:
            path.push_back('x'/'z'/'c')
            return
return
```

末段 wall-kick 重放与 1g 唯一不同: 20g 模式下 kick 后还要 `drop_bb_state`
再比 cells_key (master make_path_20g 行为). 1g 用 `try_kick_chain_to`
(无 drop), 20g 用本地 `try_kick_chain_to_drop` (kick 完再 drop 才比对). 此处
不动, 保留 commit 6c 的实现.

### 1.8 与 commit 2 `Run20gNeighbors` 的关系

`Run20gNeighbors` (commit 2 给 run_piece_20g 用) 与 `MakePath20gNeighbors`
(本 commit) 在以下三点结构性不同, 不能直接复用:

| 维度 | run_piece_20g (Run20gNeighbors) | make_path 20g (MakePath20gNeighbors) |
|---|---|---|
| drop 时机 | 出队后 drop **一次**, 再生成邻居 (邻居入队不 drop) | 邻居生成后**逐个** drop, 入队的就是 post-drop |
| 邻居集 | d / l / r / x / z / c (含 d 单步下落) | x / z / c / l / r / L / R (无 d, 多 L/R) |
| 邻居 action 字符 | d/l/r 的 action = `' '` (用空格区分 set vs cover_if) | 所有邻居 action = 真实按键 (l/r/x/z/c/L/R) |
| 二级 fan-out | 无 | 无 (rotate_move 是 1g 才有) |

可以泛化的是"drop_bb_state + first_passing_kick_bb"这两个原子操作, 都已经在
`bb::Helpers<RuleSpec>` 里, 各自的 NeighborProvider 直接调即可. 把骨架抽到
NeighborProvider 上面再"参数化" 反而引入更多模板状态机 (drop 时机 + 是否
has-d), 收益不及当前两个独立 functor 清晰.

**结论**: 不复用 `Run20gNeighbors`. 新建 `MakePath20gNeighbors` (并 Dedup,
Visitor), 命名风格与 `MakePath1g*` 完全对齐. 引擎本身一行不改.

## 二、引擎接口的兼容性

`bb_bfs_engine.h` 在 commit 3 后已稳定 (三态 EnqueueDecision + on_admit / on_pop
钩子 + emit parent_override). 本 commit 的 20g 三件套全部使用既有接口:

- NeighborProvider 不需要 parent_override (20g make_path 没有 rotate_move 的
  二级 X/Z/C, 所有邻居 parent 都默认是 cur).
- DedupPolicy 不需要 MarkOnly 返回值.
- Visitor on_admit 用作命中检测 (与 1g 同模式), on_pop 始终 true.

**引擎 0 改动.**

## 三、三件套设计

挂在 `MoveGenSearch` 类内, 与 `Run20g*` / `MakePath1g*` 同位置, 命名
`MakePath20gNeighbors / MakePath20gDedup / MakePath20gVisitor`.

### MakePath20gDedup

```cpp
struct MakePath20gDedup {
    PathMark *path_mark;
    bb::EnqueueDecision try_admit(BBState const &s, BBState const *parent, char action) {
        if (out_of_bounds(s)) return Skip;
        PrevKey pk = parent ? PrevKey{parent->r, parent->xb, parent->yb}
                            : PrevKey{0xFFu, 0, 0};
        if (!path_mark->set_bbox(s.r, s.xb, s.yb, pk, action))
            return Skip;
        return MarkAndEnqueue;
    }
};
```

### MakePath20gNeighbors

```cpp
struct MakePath20gNeighbors {
    char piece_t;
    bool allow_180, allow_LR;
    std::array<map_t, kMaxR> const *usable_arr;
    MoveGenSearch *self;

    template<class Emit>
    void expand(BBState const &cur, Emit emit) {
        auto try_drop_emit = [&](BBState const &pre_drop, char action) {
            auto sunk = self->drop_bb_state(pre_drop, *usable_arr);
            if (sunk) emit(*sunk, action);
        };
        if (allow_180) { /* x kick */ }
        /* z, c kicks */
        /* l, r single-step (with usable_at_bb pre-check) */
        if (allow_LR) { /* L multi-step + drop each step */ }
        if (allow_LR) { /* R multi-step + drop each step */ }
    }
};
```

### MakePath20gVisitor

```cpp
struct MakePath20gVisitor {
    MoveGenSearch *self;
    CellsKey index_key, index_landpoint;
    bool last_rotate, landpoint_is_none;
    BBState found{};
    bool hit = false;
    bool on_pop(BBState const &) { return true; }
    bool on_admit(BBState const &s, char, bb::EnqueueDecision) {
        if (check_hit(s)) { found = s; hit = true; return false; }
        return true;
    }
};
```

## 四、行为等价性核对清单 (20g)

按 master 20g make_path 主循环 + commit 6c 位板手写实现逐项核对:

- [x] **z (ccw kick)**: pre-drop = first_passing_kick_bb(Ccw); drop 后入队 / 命中.
- [x] **c (cw kick)**: 同上, KickDir::Cw.
- [x] **x (180 kick, allow_180)**: KickDir::Opp; allow_180 false 时跳过.
- [x] **l (left)**: pre-drop = (xb-1, yb); usable_at_bb 前置守卫一致, drop 后入队.
- [x] **r (right)**: 镜像 l.
- [x] **L (allow_LR)**: 沿 -x scan, 每步 usable_at_bb + drop_bb_state; 整段
      terminal 一步入 mark; allow_LR=false 不展开.
- [x] **R (allow_LR)**: 镜像 L.
- [x] **d / D**: 20g 邻居集不含, 与原实现一致.
- [x] **rotate_move (X/Z/C)**: 20g 不消费 allow_rotate_move, 与原实现一致.
- [x] **disable_d**: 20g 不消费, 没有二阶段 while 循环, 与原实现一致.
- [x] **fast-path on entry (空路径)**: `landpoint_is_none && cells_key(s0) ==
      index_landpoint` -> [], 与原 fast-path 1:1.
- [x] **fast-path on entry (起点命中)**: `check_hit_state(s0)` -> 写 root mark
      + build_path(s0). 与原 fast-path 1:1.
- [x] **mark 表越界**: dedup 内 bbox check, Skip.
- [x] **root 入队**: parent=nullptr, action=`'\0'` -> PrevKey r=0xFF, 与
      `path_mark_set_state_root(s0, '\0')` 同效.
- [x] **末段 wall-kick 重放**: 与原 build_path 1:1, kick 后再 drop 比 cells_key,
      KickDir 顺序 Opp(if allow_180) → Ccw → Cw, 字符 'x'/'z'/'c'.
- [x] **path_mark version**: 入 BFS 前 `path_mark_.clear()`, 与 1g 同模式.
- [x] **engine queue_buffer**: 复用类成员 `node_search_path_` (与 1g 共享, 因
      1g/20g 同时只走一条路径, 无并发使用).

## 五、改动清单

修改:
- `src/movegen_search.h`:
  - 新增 `MakePath20gNeighbors / MakePath20gDedup / MakePath20gVisitor` 三件套
    (类内, 与 `Run20g*` / `MakePath1g*` 同位置).
  - `make_path_20g_native` 主循环 (line 549~695) 替换为引擎驱动
    `bb::run_bb_bfs(...)`. 删除局部 `MarkSlot / mark_idx / try_mark_set /
    backtrack` 这几个 lambda — PathMark 已等价提供.
  - 起点 fast-path 与 build_path 末段 wall-kick 重放保留, 完全保留原行为.
  - `build_path` lambda 签名简化: 不再持 `(cur_action, cur_has_pred,
    back_keys)` 参数 (PathMark 链回溯独立完成).
- `research/flip-bits/bb_bfs_engine_commit4_notes.md`: 本文.

不动:
- `src/bb_bfs_engine.h`: commit 3 后已稳定, 无需调整.
- `src/bb_state.h`: PathMark / drop_bb_state / first_passing_kick_bb 等
  已就绪.
- `Run20g*` (commit 2) 与 `MakePath1g*` (commit 3) 三件套.
- 末段 wall-kick 重放的 try_kick_chain_to_drop lambda (位于 build_path 内,
  原样保留).

## 六、验证

- 编译: `cmake --build build -j$(nproc)` 全 target 成功.
- 行为: `oracle_diff` / `path_node_diff` / `simulate_node_diff` /
  `tag_node_diff` 全部报告 `# all diffs ok`.

## 七、对后续工作的影响

至此 `MoveGenSearch` 内三处 BFS 主循环 (run_piece_20g, make_path 1g,
make_path 20g) 全部由 `bb::run_bb_bfs` 驱动, 无任何手写 FIFO 队列. 将来增加
`search_aspin` / `search_tag` 等新 search 类型时, 只需:

1. 写一份 `<NewSearch>Neighbors` (描述邻居枚举 + 是否需要 drop / kick / 多步)
2. 写一份 `<NewSearch>Dedup` (描述 mark 表语义, 三态 EnqueueDecision)
3. 写一份 `<NewSearch>Visitor` (描述命中谓词 / emit 行为)

引擎本身不再需要变动 (除非引入"全局 visit cap"这类引擎级特性). 与
`bbstate_bfs_engine_revised.md` 第三节"修订后的方案"完全吻合.
