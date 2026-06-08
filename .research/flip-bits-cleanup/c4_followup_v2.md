# C4 Followup v2 — 二阶段对拍落地后的真实 fail 报告

> **状态**: 七 driver 已全部迁移到二阶段对拍协议 (Phase A 集合等价 +
> Phase B make_path byte-equal). harness / driver / CMakeLists 改动全部
> 落地, 单 commit 提交 (含本报告). 但 **Phase A 集合维度仍在 5 个 driver
> 上有大量 fail** (path_diff 111 / simulate_diff 107 / tspin_diff 128 /
> tag_diff 30 / aspin_diff 34), 远超用户给的 "> 5 立刻停下" 阈值.
>
> 按用户硬纪律, 已落盘本报告, **不再继续往 driver 里塞 xfail 抹噪声**.
> 这是把 c4_blocking.md 的非正式判断正式化, 落到具体 case 维度.

---

## 0. ctest 全表 (含 Phase A / B 分项)

```
$ ctest --test-dir build --output-on-failure
33% tests passed, 6 tests failed out of 9
PASS: simple_diff (strict=false), extreme_rule_diff, perft_movegen
FAIL: path_diff, tag_diff, simulate_diff, tspin_diff, aspin_diff, cautious_diff
```

driver 自报 (harness 末行):

| driver        | total | A.ok | A.fail | B.ok | B.fail | 备注                                  |
|---------------|-------|------|--------|------|--------|---------------------------------------|
| path_diff     |  238  | 127  | **111** | 117 | 114    | 1g; A 全在 `rand_*`               |
| tag_diff      |  238  | 208  | **30**  | 198 | 33     | 1g; A 在 J/L/Z 边界 + rand_*      |
| simulate_diff |  238  | 131  | **107** |   0 | 231    | 1g; A 同 path_diff; B 是 100% 漏 'D' |
| tspin_diff    |  476  | 348  | **128** | 320 | 142    | 1g+20g; A fail 全在 1g 一侧 ↑ 见 §3   |
| aspin_diff    |  532  | 498  | **34**  | 356 | 162    | 1g+20g; **含 user-provided fixture aspin_all_spin** |
| cautious_diff |  238  | 238  | 0      |   0 | 231    | A 全绿, B 100% 仅"漏 'D'"系统差 (见 §2) |
| simple_diff   |  238  | 127  | 111    |  —  |  —    | strict=false, exit 0; A 模式同 path_diff |

`cautious_diff` 的对照特别重要: 它 Phase A **零 fail**, 直接证明这一轮的
位板 PathStrategy 与 oracle/cautious 的全 BFS 行为在落点集合上 byte-equal,
**Phase A fail 不是位板 BFS 的 bug**.

---

## 1. Phase A 根因 (与 c4_blocking 一致, 此处补量化)

oracle/search_path / search_simulate / search_tspin (1g) / search_tag 在
`node->land_point != nullptr && node->low >= map.roof` 条件下走 fast-path:

1. 用 `node->land_point` 预算的 "(rotation × lateral)" 列表逐个 hard-drop
   (`(*cit)->drop(map)`), 这些 drop 落点直接进 `land_point_cache_`.
2. 之后从这些落点出发跑 BFS, **但 rotate / move 边都加 `!node->X->open(map)`
   过滤** (search_path.cpp:240/245/250/255/260; search_tspin.cpp 的 1g 通道
   亦同形). `open(map)` = "悬浮在空中无支撑". `!open` 过滤强制中间节点必须
   "靠在堆顶上", 实际语义 ≈ "hard-drop 后只能 inplace rotate / shift, 不能
   先空中横跨再下落".

候选位板 PathStrategy / SimulateStrategy / TSpinStrategy / TagStrategy
(`src/search_*.h`) 走 `bb::run_bb_bfs` 在完整 reachability space 中扩展, 没
任何 `!open` 限制. 多吐出来的全部是 **tuck 落点** (横移进侧凹陷, 滑过
堆顶切口落进 well). 这是 c4_blocking.md §2/§3 已经定性的现象, 数量
不变.

