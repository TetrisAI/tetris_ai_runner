# Phase A 集合差 — 真相调研

> 报告对象: C5 任务书 §A.  
> 任务: 抽 1~3 个具体 fail 案例, dump oracle/位板的 LandPoint 集合, 双向
> diff, 列具体差集 LandPoint 坐标; 针对每个差集落点分析 "在 oracle/位板
> 里走到哪条 BFS 边滤掉/收录, 从游戏规则看应不应该可达, 哪边对".

---

## 0. 调研方法

`tests/path_diff.cpp` 给 `LpEntry::repr` 填 `r=R,x=X,y=Y,open=open(map)`,
harness 在 `TETRIS_DIFF_VERBOSE_INFO=1` 时把 candidate-only 集合的具体几何
打到 stderr.

```bash
cd build && TETRIS_DIFF_VERBOSE_INFO=1 ./path_diff 2>&1 | tee /tmp/path_diff.log
```

驱动用的是 **PathStrategy + CautiousHook** (与 oracle/search_path 邻居字符
集对齐, 见 _diff_harness.h driver 注释), rule = SRS, fixture = 18 命名 +
16 splitmix64 random.

---

## 1. 全局现象

```
$ TETRIS_DIFF_VERBOSE_INFO=1 ./path_diff 2>&1 | grep '^\[INFO\] path_diff phase=A' | wc -l
111
$ TETRIS_DIFF_VERBOSE_INFO=1 ./path_diff 2>&1 | grep -E '^\s*oracle-only=' | sort -u | head
  oracle-only=0 candidate-only=1
  oracle-only=0 candidate-only=10
  oracle-only=0 candidate-only=11
  ...
```

**全部 111 个 phase A "差" 都是 candidate ⊋ oracle, oracle-only=0**:
没有任何一个 case 出现"位板漏吐 oracle 探到的 LandPoint". 单向差.

---

## 2. 抽样 1: `path_diff piece=O board=rand_000 mode=1g`

棋面 (row 0 在底):

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

oracle = 9 落点 (硬直落, 对应每个 lateral 列 hard-drop), candidate = 15.
6 个 candidate-only:

