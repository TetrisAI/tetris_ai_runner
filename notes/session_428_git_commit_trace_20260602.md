# 428d29df 会话 commit 追查（2026-06-02 17:48）

## 来源限定

本次只基于两类证据：

1. 428d29df 会话 artifact / 执行产物里出现的真实 commit 记录
2. 本机仓库 `/Users/zhaoming.274/Work/tetris_ai_runner` 的真实 git 对象 / refs / reflog / unreachable commits

不依赖设计文档去猜代码内容。

## 从会话产物里摘到的真实 commit 记录

### 明确出现于 handoff / status 的 commit

- `42923c1` — Route AI engines through MoveGenSearch adapter
- `aba81fb` — Extend LandingPos with rotation predecessor placeholders
- `92ec953` — Track last-rotate predecessor in MoveGen LandingPos
- `dcb9b85` — Parameterize MoveGenSearch on RuleSpec
- `4e248a0` — Drive MoveGenSearch::search through native bitboard MoveGen
- `f952b28` — Add master make_path self-check to oracle_diff
- `7077937` — Expose RuleSpec via rule headers and route TetrisEngine through the bridge
- `92d0804` — hook / dedup 方案阶段 HEAD
- `58c846a` — replace TetrisNodeMark in TagStrategy with bitboard PathMark
- `06fb133` — Compare make_path BFS hits via bitboard cells-key
- `e73c860` — Drop hardcoded SRS-7 dispatch in MoveGenSearch
- `9240004` — inline PathMarkBit bitset into PathMark, memset whole object on clear
- `fdb3092`
- `6758178`
- `d45f612`
- `46d783c`

## 本机仓库存在性核对结果

### FOUND

- `7077937` — `Expose RuleSpec via rule headers and route TetrisEngine through the bridge`
- `d45f612` — `Fix oracle_diff TetrisMap polarity and add per-piece landing dumps`
- `46d783c` — `Emit MoveGen landings in master TetrisNode status coordinates`

### MISS（本机 repo 里不存在该 commit 对象）

- `42923c1`
- `aba81fb`
- `92ec953`
- `dcb9b85`
- `4e248a0`
- `f952b28`
- `92d0804`
- `58c846a`
- `06fb133`
- `e73c860`
- `9240004`
- `fdb3092`
- `6758178`

## 额外排查

### 1. `git log --all`

只命中：
- `46d783c`
- `7077937`
- `d45f612`

### 2. reflog

未出现上述缺失 commit。

### 3. stash

无相关结果。

### 4. unreachable / dangling commits

本机 repo 存在一批 unreachable commits，但逐个扫描其提交标题与改动文件后，未发现：
- `bb_state.h`
- `movegen_context.h`
- `movegen_search.h`
- `movegen_searcher.h`
- `bb_bfs_engine.h`
- `movegen_hook.h`
- `movegen_strategy.h`
- `PathMark`
- `path_mark_`

说明这些缺失 commit 不是“对象在库里但挂不上 ref”，而是当前本机这个 repo 压根没有这些对象。

## 当前能确认的事实

1. 428d29df 会话里确实记录过一条比当前仓库更靠后的 commit 线。
2. 但这条线里的大多数关键 commit（尤其是 `92ec953`、`4e248a0`、`58c846a`、`9240004`）在你本机这个 repo 里都不存在。
3. 当前本机仓库能对上的只是一部分较早/较保守阶段：
   - `7077937`
   - `46d783c`
   - `d45f612`

## 结论

如果要“从 git commit 变更历史里找出来并 checkout”：

- **能 checkout 的真实候选只有 3 个**：`7077937`、`46d783c`、`d45f612`
- 其中：
  - `7077937` 更像会话里多份文档反复引用的“阶段基线”
  - `46d783c` / `d45f612` 是当前仓库后续 MoveGen/oracle_diff 演进
- 你记忆里的 `PathMarkBit / bb_state / PathMark` 那条关键 commit 线，**不在当前本机 repo 对象库里**

所以现在的核心不是“checkout 不 checkout”，而是：

> 你想对齐到 428d29df 的哪一个阶段？
>
> - 保守基线：`7077937`
> - 当前仓库已知更后阶段：`d45f612`
> - 你记忆中的 PathMark 线：需要先找到含这些 commit 的另一份 repo / bundle / worktree
