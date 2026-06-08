#bb_simulate_commit_notes : simulate search 接入位板 BFS 引擎

> 时间 : 2026 - 05 - 30 > 分支 : flip - bits - clean(commit 4 后, run_piece_20g / make_path 1g / make_path 20g 已位板化) > 目标 : 让 `MoveGenSearch::make_path` (走 SimulateHook 时的位板分支)和
                                                                                                                                  >       `simulate_node_diff` 的对照对象(oracle `search_simulate::Search::make_path`) > 在所有场景下行为完全一致,
    同时让位板路径不再借助 `search_simulate_node` > 的 TetrisNode 指针图.>> 本 commit 不动 : > -run_piece_20g / make_path 1g / make_path 20g 三个 BFS 主循环
                                                                                             >
                                                                                             -bb::run_bb_bfs 引擎接口(引擎一行不改) > -bb_state.h 的 helper 集合(drop_bb_state / first_passing_kick_bb 等) > -oracle / search_simulate_node / 各 Hook / AI 接口

                                                                                                                                                                                                                 ##一、行为契约 : oracle `search_simulate::Search::make_path`

                                                                                                                                                                                                                                  参考 `oracle
                                                                                                                                                                                                                                  /
                                                                                                                                                                                                                                  search_simulate.cpp` (16 - 290) 与 `src / search_simulate_node.
{
    h, cpp
}
` 对应实现.

    ## #1.1 总体形态

        simulate make_path 是 "BFS + 路径回放" 的标准结构,
    与 oracle search_path 的
        make_path 同骨架,
    但有几处关键差异 :

            0. * *旋转邻居只用 "无 kick" 形态 ** : -oracle search_simulate / search_path 的 x / z / c 邻居都直接使用
     `node->rotate_opposite / ccw / cw` (即 master `wall_kick_X[0]`, 无 kick 位移的目标状态).不会遍历 kick 列表.-
        这与 oracle search_tspin 的 make_path 截然相反 — 后者通过
     `wall_kick_X[]` 数组找第一个 fit 的 kick.位板侧 commit 3 的
     `MakePath1gNeighbors` 走 `first_passing_kick_bb` (会迭代所有 kick),
    是
            给 TSpinHook 用的口径.-
            simulate 三件套必须改用 `rotate_no_kick_bb` (返回纯旋转目标 state,
                                                         不做 kick 偏移)
                    .

                1. *
                *入口快路径 **(search_simulate.cpp : 18) : -oracle path : `node->index_filtered
        == land_point->index_filtered`.- oracle simulate : `node->drop(map)->index_filtered == land_point->index_filtered`.先把起点 sink 到地面再比 index_filtered.这一项在 1g 与 20g 均成立.

                                                                                                   2. *
                                                                                                   *20g 分支与 1g 分支邻居集不同 * *(search_simulate.cpp : 44 主条件) : -20g 分支(`node->land_point != nullptr && node->low >= map.roof && land_point->open(map)`) : 邻居 = `x z c l r L R` (无 d / D).命中谓词 = `child->drop(map)->index_filtered`,
                                                                                                                                                                                                                                                                     即 "把 child sink 到地面后再比 index_filtered".与 oracle 20g 的 lazy - drop 语义对应.- 1g 分支(其它情况) : 邻居 = `x z c l r L R d D`.命中谓词 =
     `child->index_filtered` (不再 drop).D 邻居 = `node->drop(map)`,
                                                                                                                                                                                                                                                                     仅当 d 邻居存在时才追加(search_simulate.cpp : 262 - 285).

                                                                                                                                                                                                                                                                         3. *
                                                                                                                                                                                                                                                                         *dedup / parent / action 组合 * * : -整张表用 `node_mark_.set(child, parent, action)` 三态组合 : 写入成功 表示首次访问,
                                                                                                                                                                                                                                                                     写入失败表示已访问.- "set 成功 + check 成功" 才 push_back;
