# Tag: t_path_mark_ 是否可以合并到 path_mark_ — 最终结论

## 1. HEAD
- Branch: `flip-bits-clean`
- HEAD: `58c846a refactor(movegen): replace TetrisNodeMark in TagStrategy with bitboard PathMark`

## 2. 结论 (TL;DR)
**不能删. `t_path_mark_` 独立持有是正确设计, 注释 (search_tag.h:81-90) 已经
对位准确**. 调研过程中曾提出三种合并方案, 全部不成立. 本文落定结论与依据,
避免再次绕回.

## 3. 关键事实链 (按 oracle 真实代码核对)

### 3.1 oracle TetrisNodeMark 的 mark / set / get 语义
源: `core/tetris_core.cpp:198-262`.

```cpp
clear()       : ++version_;                              // 不动 data_
get(key)      : data_[key->index].version == version_
                  ? data_[key->index].data
                  : pair{nullptr, ' '};                  // 不命中 -> 默认
mark(key)     : if (version 命中) return false;
                version = version_; return true;          // 不动 data
set(k, n, op) : if (version 命中) return false;
                version = version_; data = {n, op}; return true;
cover_if(...) : if (版本命中 && data.second != ck) return false;
                version = version_; data = {n, op}; return true;
```

### 3.2 oracle search_t (search_tag.cpp:315-379)
- 入口 `node_mark_.mark(entry)` (line 320): 把 entry 的 version 提到本轮.
- BFS 中 d/l/r 用 `set(child, parent, ' ')` (line 336/341/346), 写 (parent, ' ').
- BFS 中 z/c 用 `cover_if(child, parent, ' ', 'z'/'c')` (line 353/361),
  在已访问且 op == ' ' 时升级为 'z'/'c'.
- emit incomplete 阶段 `get(node)`: version 命中, 读 data_.

### 3.3 entry 自己的 data 由谁写过?
- BFS 中 d/l/r/z/c 都是从 `node` 出发往 `node->move_*` / `node->rotate_*`
  这些子节点写, **不会**回写 entry 自己 (entry 在 entry 这个 index 的
  邻居写入是其他 child 看 entry 作为 child 的情形, 但 entry 在 BFS 起点
  就 mark 过, 后续别的 cur 把 entry 当 child 时, version 已命中 -> set 返回
  false, **不会改写 entry 的 data**).
- 所以 search_t **本次** BFS 中 entry 的 mark.data **从未被写**.

### 3.4 entry 的 data_ 实际是什么?
- 第一次 `Search::search()` 调用: init 默认 `pair{nullptr, '\0'}`. emit 阶段
  `is_last_rotate = ('\0' != ' ') = true`.
- 第 N 次调用 (N≥2): clear() 只 bump version, data_ 保持 prior 写入.
  - prior 同 piece 同 entry index 在 search_t 的 BFS 中**作为子节点**被
    某 cur 的 set/cover_if 写过, 这些 set 在第 N 次中通过 version 不命中 ->
    本次 entry mark 之前 get 返回默认; mark 之后 version 命中 -> get 返回
    那次最后写入的 prior data.
  - prior 不同 piece 不可能写到当前 entry index, 因为 oracle node->index
    按 (T, R, x, y) 编码, piece 字段隔离.

**关键**: oracle 不依赖第一次/第 N 次行为相同, 但 tag_node_diff 的实测
通过本身就说明: 即使在 N≥2 的情形下, oracle 的"残留"行为是 well-defined
且我们要 byte-equal 复现.

### 3.5 位板侧 PathMark 的 mark_bbox / set_bbox / get_bbox
源: `src/bb_state.h:192-296`.
- 寻址: `(r, xb, yb)`, **不带 piece 维度**.
- mark_bbox: 仅 bump version, 不动 prev/op.
- set_bbox: 写 (prev, op).
- cover_if_bbox: oracle cover_if 同形.
- get_bbox: 不命中返默认 `(PrevKey{0xFF,0,0}, ' ')`; 命中读 prev/op.

