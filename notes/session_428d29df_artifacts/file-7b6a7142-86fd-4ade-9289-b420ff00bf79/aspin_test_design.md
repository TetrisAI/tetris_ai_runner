# ASpin 语义梳理与专用测试场地设计

## 目的
1. 锁住 ASpin 等价性裸奔的风险（`tools/aspin_dump.cpp` 当前虽然 `type` 字段已序列化，但只覆盖了通用 fixture，没有为每个 piece 专门构造能触发 ASpin 的口袋；导致 baseline 里 type 被 ASpin 标 1 的样本量稀薄甚至为 0）。
2. 用 Botris 规则覆盖 7 种 piece × 4 种旋转，使每个 piece 至少有一个 ASpin 落点，作为后续 hook 改造的 byte-level diff 看门狗。

## ASpin 精确语义（来自 master `search_aspin.cpp`）
```cpp
// 1g BFS 主循环, status-coord 邻位预计算指针:
if ((!node->move_down  || !node->move_down ->check(snap))
 && (!node->move_up    || !node->move_up   ->check(snap))
 && (!node->move_left  || !node->move_left ->check(snap))
 && (!node->move_right || !node->move_right->check(snap)))
{
    node_ex.type = ASpin;
}
```
- `move_*` 是同 `(piece, r)` 在 status-coord (x±1, y±1) 的预算邻居指针；`nullptr` 表示越界（同样视作不可放）。
- 谓词 = "落点四邻位都不可放" → 完全 immobile。**不依赖 last 动作、不依赖 last_rotate**，与 T-spin 体系正交。
- 新框架等价位板公式（`DefaultASpinHook::on_emit`）：
  ```
  not_up    = ~ usable[r].shifted< 0,+1>()
  not_down  = ~ usable[r].shifted< 0,-1>()
  not_left  = ~ usable[r].shifted<-1, 0>()
  not_right = ~ usable[r].shifted<+1, 0>()
  aspin_set = landings & not_up & not_down & not_left & not_right
  ```

## Botris 规则要点（已具备 ASpin 测试条件）
- 7 piece × 4 rotation，全部带踢墙表（`rule_botris.h` 16–44 行）。
- **O 也有四套独立踢墙** (`OKickR0CW … OKickR3CCW`) 且四个旋转的形状不重合（spawn X=1 → R0/R1/R2/R3 各占 2x2 但相对 base 不同）→ O 支持旋转、可参与 ASpin 判定。
- 任意 piece 的 spawn 在 (3, 20) 或 (4, 20)，棋盘 10×40，足够构造测试口袋。

## 每 piece 一个测试场地（10 列棋盘，bottom-up ASCII）

> 设计原则：每个 fixture 让目标 piece 的某个旋转能滑/转入一个紧贴四面墙的洞。`#` = 占用, `.` = 空。  
> 顶部需要留出生位 `y∈[20,23]` 通透。

### O — 2×2 capped pocket
```
##.##.####   y=4  ← 顶盖（让 O 落在 y=2..3, x=2..3 时上方 (xb,yb+1) 被堵）
##....####   y=3  ← O 占 col 2..3
##....####   y=2  ← O 占 col 2..3
##########   y=1
##########   y=0
```
预期：O 旋到能塞进 col 2..3 的口袋，四邻位（左被 col 1 堵、右被 col 4 堵、下被 y=1 堵、上被 y=4 堵）→ ASpin。

### I — 1×4 vertical capped well（可复用现有 `aspin_lwell`）
```
##########   y=5  ← 顶盖
.#########   y=4
.#########   y=3
.#########   y=2
.#########   y=1
##########   y=0
```
预期：I R1（竖向，1×4 形态）落入 col 0 → ASpin。

### T — Stub-cap pocket
T 在某个旋转下是 3×2 的 ⊥/⊢/⊣/⊤ 形。让 T R2（朝下伸 stub）刚好嵌入：
```
##########   y=5  ← 顶盖
##.....###   y=4  ← 三个空 + 中央 stub 上沿
####.#####   y=3  ← stub 槽
##########   y=2
```
T R2 横条占 y=4 的 col 2..4，stub 占 y=3 col 3 → 四邻位（上盖、下接 y=2、左右被 # 卡）皆封 → ASpin。

### L — 倒挂 L 嵌入口袋
L R3（spawn 形态的 270°）形如：
```
##
.#
.#
```
对应 board：
```
##########   y=5
###...####   y=4   ← 上排两块 col 3..4 是 L 顶
####..####   y=3   ← 中
####..####   y=2   ← 底
##########   y=1
```
让 L 在某旋转下完全嵌入两列 1×3 + 一个 hook → ASpin。具体放置在实现时跑 dump 校验即可。

### J — L 的镜像
```
##########   y=5
####...###   y=4
####.#####   y=3
####.#####   y=2
##########   y=1
```

### S — S 形锯齿 capped
```
##########   y=5
###..#####   y=4   ← S 上半
####..####   y=3   ← S 下半
##########   y=2
```
S 只有 R0/R2 形态等价、R1/R3 形态等价。这里目标是 R0：上半占 col 3..4，下半占 col 4..5。  
**注意**：上面 ASCII 写的是上半 col 3..4 + 下半 col 4..5？需要再核对，最终交给 dump 输出验证。

### Z — S 的镜像
```
##########   y=5
####..####   y=4
###..#####   y=3
##########   y=2
```

## 实现计划（待你确认后执行）

1. **代码改动**：在 `tools/aspin_dump.cpp` 增加 7 个 `make_aspin_<piece>_pocket()` fixture，加入 `boards[]` 列表。命名约定：`aspin_O_pocket / aspin_I_well / aspin_T_pocket / aspin_L_pocket / aspin_J_pocket / aspin_S_pocket / aspin_Z_pocket`。
2. **不**新增 rule、不修改 `rule_botris.*`、不动 `search_hook.h` 与 `DefaultASpinHook`（语义已确认正确）。
3. **type 字段已序列化**（dump 第 293–295 行已写 `t.type`），所以 baseline / new diff 已经能锁住 ASpin 分类等价性，无需改输出格式。
4. 不编译、提交前 clang-format。

## 风险与未决
- 上述 ASCII 仅设计意图，**精确 col/row 需在 dump 跑出 ASpin 落点后回填注释**；如果某个 fixture 没触发 ASpin，调整封口位置而非改 hook 逻辑。
- O 是否真能 ASpin：Botris O 四旋转形态不同（左上 / 右上 / 右下 / 左下 2×2），所以 O 在某些旋转下边界与 base 关系不同，理论上某个 r 能在某口袋里被四面卡住。如果 dump 跑出 O 没 ASpin，再放宽口袋（让 O 在 r=3 时刚好被堵）。

## 一次提交目标
- Title: `tools: add per-piece aspin pockets in aspin_dump`
- 仅 `tools/aspin_dump.cpp` 一个文件改动。
- Commit message：`tools: add per-piece aspin pockets in aspin_dump`