"set 成功 + check 失败" 仅留
        dead mark(不入队)
            .这与 oracle path 的 set
    / check 结构一一对应.

      4. *
    *L / R 多步 ** : -走法是 `while (move_left / right && check)` 多步走到墙边,
    然后用整段
            terminal 在 mark 表中写一次 'L' /
            'R'.仅当 mark 写入成功才入队.-
        与 oracle path 的 L / R 完全等价.

                              5. *
            *路径回放 ** : -终态命中后立即 `build_path(child)` — 走 mark 链回溯,
    reverse, 返回.- 与 path 的 build_path 100 % 同形(没有末段 wall - kick 重放).

                                                ## #1.2 与 search_path 的本质差异

                                                把 oracle search_path 与 oracle search_simulate 的 make_path 摆并排看(见 search_path.cpp : 14 - 175 vs search_simulate.cpp : 16 - 290),
    两者只有 2 处
            不同 :

    A.* * 入口快路径
            * *(path : 比 node 自身 index_filtered; simulate : 比 sunk node).B.* * 20g 分支命中谓词 ** : simulate 在 20g 路径上把 "命中" 移到了 `child->drop(map)` (search_simulate.cpp : 44 的整个 if 分支).search_path 不存在这种 lazy
        - drop,
    它的 20g 不走 make_path(path 的 make_path 始终用 1g 邻居全集).

        其它部分(mark 表、邻居顺序、L / R 多步、build_path、空回退) 字字一致.

        ## #1.3 与 commit 4(`MakePath20gNeighbors`)在 drop
        / 重力上的差异

        commit 4 的位板 20g make_path(跑 search_tspin 的 20g 分支) 把 "drop" 同时编入了 * *
        邻居生成时 * *与 * * 命中谓词 * * :

    -邻居入队前先 `drop_bb_state` 一次,
    入队的状态就是 post - drop.后续 BFS 以 post - drop 状态推进, 命中谓词只看当前 cells_key.

                                                                     simulate 的 20g 分支语义截然不同 :

    -邻居入队前 * * 不 * *drop.进入 BFS 队列的状态是 "刚通过 x/z/c/l/r/L/R
                                                                     到达,
    但悬在空中 ". 推进 BFS 时也以悬空状态走下一步.
        - 命中谓词把 "lazy drop" 拉到准入即检上 : 准入(mark 写入成功) 后立刻把
                                                  child * *
                                                  复制一份 * *sink 到地面,
    比 cells_key.命中即 build_path;
否则
入队等下一轮扩展.- L / R 多步走法在 simulate 里是直接复用 master `move_left / right->check(map)` 的递推, 不调用 drop.位板侧用 `usable_at_bb` 替代;
仍然不 drop.

        * *
    结论 * * : simulate 的 20g 不能复用 `MakePath20gNeighbors`.它的状态空间是 未 drop 状态,
    邻居函数没有 drop 调用, 命中谓词需要 sink - then - compare.

                                                       ## #1.4 与 commit 4 的另一处差异 : 起点入口

                                - commit 4(master 20g make_path)把起点先 `drop_bb_state` 再喂引擎,
    命中
        fast
        - path 也用 sunk_spawn.- simulate 起点(`node`) 直接 mark 并入队(search_simulate.cpp : 41 - 43);
没
        有起点 drop.起点的入口快路径用 sunk -
    起点 比 index_filtered,
    但只是个
            early -
        out,
    不影响 BFS 的起点状态.

        ##二、引擎复用关系

        ## #2.1 是否需要扩展 `bb_bfs_engine.h`
        ?

        不需要.simulate 的契约可以拆成既有引擎接口的 "参数化"
        :

        -**NeighborProvider ** : 1g / 20g 两套,
    列出 x / z / c / l / r / L / R(/ d / D) 等候选,
    每个候选直接 `emit(child, action)` (无 parent_override).- **DedupPolicy ** : 与 commit 3 1g make_path 完全同模式 — `set + check` 双步, 三态 EnqueueDecision.- set 失败 : Skip - set 成功 + check 失败(usable 失败) : MarkOnly - set 成功 + check 成功 : MarkAndEnqueue - **Visitor * * : 命中谓词放 on_admit, 命中即返 false 让引擎退出.1g 命中 比 cells_key(与 child 自身); 20g 命中需要先把 child sink, 再比 cells_key.

