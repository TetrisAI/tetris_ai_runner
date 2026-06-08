# commit 6c: 把 20g make_path 也移到位板原生

## 改动
- `MoveGenSearch::make_path` 在 `is_20g` 分支不再委托 `impl_.make_path` (亦即
  `search_tspin::Search::make_path_20g`), 改走新增的私有 helper
  `make_path_20g_native`.
- `make_path_20g_native` 复刻 master `make_path_20g` 的 BFS 拓扑:
  - 入口 `start_sunk = drop_bb(spawn)`; 邻居 (kick/move) 命中后再 `drop_bb`.
  - 用本地 `MarkSlot[4*kW*kH]` 存 `(visited, action, parent BBState)`, 等价
    master `node_mark_.set(child_post_drop, parent, action)`.
  - 命中谓词键沿用 1g 路径的 cells_key 等价类: `index_key` 选 `.last` 或 `.node`,
    `index_landpoint` 永远是 `.node`.
  - 末段 wall-kick 重放 (`'x'/'z'/'c'`) 自带 drop 步: `try_kick_chain_to_drop`
    把 `first_passing_kick` 命中点 sink 后再比 cells_key, 以匹配 master
    `wall_kick_node->drop(map)` 的两步.
- L/R 多步移动按 master 'L' / 'R' 一边走一边 drop, 整体作为一步入 mark.

## 验证
- `oracle_diff` 全绿 (1g + 20g, 7 piece × 17 board, oracle landings + path
  self-check + mg-path-self-check).

## 现状
- `MoveGenSearch::impl_` 现在仅在 `search()` 末尾的 `default:` 分支留作非 SRS
  piece 兜底 (上方已有 `assert(false)`, 实际不可达).
- `search_tspin::Search` 仍被引用作两个用途:
  1. `MoveGenSearch::impl_` 字段本身.
  2. `impl_.check_ready` / `impl_.check_mini_ready` 在 `run_piece_20g<'T',true>`
     中复用. 两个谓词只读 `block_data_`, 公开后 OK.

## 下一步候选
1. **commit 6d**: 把 `check_ready`/`check_mini_ready` 移植到 MoveGenSearch
   (或独立 helper namespace), 完全切断对 `search_tspin::Search` 私有数据的依赖.
2. **commit 6e**: 删 `impl_` 字段, 不再 `init` 老引擎.
3. **commit 6f**: 完全对齐 spin tag, 摘掉 oracle_diff 20g flavor 的 zero-spin
   tolerant 比较.

## TODO
- `make_path_20g_native` 当前不实现 `allow_rotate_move` (新框架下没用过, master
  20g 路径也没这套支路, 与 1g `make_path` 的 `if (config_->allow_rotate_move)`
  对齐). 如果未来 20g 接入 allow_rotate_move, 这里要补 'X'/'Z'/'C'.
- 末段 build_path 用了一个 `(void)cur_action; (void)cur_has_pred; (void)s0;`
  抑制未使用警告; 这是从 1g 路径搬过来的 closure 形态, 简化空间. 可在后续
  收尾时去掉.
