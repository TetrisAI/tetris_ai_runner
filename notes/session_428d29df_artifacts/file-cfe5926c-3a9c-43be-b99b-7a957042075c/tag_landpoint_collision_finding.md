# Tag Land-Point 复合 Key 必要性结论

## 背景

阶段 5 一次性调研工具 `tools/tag_landpoint_collision_dump.cpp` 直接对接
`oracle::search_tag::Search`，扫描 17 个 fixture 棋盘 × 7 piece (`OITLJSZ`) × {1G, 20G}
共 238 个场景，按 `index_filtered` 对 land-point cache 做 group-by，统计同一
`index_filtered` 下 `is_last_rotate` / `is_ready` 是否出现分歧。

测试矩阵复用 `tests/path_node_diff.cpp` 的 17 个 board fixture：
`empty / tst_triple / stsd / sspin / tsd / tss_left / tss_right / donation /
pc_opener / sealed_top / i_well_left / i_well_right / t_kick_left /
j_left_well / bottom_pocket / sz_wall_spin / opp_chamber`。

构造方式：标准 10x40 棋面，spawn piece，`is_20g` 时再 `drop` 到顶部。

## 输出（2026-05-29 commit `pre-phase-6` 头部）

```
PSM Summary:
  total_land_points = 3802
  unique_index_count = 3802
  collisions(same_index_filtered, different is_last_rotate) = 0
  collisions(same_index_filtered, different is_ready) = 0
  collisions(same_index_filtered, different (is_last_rotate, is_ready)) = 0
  per-piece breakdown:
    piece=O total=263 unique=263 coll_rot=0 coll_ready=0 coll_both=0
    piece=I total=401 unique=401 coll_rot=0 coll_ready=0 coll_both=0
    piece=T total=777 unique=777 coll_rot=0 coll_ready=0 coll_both=0
    piece=L total=761 unique=761 coll_rot=0 coll_ready=0 coll_both=0
    piece=J total=763 unique=763 coll_rot=0 coll_ready=0 coll_both=0
    piece=S total=422 unique=422 coll_rot=0 coll_ready=0 coll_both=0
    piece=Z total=415 unique=415 coll_rot=0 coll_ready=0 coll_both=0
```

`total_land_points == unique_index_count == 3802`，所有 piece 的碰撞计数全为 0。

## 结论

**单 key（`index_filtered`）足够**。

阶段 6 迁移 `oracle/search_tag` 时，`TagDedup` 直接复用 `search_path_node::IndexedDedup`
（按 `node->index` 标记）即可，**不需要**引入 `(index_filtered, is_last_rotate)` 或
`(index_filtered, is_last_rotate, is_ready)` 复合 key。`TetrisNodeWithTSpinType` 的
`is_last_rotate` / `is_ready` 字段在 BFS 输出阶段一次性计算并附加到 land-point 即可，不影响图遍历去重粒度。

## 附注

- 当前 fixture 主要覆盖 SRS 10x40 主流场景；扩容到极端规则或自定义棋面时若发现新碰撞，可重跑此工具复核。
- `oracle/search_tag::search_t` 内部已用 `TetrisNodeMarkFiltered::mark` 按
  `node->index` 入 cache，本次扫描的零碰撞结果正是该约束的外部观测确认。
- 工具已落地为单独 binary `tag_landpoint_collision_dump`，不进入生产链接。
