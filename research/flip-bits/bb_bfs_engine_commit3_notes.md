# Commit 3 笔记: make_path 1g 接入位板 BFS 引擎

> 时间: 2026-05-30
> 分支: flip-bits-clean (commit 2: run_piece_20g 已切引擎)
> 目标: 把 `MoveGenSearch::make_path` 在 **非 20g** 路径下手写的 BFS 主循环
>       (`movegen_search.h:217-551`) 改造为 `bb::run_bb_bfs(...)` 驱动, 与
>       `Run20g*` 三件套共用引擎; 行为 (路径输出 byte-for-byte) 与之前完全等价.
> 不在范围: `make_path_20g_native` (commit 4); `run_piece_*` (已落地或非本期).

## 一、make_path 1g 现有 BFS 的状态/邻居/去重契约

代码位置: `src/movegen_search.h::MoveGenSearch::make_path` 中 `Hook::config_is_20g` 为
false 的整段 (大约 line 230-551).

### 1.1 状态 (State)

- `bb::BBState { uint8_t t, r; int8_t xb, yb }`. **入队前不 drop** (1g 主循环对
  普通邻居走的是单步坐标增量, 不像 20g 那样 sink); 仅 `D` (硬下落) 一条邻居在
  入队前调用 `drop_bb_state`.

### 1.2 起点 (entry)

- `entry_state = state_from_node(node)` (master `node` 已经过 `node->check(map)`
  guard, 不需要再 drop).
- 起点写 PathMark root: `path_mark_set_state_root(entry_state, '\0')` —
  PrevKey r=0xFF 表示无前驱, action 字符 `\0` (build_path 通过 PrevKey.has() 截断).
- BFS 前还有两个 fast-path:
  1. `Hook::is_landpoint_none(land_point) && cells_key_for(node) ==
     cells_key_for(land_point.node)` — 整体空路径 (起点已经在目标 cells 上).
  2. `index_filtered_eq_state(entry_state, index) ||
     (!is_landpoint_none && cells_key_for_state(entry_state) == index_landpoint &&
      Hook::config_last_rotate(config_))` — entry 即命中, 直接 build_path(entry).

### 1.3 邻居顺序 (master `search_tspin.cpp:155~440` 严格对齐)

每次出队 cur, **不 drop**, 按下表生成邻居 (顺序与 master `search` 1g 分支
逐字母对齐):

| 顺序 | 操作 | 邻居生成 | 准入语义 | 备注 |
|---|---|---|---|---|
| 0  | `D` (disable_d 阶段)         | `drop_bb_state(cur)` | set      | 仅在 disable_d=true 阶段生成且**只生成 D**. |
| 1  | `x` 180-kick                 | `first_passing_kick_bb(t, Opp, cur)` | set | 仅 allow_180. |
| 2  | `z` ccw-kick                 | `first_passing_kick_bb(t, Ccw, cur)` | set | |
| 3  | `c` cw-kick                  | `first_passing_kick_bb(t, Cw, cur)`  | set | |
| 4  | `l` 单步左移                  | `(cur.r, cur.xb-1, cur.yb)` | set | 命中后仅当 set 成功才尝试 X/Z/C (rotate_move). |
| 4a | `X` (rotate_move + allow_180) | `rotate_no_kick_bb(Opp, nL_state)` | set | parent = `nL_state`, **不是** cur; action='X'. |
| 4b | `Z` (rotate_move)             | `rotate_no_kick_bb(Ccw, nL_state)` | set | parent = nL_state; action='Z'. |
| 4c | `C` (rotate_move)             | `rotate_no_kick_bb(Cw, nL_state)`  | set | parent = nL_state; action='C'. |
| 5  | `r` 单步右移                  | `(cur.r, cur.xb+1, cur.yb)` | set | 同 'l' 一样支持 X/Z/C, parent = nR_state. |
| 6  | `L` (allow_LR)               | 沿 -x 一直走到边界 | set    | 不 drop, 直接整段 1 步入 mark. |
| 7  | `R` (allow_LR)               | 沿 +x 一直走到边界 | set    | 同 L. |
| 8  | `d` 单步下落 (allow_d)        | `(cur.r, cur.xb, cur.yb-1)` | set | 命中后才尝试 D. |
| 8a | `D` (allow_D 且 allow_d)      | `drop_bb_state(cur)` | set | 与 d 紧耦合 (master `if (move_down && set... && check) { push_d; if (allow_D) push_D; }`). |
| 9  | `D` (allow_D 且 !allow_d)     | `drop_bb_state(cur)` | set | 仅当 allow_d=false 时单独走该分支. |

注: 以上所有 (x, z, c, l, r, L, R, d, D, X, Z, C) **入队前都不 drop**, 与 20g
邻居入队前 drop 完全相反. 这是 1g 与 20g 的核心差异.