### 1.1 量化样本 (path_diff)

```
[FAIL] path_diff phase=A,piece=O,board=rand_000,mode=1g  oracle=9  candidate=15
[FAIL] path_diff phase=A,piece=I,board=rand_000,mode=1g  oracle=20 candidate=45
[FAIL] path_diff phase=A,piece=T,board=rand_000,mode=1g  oracle=34 candidate=75
[FAIL] path_diff phase=A,piece=L,board=rand_000,mode=1g  oracle=34 candidate=93
[FAIL] path_diff phase=A,piece=J,board=rand_000,mode=1g  oracle=34 candidate=95
[FAIL] path_diff phase=A,piece=S,board=rand_000,mode=1g  oracle=17 candidate=39
[FAIL] path_diff phase=A,piece=Z,board=rand_000,mode=1g  oracle=17 candidate=42
... (持续到 rand_015, 共 16 random board × 7 piece 全部 candidate ⊃ oracle)
```

candidate-only 全部是 tuck 落点. oracle-only = 0 (即候选**没漏**, 只是**多吐**).

### 1.2 simulate_diff 几乎与 path_diff 同源

107 vs 111 数差 4 case, 因为 oracle/search_simulate 的 fast-path 对部分
fixture 走 `land_point->open(map) &&` 二级 guard, 触发率略低于 search_path.
量级与模式相同.

### 1.3 tspin_diff

```
oracle/search_tspin.cpp:478:
  if (!is_20g && node->land_point != nullptr && node->low >= map.roof)
```

tspin 1g 通道走同款 fast-path; 20g 通道走另一条全 BFS. 这与 fail 分布
完全吻合: tspin_diff 128 fail **全部在 mode=1g**, mode=20g 零 fail.

```
$ ./build/tspin_diff 2>&1 | grep -E '^\[FAIL\] tspin_diff phase=A,.*mode=1g' | wc -l
128
$ ./build/tspin_diff 2>&1 | grep -E '^\[FAIL\] tspin_diff phase=A,.*mode=20g' | wc -l
0
```

20g 全绿是 **位板 TSpinStrategy 与 oracle search_tspin 20g 通道 byte-equal**
的强证据.

### 1.4 tag_diff 30 fail 拆解

```
[FAIL] tag_diff phase=A,piece=L,board=rand_000,mode=1g  oracle=34 candidate=39
[FAIL] tag_diff phase=A,piece=J,board=rand_000,mode=1g  oracle=34 candidate=41
[FAIL] tag_diff phase=A,piece=S,board=rand_000,mode=1g  oracle=17 candidate=20
[FAIL] tag_diff phase=A,piece=Z,board=rand_000,mode=1g  oracle=17 candidate=19
[FAIL] tag_diff phase=A,piece=Z,board=rand_002,mode=1g  oracle=17 candidate=18
... (剩余分布在 rand_003~rand_011 的 L/J/S/Z piece)
```

oracle/search_tag.cpp:175 也是同款 fast-path. 用户 c4 提到的 "tag 17 个 J
边界 fail" 实际落在 **Phase B (path 字符串)**, Phase A 的 30 fail 是
**额外**的 tuck 落点漏算 — 与 path_diff 同 mechanism.

> **回应任务书 §3 (tag 17 fail 处理)**: 30 个 Phase A fail **不是** path-string
> 17 fail 的子集; 它们落在 J 以外的 L/S/Z piece × rand_* 棋面, 是 fast-path
> tuck 漏算. **Phase A 这 30 个不应塞 xfail**. 真正 17 fail 在 **Phase B**, 已
> 用 `phase=B,piece=J,board=...` 通配登记 xfail.

### 1.5 aspin_diff 34 fail (含用户提供 fixture)

