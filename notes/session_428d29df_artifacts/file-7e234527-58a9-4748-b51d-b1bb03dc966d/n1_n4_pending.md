# Hook 重构方案 - 用户已确认 N.1-N.4 待做 (落盘归档)

> 用户裁决: 2026-06-01. **N.1-N.4 全部同意, 但先暂停**. 优先做 tag 位板化 (原 N.5).
> 完成 tag 位板化后再回来按 N.1 → N.4 顺序执行.

## 已确认方案 (N.1-N.4, pending)

### N.1 — Payload 形态收敛
- `NoTSpinPayload` 删, 新增 `EmptyPayload` (NoHook/NoSpinHook/CautiousHook 用)
- `TSpinPayload` + `ASpinPayload` 合并为 `SpinTypePayload { type, last_x, last_y, last_r, has_last_rot }`
- 4 种 Payload → 2 种 (Empty / SpinType)

### N.2 — LandPoint 形态收敛
- 新增 `m_tetris::PlainLandPoint { TetrisNode const *node; }`
- NoSpinHook + CautiousHook 切到 PlainLandPoint, 不再借用 tspin::NodeEx
- 同步改 ai.cpp QQTetrisSearch 与 c2_ai 的 LandPoint 形态

### N.3 — block_buffer 魔数命名
- `block_buffer[52]` + `+10` → 命名常量 `kBlockBufferLen / kSafeXDiffPad`
- 注释来源 (oracle 1:1)
- 维持 byte-equal

### N.4 — BaseSpinHook CRTP + is_landpoint_none 反转
- 新增 `BaseSpinHook<Derived>` CRTP, 默认实现:
  - `is_landpoint_none / get_last_node / 7 个 config_*`
  - `on_init_rotations / on_rotate_reach / on_emit / on_search_state_init` 全空
  - `check_ready / check_mini_ready` 默认 false
  - `resolves_last_1g=false`, P2-P5 默认常量
- **关键修正**: `is_landpoint_none` 反转改名为 `lp_requires_last_rotate(lp)`,
  让 ASpin 路径"起点==终点"享受自落点短路
- visitor 命中谓词从 `!is_none && last_rotate && k==index_landpoint`
  简化为 `requires_last_rotate && k==index_landpoint`

## tag 位板化 (本次执行)

### 范围
search_tag.h 内仍依赖 master 旋转指针 / master 几何的代码:
1. `TagSearch1gNeighbors` / `TagSearch20gNeighbors` 内 `cur_node->rotate_*` 指针访问
2. `run_piece_20g_native` 内 `drop()`, `status.r`, `move_left/right`
3. `search_t_native` 内 `SpinHook::check_ready(map, sunk_node, state)` (T-spin 3-corner master 判定)
4. `make_path_native` 内 `node->index_filtered`, `last->rotate_*` 比对

### 完成后会触发 (N.5)
- 删 TSpinHook::SearchState (x_diff/y_diff/block_buffer)
- 删 TSpinHook::on_search_state_init / check_ready / check_mini_ready
- A4/A5/A6 接口面收窄

### 验收
- `oracle_diff` byte-equal (tag strategy × 1g/20g × 7 piece × 5 random map)
- `extreme_rule_diff` 全过
- 单一 commit 完成

## 用户编码纪律 (本次执行务必遵守)
- 不动 `oracle/` 任何文件
- 不动 `ai.cpp` 实例化, 不动下游 AI eval/get
- 局部变量不加 const
- 提交前格式化 (clang-format)
- Commit message 英文, 不含不可见字符 / 不影响 json 解析的字符
- Commit message 描述 "本 MR 相对目标分支的差异", 不写本次具体改了什么
- **不要 push, 等用户确认后才 push**