### 1.4 准入策略 (DedupPolicy) — set vs MarkOnly

mark 用 `PathMark` 已位板化 (cell-version key, `(r, xb, yb)` 三元组), 同 master
`node_mark_` 单 piece 内 `node->index` 一一对应:

- **set** 语义 (1g 全部邻居都用): `set_bbox` 先 bounds check → version 检查
  (cell_ver_ != version_ 即首次到达) → 写 `cell_prev_ / cell_op_` 并 bump
  cell_ver_. 首次到达返回 true; 否则 false.
- master 1g `make_path` 同样只用 set (不存在 cover_if 升级).

#### `cover_if` → `MarkOnly` 的映射证明

1g `make_path` 中 master 的固定模式:

```cpp
if (node->move_left && node_mark_.set(node->move_left, node, 'l')
    && node->move_left->check(map)) { push + rotate_move; }
```

`set` 在前, `check` 在后, 短路 `&&`. 当 `set` 返回 true (首次到达)、`check`
返回 false (碰撞) 时: **mark 已被写入, 节点不入队** — 即 `MarkOnly`.

bitboard 的 `usable_at_bb` 等价于 master 的 `move_left && check(map)`
(bbox 内且 board 不冲突), 现行实现以 `usable_at_bb && set` 顺序写入, 把
master 的 "设置 mark 但不入队" 完全消除. 数学上证明等价 (路径输出不变):

- mark 表的 key = (r, xb, yb). usable_at_bb 是 (r, xb, yb) 的纯函数, 与 BFS
  路径无关. 一个坐标若 usable 失败, 任意路径到达都失败.
- 因此一个 "dead slot" 在 master 中即使被预占, 也不影响其它路径 (它们到达同
  slot 都会被 `check` 拒绝, 最多多写一次浪费; bitboard 中跳过该写入).
- target 必为 usable (否则不构成合法 land_point), 所以 target 永远不是 dead
  slot, 路径回放也永远不会经过 dead slot.

引擎接入时为了"真实用上 MarkOnly"(任务硬约束), 我们重新排序 dedup 内部:
**先 set, 再 usable** —

| 情形 | 返回 |
|---|---|
| 越界 (xb/yb 不在 [0, k) 或 r >= kMaxR) | `Skip` |
| `set_bbox` 返回 false (已写入) | `Skip` |
| `set_bbox` 成功且 usable 失败 | `MarkOnly` |
| `set_bbox` 成功且 usable 通过 | `MarkAndEnqueue` |

行为差异与原 bitboard 相比: 多写若干 dead slot 的 mark (无人读), 同 master 一致
(master 也会写). 路径输出不变.

### 1.5 hit 检测时机

master 把 hit 检测**内联在 push 之前**:

```cpp
if (set && check) {
    if (index_filtered_eq) return build_path;
    push;
}
```

这意味着 target 出现的瞬间立即 build_path, 不入队. bitboard 重写时把 hit 检测
移到 **Visitor::on_pop** (即 target 入队 → 弹出时检测), 路径回放结果不变 (target
的 mark 已在入队时写好), 仅多走一次 pop.

例外: entry_state 作为 fast-path 仍在引擎之外检测, 与原代码一致 (避免 root 被
push 后再出队的多余开销; 也避免引擎主循环因为起点即命中而多绕一圈).

### 1.6 disable_d 二阶段循环

外层 `while (true)` 控制 disable_d 阶段切换:

```cpp
disable_d = (initial 三谓词 spawn_low/roof/open_bb);
while (true) {
    path_mark_.clear();
    BFS(disable_d);
    if (disable_d) disable_d = false;  // 第二轮
    else break;
}
```

引擎不感知此机制, 由 `make_path` 外层驱动两轮; 每轮:
- 重置 PathMark (清 dedup);
- 各自构造 NeighborProvider (内部 disable_d 标志固定);
- 共用 entry_state 与 build_path lambda.

### 1.7 路径回放 (build_path)

对找到的终态 cur (entry / target / kick chain 起点), 回溯:

```cpp
while (true) {
    auto [prev, op] = path_mark_.get_bbox(cur.r, cur.xb, cur.yb);
    if (!prev.has()) break;
    path.push_back(op);
    cur = prev;
}
std::reverse(path.begin(), path.end());
```

末段 wall-kick 重放 (target_cells != index_landpoint 时): 调
`try_kick_chain_to(piece_t, KickDir::Opp/Ccw/Cw, last_state, ...)` 依次试 'x'/'z'/'c',
首次成功即 push 字符, return. 这部分不属于 BFS 主循环, 维持原样.

回放方案: 所有 dedup 写入都使用 PathMark.set_bbox (BFS 入队/MarkOnly 都写); BFS
结束后通过 PathMark.get_bbox 完整回溯. 与原实现 byte-for-byte 等价.

