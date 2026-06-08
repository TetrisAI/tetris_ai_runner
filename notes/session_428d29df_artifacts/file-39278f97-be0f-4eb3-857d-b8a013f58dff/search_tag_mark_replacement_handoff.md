# SearchTag `node_mark_t_` -> bb::PathMark Replacement Handoff

> 任务: 把 `src/search_tag.h::TagStrategy` 残留的 oracle `m_tetris::TetrisNodeMark`
> (按 `node->index` 寻址、随 `version_` 累积) 替换为位板等价组件; 与
> `PathStrategy` / `SimulateStrategy` 选用同款 (`bb::PathMark`, 按
> `(r, xb, yb)` 索引、版本化清理).

## 1. 当前 HEAD
- Branch: `flip-bits-clean`
- HEAD: `8be0069 refactor(movegen): introduce bitboard-native TagStrategy and retire search_tag_node`
- 工作区干净 (除 untracked notes).

## 2. 旧组件用法盘点 (`search_tag.h` + oracle/search_tag.cpp)

| 位置 | 用法 | 等价 |
|------|------|------|
| `tag::ExtrasMixin::node_mark_t_` | 跨 `search()` 持久 `TetrisNodeMark`, 仅 T-search 使用 | `path_mark_` (PathMarkMixin 已经在 Context 里) |
| `init` 中 `node_mark_t_.init(node_max())` | 容量按 master node 数 | PathMark 容量按 `kR*kW*kH` 编译期常量, 不需要 init |
| `search_t_native` 起始 `clear()` | 跨调用 version 累积 | `path_mark_.clear()` 同语义 |
| `mark(entry)` (action==0) | 仅 set version, mark.data 保留 (=> 默认 init `(nullptr, '\0')`) | `set_bbox(r,xb,yb, PrevKey{0xFF,0,0}, '\0')`; `pk.has()=false`, op='\0' — 与 oracle `get(entry)` 返回 `(nullptr, '\0')` 一致 |
| `set(child, parent, ' ')` (d/l/r) | 仅写新 cell 的 (parent, ' ') | `set_bbox(r,xb,yb, parent_pk, ' ')` |
| `cover_if(child, parent, ' ', 'z'/'c')` (z/c) | 已访问且 op==' ' 时升级到 'z'/'c' | **新增** `cover_if_bbox(r,xb,yb, parent_pk, ' ', op)` 到 `bb::PathMark` (与 `set_bbox` 同 API 形态, 增加 `ck` 参数) |
| `get(node)` (incomplete emit) | 取最终 (parent, op) | `get_bbox(r,xb,yb)` + `state_to_node(parent_state)` |

## 3. cells_key↔node 双射保证 (与 Path/Simulate 同款)
- 单一 piece T 的 BFS 中, `(r, xb, yb)` 与 master `TetrisNode::index` 一一对应
  (master prepare 阶段按 (T, r, x, y) 生成节点, 与 bbox 公式
  `xb = status_x + origin.x, yb = status_y - origin.y` 双射).
- 因此把按 `node->index` 寻址的 mark 表换成按 `(r, xb, yb)` 寻址的 PathMark,
  写入/查询/版本化清理三件事的语义完全等价.

## 4. cover_if 等价性论证
oracle:
```cpp
bool cover_if(key, node, ck, op) {
  Mark& m = data_[key->index];
  if (m.version == version_ && m.data.second != ck) return false;
  m.version = version_;
  m.data = {node, op};
  return true;
}
```
新 `bb::PathMark::cover_if_bbox`:
```cpp
bool cover_if_bbox(r, x, y, prev, ck, op) {
  if 越界 return false;
  i = y*kW + x;
  if (cell_ver_[r][i] == version_ && cell_op_[r][i] != ck) return false;
  cell_ver_[r][i] = version_;
  cell_prev_[r][i] = prev;
  cell_op_[r][i] = op;
  return true;
}
```
完全同形, 只把 "node->index 寻址" 换成 "(r, xb, yb) 寻址". 由于 `(r, xb, yb)` 与
`node->index` 在 T 的 BFS 中双射, mark/set/cover_if 三态收敛到完全同一组结果.