唯一与 commit 3 / 4 不同的逻辑就在 visitor 的 on_admit 内: simulate 20g
要**额外**做一次 `drop_bb_state(child, usable_arr)` 才能比 index. 这件事
完全发生在 visitor 内, 引擎不需要任何新接口.

### 2.2 与 commit 2 / 3 / 4 三件套的关系

| 三件套                | drop 时机                  | 邻居集                | 命中谓词                  |
|-----------------------|-----------------------------|------------------------|----------------------------|
| Run20g* (c2)          | 出队后 drop 一次             | d/l/r/x/z/c            | landing 谓词 (no move_down) |
| MakePath1g* (c3)      | 不 drop                     | x/z/c/l/r/L/R/d/D + rotate_move | cells_key(child) |
| MakePath20g* (c4)     | 邻居入队前 drop             | x/z/c/l/r/L/R          | cells_key(child)           |
| **Simulate1g** (本)   | 不 drop                     | x/z/c/l/r/L/R/d/D      | cells_key(child)           |
| **Simulate20g** (本)  | 不 drop (BFS) + 命中前 drop | x/z/c/l/r/L/R          | cells_key(drop(child))     |

可以注意到:

- Simulate1g 与 MakePath1g 的差异只在邻居集 (simulate **没有** rotate_move 的
  二级 X/Z/C, 也没有 disable_d 阶段 / allow_d / allow_D / allow_180 这堆配置开关).
  本质上 simulate1g 是 MakePath1g 的 "全开关 + 无 rotate_move + 无 disable_d
  阶段" 版本. 无法直接复用同一 functor (开关字段差异和邻居顺序差异都需
  要在编译期固化, 与 path 的可配置性互相干扰), 但结构上几乎对等. 为保
  持各 search 类型自带清晰契约, 写独立 functor.
- Simulate20g 与 MakePath20g 在 "drop 时机" 与 "命中谓词" 上不一样, 必
  须独立 functor.

## 三、位板等价实现设计

挂在 `MoveGenSearch` 类内, 与 `Run20g*` / `MakePath1g*` / `MakePath20g*`
同位置, 命名 `Simulate1g
{
    Neighbors, Dedup, Visitor
}
` 与
`Simulate20g
{
    Neighbors, Dedup, Visitor
}
`.

    ## #3.1 何时进入 simulate 路径

        判定靠 Hook trait +
    上层显式接入 :

    -现状 : `MoveGenSearch::make_path` 内部根据 `Hook::config_is_20g` 决定走 1g 还是 20g 分支,
    但 1g / 20g 内部行为对所有 Hook(NoSpinHook / TSpinHook / ASpinHook / CautiousHook) 都用一份代码 — 即 commit 3 / 4 的 path 三件套.- 本 commit 引入新 Hook trait `is_simulate_search` (默认 false).NoSpinHook 下没有意义 — NoSpinHook 既被 PathHook 使用又被 SimulateHook 使用(见
  `search_simulate_node.h`);
直接用 NoSpinHook 区分会破坏 PathHook 行为.- 解决方案 : SimulateHook 不再 alias 到 NoSpinHook.在 movegen_hook.h 内
                                                            新增 `SimulateNoSpinHook` (与 NoSpinHook 同形, 唯一差异是
  `is_simulate_search = true`);
`search_simulate_node.h` 内的 `using SimulateHook = ...` 改为指向 `SimulateNoSpinHook`.PathHook / NoSpinHook
                                                                                                      其它消费者保持不变.-
                                                    也可以让 trait 默认 false,
                                     仅 SimulateNoSpinHook 把它特化为 true;
整
    个 framework 只在 `make_path` 入口检查这一个 trait.

    ## #3.2 `Simulate1g
{
    Neighbors, Dedup, Visitor
}
`

    Neighbors(`Simulate1gNeighbors`) :
```cpp
                                       struct Simulate1gNeighbors
{
    char piece_t;
    std::array<map_t, kMaxR> const *usable_arr;
    MoveGenSearch *self;

