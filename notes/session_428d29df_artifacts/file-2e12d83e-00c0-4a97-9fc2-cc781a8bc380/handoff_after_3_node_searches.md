# Handoff: 3 个 _node search 已落地, 等用户确认 M4/M5

> HEAD: a563bc2
> 分支: flip-bits (本地 unpushed)

## 已完成 (3 笔本地 commit, 未 push)

| commit | 内容 |
|---|---|
| 3bf36f7 | search_cautious_node: BfsEngine-based 替代 oracle 风格 search_cautious |
| 542755f | search_aspin_node: BfsEngine-based 替代 search_aspin (含 ASpin 角点判定 + 1g/20g 分支) |
| a563bc2 | search_tspin_node: BfsEngine-based 替代 search_tspin (含 T-spin block_data + last 前驱反查) |

每笔 commit:
- 仅新增 src/search_*_node.{h,cpp}
- 把 .cpp 加入 tetris_ai / tetris_ai_runner 两个 SHARED 目标 (产线 ABI 不变, 只是预编译 _node 文件, 还没切 caller)
- clang-format 过

## 未做 (待用户拍板)

### M4: 切 caller 到 _node

要改的文件 (共 8 处):
- `src/ai_zzz.h`: typedef 4 处 search_tspin::Search → search_tspin_node::Search; 2 处 search_aspin::Search → search_aspin_node::Search
- `src/ai_misaka.h`: typedef 1 处 search_tspin::Search → search_tspin_node::Search
- `src/ai.cpp`:
  - line 105/107/111/113/117/166: srs_ai 用的 `MoveGenSearch<rule_toj::TetrisRule::rule_spec>` → `search_tspin_node::Search`
  - line 324/325/340: `search_aspin::Search` → `search_aspin_node::Search`
  - line 598: `search_cautious::Search` → `search_cautious_node::Search`
  - 调头文件
- `src/pso.cpp`: `MoveGenSearch<rule_srs::TetrisRule::rule_spec>` → `search_tspin_node::Search` (3 处)
- `src/ppt_pso.cpp`: 同 pso.cpp (3 处)
- `src/botris.cpp`: `search_aspin::Search` → `search_aspin_node::Search` (1 处)
- `src/cmd_tris.cpp`: 同 botris.cpp (1 处)
- `src/vs.cpp`: `search_cautious::Search` → `search_cautious_node::Search`

CMake 改动: 各 executable target 的 source 列表里把对应 _node.cpp 加上.

### M5: 把 src/search_{aspin,cautious,tspin}.{h,cpp} + 框架头搬到 oracle/

**前置依赖**: M4 必须先完成 (产线不再依赖 src/search_*.h, 才能搬 .h 到 oracle/).

搬运清单:
- `src/search_aspin.{h,cpp}` → `oracle/search_aspin.{h,cpp}` (用 git 5285fde~1 的 691 行手写干净版本, 不要 HEAD 的 façade 版本)
- `src/search_cautious.{h,cpp}` → `oracle/search_cautious.{h,cpp}` (HEAD 版本本身就是干净手写)
- `src/search_tspin.{h,cpp}` → `oracle/search_tspin.{h,cpp}` (HEAD 版本本身就是干净手写)
- `src/search_hook.h` → `oracle/search_hook.h` (DefaultTSpinHook / DefaultASpinHook 是 MoveGenSearch 老框架的一部分)
- `src/movegen_search.h` → `oracle/movegen_search.h`
- `src/tetris_movegen.h` → `oracle/tetris_movegen.h` (MoveGen 实现, 仅 movegen_search 用)
- `src/piece_filter_index.h` → `oracle/piece_filter_index.h` (kFilteredTable, 仅 movegen 用)
- `src/tetris_rule_spec.h` → `oracle/tetris_rule_spec.h` (RuleSpec ops 元数据, 仅 movegen 用)

CMake 改动:
- tetris_oracle 加 4 对 .cpp 进编译列表
- 产线 target 删 search_aspin.cpp / search_tspin.cpp / search_cautious.cpp
- oracle_diff / extreme_rule_diff / aspin_dump / perft_movegen 链 tetris_oracle, include 'oracle' (本来就有)
- rule_extreme.cpp 仍 include kFilteredTable, 走 oracle/, 加 oracle 到 extreme_rule_diff PRIVATE include 已经有

