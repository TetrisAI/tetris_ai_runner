# idx=1262 O/rand_000 tuck 漏吐 — 在哪个 search 里、走到哪条边丢的

> 关键字: idx=1262, O piece, rand_000, tuck, oracle search_path fast-path, !open
>          BFS lateral edge filter

---

## 1. LpKey 解码

```
idx=1262 spin=0 last=-1 r=0 x=5 y=7 open=0
```

- piece = O (rotation R0 唯一占位 → r=0)
- 落点几何 (lateral=5, row=7, supported, 完全悬浮的 tuck)
- 同一棋面下其它 5 个 candidate-only 见 phase_a_truth.md §2

## 2. 棋面 (rand_000, row 0 在底)

```
y=12 ..........
y=11 ..........
y=10 ......#...
y= 9 ..#....#..
y= 8 ....#.....
y= 7 ....#.....
y= 6 #.........
y= 5 ..#...#...
y= 4 ..........
y= 3 ...#.#....
y= 2 ....#.....
y= 1 ..#...#...
y= 0 ..........
```

map.roof = 11 (列 6 在 y=10 有 #).

## 3. 谁吃 / 谁丢

| 驱动           | candidate 端                                    | oracle 端                | idx=1262 是否在 candidate-only |
|----------------|--------------------------------------------------|--------------------------|---------|
| `path_diff`    | `Searcher<PathStrategy, CautiousHook>`           | `oracle/search_path`     | **是**  |
| `simulate_diff`| `Searcher<SimulateStrategy, CautiousHook>`       | `oracle/search_simulate` | **是**  |
| `tspin_diff`   | `Searcher<PathStrategy, TSpinHook>`              | `oracle/search_tspin`    | **是**  |
| `aspin_diff`   | `Searcher<PathStrategy, ASpinHook>`              | `oracle/search_aspin`    | **是**  |
| `simple_diff`  | 双位板对拍 (PathStrategy vs SimpleStrategy)      | (无 master oracle)       | **是**(双位板都吐, 仅 INFO) |
| `cautious_diff`| `Searcher<PathStrategy, CautiousHook>`           | `oracle/search_cautious` | 否 (cautious oracle 全 BFS 无 fast-path) |
| `tag_diff`     | `Searcher<TagStrategy, TSpinHook>`               | `oracle/search_tag`      | 否 (TagStrategy 1g 不输出 O 单 r 维) |

> **真实漏吐的 search**: `oracle/search_path` (路径基础 BFS), 它被 search_tspin /
> search_simulate / search_aspin 直接复用作为底座 BFS, 故 4 个驱动同时观测到.

## 4. 漏吐的具体边 (oracle/search_path.cpp)

触发条件 (`oracle/search_path.cpp:187`):

```cpp
if (node->land_point != nullptr && node->low >= map.roof)
{
    // ↑ O piece spawn 节点 land_point 非空, low=18 ≥ map.roof=11 ⇒ 进入 fast-path
    for (auto cit = node->land_point->begin(); ...)
        node_search_.push_back((*cit)->drop(map));
    // 9 个 lateral hard-drop 入队 (oracle=9)
    ...
    // tuck-by-2 启发 (line 198-226)
    ...
    // BFS 主循环 (line 227-270)
    do {
        for (...; cache_index < max_index; ++cache_index) {
            node = node_search_[cache_index];
            if (!node->open(map) && (!node->move_down || !node->move_down->check(map)))
                land_point_cache_.push_back(node);
            // l 边
            if (node->move_left && node_mark_.mark(node->move_left)
                && !node->move_left->open(map)        // ←★ 这里把 (4,8) → (5,8) 的对偶 'r' 邻居挡掉
                && node->move_left->check(map))
                node_search_.push_back(node->move_left);
            // r 边
            if (node->move_right && ... && !node->move_right->open(map) ...)
                node_search_.push_back(node->move_right);  // ←★ 行级类似, 挡 (5,8)
            ...
        }
    } while (...);
}
```

### 4.1 (5,7) 漏吐的精确边

- spawn 后 fast-path 注入了 lateral=4 的 hard-drop 节点 ≈ `(4, 8)` (列 4 在 y=8 有 #, hard-drop 落在它上方)
- BFS 主循环从 `(4, 8)` 准备 'r' 邻居 → `(5, 8)`
- 检查 `(5, 8)` 的 `open(map)`:
  - O piece bottom=(5..6, 8..9), 列 5 top=9 (列 5 y=8 是 #), 列 6 top=11 (列 6 y=10 是 #)
  - piece 占位高度 (8,8) 都低于两列 top (9,11) ⇒ piece 完全没贴到任何支撑 ⇒ `open(map) = 1`
- BFS 边条件 `!move_right->open(map)` = `!1` = false ⇒ **`(5, 8)` 不入队**
- BFS 永远走不到 `(5, 8)`, 自然下不到 `(5, 7)`
- → idx=1262 在 oracle 集合里缺席

### 4.2 候选位板侧为什么吃到

`src/search_path.h::PathStrategy::run_piece` 调
`movegen::MoveGen::generate(...)`. 后者对 r=0 的 `usable_arr[0]` 位板做完
reachability BFS, **没有任何 `open(map)` 过滤**, 只看 `usable_at_bb` (该
piece bbox 是否在 board 内合法). `(5, 8)` 是 piece bbox 完全空, 位板合
法 ⇒ 入队 ⇒ 走 'd' 到 `(5, 7)` ⇒ landable[(5,7)] = 1 (列 5 的下一格
y=7 piece 已经触底? 实际 (5,7) supported by 列 5 y=8 # — open=0 报告
correct) ⇒ candidate 入 landings.

## 5. 一句话答你的问题

> "idx=1262 是具体哪个 search、在触发 search 时丢落点了?"

**oracle/search_path::Search::search() 漏吐**. 命中触发条件
`node->land_point != nullptr && node->low >= map.roof` 进 fast-path 后,
BFS 主循环里 'r' 横移边的 `!move_right->open(map)` 过滤把
`(r=0, x=5, y=8)` 这个空中中转节点直接 reject, 导致后续无法走 'd' 到
达 (5, 7). 这是 oracle 整个 search_path / search_tspin / search_simulate /
search_aspin 共享的同一个 fast-path BFS, 所以 4 个对拍驱动 (path / tspin /
simulate / aspin) 同时观测到 idx=1262 在 candidate-only.

cautious_diff 不命中是因为 `oracle/search_cautious` 自身根本没这条
fast-path, 全程跑完整 BFS, 与位板等价. tag_diff 不命中是因为 TagStrategy
1g 输出维度受 17 个 xfail 影响, 这个具体 key 不在覆盖集合内.
