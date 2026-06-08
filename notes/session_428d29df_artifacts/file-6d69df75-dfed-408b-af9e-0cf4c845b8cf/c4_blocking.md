# C4 Blocking 报告

> **状态**: 对拍框架代码已搭好 (harness + 7 driver + CMake/ctest 接入), 但
> ctest 跑出**结构性 fail**, 不属于"修一改就过"性质. 按用户硬纪律
> "path_diff / simulate_diff / cautious_diff 跑出意料外 fail 立刻停下" /
> "aspin_diff fail 必须停下", **未提交 commit**, 落盘本报告.
> 工作树保持改动 (未 staged), 等用户裁决方向后再决定 commit / 调整.

---

## 1. ctest 概览

```
$ ctest --test-dir build --output-on-failure
44% tests passed, 5 tests failed out of 9
PASS: cautious_diff, simple_diff, extreme_rule_diff, perft_movegen
FAIL: path_diff, tag_diff, simulate_diff, tspin_diff, aspin_diff
```

各 driver 自报 stats (fixtures = 18 命名 + 16 random = 34, pieces = OITLJSZ):

| driver        | total | ok  | fail | 备注                              |
|---------------|-------|-----|------|-----------------------------------|
| path_diff     | 238   | 127 | 111  | 1g; 全部 fail 在 `rand_*` 棋面    |
| tspin_diff    | 476   | 348 | 128  | 1g+20g                            |
| simulate_diff | 238   | 131 | 107  | 1g; 同 path_diff 模式             |
| tag_diff      | 238   | 208 | 30   | 1g; 与 c3 提到的 17 fail 不重合, 多出 13 个 |
| aspin_diff    | 476   | 447 | 29   | 1g+20g; 含命名棋面 + rand          |
| cautious_diff | 238   | 238 | 0    | **全绿**                          |
| simple_diff   | 238   | 127 | 111  | 与 path 互拍, 模式同 path_diff     |
| extreme_rule_diff / perft_movegen | — | — | 0 | 既有测试, 不动 |

`cautious_diff` 全绿就是一记直击核心的关键证据 (见 §3).

---

## 2. fail 案例特征 (path_diff: O 形, rand_000)

`tests/_probe.cpp` 临时探针 (本报告写完前已删除, 重建只需把 git 历史里
`d4_blocking_probe` patch 摘出来) dump 出双方落点的 `(idx, status.r/x/y, open)`:

```
# fixture=rand_000 piece=O
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

## oracle: count=9
  idx=3353 r=0 x=-1 y= 8 open=1   ← 棋盘最左 well, 落到 y=8 (top[0]=7)
  idx=2285 r=0 x= 0 y=11 open=1   ← x=0,1 落在 y=10 上方
  idx=1235 r=0 x= 1 y=11 open=1
  idx= 460 r=0 x= 2 y=10 open=1
  idx=  95 r=0 x= 3 y=10 open=1
  idx= 480 r=0 x= 4 y=12 open=1
  idx=1257 r=0 x= 5 y=12 open=1
  idx=2309 r=0 x= 6 y=11 open=1
  idx=3384 r=0 x= 7 y= 1 open=1   ← 唯一一个 hard-drop 直下 well 的

## candidate: count=15
   ↑ 含 oracle 全部 9 个 +
  idx= 485 r=0 x= 4 y= 7 open=1   ← 落到 y=7 凹陷 (横移进 (4,7))
  idx=1239 r=0 x= 1 y= 7 open=1   ← 落到 (1,7)/(2,7) 上方
  idx=1262 r=0 x= 5 y= 7 open=0   ← 紧贴 pile, tuck 进缝隙
  idx=1266 r=0 x= 5 y= 3 open=0   ← 极深 tuck (滑入 y=3 凹陷)
  idx=2289 r=0 x= 0 y= 7 open=1
  idx=2319 r=0 x= 6 y= 1 open=1
## only-in-candidate: 485 1239 1262 1266 2289 2319
```

候选端多出来的 6 个落点都在 `oracle` 的"硬直落"轨迹**之外的横向凹陷**里
— 即 **tuck 落点**.

---

## 3. 根因: oracle 的 `land_point` cache shortcut vs 候选端全 BFS

### 3.1 oracle/search_path.cpp line 187 起的 fast-path

