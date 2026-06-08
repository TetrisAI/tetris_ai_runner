# ASpin 语义梳理与专用测试场地设计

## ⚠️ 实测发现 (重要事实修正)

跑 `aspin_dump` 在原有 11 个 fixture (empty / donation / pc_opener / sealed_top /
sspin / sz_wall_spin / opp_chamber / bottom_pocket / aspin_lwell / aspin_rwell /
aspin_center_pocket) × 7 piece × {1g, 20g} = 154 组合下:

```
type=1 总数: 0
```

**所有原有 fixture 都没有触发任何 ASpin** — 这意味着 `aspin_diff` 看门狗自一开始
就只是在守 type=0 的 alias 落点位置, 从未真正锁住过 ASpin 分类语义. 这是之前
"剩余工作"清单里"ASpin 等价性裸奔"风险的实质来源, 比预期更严重.

## 设计难点 (实测验证后总结)

理论上 ASpin 触发条件简单 ("落点四邻 immobile"), 但要让 BFS 同时:
1. **可达** (cap 必须留缺口让 piece 从外部进入);
2. **immobile** (落地后四邻必须被堵).

在 SRS-like wall-kick 体系下这两点常互斥:
- cap 完全封顶 → BFS 不可达 → search 不会输出该落点 (无论 type 是什么);
- cap 留缺口 → piece 多半飘到 cap 之上而非进入 pocket.

唯一可靠路径: 让 piece 通过 **特定旋转 + Botris kick table 偏移** 进入 immobile
pocket. 这要求精确推算 kick table, 不能凭 ASCII 直觉盲设计.

## 当前 commit 实际产出

加入 1 个 fixture `aspin_J_pocket`, 几何如下 (双错位 cap + 2x4 well):
```
##########   y=0
##.####.##   y=1  cap 错位 (col 2/7 缺口)
##....####   y=2  2x4 well 下半
##....####   y=3  2x4 well 上半
##.####.##   y=4  顶盖错位
..........   y=5
```

实测触发:
- `aspin_J_pocket × J × 1g`: idx=2033 row=1 type=1
- `aspin_J_pocket × J × 20g`: idx=2033 row=1 type=1

其他 6 piece 在该 fixture 下皆 type=0 (符合预期, 只针对 J 设计).

## 之前盲设计失败的 fixture (已删除)

我先按"每 piece 一个 kick-pocket"的设计原则盲写了 7 个 fixture
(`aspin_<piece>_pocket`), 但实测只有 J 那个意外触发了 ASpin (因为 J 形态恰好
匹配 2x4 well + 错位 cap), 其他 6 个 (I/O/T/L/S/Z) 全部 type=1=0:

| fixture | 失败原因 |
|---|---|
| aspin_I_pocket (col 9 1x4 well + col 5 缺口) | I 进 well 必须先旋成 R1, BFS 路径不可达 |
| aspin_O_pocket (2x4 well + 错位 cap) | O 形态对称, 无法在 well 中四邻 immobile |
| aspin_T_pocket (T-stub 槽) | cap 缺口 col 2 离 stub 太远, T 飘不进 |
| aspin_L_pocket | 同上, kick path 推算不出来 |
| aspin_J_pocket (单错位) | J 飘进但邻位有空 |
| aspin_S_pocket / aspin_Z_pocket | S/Z 双柱嵌入需精确 kick |

**结论**: 不离线推算 Botris kick table 是无法盲设计 7 piece × ASpin pocket 的.
本 commit 先收一个最小活样本, 把 ASpin 看门狗从"恒 type=0"提升到"至少有 1 个
type=1 锚点", 后续若需要扩展覆盖, 必须配合 kick table 推算或把 fixture 改成
**"对照 baseline 跑 master 实现 + 新框架, 比对 type 序列一致"** 的形态.

## 后续方向 (不在本 commit 范围)

1. 实现一个 `aspin_diff_master.cpp`: 同时跑 master `search_aspin` (旧 BFS) 和
   新框架 `MoveGenSearch<…, DefaultASpinHook>`, 在每个 fixture 上比 type 序列.
   这样即使 fixture 不触发 ASpin, 也能锁住"两侧都不触发"这个等价性.
2. 离线推算 Botris kick table, 为每个 piece 设计精确 fixture.
3. 接受当前 1 个锚点足够看门狗, 不再扩展.

## 提交内容

- File: `tools/aspin_dump.cpp`
- Diff stat: +26 行 (1 个 fixture + 注释)
- Title: `tools: add aspin_J_pocket fixture for live ASpin watchdog sample`


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