## 5. 入口 (action==0) 的"无数据写入"保留
oracle `mark(entry)` 不写 data, 因此 `get(entry)` 永远读到默认初始化值
`(nullptr, '\0')` (entry 在 BFS 中无法被回溯到, 因此 BFS 内的 set/cover_if
也不会写到 entry 的 index). 我们用 `set_bbox(entry_state, PrevKey{0xFF,0,0}, '\0')`
显式写入哨兵: `pk.has()` 在哨兵下返回 false → `last = nullptr` (= oracle nullptr);
`op = '\0'` → `is_last_rotate = ('\0' != ' ') = true` (= oracle 同结果).
跨调用的稳定性: 每次都写同样的哨兵, 一致.

## 6. 跨 piece 行为 (T 与非 T 共享 path_mark_) 等价性
- oracle `node_mark_` 对所有 piece 共用; 非 T (z/c/l/r/d) 与 T (search_t)
  写入域按 `node->index` 隔离.
- bb 端 `path_mark_` 在每次 search() 顶层 (`run_piece_1g_native`,
  `run_piece_20g_native`, `search_t_native`) 起始都 clear() 一次, 因此跨
  调用之间无残留可见; 单次 search() 调用只走 1 个分支 (T 或非 T), 同一分支内
  写入的 cells 与 oracle 写入的 indices 双射.

## 7. TagStrategy Context mixin 列表
**前**:
```cpp
MoveGenContext<RuleSpec,
               PathMarkMixin<RuleSpec>,
               BfsQueueMixin<RuleSpec>,
               StateNodeLutMixin<RuleSpec>,
               detail::tag::ExtrasMixin<SpinHook, RuleSpec>>;
// ExtrasMixin 内部持有 m_tetris::TetrisNodeMark node_mark_t_
```
**后**:
```cpp
MoveGenContext<RuleSpec,
               PathMarkMixin<RuleSpec>,
               BfsQueueMixin<RuleSpec>,
               StateNodeLutMixin<RuleSpec>,
               detail::tag::ExtrasMixin<SpinHook, RuleSpec>>;
// ExtrasMixin 仅 context_ / config_ / land_point_cache_ / hook_state_,
//   与 detail::path::ExtrasMixin / detail::simulate::ExtrasMixin 同形;
//   不再持有 m_tetris::TetrisNodeMark.
```
mixin **类型列表**完全不变, 只是 `tag::ExtrasMixin` 内部去掉 `node_mark_t_` 字段.

## 8. 实施清单
1. `src/bb_state.h`: 给 `PathMark` 加 `cover_if_bbox` 方法 (单文件单方法, 不动其他).
2. `src/search_tag.h`:
   - 删 `tag::ExtrasMixin::node_mark_t_` 字段 (含 include 注释).
   - 删 `init()` 里的 `ctx.node_mark_t_.init(...)`.
   - `search_t_native`: `clear()` 改 `ctx.path_mark_.clear()`; `incomplete` 容器
     改存 `bb::BBState` (出 BFS 后回查 master + 父态).
   - `TagSearchTDedup`: 字段从 `m_tetris::TetrisNodeMark *` 改 `PathMark *`;
     try_admit 改用 set_bbox / cover_if_bbox; 不再需要 `state_to_node` 把
     parent 还原成 master (PathMark 的 PrevKey 直接承载位板坐标).
3. 编译 + 4 个 diff 验证.
4. clang-format + 单一 commit.

## 9. 风险点
- 若 byte-equivalence 在某些边角 (e.g., entry 自身被回访) 失效, `tag_node_diff`
  会红. 按任务约定: 立即停手, 不强行 commit, 用 `ask_user` 汇报.