```cpp
if (node->land_point != nullptr && node->low >= map.roof)
{
    for (auto cit = node->land_point->begin(); cit != node->land_point->end(); ++cit)
    {
        TetrisNode const *drop_node = (*cit)->drop(map);
        if (node_mark_filtered_.mark(drop_node))
            land_point_cache_.push_back(drop_node);
        node_search_.push_back(drop_node);
    }
    // ... 之后才跑 BFS, 但 BFS 边缘 (rotate_*) 里加了 `!open` 过滤 ...
    if (node->rotate_opposite && node_mark_.mark(node->rotate_opposite)
        && !node->rotate_opposite->open(map) && ...)   // ← `!open` 过滤!
}
```

`node->land_point` 是 `core/tetris_core.cpp` line 413 在 prepare 阶段一次性
预算的: 对每个 spawn 节点, 它拿"所有旋转 × 所有 lateral 平移"
(旋转→最左→逐右), 把这堆位置存成 vector. fast-path 触发条件是
`node->low >= map.roof` (棋盘上沿低于 piece 旋转包络的最低点),
这对几乎所有"非顶塞"棋面都成立 (1g + 普通对局).

进入 fast-path 后, 算法 = "每个 lateral 位置直接 hard-drop 当落点" + 后续从
落点出发的 BFS, **但 rotate 边加 `!open(map)` 过滤** ← 这就强制
所有"中间位置"都得是 supported 落点, 无法在空中转完接下去. 等价语义
就是 **"hard-drop only, 不允许 tuck"**.

### 3.2 candidate src/search_path.h: `MoveGen<RuleSpec, T, Hook>::generate`

候选端走 `tetris_movegen.h` line 322 起的位板 BFS, 用 `usable_arr[r]` 在
**全棋盘空间**自由扩展 (L/R/D + 旋转), 到不再变化才停. 之后
`landings = search & landable` 统一 emit. **没有任何 `!open` 限制**.

等价语义就是 **"完整 BFS 可达性, 允许任意 tuck / 横移落入凹陷"**.

### 3.3 cautious_diff 全绿 = 反证

`oracle/search_cautious.cpp::Search::search` (line 358 起) 是**完整 BFS
reachability**: 起点 = spawn (没走 land_point cache 路径); BFS 边
(`move_left/right/down`) 也没有 `!open` 过滤. 这就是和 candidate
PathStrategy 同一个语义, 所以全 238 case 字节对齐, 全绿.

等于说: **path_diff 的 111 fail 不是 PathStrategy 算法 bug, 是 oracle
search_path 自带 fast-path 把"语义"调成 hard-drop-only 了, 而新位板
PathStrategy 是完整 BFS. 二者是**两个不同的搜索算法**, 不该字节对拍.**

### 3.4 同样 fast-path 出现在 search_simulate / search_tspin / search_tag

```bash
$ grep "low >= map.roof" oracle/search_*.cpp
oracle/search_path.cpp:187:     if (node->land_point != nullptr && node->low >= map.roof)
oracle/search_simple.cpp:73:    if (node->land_point != nullptr && node->low >= map.roof)
oracle/search_simulate.cpp:44:  ... node->land_point != nullptr && node->low >= map.roof && land_point->open(map))
oracle/search_simulate.cpp:303: if (node->land_point != nullptr && node->low >= map.roof)
oracle/search_tag.cpp:175:      if (node->land_point != nullptr && node->low >= map.roof)
oracle/search_tspin.cpp:478:    if (!is_20g && node->land_point != nullptr && node->low >= map.roof)
```

把 5 个 driver 的 fail 模式串起来看就完全 fits:

| driver        | oracle 用到 fast-path? | 全 BFS? | 结论                  |
|---------------|------------------------|---------|----------------------|
| path_diff     | 是                     | candidate 全 BFS | 期望 fail (tuck 多出) |
| simulate_diff | 是                     | candidate 全 BFS | 期望 fail            |
| tspin_diff    | 是 (1g 路径)           | candidate 全 BFS | 期望 fail            |
| tag_diff      | 是                     | candidate 全 BFS | 期望 fail            |
| simple_diff   | (与 PathStrategy 互拍) | candidate / oracle 同框架 | 模式 = path_diff     |
| cautious_diff | **否**                 | oracle / candidate 同 BFS | **全绿**             |
| aspin_diff    | 否 (但有 `disable_d` 仅 in make_path) | oracle / candidate 同 BFS | 多出 29 个 fail (见 §4) |