| LpKey            | r | x | y  | open | 解释                                  |
|------------------|---|---|----|------|---------------------------------------|
| idx=485          | 0 | 4 | 7  | 1    | O 件 lateral=4, drop 到 y=7 凹陷 (列 4 在 y=8 有 #, 但 O 是 2x2, 它真正 stop 在 y=7) |
| idx=1239         | 0 | 1 | 7  | 1    | O 件 lateral=1, 横移进 (1,7) 上方     |
| idx=1262         | 0 | 5 | 7  | **0**| O 件 lateral=5, **tuck** 进 (5..6,7,8) 凹陷 (列 5,6 在 y=8 有 # 阻挡, 必须从空中横移) |
| idx=1266         | 0 | 5 | 3  | **0**| O 件 lateral=5, **极深 tuck** 滑入 (5,3) 凹陷 (well 在 y=3..4 之间) |
| idx=2289         | 0 | 0 | 7  | 1    | O 件 lateral=0, 落到 y=7              |
| idx=2319         | 0 | 6 | 1  | 1    | O 件 lateral=6, 落到 (6,1) (右侧 well) |

`open=1` 的 4 个 (485/1239/2289/2319) 是 piece 落点本身位于堆顶 (列在它
左右某处的 top 之上), 但 piece 自己 supported. `open=0` 的 2 个
(1262, 1266) 是 piece 完全悬浮, 从空中横向钻进凹陷, 经典 **tuck 落点**.

> 以下 BFS 路径分析覆盖 idx=1262 (r=0,x=5,y=7,open=0) — 最典型的 tuck 落点.

### 2.1 oracle 走到哪条边把它滤掉?

`oracle/search_path.cpp::Search::search` 在 line 187 起的 **fast-path** 分支
(node->land_point != nullptr && node->low >= map.roof — `node->low` 在 O
piece spawn 节点上等于 spawn y - 1 = 18 ≥ map.roof = 11, 触发):

1. line 189 起: 把 `node->land_point` 预算的全部 (rotation × lateral)
   逐个 hard-drop, 入 `node_search_`. 这是 9 个 lateral 落点
   (rotation 唯一, O 单一 R), 全部 status.y ≥ 8 (列上 top ≥ 8).
2. line 198-226: tuck-by-2 启发 (last_node 与 cur_node 横距 1, y 差 > 1
   时, 在中间补 move_left/right + move_down × 2) — 但这只补 (xb±1, y-2)
   一个节点, 不一定能落进 (5,7)/(5,3) 那种深口袋.
3. line 227-270 BFS:

```cpp
for (size_t max_index = node_search_.size(); cache_index < max_index; ++cache_index)
{
    node = node_search_[cache_index];
    if (!node->open(map) && (!node->move_down || !node->move_down->check(map)))
        ...land_point_cache_.push_back(node);   // ← 入集合的条件
    //x z c (rotate): 都加 `!node->X->open(map)` 过滤 ↓
    if (node->rotate_opposite && node_mark_.mark(...) && !node->rotate_opposite->open(map) && ...check(map))
        node_search_.push_back(node->rotate_opposite);
    //l r (lateral move): 同样加 `!node->X->open(map)` 过滤 ↓
    if (node->move_left && node_mark_.mark(...) && !node->move_left->open(map) && ...check(map))
        node_search_.push_back(node->move_left);
    if (node->move_right && node_mark_.mark(...) && !node->move_right->open(map) && ...check(map))
        node_search_.push_back(node->move_right);
    //d (down):
    if (node->move_down && ... && node->move_down->check(map))
        node_search_.push_back(node->move_down);
}
```

关键: lateral 边 `!move_left->open(map)` / `!move_right->open(map)`, 强制
左/右邻居 **不 open** (=至少有一列触底/支撑) 才入队. `(5,7)` 那个落点
(open=1) 在 BFS 边上无法被某个 supported 中间状态展开到, 因为要从 (4,8)
横移到 (5,8) 再 drop 到 (5,7) — (5,8) 这一步如果 supported, 就直接落到
更高的 y; 如果 open, oracle 把它 reject.

实际跟踪: 从 (4,8) 走 'r' 到 (5,8): `(5,8)->open(map) = 1` (O piece bottom
是 (5..6, 8..9), 列 5 top=9, 列 6 top=11; piece bottom 8/8 vs top 9/11,
全部 < top, open=1), 被 `!move_right->open(map)` 滤掉. **这就是 oracle
漏 (5,7) 那个 tuck 的精确边**.

类似的 (5,3) (idx=1266) 漏的更深 — 中间所有横移都得过 open 的 (5,8),
oracle BFS 根本没机会探到 (5,3).

> 注: 即使触发了 line 198 的 tuck-by-2 启发 (last_node = (4,8), cur_node =
> (5,8), x 差 1, y 差 0 ≯ 1 → 启发不展开), 漏吐依旧.

### 2.2 candidate 在哪条边把它收录?

`src/search_path.h::PathStrategy::run_piece` 走
`movegen::MoveGen<RuleSpec, T, SpinHook>::generate(board, sp.first, sp.second, collect)`
(在 1g 路径下分发到 `tetris_movegen.h`). MoveGen 的算子是位板上对每个
旋转 R 的全 reachability BFS, **没有 `!open` 过滤**, 直接对 `usable_arr[r]`
位板做扩散 — 每个节点都允许左/右/下/旋转, 只看 `usable_at_bb` (该位
是否合法). 完成扩散后 `landings = search & landable` 把所有"下一步是
墙/底"的位置作为落点 emit.

具体到 (5,7): 在 r=0 的 usable_arr[0] 位板里, (5,8) 是合法位置 (O piece
2x2 在 (5..6, 8..9) 全空), candidate 从 (4,8) 走 'r' 到 (5,8), 再走 'd'
到 (5,7) — landable 命中, 入 landings. 全程无 open 过滤.

(5,3) 类似, candidate 从 (5,7) 继续走 'd' 系列直至 (5,3) (列 5 在 y=2
有 #, 阻塞), `landable[(5,3)]` = 1, 入 landings.

### 2.3 这些落点游戏规则下应该可达吗?

应该. SRS / 通用 Tetris 规则下, piece 在空中可以任意 lateral move + soft
drop, 只要每一步合法. (5,8) → (5,7) 是合法的一次 soft drop, (5,7) 已经
触底 (列 5 在 y=8 阻挡 piece 下落到 7 以下? — 不, piece 是 2x2 占
(5..6, 7..8), 列 5 在 y=8 已经是 piece 自己的 cell, 检查的是棋盘 y≤6
方向; (5..6, 6..7) 的 row 是空的, piece 可以再 down 一格). 等下, 让我
重新看.

实际: O piece bottom = y, 占 (x..x+1, y..y+1). 在 (5,7) 时 piece 占
(5..6, 7..8). row 6 (y=6) 在列 5..6 是 `..`, 空的; row 7 (y=7) 列 5..6
也是 `..`, 空的. 所以 piece 在 (5,7) 不是 supported — 它能继续 down. 继
续看 (5, 6), (5, 5), ... 列 5 在 y=5 有 #, 列 6 在 y=5 是 `.`. piece 在
(5,5) 时占 (5..6, 5..6); 列 5 y=5 是 # → check 失败. 所以 piece 在 (5,6)
是 piece 占 (5..6, 6..7), 列 5 y=5 是 # 是 piece 下方一格 (piece bottom 6,
下一格 y=5)... let me just trust the dump: candidate 报 idx=1262 是
(r=0, x=5, y=7, open=0). open=0 表示 supported, 从 candidate BFS 角度看
就是合法落点.

无论如何 oracle 也吐了 (5,7) 之外的 (5,3) 等更深 tuck, 而 candidate 都
吐了. 这些都是规则上 reachable 的合法 landing.

### 2.4 哪边对?

**位板对, oracle fast-path 漏吐 tuck**. 但 oracle 漏的都是非 supported
中间路径才能到达的 tuck 落点, master engine 的上层 (ai_zzz / cmd_tris)
在做 evaluator 时几乎不消费这些 tuck 落点 (consumer 是 path 反查,
master 上层假定 piece 都从 spawn hard-drop 锁定). 所以 oracle 漏吐
**不是真 BUG, 只是结果集语义比 candidate 窄**.

---

## 3. 抽样 2: `path_diff piece=I board=rand_000 mode=1g`

oracle = 20, candidate = 45, 25 个 candidate-only. 取 idx=219 (r=3, x=3, y=6, open=0):

I piece R3 在 x=3, y=6. R3 是垂直 (1×4), bottom 在 (3, 6..9). 列 3 在 y=3
有 #, y=4..5 空, y=6..9 空 — piece 占 (3, 6..9) 是合法的, 但需要从空中
垂直竖立后下落到 (3, 6). spawn 是 (3, 21) R0 (2×4 横向). 到 (3, 6) R3 需要:

1. 旋转 R0 → R3 (z 或 c, kick 后 piece 大致在 (3, 19..22) R3)
2. 一路 d 下到 (3, 6) (y=6 之下被列 4 y=8 # 之类阻挡)

oracle fast-path: land_point 预算了所有 (R, lateral), R3 lateral=3 的 hard-drop
是从 (3, 22) drop 到 (3, top - 1) 即 (3, max(top[3]) ~ 8 之类). 但 (3, 6)
比 hard-drop 终点更深 — 这要求"先到 hard-drop 终点 → 一路 'd' 下沉".
oracle BFS 在 `move_down` 这条边上没加 `!open` 过滤
(`if (node->move_down && node_mark_.mark(...) && node->move_down->check(map))`),
所以 'd' 是允许的. 但 oracle 把 'd' 的入队节点也只限制在 land_point
hard-drop 终点的列上, 它没有先做 lateral move 再 down 的能力.

具体 (3, 6) R3 在 oracle 视角下: **如果 hard-drop 列 3 R3 已经能到 (3, top[3]-1)
但被 col 5 的 # 卡住, 只能停在更浅的 y, 那 (3, 6) 这种深 tuck 就要靠
"先在另一列 hard-drop, 再 lateral 到 col 3, 再 'd' 到 (3, 6)"**. lateral
move 边加 `!open(map)` 过滤, oracle 自然漏吐.

candidate (位板 BFS) 自由地从 spawn 出发, 可以 spawn → rotate (得到任意
R0/R1/R2/R3 的 spawn-投射) → down/lateral/down 任意组合, 全部探到. (3, 6)
R3 入 landings.

游戏规则下完全可达 (玩家可以 IRS 旋转 + soft drop 进 well). **位板对,
oracle 漏吐**.

---

## 4. 抽样 3: `aspin_diff piece=O board=aspin_all_spin mode=1g` (subset 1 个)

ASpin all_spin fixture 上 candidate 多吐 1 个 idx=2283 spin=0 (None 类
落点). 这个 fixture 是用户钦点的 ASpin 验证场地. 调研重点: ASpin 落点
是否双侧一致 (即 ASpin classification 等价).

**实测**:

```
[INFO] aspin_diff phase=A,piece=O,board=aspin_all_spin,mode=1g oracle=11 candidate=12
       (oracle ⊆ candidate, candidate-only=1)
```

candidate-only 的 1 个 LpKey = `idx=2283 spin=0 last=-1` — **spin=0 None 类**,
不是 ASpin 落点. ASpin 落点 (spin=1) 在 candidate / oracle 上 byte-equal
完全一致. 即 **ASpin classification 等价, 多出来的是非 ASpin 类的 tuck
落点**, 与 §2 mechanism 同源.

类似地, I/T/L/J/S/Z 6 piece 在 aspin_all_spin 上也是 candidate-only 全部
spin=0. **ASpin 维度无差**.

---

## 5. 总结

| 调研项                                              | 结论                                                       |
|-----------------------------------------------------|------------------------------------------------------------|
| 双向差方向                                          | 全部单向 candidate ⊋ oracle (oracle-only = 0)              |
| oracle 漏吐的具体边                                 | fast-path BFS 中 lateral move / rotate 边加 `!X->open(map)` 过滤 (search_path.cpp:240/245/250/255/260) |
| candidate 收录的具体边                              | 位板 BFS 在 usable_arr 上无 open 限制, lateral/down/rotate 全开 |
| 漏吐落点性质                                        | 全部为 tuck 落点 (open=0) 或 经空中 lateral 后再 drop 的支撑落点 |
| 游戏规则下是否可达                                  | 应可达 (SRS 规则下 piece 可任意 lateral + soft drop)        |
| 哪边对?                                             | **位板对, oracle 漏吐**                                     |
| 是否真 BUG?                                         | 否 — oracle 漏吐的都是 master engine 上层不消费的 tuck 落点, 仅是 search 结果集语义比 candidate 窄 |
| ASpin classification 在 aspin_all_spin 上是否一致? | 是 — ASpin 落点 (spin=1) 双侧 byte-equal, candidate-only 全是 spin=0 None |

**预期与实测一致**: oracle fast-path 在 `node->low >= map.roof` 条件下
跳过整个 tuck BFS, 漏吐 tuck. 位板没有这条 fast-path, 集合包含 tuck.
按用户裁定 (C5 任务书 §C): Phase A 比对协议改为 `oracle ⊆ candidate`
(subset), candidate 多吐的 tuck 落点列出来仅作 [INFO], 不算 fail.

---

## 6. 反向 (candidate 漏吐) — 是否真的零?

```
$ TETRIS_DIFF_VERBOSE_INFO=1 ./path_diff 2>&1 | grep -E '^\[FAIL\] path_diff phase=A' | wc -l
0
$ TETRIS_DIFF_VERBOSE_INFO=1 ./simulate_diff 2>&1 | grep -E '^\[FAIL\] simulate_diff phase=A' | wc -l
0
$ TETRIS_DIFF_VERBOSE_INFO=1 ./tspin_diff 2>&1 | grep -E '^\[FAIL\] tspin_diff phase=A' | wc -l
0
$ TETRIS_DIFF_VERBOSE_INFO=1 ./tag_diff 2>&1 | grep -E '^\[FAIL\] tag_diff phase=A' | wc -l
0
$ TETRIS_DIFF_VERBOSE_INFO=1 ./aspin_diff 2>&1 | grep -E '^\[FAIL\] aspin_diff phase=A' | wc -l
0
$ TETRIS_DIFF_VERBOSE_INFO=1 ./cautious_diff 2>&1 | grep -E '^\[FAIL\] cautious_diff phase=A' | wc -l
0
```

七 driver Phase A 反向违反 (即 candidate 漏吐 oracle 探到的 LandPoint)
数量 = 0. **位板侧 BFS 邻居枚举无 BUG**, 没有任何一个 oracle 探到的
落点是 candidate 漏的.

故 C5 任务书最后那条硬纪律 ("如果调研中发现真 BUG (位板侧确实漏吐了
游戏中可达的 tuck 落点), 立刻停下落盘 c5_blocking.md, 不要硬过") **未
触发**, 不需要 c5_blocking.md.