## 二、引擎接口的小幅扩展

为了在 NeighborProvider 内表达 "rotate_move 的 X/Z/C 以中间状态 nL/nR 为 parent",
emit lambda 需要支持显式传入 parent 覆盖, 并把 dedup 决策回传给 NeighborProvider
(决定是否进一步 fan out 二级 children). 改动 `bb_bfs_engine.h`:

```cpp
auto emit = [&](State const &child, char action,
                State const *parent_override = nullptr) -> EnqueueDecision
{
    State const *parent = parent_override ? parent_override : &cur;
    EnqueueDecision d = dedup.try_admit(child, parent, action);
    if (d == EnqueueDecision::MarkAndEnqueue)
        queue_buffer.push_back(child);
    return d;
};
```

向后兼容: commit 2 的 `Run20gNeighbors::expand` 调用 `emit(n, action)` 不变
(新参数有默认值), 返回值忽略亦可. 该扩展是引擎接口的合理增强, 与 commit 2 的设计
方向一致 (引擎只透传 dedup 决策).

## 三、三件套设计

代码挂在 `MoveGenSearch` 类内 (与 `Run20gNeighbors/Dedup/Visitor` 同侧, 享受
piece 模板 + private 静态助手), 命名 `MakePath1gNeighbors / MakePath1gDedup /
MakePath1gVisitor`.

### MakePath1gDedup

```cpp
struct MakePath1gDedup {
    PathMark *path_mark;
    std::array<map_t, kMaxR> const *usable_arr;

    EnqueueDecision try_admit(BBState const &s, BBState const *parent, char action) {
        if (s.xb < 0 || s.xb >= kW || s.yb < 0 || s.yb >= kH || s.r >= kMaxR)
            return Skip;
        PathMark::PrevKey pk;
        if (parent == nullptr) {
            pk = {0xFFu, 0, 0};  //root sentinel.
        } else {
            pk = {static_cast<uint8_t>(parent->r),
                  static_cast<int8_t>(parent->xb),
                  static_cast<int8_t>(parent->yb)};
        }
        if (!path_mark->set_bbox(s.r, s.xb, s.yb, pk, action))
            return Skip;
        if (!Helpers::usable_at_bb(s.r, s.xb, s.yb, *usable_arr))
            return MarkOnly;
        return MarkAndEnqueue;
    }
};
```

### MakePath1gNeighbors

```cpp
struct MakePath1gNeighbors {
    char piece_t;
    bool allow_180;
    bool allow_LR;
    bool allow_D;
    bool allow_d;
    bool allow_rotate_move;
    bool disable_d;
    std::array<map_t, kMaxR> const *usable_arr;

    template<class Emit>
    void expand(BBState const &cur, Emit emit) {
        if (disable_d) {
            //仅生成 D. 即便不命中, set 写 mark, 但 drop 后 yb 已到底,
            //  下一次 expand 自身会被 PathMark::set_bbox 同 slot 拒绝 (slot 已写),
            //  BFS 自动收敛. 与 master 'disable_d' 一阶段单步 D 的行为等价.
            auto D = drop_bb_state(cur, *usable_arr);
            if (D) emit(*D, 'D');
            return;
        }
        if (allow_180) {
            auto wk = first_passing_kick_bb(piece_t, KickDir::Opp, cur, *usable_arr);
            if (wk) emit(*wk, 'x');
        }
        { auto wk = first_passing_kick_bb(piece_t, KickDir::Ccw, cur, *usable_arr);
          if (wk) emit(*wk, 'z'); }
        { auto wk = first_passing_kick_bb(piece_t, KickDir::Cw, cur, *usable_arr);
          if (wk) emit(*wk, 'c'); }
        // l + 可选的 X/Z/C
        {
            BBState nL = cur; nL.xb--;
            EnqueueDecision d = emit(nL, 'l');
            if (d == MarkAndEnqueue && allow_rotate_move) {
                if (allow_180) {
                    auto rn = rotate_no_kick_bb(piece_t, KickDir::Opp, nL);
                    if (rn) emit(*rn, 'X', &nL);
                }
                { auto rn = rotate_no_kick_bb(piece_t, KickDir::Ccw, nL);
                  if (rn) emit(*rn, 'Z', &nL); }
                { auto rn = rotate_no_kick_bb(piece_t, KickDir::Cw, nL);
                  if (rn) emit(*rn, 'C', &nL); }
            }
        }
        // r + 可选 X/Z/C (同上, 镜像)
        ...
        if (allow_LR) {
            // L/R 多步沿边界滑, 一步入 mark.
            ...
        }
        if (allow_d) {
            BBState nd = cur; nd.yb--;
            EnqueueDecision d = emit(nd, 'd');
            if (d == MarkAndEnqueue && allow_D) {
                auto D = drop_bb_state(cur, *usable_arr);
                if (D) emit(*D, 'D');
            }
        } else if (allow_D) {
            auto D = drop_bb_state(cur, *usable_arr);
            if (D) emit(*D, 'D');
        }
    }
};
```

