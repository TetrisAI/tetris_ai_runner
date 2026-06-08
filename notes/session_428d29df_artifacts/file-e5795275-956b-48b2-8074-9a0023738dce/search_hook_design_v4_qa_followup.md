# v4 设计反馈跟进 + Q2/Q5/Q6/Q7 重新解释

## 用户已拍板（v4）

- ✅ Q1：BfsEngine 作为统一框架，search 和 make_path 都基于它 → **是**
- ⏳ Q2：simple make_path（贪婪回放）能否塞进 BfsEngine？ → **要探索看看能不能**
- ✅ Q3：编译期 template trait → **是**
- ✅ Q4：20G 在 NeighborProvider 内自适应 → **是**
- ❓ Q5/Q6/Q7：没理解，需要重新解释

---

## Q2 探索：simple make_path 能否套进 BfsEngine？

### oracle simple make_path 实际做了什么（`search_simple.cpp:15-52`）

```cpp
// 1. 旋转匹配
while (node->status.r != land_point->status.r) {
    node = node->rotate_counterclockwise; path.push_back('z');
}
// 2. 横向匹配（往 land_point.x 走）
while (node->status.x < land_point->status.x) {
    node = node->move_right; path.push_back('r');
}
while (node->status.x > land_point->status.x) {
    node = node->move_left; path.push_back('l');
}
// 3. 试 D 直接 drop
if (node->drop(map) == land_point) { path.push_back('D'); return; }
// 4. 否则用 d 一步步往下
while (node->status.y > land_point->status.y) {
    node = node->move_down; path.push_back('d');
}
// 5. 最后再 drop
if (node->drop(map) != land_point) return failure;
```

这是**确定性线性回放**：每一步都有"唯一对的方向"，没有分支/回溯。

### 套进 BfsEngine 是否合适？

**理论上可以**：
- NoVisited
- NeighborProvider 极端：只产生 1 个邻居（按当前 node 与 target 的差异决定方向）
- Visitor=TargetHit

**但语义不自然**：
- BfsEngine 的核心价值是"图遍历"——队列里同时存在多个候选
- simple make_path 没有候选选择，每步唯一确定
- 强行套进 BfsEngine 等于用 BFS 框架表达"线性循环"，反而绕弯

**结论**：可以套，但不该套。simple make_path 保持独立函数，对外通过 `PathBuilder` concept 接口统一。

→ **Q2 答复：可以套但不应套**。理由：BfsEngine 的语义是图遍历，simple make_path 是确定性线性回放，两者范式不同。

---

## Q5 重新解释：tag 的 land_point 是否需要"复合 key"

### 问题背景

去重 key 决定**何时认为两个落地点是"同一个"**。其他 search（path/simulate/simple）都用 `index_filtered`（绝对位置）。

### tag 的特殊情况

tag 的 land_point cache 里每个元素是 `TetrisNodeWithTSpinType`，包含：
```cpp
struct TetrisNodeWithTSpinType {
    TetrisNode const *node;     // 位置
    TetrisNode const *last;     // 前驱
    bool is_last_rotate;        // 上一步是否旋转
    bool is_ready;              // 是否满足 T-Spin 3-corner
};
```

→ **同一个绝对位置，可能因为不同的"到达路径"产生不同的 TSpin 标签**：
- 路径 A 到达 (5, 4)：上一步是旋转 → `is_last_rotate=true` → 是 T-Spin
- 路径 B 到达 (5, 4)：上一步是平移 → `is_last_rotate=false` → 不是 T-Spin

### 两种处理方式

**方式 A：单 key (index_filtered)，先到为准**
- 第一次到达 (5,4) 时记录，后续再到达直接丢弃
- 优点：简单、与 path/simulate 一致
- 缺点：可能丢失 T-Spin 形态（如果先到的是平移路径，后来的旋转路径就被丢）

**方式 B：复合 key (index_filtered, is_last_rotate)**
- 同一位置允许保留 2 个版本（旋转到达 / 非旋转到达）
- 优点：完整保留 T-Spin 信息
- 缺点：land_point 数量翻倍，AI 评估开销增加