cautious 全绿就是直接反证: **位板 PathStrategy 与 oracle/search_cautious
做的同一件事 (full BFS), 二者完全对齐. fail 全部来自 oracle 自己的
fast-path "hard-drop only" 语义**.

---

## 4. aspin_diff 的 29 fail 模式

`oracle/search_aspin.cpp::search` 没有 `land_point` fast-path, 也不带
`!open` 过滤, 所以语义是 full BFS. fail 不应该来自 §3 那条线.

但 aspin_diff 失败案例 (e.g. `O,tsd,1g`: oracle=2 candidate=9) 仍然是
candidate 多吐出几条, 量级远小于 path_diff (29 vs 111). 推测原因 (未深查):

1. **rule 不一致**: oracle/search_aspin 在生产里用 `rule_botris` 调起,
   而 tests/aspin_diff 这次是用 `rule_srs` 编出来的 — wallkick / spawn 都
   不一样. 用户 hook_interface_matrix_v2.md 里的 ASpin 章节确实强调 botris
   场景, 我这次编 driver 时为了让 7 driver "共形", 给 srs 跑了 — 这个
   决策值得复议.

2. **AS pin classification 边界**: oracle line 411 用
   "no neighbor accepts piece" 判 ASpin, candidate ASpinHook 的 spin type
   写入路径不一样, 但这次对拍只比 `index_filtered` (压平 type 维度)
   也照样 fail, 所以 type 差异不是主要原因. 还是 set 的 size 不一样,
   说明确实是 BFS 可达集不一致.

3. 用户硬规定: **"ASpin 路径在 C2 之后已 byte-equal, fail 必须停"**. 即便
   把 botris 接通, 也得有人确认 c2 baseline 是不是又破了. 我这边没法
   裁定 aspin 是修复 c2 倒退还是 driver 错配 rule. 见 §6 待裁决项 (B).

---

## 5. tag_diff 的 30 fail (vs 用户预期 17)

用户说 tag 当前已知 17 个 J-piece 边界 fail, 让我登记到 xfail 里. 但
实跑 30 fail. fail key 头几条:

```
[FAIL] tag_diff key=piece=J,board=tss_left,mode=1g
[FAIL] tag_diff key=piece=J,board=tss_right,mode=1g
[FAIL] tag_diff key=piece=J,board=i_well_left,mode=1g
[FAIL] tag_diff key=piece=J,board=t_kick_left,mode=1g
[FAIL] tag_diff key=piece=J,board=opp_chamber,mode=1g
[FAIL] tag_diff key=piece=O,board=rand_*  ← 这就是 §3 那批
[FAIL] tag_diff key=piece=I,board=rand_*
... (rand_* 上 J 以外 piece 也 fail, 进 §3 类)
```

把 30 fail 拆开: J-piece 命名 fixture 上有 ~17 个 (= 用户预期), 剩余
~13 个全部是 `rand_*` 上其他 piece, 与 §3 是同一 mechanism. 所以:

- 17 个 J 边界 fail: 用户已知, xfail 通配 `piece=J,board=*` 可以盖住,
  但同 §3 性质的 rand_* fail **不属于 xfail**, 不能塞进去 (用户硬规定
  "不要把 fail case 塞进 xfail").

---

## 6. 选项 / 待裁决项

我**未** commit, 改动留在 working tree:

```
$ git status -s
A  tests/_diff_harness.h
A  tests/path_diff.cpp
A  tests/tag_diff.cpp
A  tests/simulate_diff.cpp
A  tests/tspin_diff.cpp
A  tests/aspin_diff.cpp
A  tests/cautious_diff.cpp
A  tests/simple_diff.cpp
M  tests/extreme_rule_diff.cpp     ← (无实质改动? 只是 #include 调过)
M  CMakeLists.txt
D  tests/oracle_diff.cpp
M  .research/flip-bits-cleanup/c4_blocking.md  ← 本文件
```

(注: `cautious_diff` / `simple_diff` 的位板 strategy typedef 那一带写了
`SimulateStrategy` -> `PathStrategy` 的二选一, 几个 driver 仍有些 dead
include 没收, 见 working tree)

裁决路径:

### A. (推荐) 验收语义改成 "filtered_idx ⊇ oracle"