风险点:
- aspin_dump 工具 (tools/aspin_dump.cpp) 当前 include search_aspin.h + search_hook.h, 全都搬 oracle 后, aspin_dump target 必须 include oracle/. CMake 第 230 行已是 `target_include_directories(... PRIVATE src)`, 需要加 oracle.
- ai_zzz.cpp 现在不直接 include search_aspin.h / search_tspin.h, 它通过 ai_zzz.h 间接拿类型, M4 切完了 ai_zzz.h 就跳到 _node 头, ai_zzz.cpp 不变.

## 已知偏差 (用户已说"回归以后再说")

1. **search_tspin_node**:
   - last 前驱反查走 ParentTrackingDedup 的 (parent, action) 链, hop 上限 64 (master 是无上限到队列起点). 大概率覆盖, 但极端长链可能漏算.
   - 不实现 oracle 的 disable_d 双轮 BFS 优化 (不影响正确性, 只影响性能).
   - 没实现 20g 分支 (master 的 make_path_20g + path_redesign 那一套). config_->is_20g 时, 会走 1g 邻居 + d, 行为偏 1g.
2. **search_aspin_node**:
   - 不用 TetrisMapSnap, 全程用 TetrisMap (master 用 snap 加速 check). 性能下降, 正确性不变.
   - 20g 分支: master 走 build_snap → drop → snap-only check; 这里直接用 emit-drop, 正确但慢.
3. **search_cautious_node**:
   - 不再走 oracle 的"先 drop_index 命中, 再 index 命中"双轮 BFS, 改成单轮 BFS. 单轮包含 d/D 邻居, 命中条件统一是 index_filtered. 在大多数 board 上语义等价 (因为 d/D 都进队列), 但路径长度可能不同 (oracle 优先短路径).

## 下一步 (待用户回复)

请确认:
- **R1**: 接受 3 个 _node 模块的语义偏差? 还是某条偏差需要先补齐?
- **R2**: M4 / M5 是否按上述清单走? 顺序 M4 → M5?
- **R3**: M5 之后产线 ABI 上, search_aspin / search_tspin / search_cautious 的全局符号 (Search 类) 还会出现吗? (答: 不会, 因为 search_*.cpp 不再编入产线 .so)

确认后我开始 M4. 不在用户回复前推送.

## 文件检查清单

```
src/search_cautious_node.h        # 新增, 233 行
src/search_cautious_node.cpp      # 新增, 86 行
src/search_aspin_node.h           # 新增, 350 行
src/search_aspin_node.cpp         # 新增, 99 行
src/search_tspin_node.h           # 新增, 290 行
src/search_tspin_node.cpp         # 新增, 117 行
CMakeLists.txt                    # 改: 仅给 tetris_ai / tetris_ai_runner 两个 SHARED 加 _node.cpp
```

## 关键设计抉择记录

1. **复用 search_path_node 的 IndexedDedup / ParentTrackingDedup / TargetHitVisitor**: 三个 _node 模块 include search_path_node.h 而不是各自重写 dedup. 避免符号重复, 也保持 BfsEngine 调用约定一致.
2. **wall_kick 链处理**: 与 search_path_node 不同 (后者只 emit `node->rotate_*`), 这三个 _node 都展开 `wall_kick_*` 数组, 命中第一个 check(map) 通过的 break. 与 oracle 旧代码 1:1 一致.
3. **NeighborProvider 模板参数化 (Allow180/AllowLR/FastMoveDown)**: Config 参数在 init 时就定了, 编译期模板特化避免运行时 if-branch.
4. **TetrisNodeWithASpinType / TetrisNodeWithTSpinType ABI**: 同形 struct 字段顺序与 oracle 完全一致 (memset 占用相同字节布局), 这样上层 ai_zzz / ai_misaka 的 typedef 替换是无缝的, 不需要改 evaluate / commit 代码.