### Q5 解释（去术语）

**问题**：tag 是否需要让 BfsEngine 在去重时"区别对待 旋转到达 vs 非旋转到达"？

如果不区别对待 → 可能丢 T-Spin；如果区别对待 → land_point 翻倍。

### 提议：先验证 oracle 实际行为

写一个一次性工具 `tools/tag_landpoint_collision_dump.cpp`：扫一批典型 board，统计 oracle tag 的 land_point cache 中是否真的存在「同 index 但不同 is_last_rotate / is_ready」的元素。
- 如果存在 → 必须用方式 B
- 如果不存在 → 可以用方式 A（更简单）

这样不预设答案，用事实拍板。

---

## Q6 重新解释：search 与 make_path 的"邻居生成"是否共用

### 问题背景

每个 search 实现都有 search 和 make_path 两个函数，**它们调用的"邻居动作"可能相同也可能不同**。

### oracle 现状

| 实现 | search 邻居 | make_path 邻居 | 是否相同 |
|---|---|---|---|
| simple | 左到底/右到底/旋/drop | r/l/z/D/d 单步 | **不同** |
| path | l/r/L/R/x/z/c/d/D | l/r/L/R/x/z/c/d/D | 相同 |
| simulate | 同 path | 同 path | 相同 |
| tag | l/r/x/z/c/d (无 L/R) | l/r/x/z/c/d (无 L/R) | 相同 |

### 新框架是否强制 search 与 make_path 用同一个 NeighborProvider？

**方式 A：强制共用**
- 优点：代码量少，配置简单
- 缺点：simple 没法实现（search 必须粗、make_path 必须细）

**方式 B：不强制，各自配置**
- 优点：灵活，simple 可实现
- 缺点：配置略多（path/simulate/tag 要写两次同样的 NeighborProvider）

### Q6 解释（去术语）

**问题**：`Search` 类是否应该有 1 个 NeighborProvider（search 与 make_path 共用）还是 2 个（各自配）？

**倾向**：方式 B，两个独立 trait 槽位。需要的实现就配两个相同的（别名一下即可），simple 配两个不同的。

---

## Q7 重新解释：Visitor 的"STOP/CONTINUE"返回值

### 问题背景

BfsEngine 的核心循环：
```cpp
while (queue not empty) {
    node = pop
    [访问 node]   ← 这里要不要让 Visitor 决定"是否提前结束 BFS"？
    expand neighbors
}
```

### 两种语义

**方式 A：永不提前停止**
- BfsEngine 自己判定结束（队列空）
- search 用：永不停
- make_path 用：找到 target 也不停（要等队列空才返回，浪费）

**方式 B：Visitor 可主动 STOP**
- Visitor::on_node 返回 enum {Continue, Stop}
- search 永远 Continue
- make_path 命中 target 时返回 Stop，立刻退出循环（与 oracle 一致：`search_path.cpp:52` 命中即 return）

### Q7 解释（去术语）

**问题**：BfsEngine 内的"节点访问回调"是否需要支持"我看完了，提前结束 BFS"这个语义？

**倾向**：是。search 永远不提前停（性能不变）；make_path 命中 target 立刻退出（与 oracle 一致）。否则 make_path 必须等 BFS 跑完才能返回，性能退化。

---

## 收敛后的待确认问题（最小集）

### Q5 → 是否同意：先写 `tools/tag_landpoint_collision_dump.cpp` 用事实决定 key 形态？
- 同意/不同意

### Q6 → search 与 make_path 用 1 个还是 2 个 NeighborProvider trait 槽位？
- 倾向 2 个独立槽位，需要时配置相同别名即可

### Q7 → Visitor 是否支持「提前 STOP」返回值？
- 倾向支持，理由是 make_path 找到 target 立即返回（oracle 现状）

---

## 状态

- branch: `flip-bits-clean`
- HEAD: `6758178` (Phase 0 已 commit 未 push)
- v4 设计 Q1/Q3/Q4 已确认，Q2 探索完成（结论：可套但不应套），Q5/Q6/Q7 等待用户基于上面解释回复
