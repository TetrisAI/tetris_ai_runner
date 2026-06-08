# Q-Path 恢复点 (2026-05-31)

## 用户决定
选方案 Q (干净化), 单 commit, 强行做, 崩了接受. 但 context 已 82%, 我评估 Q 范围内无法完成且无法验收.

## Q 的精确范围 (用户陈述)
1. ai.cpp 切位板 BBState 入口
2. 删 state_node_lut / master 反查
3. search_tag expand 重写 (master 旋转指针 → 位板 wallkick)
4. 撤 src/search_*_node.{h,cpp} + tests/*_node_diff.cpp
5. 撤 CMake path_node_diff / simulate_node_diff / tag_node_diff target
6. oracle diff 验收

## 已知不可在当前会话做完的硬约束
- m_tetris::TetrisEngine 模板要求注入的 search 类型暴露 master 签名 init/search/make_path. 不改 TetrisEngine, ai.cpp 拿不到 BBState 入口.
- search_tag.h 的 expand 在 1g/20g/search_t/make_path 共 ~15 处直接 `cur_node->rotate_*` 走 master 旋转指针, 重写位板等价 wallkick 是新算法工作量.
- 必须 oracle diff (4 个 binary) 全部 byte-equal 才算落地.

## 替代落地建议 (新会话起跑用)

### Q.0 (本会话可做, 单 commit, 风险低)
重命名 `src/search_aspin.{h,cpp}` → `src/search_aspin_node.{h,cpp}` (aspin 整个文件就是 façade, 100% 胶水).
namespace `search_aspin` 保持 (botris.cpp / cmd_tris.cpp / ai.cpp / aspin_dump.cpp / movegen_hook.h 这些消费方 API 完全不变).
CMake: 改 5 处源列表 (tetris_ai / tetris_ai_runner / botris / cmd_tris / aspin_dump).
ai_zzz.h 中 `search_aspin::Search::ASpinType` 等类型引用不变.

### Q.1 (新会话, 单 commit)
search_path.h / search_simulate.h / search_simple.h 各拆成两半:
- `search_<name>.h`: 只保留 <Name>Strategy struct (纯位板)
- `src/search_<name>_node.{h,cpp}` 重写: wrap `movegen::Searcher<<Name>Strategy, *Hook, rule_spec>` 暴露 master 签名 (替代当前 BfsEngine 风格实现, 等价位板, 通过 *_node_diff 对拍验收)
- ai.cpp 不动

### Q.2 (新会话, 单 commit, 高风险)
search_tag.h 同 Q.1 拆分, **额外**重写 expand 内 ~15 处 master 旋转指针为位板 wallkick. 必须 oracle diff (tag_node_diff) 全部 byte-equal.

### Q.3 (新会话, 单 commit)
ai.cpp 切位板 BBState 入口, 删 state_node_lut, 撤 search_*_node, 撤 *_node_diff, 撤 tetris_oracle 之外的 master-graph 入口.

## 文件清单 (供新会话参考)

### 当前 src/search_*.h 不纯净点 (来自 strategy_purity_audit.md)
- search_path.h: init/search/make_path(TetrisNode...) + state_node_lut + run_piece master get + Run20gVisitor 反查
- search_simulate.h: 同
- search_simple.h: 同 + node->land_point spawn-row fast path
- search_tag.h: 同 + expand 内 master 旋转指针深度耦合 (≈15 处)
- search_aspin.{h,cpp}: 整个文件 = facade

### 当前 search_*_node.{h,cpp} 内容
均为 BfsEngine + IndexedDedup + ParentTrackingDedup 风格的位板生产实现, 持有 `bfs::BfsEngine<...> engine_*`. 不是空容器, 不是 master 实现.

### 当前由 *_node_diff 对拍的 candidates
都是 src/search_*_node 的 BfsEngine 实现, 对照 oracle/search_*.cpp 的 master-graph 真值. 撤 _node 等价于撤这一对照体系.

## 当前提交栈 (flip-bits-clean)
```
9bfffa0 oracle: archive search_aspin master-graph BFS
c8d331e refactor(aspin): drop master-graph members from Search facade
9240004 refactor(bb): inline PathMarkBit bitset into PathMark, memset whole object on clear
```
均未推送.