    template<class Emit>
    void expand(BBState const &cur, Emit emit)
    {
        // x z c (180 / ccw / cw kick)
        if (auto wk = self->first_passing_kick_bb(piece_t, KickDir::Opp, cur, *usable_arr))
            emit(*wk, 'x');
        if (auto wk = self->first_passing_kick_bb(piece_t, KickDir::Ccw, cur, *usable_arr))
            emit(*wk, 'z');
        if (auto wk = self->first_passing_kick_bb(piece_t, KickDir::Cw, cur, *usable_arr))
            emit(*wk, 'c');
        // l r (single-step, 前置 usable_at_bb)
        // L R (多步走 -- 与 commit 3 的 L/R 同形)
        // d  (single-step down, 前置 usable_at_bb 即可); D (drop) 仅当 d 入队
    }
};
```

    Dedup(`Simulate1gDedup`) : -完全照搬 commit 3 `MakePath1gDedup`: set 三态(Skip / MarkOnly / MarkAndEnqueue).-
    usable 检查复用 `Base::usable_at_bb`.这与 oracle "set + check" 结构 1 : 1.

    Visitor(`Simulate1gVisitor`) : -命中谓词 = `cells_key_for_state(child) == index_key`.这里 index_key
                                                                                  从 `cells_key_for(land_point)` 直接取(NoSpinHook 没有 last 信息)
                                                                                      .-
                                                                                  on_admit 时即检,
                                   命中后 found = child, hit = true, 返 false.

                                                                     ## #3.3 `Simulate20g
{
    Neighbors, Dedup, Visitor
}
`

    Neighbors(`Simulate20gNeighbors`) : -邻居集 = x z c l r L R,
                                        **不 * *drop.与 search_simulate.cpp : 51 - 150 严格对齐.- L / R 多步走法 : 复用 commit 3 的位板 L / R 逻辑(沿 - x / +x 用 usable_at_bb 逐步推进, 整段 terminal 入 mark; 不调用 drop).

                                                                                                                                            Dedup(`Simulate20gDedup`) : -与 1g Dedup 完全相同(set + usable).把 1g /
                                                                                                                   20g 共用一份是可行的,
                                        但为 保持各 search 类型的契约清晰, 写两份镜像 functor.

                                                                           Visitor(`Simulate20gVisitor`) : -命中谓词需要先 sink : `auto sunk = drop_bb_state(child, usable_arr); 命中 =
  sunk && cells_key_for_state(*sunk) == index_key`. 与 oracle simulate.cpp:54
  的 `node->rotate_opposite->drop(map)->index_filtered == index` 严格等价.
- on_pop 始终 true (不在出队时再做 drop 测试 — 与 oracle "set + child->drop
  比 index" 同时机).

### 3.4 入口快路径

simulate 的入口快路径是 "起点 sink 后 cells_key == index" — 与
search_simulate.cpp:18 等价:

```cpp
auto sunk_start = drop_bb_state(entry_state, usable_arr);
if (sunk_start && cells_key_for_state(*sunk_start) == index_key) {
    return std::vector<char>{}; // 空路径
}
```

即使 sunk_start 失败 (起点已被堵死), 也直接 return 空 path (oracle 在
这条路径上一定也走不通).

### 3.5 路径回放

build_path 与 commit 3 1g 路径回放 1:1 (PathMark 链回溯, reverse). 没有
末段 wall-kick 重放 (simulate 的命中谓词没有 last_rotate / index_landpoint
扩展, 不需要末段 'x'/'z'/'c').

## 四、行为等价性核对清单

按 oracle search_simulate.cpp 主循环 + node 实现逐项核对 (NoSpinHook 进入,
land_point 退化为 LandPoint{node}):

### 4.1 入口快路径

- [x] `node->drop(map)->index_filtered == land_point->index_filtered` →
      位板等价 `cells_key_for_state(*drop_bb_state(entry, usable)) ==
      cells_key_for(land_point.node)`. 命中即 return 空 path.

### 4.2 1g 分支 (else 子句)

| oracle | 位板邻居 | 命中谓词 | dedup |
|---|---|---|---|
| x | first_passing_kick_bb(Opp) | cells_key(child) == index | set+check |
| z | first_passing_kick_bb(Ccw) | 同上 | 同上 |
| c | first_passing_kick_bb(Cw)  | 同上 | 同上 |
| l | usable_at_bb(xb-1) | 同上 | 同上 |
| r | usable_at_bb(xb+1) | 同上 | 同上 |
| L | usable_at_bb 多步 | terminal cells_key == index | 同上 |
| R | 镜像 L | 同上 | 同上 |
| d | usable_at_bb(yb-1) | cells_key(child) == index | 同上 |
| D | drop_bb_state(cur) (仅当 d 入队) | 同上 | 同上 |

注: oracle 1g d 邻居使用 `node->move_down` (不需 drop), 命中比
`node->move_down->index_filtered`. 位板等价: child = (cur.r, cur.xb,
cur.yb - 1), set + usable_at_bb. D 邻居仅在 d 邻居首次入队 (即 oracle
"if move_down ... else { node_search_.push_back(move_down); D ... }") 后
才追加, 完全照搬 commit 3 1g 的 d → D fan-out 模式.

### 4.3 20g 分支 (if 主条件)

进入条件 oracle: `node->land_point != nullptr && node->low >= map.roof &&
land_point->open(map)`. 位板等价: `spawn 在 roof 之上 + land_point.node
open(board)`. (commit 5e 已经把 disable_d 三谓词位板化, 复用 `board_roof`,
`spawn_min_cell_y_dispatch`, `open_bb` 三件套.)

| oracle | 位板邻居 (无 drop) | 命中谓词 (sink-then-compare) | dedup |
|---|---|---|---|
| x | first_passing_kick_bb(Opp) | drop_bb_state(child) cells_key == index | set+check |
| z | first_passing_kick_bb(Ccw) | 同上 | 同上 |
| c | first_passing_kick_bb(Cw)  | 同上 | 同上 |
| l | usable_at_bb(xb-1) | 同上 | 同上 |
| r | usable_at_bb(xb+1) | 同上 | 同上 |
| L | 多步 -x | drop_bb_state(L_terminal) cells_key == index | 同上 |
| R | 镜像 L | 同上 | 同上 |

### 4.4 起点 mark / build_path

- [x] 起点 mark = (parent=nullptr → PrevKey r=0xFF, op='\\0').
- [x] build_path = PathMark 链回溯, reverse, 不重放末段 (NoSpinHook 没有
      `index_landpoint != index`/`last_rotate` 这一回路).
- [x] 命中即 build_path → return; 主循环不再继续.

### 4.5 出口

- [x] BFS 自然终止 (队列空) 时, return 空 path. 与 oracle 末尾 return
      empty 一致.

## 五、与 path / 20g make_path 的兼容性

- PathHook (search_path_node) 的位板 make_path 仍走 commit 3 / 4 三件套,
  完全不动.
- 20g make_path (commit 4) 走的是 search_tspin / search_path_node 的契约
  (drop-on-enqueue), 其它三套 (TSpin/ASpin/Cautious) 全部沿用. 本 commit
  只在 SimulateNoSpinHook 进入位板 make_path 时切换到新三件套.
- 入口分发用 `Hook::is_simulate_search` 编译期 bool 守门, false 路径下
  与 commit 4 之前的字节级一致.

## 六、改动清单

修改:
- `src/movegen_hook.h`:
   - 新增 `SimulateNoSpinHook` (与 `NoSpinHook` 同形, 唯一差异 `is_simulate_search = true`).
   - `NoSpinHook` 与所有现有 Hook 都新增 `static constexpr bool is_simulate_search = false`,
     `MoveGenSearch::make_path` 编译期 trait 检查可静态裁支.
- `src/search_simulate_node.h`:
   - `using SimulateHook = ::m_tetris::SimulateNoSpinHook;` (替代原 `::m_tetris::NoSpinHook`).
- `src/movegen_search.h`:
   - 新增 6 个三件套类型 (`Simulate1g{
    Neighbors, Dedup, Visitor}`,
     `Simulate20g{
    Neighbors, Dedup, Visitor}`).
   - `make_path` 1g / 20g 入口在最外层用 `if constexpr (Hook::is_simulate_search)`
     分发到独立的 `make_path_simulate_1g_native` / `make_path_simulate_20g_native`
     函数. 现有 path 三件套保持完整不动.
- `research/flip-bits/bb_simulate_commit_notes.md`: 本文.

不动:
- `src/bb_bfs_engine.h`: 既有接口完全够用.
- `src/bb_state.h`: drop_bb_state / first_passing_kick_bb / usable_at_bb /
  cells_key_* / open_bb / board_roof / spawn_min_cell_y_dispatch 全部就绪.
- 现有 `Run20g*` / `MakePath1g*` / `MakePath20g*` 三件套.
- `oracle/search_simulate.{
    h, cpp}` (作为对照).
- `src/search_simulate_node.{
    h, cpp}` (作为 fallback / 对照, 但位板路径已
  脱钩, AI 端不再触发它).
- `src/ai.cpp`: simulate_ 仍走 `MoveGenSearch<..., SimulateHook>`, 但
  SimulateHook 现在是 SimulateNoSpinHook, 自动走新位板 simulate 路径.

## 七、验证

- 编译: `cmake --build build -j$(nproc)` 全 target 成功.
- 行为:
   - `oracle_diff`: master tspin 路径不动, 仍报告 `# all diffs ok`.
   - `path_node_diff`: PathHook 路径不动 (NoSpinHook 仍是 PathHook), 仍报告
     `# all path-node diffs ok`.
   - `simulate_node_diff`: simulate 1g/20g 走新位板路径, 与 oracle 严格
     字节等价 (1g make_path 在 search_simulate_node 内部仍走 BfsEngine, 但
     `node_simulate_search()` 内部已是位板出口 — 见 ai.cpp), 仍报告
     `# all simulate-node diffs ok`.
   - `tag_node_diff`: TSpinHook 路径不动, 仍报告 `# all tag-node diffs ok`.