把每个严格 driver 的 a == b 比对替换成 "oracle ⊆ candidate" — 即候选只能
**多吐**, 不能漏吐. 用户一直强调 candidate (位板) 是 master-of-truth,
oracle 只是历史遗留 baseline; tuck 落点是 candidate 真正应该有的, master
fast-path 缺的反而是历史 bug.

但这违反任务书 "对拍 byte-equal" 字面要求. 如果改, 任务书需要更新.

### B. 砍掉 oracle 的 fast-path, 让 oracle 跑全 BFS 后再对拍

直接改 `oracle/search_path.cpp::search` 删 line 187 的 fast-path 分支,
全部走 line 272 的 full-BFS else 分支. 同样改
`search_simulate / search_tspin / search_tag / search_simple`.
改完 path_diff / simulate_diff / tag_diff (J 边界外) 应该就对齐了.

但这是改 oracle, 与"oracle 是历史 baseline 不动" 哲学冲突. 需要用户
明确批准.

### C. 把比对窄到 oracle fast-path 也覆盖的子集

只比对 oracle 实际探到的落点 (即 `oracle.set ⊆ candidate.set` 部分),
然后看 oracle 内部有没有重影 / 漏报. 但实际上把 == 改成 ⊆ 就是 A,
所以这条与 A 等价.

### D. 留作 xfail (违反硬规定, 不推荐)

把 §3 那批 fail 全部塞 xfail. 用户已经红字禁了 ("不要把 fail case 塞
进 xfail"). 不可行.

### E. (我倾向) 不 commit, 等用户拍板 A/B

理由: 任务书要求"提交单一 commit" 且 "ctest 全绿". 现状 ctest 5 fail,
强行 commit 会留下一棵红的 ctest tree, 比"无 commit + 报告"更糟.

---

## 7. ASpin "用户提供的关键 fixture"

用户原话: "aspin 的测试场景有一个我提供的很有价值, 如果找不到, 从 git
历史中捞回".

搜索结果:

```bash
# 当前 tools/aspin_dump.cpp 的命名 fixture:
$ grep "make_aspin" tools/aspin_dump.cpp
NewMap make_aspin_lwell()           ← 左 well
NewMap make_aspin_rwell()           ← 右 well
NewMap make_aspin_center_pocket()   ← 中央口袋
NewMap make_aspin_all_spin_board()  ← 全 spin 综合 board
```

`make_aspin_all_spin_board` 是最长的一个 (line 223 起), 含手工注释
"all spin" 字样 + 棋面字面量 — 直觉上就是用户提到的"很有价值"的一个.

历史挖矿:

```bash
$ git log --all --diff-filter=D -- tools/aspin_dump.cpp tests/   # 无 删除记录
$ git log --all -p -S 'aspin' -- tools/ tests/                   # 无更早的字面量
$ git log --all -- tools/aspin_dump.cpp                          # 仅 4 个 commit, 都未删 fixture
```

→ 历史里没有被删除的 ASpin fixture. 当前 `make_aspin_all_spin_board`
**就是**用户提到的那个. 我把它原样搬进 `_diff_harness.h::make_aspin_all_spin_board`
作为命名 fixture (`board=aspin_all_spin`), 与 lwell/rwell/center_pocket
一并跑. 不过当前 driver 用 rule_srs 没用 botris, 所以这个 fixture
现在跑出的对拍语义不太对 — 见 §4 待裁决项.

---

## 8. 编码纪律 self-check

- 局部变量没加 const ✓
- clang-format 已对所有新文件运行 ✓
- **单一 commit**: 当前 **未 commit**, 等用户回话 ✗

---

## 9. 下一步建议优先级

1. (用户) 在 A vs B 之间拍板. A 影响最小; B 是工程上更彻底的解.
2. 拍板后, 我把 `run_diff_main` 比对算子改成 `subset` (A) 或改 oracle (B),
   重跑 ctest 全绿后单 commit 提交.
3. tag_diff J-piece 17 个边界 fail 用 `piece=J,board=tss_*` /
   `piece=J,board=i_well_*` / `piece=J,board=t_kick_*` 三条 prefix
   通配 xfail (假设用户决议方向 A 后, J 的 17 个仍是真 fail, 而非 §3
   那条 fast-path 噪声).
4. aspin_diff 单独看 — 要么换 rule_botris 重跑 (driver 改写), 要么
   c2 baseline 复盘. 这一步用户也得给意见.