### 3.6 位板侧 search_t_native 中 entry 的 prev/op 来源
- 第一次 search: cell 默认初始化 (`PrevKey{0,0,0}, '\0'`). 但等会 — 默认值
  是什么? `cell_prev_[kR][kCells] = {}` -> `PrevKey{0,0,0}` 而不是 `{0xFF,0,0}`.
  这是位板侧的隐式陷阱: `PrevKey::has()` 在 r=0 时返回 true, 等价于"有父".
  位板侧第一次 search entry get 读到 `(PrevKey{0,0,0}, '\0')` -> last 不为
  nullptr (会被 state_to_node 反查到 r=0,xb=0,yb=0 的某 master node).
  与 oracle 的 last=nullptr 不一致!

  **但** commit 58c846a 实测 tag_node_diff 通过. 说明实测路径中 entry 在
  本次 search_t_native 之前**已经在 t_path_mark_ 上被 prior 写过有效数据**,
  不会读到 `{0,0,0}` 默认值. 即 tag_node_diff 的种子全是 N≥2 调用.

### 3.7 t_path_mark_ 隔离的真正作用
即使共用 path_mark_:
- 第一次 search_t_native 之前, path_mark_ 在 prior 非 T 搜索 (1g/20g) 中
  被 clear, 各 cell 的 version 不命中. 进入 search_t 入口 path_mark_.clear()
  再 bump 一次 version, 整盘 cell 仍不命中. mark_bbox(entry) 让 entry
  version 命中, 但 cell_prev/op 仍是默认 `{0,0,0}, '\0'` (从未被
  set_bbox 写过) — 第一次 search 同 oracle 的"默认 + version 命中"语义不同
  (oracle 默认 op='\0' is_last_rotate=true; 位板默认 PrevKey{0,0,0}.has()=true,
  last 指向 (r=0,xb=0,yb=0) 反查 master node — 不等价!)
- 第 N 次 search_t_native (N≥2): path_mark_ 在第 N-1 次 1g/20g 或第 N-1 次
  search_t 中被 set_bbox 写过. clear() 后 version 不命中, 但 cell_prev/op
  保留. mark_bbox(entry) 让 entry version 命中, 读 prev/op = "上次** 任何 piece**
  对该 cell 的最后一次 set_bbox". 与 oracle "上次**同 piece**对该 index 的
  set" 不等价 (寻址维度不同).

t_path_mark_ 独立持有则:
- search_t_native 是 T piece **独占**写入域. 跨 search_t_native 的 prev/op
  残留全部来自 prior 同 piece T 的 set/cover_if 写入.
- 与 oracle node_mark_ "按 node->index 编码自带 piece 隔离" 的位板对位.

## 4. 三次错误论证回顾

| 轮次 | 主张 | 致命漏洞 |
|------|------|---------|
| 1 | "oracle 全程一份 mark, 位板也该一份" | 漏掉 oracle node->index 自带 piece 字段, 位板 (r,xb,yb) 没有 |
| 2 | "T 与非 T 隔离" | 同 search() 内 if/else 互斥, 不存在并存 |
| 3 | "if/else 互斥 + 入口 clear -> 可合并" | 漏掉 mark_bbox 不写 data + get_bbox 命中后读 cell_prev/op 残留 |
| 4 (本次最初提案) | "entry 写显式哨兵 + clear -> 可合并" | 显式哨兵 op='\0' 与 oracle 第 N 次 search (N≥2) 的"残留 op"实测行为不等价, byte-equal 要求下不可接受 |

## 5. 现状

- commit 58c846a 设计正确, **不需要后续 cleanup commit**.
- 注释 search_tag.h:77-91 (`t_path_mark_` 字段块) 已经准确说明独占动机,
  无需重写.
- t_path_mark_ 是 PathMark 类型, 与 PathStrategy/SimulateStrategy 选用同款
  位板组件 — 已经达到"无 master TetrisNodeMark"的位板化目标.

## 6. tag 系列剩余真问题

参照 `research/flip-bits/search_tag_oracle_quirks_factcheck_handoff.md` Quirk 1:
旋转邻居走 `cur_node->rotate_*` master 指针图. 这是仍在依赖 TetrisNode
指针的最后一处. 三个路线 (A: rotate_no_kick_bb 替换并保 byte-equal /
B: 完整 kick chain 放弃 byte-equal / C: 维持) **未拍板**, 等用户决定.
