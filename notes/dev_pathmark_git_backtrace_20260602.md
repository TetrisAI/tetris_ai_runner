# dev 分支 PathMark / bb_state 历史回溯（2026-06-02 17:29）

## 目标

只基于 git 历史与源码对象，确认：

1. 当前沙盒 `dev` 是否真的来自本机同步
2. 历史里是否存在 `PathMark` / `bb_state.h` / 相关源码实现
3. 如果不存在，是被删除/移动了，还是根本没进当前导入的这条分支线

## 核心结论

### 结论 1：当前沙盒 `dev` 的确不是从远端 GitHub 分支拉来的，而是从一个本地 bundle 导入的

`git reflog --all` 显示：

- `2026-06-02 14:39:42 +0800`
- 执行的是：
  - `fetch notes/session_428d29df_artifacts/file-b7c37fb6-910d-4161-aaf9-63e04f4f8b78/flip-bits.bundle refs/heads/flip-bits:refs/heads/dev: storing head`
- 随后：
  - `checkout: moving from flip-bits to dev`

也就是说：

- 当前 `dev` 是**从 bundle 里的 `refs/heads/flip-bits` 映射到本地 `refs/heads/dev`** 得到的
- 不是直接 `git fetch origin dev`

### 结论 2：这个 bundle 里装的分支头就是当前 `6cf6726`，而且 bundle 自称“完整历史”

`git bundle verify notes/.../flip-bits.bundle` 显示：

- bundle contains:
  - `6cf67260dcfe9002b956b4f14633d367d3c68da7 refs/heads/flip-bits`
- `The bundle records a complete history.`

这说明就**当前导入进来的这条分支线**而言：

- 沙盒里没有少拿几段增量历史
- 当前 `dev` 就是 bundle 里的那条完整分支历史

### 结论 3：在当前仓库可见的全部 refs / 全部历史对象里，没有任何 `PathMark` / `PathMarkBit` / `bb_state.h` 的源码历史

我做了几类只针对源码的检查：

#### A. 查文件历史

- `git log --all --full-history -- src/bb_state.h src/movegen_context.h src/movegen_search.h src/search_tag_node.h src/search_tag_node.cpp`
- 结果：**无输出**

说明现有 refs 下，git 历史里**从未出现过这些路径**。

#### B. 查源码内容历史

- `git log --all -G 'PathMark|PathMarkBit|path_mark_|bb_state' -- '*.h' '*.cpp'`
- 结果：**无输出**

说明现有 refs 下，源码 diff 里**没有任何一次引入/修改这些符号**。

#### C. 查任意历史提交中的 `src/` 内容

- 对 `git rev-list --all` 全量提交做源码对象级 grep
- 没有找到任何包含 `PathMark` / `PathMarkBit` / `path_mark_` 的 `src/*.h/*.cpp` 历史提交

说明不是当前 HEAD 才没有，而是**整个现有提交图里都没有这条源码线**。

### 结论 4：没有任何“被删除或重命名掉”的证据

我又查了：

- `git log --all --diff-filter=DR --summary -- src/bb_state.h src/movegen_context.h src/movegen_search.h src/search_tag_node.h src/search_tag_node.cpp`

结果：**无输出**。

这说明：

- 没有删除记录
- 没有 rename 记录
- 没有“先有后没”的源码证据

所以当前最符合事实的判断不是“后来丢失了”，而是：

> **当前导入到沙盒的这条分支历史里，从一开始就没有 PathMark / bb_state 那套实现。**

## dev 这条线实际包含了什么

`git diff --name-status e3b909b..6cf6726` 显示：

- 新增：
  - `src/tetris_map.h`
  - `src/tetris_movegen.h`
  - `src/tetris_rule_spec.h`
  - `src/tetris_shape.h`
  - `src/tetris_simd.h`
  - `tests/oracle_diff.cpp`
  - `tests/perft_movegen.cpp`
- 修改：
  - 大量 `rule_*.{h,cpp}`
  - `tetris_core.{h,cpp}`
  - `chash.h`
  - `CMakeLists.txt`

这里面**没有**：
- `bb_state.h`
- `movegen_context.h`
- `movegen_search.h`
- `search_tag_node.*`

也就是说，这条 `dev` 线的位板化路线，实际就是你现在看到的：

- `tetris_movegen.h`
- `tetris_map.h`
- `tetris_shape.h`
- `tetris_rule_spec.h`

而不是 PathMark / bb_state 那条后续架构线。

## 额外发现：仓库里还有别的 bundle，但也不能证明 PathMark 已导入当前提交图

仓库 `notes/session_...` 下还有：

- `flip-bits-clean.bundle`
- `check-andn.bundle`
- 另一个 `flip-bits.bundle`

但就当前仓库 refs 而言：

- `flip-bits-clean.bundle` 里头只有 `e3b909b refs/heads/flip-bits-clean`
- 当前仓库没有把它 fetch 成本地分支
- `check-andn.bundle` 也没有进入当前 refs 图

所以这些 bundle **只能说明仓库目录里存在其他导出物**，不能说明它们已经并入了当前沙盒 `dev` 的历史。

## 对“为什么会丢失”的最准确回答

就目前 git 证据来说，**不是“丢失”**，而是下面这个情况：

1. 你本机曾经导出过一个 `flip-bits.bundle`
2. 这个 bundle 的头就是 `6cf6726`
3. 沙盒把它导入成了当前 `dev`
4. 但这个 bundle 自身就不包含 `PathMark / bb_state` 那条源码历史

因此：

> **当前沙盒并不是把带 PathMark 的历史同步过来后又弄丢了，而是最初导入的 bundle 就不是那条包含 PathMark 的分支线。**

## 当前最可能的来源解释

按 git 证据，最可能有三种：

### 解释 A
本机当时导出的就是“MoveGen 位板化线”，不是“PathMark 体系线”。

### 解释 B
PathMark 体系存在于本机的另一个分支 / 另一份 bundle，但这次只同步了 `flip-bits.bundle`，没同步对应那一份。

### 解释 C
PathMark 体系存在于未导入当前仓库 refs 的 bundle（例如 `flip-bits-clean.bundle` / `check-andn.bundle` 对应的另一条工作线），但当前沙盒并没有把它 fetch 成本地分支，因此在现有提交图里不可见。

## 如果继续追，下一步最有效的办法

只基于 git / 源码，下一步应该做的是：

1. **把其他 bundle 也 fetch 成临时本地分支**，再查它们的源码树里有没有 `bb_state.h / PathMark*`
2. 如果这些 bundle 仍然没有，那就说明当前工作目录下的 bundle 全都不是你记忆里的那条线
3. 这时需要你给：
   - 正确的 bundle 文件
   - 或正确的分支名 / commit
   - 或本机那边对应分支的 HEAD

这样才能把真正带 `PathMarkBit / last rot / last node` 的源码历史接进来