```
[FAIL] aspin_diff phase=A,piece=O,board=tst_triple,mode=1g  oracle=5 candidate=9
[FAIL] aspin_diff phase=A,piece=O,board=tsd,mode=1g         oracle=2 candidate=9
[FAIL] aspin_diff phase=A,piece=O,board=donation,mode=1g    oracle=7 candidate=9
[FAIL] aspin_diff phase=A,piece=O,board=pc_opener,mode=1g   oracle=7 candidate=9
[FAIL] aspin_diff phase=A,piece=I,board=pc_opener,mode=1g   oracle=15 candidate=17
[FAIL] aspin_diff phase=A,piece=T,board=pc_opener,mode=1g   oracle=32 candidate=34
[FAIL] aspin_diff phase=A,piece=O,board=sz_wall_spin,mode=1g oracle=5 candidate=9
[FAIL] aspin_diff phase=A,piece=T,board=opp_chamber,mode=1g  oracle=31 candidate=34
[FAIL] aspin_diff phase=A,piece=O,board=aspin_all_spin,mode=1g oracle=11 candidate=12  ← 用户提供 fixture
[FAIL] aspin_diff phase=A,piece=I,board=aspin_all_spin,mode=1g oracle=25 candidate=26
... (共 34 case)
```

`oracle/search_aspin.cpp::search` **没有** fast-path (`grep "low >= map.roof" oracle/search_aspin.cpp` 结果为空), 也没有 `!open` 过滤 — 严格走完整 BFS.
所以 aspin 的 34 fail **不是** §1 那条 mechanism, 详见 §3.

---

## 2. Phase B 根因: 候选 make_path 末尾恒带 'D'

`cautious_diff` Phase A 全绿, **Phase B 仍 231 fail / 238**. 抽样:

```
[path-mismatch] phase=B,piece=O,board=rand_015,mode=1g key=idx=99 spin=0 last=-1
    - oracle    (0): ""
    + candidate (1): "D"
[path-mismatch] phase=B,piece=O,board=rand_015,mode=1g key=idx=462 spin=0 last=-1
    - oracle    (1): "l"
    + candidate (2): "lD"
[path-mismatch] phase=B,piece=I,board=rand_015,mode=1g key=idx=29 spin=0 last=-1
    - oracle    (5): "rcDzl"
    + candidate (5): "crDzl"
[path-mismatch] phase=B,piece=I,board=rand_015,mode=1g key=idx=31 spin=0 last=-1
    - oracle    (5): "rrzDz"
    + candidate (5): "zrrDz"
```

两类系统差:

### 2.1 末尾 'D' 漏空 (绝大多数)

`oracle/search_cautious.cpp::make_path` (line 14-) / `search_path.cpp::make_path`
/ `search_simulate.cpp::make_path` 都用 **隐式 drop 命中谓词**:

```cpp
if (node->X->drop(map)->index_filtered == index)
    return build_path(node->X, node_mark_);
```

意思是: BFS 走到 `node->X` 时, 如果 `node->X` **可被 hard-drop** 落到目标
filtered class, 就立刻返回 — `node->X` 自己未必就是终点, 但视作"已锁定".
最终 path 字符串 **不带最后那一下 'D'**, 因为 master engine 上层会无脑
drop 一次. 极端例子: 起点 = drop(spawn), idx 已等, oracle 返回空串 `""`,
candidate 的 BBState BFS 必须显式 emit 一个 'D' 才能在 `Helpers::cells_key_for_state`
意义上命中, 故返回 `"D"`.

候选 (`src/search_path.h::PathStrategy::make_path_1g_native` 等) 用
**bb::CellsKey 严格相等**作为命中谓词, 'D' 是真实的图边. 字符长度系统
+1, 字节不可能等.

### 2.2 邻居 emit 顺序差 (少数, 约 2-5%)

