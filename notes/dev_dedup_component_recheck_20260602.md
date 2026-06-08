# dev 分支 dedup 等价实现复核（2026-06-02）

## 复核背景

- 上一轮把 `tetris_movegen.h` 里的 `kCanonicalR` 当成 `TetrisNodeMarkFiltered` / `TetrisNodeMark` 的位板等价实现。
- 用户纠偏：`dev` 里应该已经有更直接的等价实现，名字和 `dedup` 相关。
- 本轮目标：只在当前沙盒 `tetris_ai_runner` 的 `dev` 分支、只看 `src/`，核实这一说法是否成立。

## 当前仓库事实

### 1. 当前 HEAD

- branch: `dev`
- commit: `6cf67260dcfe9002b956b4f14633d367d3c68da7`
- 工作区：仅有未跟踪 `notes/`

### 2. 当前 `src/` 中没有已落地的 dedup / PathMark 新组件

在 `tetris_ai_runner/src/` 内检索：

- `Dedup`
- `PathMark`
- `try_admit`
- `EnqueueDecision`
- `bb::PathMark`
- `MakePath1gDedup`
- `MakePath20gDedup`
- `TagSearchTDedup`

结果：**均未命中**。

同时：

- `src/search_path.h` 仍然是旧接口，字段仍是
  - `m_tetris::TetrisNodeMark node_mark_`
  - `m_tetris::TetrisNodeMarkFiltered node_mark_filtered_`
- `src/search_tag.h` 也仍然是旧接口，字段仍是
  - `m_tetris::TetrisNodeMark node_mark_`
  - `m_tetris::TetrisNodeMarkFiltered node_mark_filtered_`

说明在**当前 dev HEAD** 中，用户所说的“dedup 相关位板等价组件”**并没有落到 `src/` 代码里**。

## 反证：真正带 dedup 命名的内容只出现在历史调研产物中

在仓库 `notes/session_...` 里，确实能找到大量后续方案/分支记录，里面明确出现：

- `bb::PathMark`
- `TagSearchTDedup`
- `MakePath1gDedup`
- `MakePath20gDedup`
- `DedupPolicy`

但这些内容当前只存在于**调研文档**，不在当前 `src/` 的已落地代码中。

## 最关键的两份证据

### A. `search_tag_mark_replacement_handoff.md`

该文档明确写：

- 目标是把 `search_tag` 残留的 `TetrisNodeMark` 替换成 `bb::PathMark`
- 分支是 `flip-bits-clean`
- HEAD 是 `8be0069`
- 新实现会给 `bb::PathMark` 增加 `cover_if_bbox`
- `TagSearchTDedup` 会从 `TetrisNodeMark*` 改成 `PathMark*`

这说明：

1. **确实存在你说的那类“更直接等价”的 dedup 设计 / 实现方向**；
2. 但它描述的是 **`flip-bits-clean` 分支**，不是当前沙盒里的 `dev@6cf6726`。

### B. `search_tag_bb_factcheck.md`

这份文档更直接，结论是：

- `search_tag_node` 仍是指针图 BFS
- tag 的位板原生化**尚未完成**
- 需要新建 `TagStrategy`
- 需要删 `search_tag_node.{h,cpp}`

这进一步说明：

- 那套 `Dedup / PathMark` 方案在这批记录对应的工作流里，是**正在推进或准备落地**的内容；
- 不是当前 `dev` 分支 `src/` 已经提交好的事实。

## 对旧组件的准确对应关系（基于现有证据）

### 当前 `dev@6cf6726` 已落地部分

- `TetrisNodeMark` / `TetrisNodeMarkFiltered` 的**旧版直接实现**仍在：
  - `src/tetris_core.h`
  - `src/search_path.h`
  - `src/search_tag.h`
  - `src/search_tspin.cpp` 等
- `tetris_movegen.h` 里的：
  - `search[r]`
  - `kCanonicalR`
  只能算 **MoveGen 结果级 / 状态级去重机制**，不能准确说成“已替代旧 mark 类的 dedup 组件”。

### 真正更直接的“位板版 mark 等价物”

从现有记录看，真正要等价替代旧组件的是：

- **`bb::PathMark`**：等价旧的 mark 表存储
  - 以 `(r, xb, yb)` 寻址，替代旧 `node->index`
  - 提供 `clear / set_bbox / get_bbox / cover_if_bbox`
- **若干 `*Dedup` 类型**：等价旧 BFS 搜索阶段的 mark / cover_if 使用方式
  - 如 `TagSearchTDedup`
  - `MakePath1gDedup`
  - `MakePath20gDedup`

也就是说，**真正和 `TetrisNodeMark / TetrisNodeMarkFiltered` 对得最直接的，不是 `kCanonicalR`，而是文档里那套 `PathMark + *Dedup` 体系**。

但需要强调：

> 这套体系在当前沙盒的 `dev` 分支 `src/` 中，还没有找到已提交代码；目前只能在历史调研产物/后续分支记录中找到明确证据。

## 结论

- 如果问题限定为 **当前沙盒 `tetris_ai_runner` 的 `dev@6cf6726` + `src/`**：
  - **没有**已落地的 dedup 命名位板等价组件；
  - `TetrisNodeMark` / `TetrisNodeMarkFiltered` 仍直接存在于 `search_path.h`、`search_tag.h` 等旧搜索实现中。
- 如果问题放宽到 **该仓库后续调研记录 / 其它分支演进**：
  - 真正更直接等价旧 mark 系统的，是 **`bb::PathMark` + `TagSearchTDedup / MakePath*Dedup`** 这套设计；
  - 其中 `PathMark` 对应“按 index 的 mark 存储”，`*Dedup` 对应“搜索阶段的 set / mark / cover_if 准入策略”。

## 给后续汇报的推荐说法

- 先明确纠正：上一轮把 `kCanonicalR` 当成 mark 等价物不准确。
- 再明确区分两层：
  1. **当前 dev 已落地**：MoveGen 里有 canonical/filter 去重，但这不是 old mark 的直接替身。
  2. **真正 direct equivalent（从现有记录可证）**：是后续方案/分支里的 `bb::PathMark + *Dedup`。
- 最后显式指出冲突：
  - 用户说“dev 已有”，
  - 但当前沙盒 `dev@6cf6726` 的 `src/` 代码核查结果不支持这一点。