注意 X/Z/C 子邻居以 `&nL` (中间状态) 作为 parent 传入 emit, 这样 dedup
里 PathMark 写入的 PrevKey 指向 nL 而非 cur — 与 master 完全一致.

### MakePath1gVisitor

```cpp
struct MakePath1gVisitor {
    BBState const *target_index;       //命中谓词 (CellsKey)
    CellsKey index_key;
    CellsKey index_landpoint_key;
    bool last_rotate;
    bool landpoint_is_none;
    BBState found{};
    bool hit = false;

    bool on_pop(BBState const &s) {
        CellsKey k = cells_key_for_state(s);
        if (k == index_key) { found = s; hit = true; return false; }
        if (!landpoint_is_none && k == index_landpoint_key && last_rotate) {
            found = s; hit = true; return false;
        }
        return true;
    }
};
```

发现命中后返回 false 让引擎立即停; `make_path` 检测 `hit`, 调用 build_path(found).

## 四、行为等价性核对清单 (1g)

按 master `search_tspin.cpp` 1g `make_path` 主循环逐项核对:

- [x] **z (ccw kick)**: 同字符 `'z'`, set 语义.
- [x] **c (cw kick)**: 同字符 `'c'`, set.
- [x] **x (180 kick, allow_180)**: set.
- [x] **l (left)**: set; 命中 set 后才允许 rotate_move 二级展开.
- [x] **r (right)**: set; 同上.
- [x] **L (allow_LR)**: set, 整段一步入 mark.
- [x] **R (allow_LR)**: 同 L.
- [x] **d (allow_d)**: set; 命中 set 后允许 D 嵌套.
- [x] **D (allow_D)**: set; allow_d 嵌套场景与单独场景两条分支均覆盖.
- [x] **disable_d 阶段**: 仅 D, 路径在 path_mark 中标记后自动收敛 (BFS 第二轮
      靠外层 while 控制).
- [x] **rotate_move (X/Z/C)**: parent 改为 nL/nR, action 大写, 仅在 nL/nR 入队
      成功 (`emit` 返 MarkAndEnqueue) 后展开.
- [x] **MarkOnly**: dedup 内 `set_bbox` 成功且 `usable_at_bb` 失败时返回
      MarkOnly. 该分支输出与原 bitboard 等价 (target 必 usable, 不影响路径).
- [x] **fast-path on entry**: `is_landpoint_none && cells == land_point.node cells`
      → 空路径; entry 命中谓词 → build_path(entry). 两者保持在引擎之外.
- [x] **末段 wall-kick 重放 (build_path 内)**: 与原实现完全相同 (try_kick_chain_to
      序列). 不在引擎中.
- [x] **path_mark version**: 每次进入 BFS 阶段 (含 disable_d 切换) 调
      `path_mark_.clear()`, version 递增, 等价原行为.

## 五、改动清单

修改:
- `src/bb_bfs_engine.h`:
  - emit lambda 增加 `parent_override` 默认参数 (向后兼容 commit 2).
  - emit lambda 改为返回 `EnqueueDecision` (供 NeighborProvider 决定是否 fan
    out 二级 children).
  - `Run20gNeighbors::expand` 调用形式不变 (新增的返回值与可选参数都向后兼容).
- `src/movegen_search.h`:
  - 新增 `MakePath1gNeighbors / MakePath1gDedup / MakePath1gVisitor` 三件套
    (类内, 与 `Run20g*` 同位置).
  - `make_path` 1g 主循环 (line 327-549) 替换为引擎驱动: `bb::run_bb_bfs(...)`
    嵌套在 disable_d 二阶段 while 之内.
  - 保留 fast-path on entry 与 build_path 末段 wall-kick (与原实现 1:1 等价).
- `research/flip-bits/bb_bfs_engine_commit3_notes.md`: 本文.

不动:
- `make_path_20g_native` (commit 4 的事).
- `run_piece_20g` / `run_piece` (commit 2 已落地或非本期).
- `bb::Helpers` (commit 1 已稳定).
- `path_mark_` 与 `node_search_path_` 类成员 (后者 commit 4 后再决定是否复用).

## 六、验证

- 编译: `cmake --build build -j$(nproc)` 全 target 成功 (oracle_diff /
  perft_movegen / tetris_ai / extreme_rule_diff / *_diff).
- 行为: `./build/oracle_diff` 报告 `# all diffs ok` (1g + 20g 全 piece × 全场地).