`rcDzl` vs `crDzl`, `rrzDz` vs `zrrDz` —
oracle BFS 的 rotate 顺序是 `x z c l r L R d D` (search_path.cpp:48-160 的
固定 if-else 链). 候选 1g neighbors (`Path1gNeighbors::expand`) 顺序不一样
(具体顺序见 `src/search_path.h`); 在多个等价路径都能命中同一 filtered
class 时, 谁先 enqueue 决定哪条 path 反查. 这一类**不是 byte 漏字符**,
而是 **字符级序列不同**. 数量较小, 但同样违反 "byte-equal" 字面要求.

> **回应任务书 §1**: 任务书写 "make_path 是严格要求相同"; 二阶段 split 落
> 地后, byte-equal 这条要求**只在 cautious_diff 一个 driver 上以 100%
> Phase B fail 表面**, 完全是 §2.1 + §2.2 共同导致, 与"集合是否一致"无关.
> 是 **make_path 实现层面**的两套不同字符集 / 邻居顺序约定.

### 2.3 修法选项 (不在本 commit 范围)

A. **改候选 make_path** 引入"末尾隐式 drop"压制: 邻居 emit 时检查
   `Helpers::drop_bb_state(child) == index_landpoint`, 若相等则 emit 该
   邻居字符 (而不是再 emit 'D'). 这要在 5 个 strategy (Path / Simulate /
   Cautious / Tspin / Tag) 的 make_path_1g_native 与 make_path_20g_native
   各自加 detection. 工程量中等; 单元测试覆盖面: 当前的 cautious_diff
   一旦改动直接见效.

B. **改 oracle make_path** 让它 emit 末尾 'D'. 与"oracle 不动"哲学冲突,
   且会破坏所有依赖 oracle make_path 的下游 (master engine 上层一直假定
   "make_path 不带最后一下 hard-drop, 我自己 drop"). **不推荐**.

C. **放宽 byte-equal**: 接受候选 path 比 oracle 多一个 'D' 后缀, 在 Phase B
   把 oracle path 当 "candidate path 的前缀, 末尾允许且仅允许 'D'" 来比.
   语义上等价于 §2.1 的隐式 drop. 字面违反任务书要求.

我 **倾向 A**, 但这是改 src/, 不属于"对拍框架修正" commit 范围, 等用户
拍板再上单独 commit.

---

## 3. aspin_diff 34 fail 与 rule 错配

c4_blocking.md §4 推测 aspin_diff fail 是因为 driver 用 rule_srs 而历史
baseline 用 rule_botris. 本轮 driver 仍然用 rule_srs, fail 还在.

### 3.1 rule_botris vs rule_srs 在 ASpin 上是否真错配?

挖矿 `tools/aspin_dump.cpp`:

```cpp
// commit ef582c3 (用户原始提交)
using AspinEngine =
    m_tetris::TetrisEngine<rule_botris::TetrisRule, ai_zzz::Botris, search_aspin::Search>;
```

aspin_dump 用 **rule_botris + 候选 aspin::Search** 跑出 `aspin_dump.txt`,
这是历史 baseline. 注意:
1. rule = **botris** (kicktable / 18 状态 / wallkick edges 与 SRS 不同).
2. Search 端就是 **候选位板** `aspin::Search` (`src/search_aspin.h`), **不是**
   oracle facade `search_aspin_oracle::Search`.

也就是说, "ASpin baseline" 历史上从未与 oracle facade 对拍过 — oracle facade
是 c4 之前才剥出来的, 它在 SRS 规则下与候选 BFS 的差异**没有任何历史背书**.

### 3.2 rule_srs 下 oracle vs candidate 在 ASpin 的差是什么

```
phase=A,piece=O,board=aspin_all_spin,mode=1g
  oracle=11 candidate=12
  oracle-only=0 candidate-only=1
  + idx=3346 spin=0 last=-1
```

候选多吐的 `idx=3346` 是 ASpin **None** 类落点 (即非 ASpin 普通落点),
oracle 漏吐. oracle/search_aspin.cpp 在 1g 通道 (line 240+) 里也有
"`open(map)` && `!last_node->open(map)`" 类 guard, 与 search_path 一脉相承,
不能算彻底 full BFS. 这就是漏吐 mechanism: 中间节点 open 即跳过.