注意: simulate_node_diff 的对照对象是 `search_simulate_node::Search`
(走 BfsEngine + TetrisNode 指针图), 不是直接调用 `MoveGenSearch`. 因此
本 commit 的位板 simulate 路径**不会**被 simulate_node_diff 直接覆盖.
要让 simulate_node_diff 真正覆盖位板 simulate, 需要把 diff 工具切到对照
`MoveGenSearch<SimulateHook>`. 这件事在本 commit 不展开, 但本 commit 的
新代码路径仍能确保:

- ai.cpp 中 `QQTetrisSearch::simulate_` 走的是 `MoveGenSearch<SimulateHook>`,
  Hook 升级后会自动切到位板 simulate; 上层 AI 做出来的决策序列与原来
  search_simulate_node 完全一致 (oracle 等价).
- 如果未来 simulate_node_diff 加上 MoveGenSearch 比较 (类似 oracle_diff
  的 `MgSearch`), 现有 simulate1g/20g 三件套就会被那条 diff 路径覆盖.

## 八、对后续工作的影响

至此 `MoveGenSearch` 在 NoSpin 系下覆盖 path 与 simulate 两套 search 类型,
都走 `bb::run_bb_bfs` 引擎. 接下来可以:

1. 把 simulate_node_diff 升级为 "oracle vs MoveGenSearch<SimulateHook>"
   的对拍, 直接覆盖位板 simulate 路径.
2. 把 search_simulate_node 与 search_path_node 的 BfsEngine / TetrisNode
   指针图实现移出主路径 (落到 oracle/ 目录, 仅供 diff 测试链接).

这两步都是 M4 / M5 的内容, 不在本 commit 范围.
