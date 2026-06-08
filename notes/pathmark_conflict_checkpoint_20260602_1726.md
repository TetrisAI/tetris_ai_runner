# PathMark 追查冲突记录（2026-06-02 17:26）

## 当前核查结果

- 当前沙盒仓库：`tetris_ai_runner`
- 当前分支：`dev`
- 当前 `src/` 目录中 **不存在**：
  - `bb_state.h`
  - `PathMark`
  - `PathMarkBit`
  - `path_mark_`
- 直接文件清单显示 `src/` 只有旧 `search_path.* / search_simulate.* / search_tag.* / search_tspin.*` 等文件。
- `glob **/bb_state.h` 也未命中，说明不只是 `src/` 没有，整个当前仓库工作树都没有这个文件。

## 与用户当前说法的冲突

用户这轮要求是：
- 以 **dev 分支 src/** 为准
- 直接 grep 找出全部 PathMark 实现
- 其中应有 3 类：
  1. `PathMarkBit`
  2. 保存 `last rot`
  3. 保存 `last node`

但当前沙盒 `dev` 工作树不支持这一前提，因为：
- 根本没有 `bb_state.h`
- 根本 grep 不到 `PathMark*`

## 最可能的几种解释

### 解释 A
用户说的“dev 分支”指的是**另一个分支状态/另一台机器上的 dev**，不是当前沙盒 `dev@6cf6726`。

### 解释 B
用户说的 PathMark 家族存在于**仓库历史提交或其它分支**，但当前工作树未 checkout 到对应提交。

### 解释 C
用户记忆里的“PathMarkBit / last rot / last node 三类”来自**后续重构后的 movegen/search 体系**，而不是当前这个仓库快照。

## 下一步建议（待用户拍板）

1. **按当前沙盒事实继续**：我转去查 git 历史 / 其他本地分支，把真正含 `PathMark*` 的提交找出来，再只基于源码汇报。
2. **用户纠正目标仓库/分支**：如果不是这个 `tetris_ai_runner/dev`，请用户给正确仓库或 commit。
3. **用户允许查历史调研产物对应提交号**：我从 notes 里提到的 commit / branch 反查源码，而不再硬限定当前工作树。