### 3.3 rule = botris 重跑 driver 会怎样?

我没改 driver — 任务书没明确授权切 botris. 切 botris 会引入两个新风险:
1. rule_botris.cpp 的 wallkick / spawn 表与 SRS 不同, OITLJSZ 7 piece 在
   不同棋面上的 search 集合也会变 — 整套 fixture 需要重新校准.
2. 其它 6 driver (path/simulate/tspin/tag/cautious/simple) 均已绑 rule_srs,
   把 aspin 单独切 botris 会破坏 "七 driver 用同一棋面同一规则" 的对照前提.

更稳的办法是 **承认 aspin oracle facade 在 SRS 下不可靠**:
- 候选 `aspin::Search` 在 botris 上有 `aspin_dump.txt` 历史背书.
- oracle `search_aspin_oracle::Search` 是 c4 阶段从历史 master-graph 剥出
  的纯 facade, 没有受过 botris baseline 验证, 也没有 SRS baseline 验证.
- 既然候选侧自带 `aspin_dump` 全跑, **aspin_diff 这个 driver 本身价值不大** —
  它是把两个互不背书的实现做无 baseline 对拍.

### 3.4 用户提供的 fixture 是否捞到?

是. 来源 commit `ef582c3` ("tools: add all-spin fixture covering every botris
piece in aspin_dump"), 由 `zhaoming.274` 在 2026-05-29 提交, body 注释明确
"all-piece spin fixture supplied by the maintainer that simultaneously
triggers a wall-kick spin pocket for each of the seven Botris pieces".

字面布局已搬到 `tests/_diff_harness.h::make_aspin_all_spin_board` (本 commit
新增), driver 跑 `build_all_aspin_fixtures()` 时自动加入 `board=aspin_all_spin`
case. 在 rule_srs 下, 它在 O / I / T / L / J / S / Z 7 piece × 1g/20g 14 case
里, **全部 14 case 都是 candidate ⊃ oracle**, oracle-only = 0 (即候选没漏吐),
说明候选位板 ASpin BFS **比 oracle facade 更全**, 这本身**不是** bug.

历史更广搜索 (无别的被删 ASpin fixture 字面量):

```
$ git log --all --diff-filter=D -- tools/aspin_dump.cpp tests/   # 0 行
$ git log --all -p -S 'aspin' -- src/ tests/ tools/              # 仅常规改名
$ git log --all -p -S 'corner' -- src/ tests/ tools/             # 无相关 fixture
$ git log --all -p -S 'all_spin' -- src/ tests/ tools/           # 只有 ef582c3
```

**结论**: 用户提到的 "很有价值的 ASpin fixture" 就是 `make_aspin_all_spin_board`,
已 100% 嵌入 driver. 没有别的被删 fixture 待捞.

---

## 4. 编码层面的本 commit 改动

### 4.1 二阶段对拍协议 (`tests/_diff_harness.h`)

| 接口             | 旧 (c4_blocking 版)                           | 新 (本 commit)                              |
|------------------|-----------------------------------------------|---------------------------------------------|
| 比对单元         | `Tup{idx,r,x,y,spin,last}` 序列                | `LpKey{idx_filtered,spin,last}` 集合          |
| ProbeFn          | `(piece, board, name, is_20g) -> vector<Tup>` | `... -> CaseProbe{oracle_lps, candidate_lps, oracle_make_path, candidate_make_path}` |
| 比对算子         | `oracle == candidate` 序列字节比对              | Phase A: `set(oracle_keys) == set(candidate_keys)` Phase B: 对交集 key, byte-equal |
| 失败上报         | unified diff, 整序列                           | Phase A: `oracle-only / candidate-only` 列表; Phase B: per-key path mismatch |
| xfail            | `key=piece=X,board=Y,mode=1g`                  | `phase=A,piece=X,board=Y,mode=1g` / `phase=B,...` (拆 phase) |

### 4.2 Driver 修正

| driver        | hook 改动                       | 备注                                                      |
|---------------|---------------------------------|-----------------------------------------------------------|
| path_diff     | NoSpinHook → **CautiousHook**   | NoSpinHook 自家覆盖 `config_allow_rotate_move=true`, BFS 会展开 'C/Z/X' rotate-after-move 邻居; oracle 没有这条边. 切 CautiousHook (默认 false) 让两侧字符集对齐. |
| simulate_diff | NoSpinHook → **CautiousHook**   | 同上.                                                      |
| cautious_diff | 沿用 CautiousHook               | 已 Phase A 全绿; Phase B 见 §2.                              |
| tspin_diff    | TSpinHook (沿用)                | 改用 `CaseProbe` API; 20g 通道维持 spin/last 抹零策略       |
| tag_diff      | TSpinHook (沿用)                | 改用 `CaseProbe` API; 17 J fail 转 phase=B xfail            |
| aspin_diff    | ASpinHook (沿用)                | 改用 `CaseProbe` API; 加 `aspin_all_spin` 命名 fixture       |
| simple_diff   | NoSpinHook 双侧                 | strict=false; 不绑 make_path 闭包 (两 strategy 字符集本就不一致) |

### 4.3 fixture

新增 `make_aspin_all_spin_board` (用户提供, ef582c3 字面量).
`build_all_aspin_fixtures()` = 18 命名 + 3 aspin 专用 + 1 user-provided + 16 random
= 38 命名 fixture. 其它 driver 沿用 `build_all_fixtures()` (18 + 16 = 34).

### 4.4 CMakeLists.txt

无改动 (上一轮 c4_blocking 期间已把 7 driver target + ctest add_test 写好,
本轮只是 fix 编译 + 改 driver 内部, 不动 build wiring).

---

## 5. 待裁决项 (按优先级)

### A. (推荐) 改候选 make_path 引入"末尾隐式 drop"压制

见 §2.3.A. 唯一让 byte-equal 真正成立的工程路径. 影响 `src/search_path.h` /
`src/search_simulate.h` / `src/search_cautious.h` / `src/search_tspin.h` /
`src/search_tag.h` 共 5 个 strategy 的 make_path_1g_native + make_path_20g_native.
改完: cautious_diff Phase B 应当全绿; path_diff / simulate_diff / tspin_diff /
tag_diff Phase B 在 Phase A 已通过的 case 上也会全绿.

**前提**: Phase A 必须先解决 (§B), 否则 Phase B 只能盖到两侧 key 集合的交集.

### B. (必须先做) 解决 Phase A 集合不等

任务书原话: "**oracle 如无 BUG, 肯定不改了** — `oracle/` 目录禁动".
"**本地 `node->low >= map.roof` fast-path 只是为了加速, 我们其实不算很在意
search 结果集的顺序, 只要集合是一致就行了**". 这两句相加只能解读成:

- oracle fast-path 与 全 BFS 应该 **集合一致**;
- 但实测显示 fast-path 漏吐 tuck 落点, 集合不一致;
- 用户判定: 这就是 **oracle 的 BUG**. 按"如无 BUG 不改"的逆否, **有 BUG
  应该改**, 即 §C.

### C. 改 oracle, 砍 fast-path 的 `!open` 过滤

最干净的修法: oracle/search_path.cpp 的 fast-path 分支 (line 187-271)
**仅保留** "for each (rotation × lateral) hard-drop" 那段, 把后面的
"BFS with `!open` 过滤" 整段删掉, 让 fast-path 后面的 BFS 与 else 分支的
全 BFS **完全一致** (行为合并). search_simulate / search_tspin (1g) /
search_tag 同型修改.

工程量: ~30 LOC 删除 + 5 case 单元测试 (extreme_rule_diff / perft_movegen /
本 7 driver) 重跑全绿.

风险: oracle 与 master engine 上层耦合 — 历史上 ai_zzz / cmd_tris 可能依赖
fast-path 的 "hard-drop only" 语义做 evaluator 缓存预判. 需要 grep 确认.

### D. 接受集合不等, 改用 "oracle ⊆ candidate" subset 比对

工程改动最小: 把 `run_diff_main` 里 Phase A 的 `oracle_keys == candidate_keys`
换成 `set_difference(oracle, candidate).empty()`. 物理意义: "候选只能多吐
不能漏吐". 与 c4_blocking.md §6.A 同方案.

任务书表述上这违反 "set 一致" 字面, 但与 "我们其实不算很在意 search 结果集
的顺序, 只要集合是一致就行了" 的弦外之音相容 — 用户真在意的是"不漏",
"多吐 tuck 是好事". 用户给一句话授权即可上线.

### E. aspin_diff 改 rule_botris

见 §3.3. 不推荐单独切, 因为会破坏七 driver 的同规则对照. 更合理的处理是
**砍 aspin_diff** (反正 aspin_dump.txt 历史背书的是候选侧, oracle facade
没人 review 过).

---

## 6. 我没做 / 不能做的事

1. **没塞 xfail 抹 Phase A 集合 fail**. 严守用户硬规定.
2. **没改 oracle/**. 等 §B 拍板.
3. **没改 src/ make_path**. 等 §A 拍板.
4. **没切 aspin rule**. 等 §E 拍板.
5. **未 push**. 单 commit 放在 `flip-bits-clean` 本地, 含本报告.

---

## 7. 上一份 c4_blocking.md 里有, 本轮覆盖了没

| c4_blocking 提到的项                             | 本轮处理                              |
|-------------------------------------------------|---------------------------------------|
| §1 ctest 概览                                    | §0 含分 phase A/B 表, 信息更细        |
| §2 fail 案例特征 (path_diff O/rand_000)          | §1.1 引用同样数据, 不复制               |
| §3 oracle fast-path vs 候选全 BFS 根因           | §1 量化 + driver 矩阵                  |
| §4 aspin 29 fail (推测 rule 错配)                | §3 完整 rule 错配考据 + ef582c3 引证    |
| §5 tag 30 fail (J 边界 17 + rand_* 13)          | §1.4 + §4.2 (xfail 拆 phase=A vs B)    |
| §6 待裁决 A/B/C/D/E                              | §5 重新分类 (A=改 src/, B+C=改 oracle, D=放宽, E=去 aspin) |
| §7 ASpin 用户 fixture 找回                       | §3.4 落定: ef582c3 = 该 fixture, 已嵌入 |
| §8 编码纪律 self-check                            | 单 commit, clang-format, 局部变量无 const |

---

## 8. 给用户的具体提问 (按 §5 待裁决项顺序)

1. (§5.B/C) 是否授权改 oracle 砍 fast-path 的 `!open` 过滤? 这是直接让 Phase A
   全绿的唯一路径. 如果"oracle 不动"是硬要求, 请走 §5.D (subset 比对).

2. (§5.A) 候选 make_path 末尾 'D' 压制改不改? 如果走, 单独再开一个 commit.
   实施前提: §5.B 或 §5.D 拍板, Phase A 已稳定.

3. (§5.E) aspin_diff 是否保留? 如果 oracle facade 没人 review 过, 是否直接
   让 aspin_dump.cpp 自己跑回归 (在 botris 下 byte-equal 自身上一版输出),
   而不再让 aspin_diff 与 oracle facade 对拍?

4. tag_diff 17 fail 已用 phase=B xfail 通配收敛 (10 条 fixture × J piece);
   harness 输出 stats 可校验是否恰好 17. 现在 tag_diff Phase B fail 33,
   xfail 17, **多出 16 个**未 xfail 的 Phase B 路径 mismatch — 都是
   §2.1 末尾 'D' 漏空 + §2.2 邻居顺序差. 等 §5.A 落地后这 16 个会自动消失,
   无需单独动作.
