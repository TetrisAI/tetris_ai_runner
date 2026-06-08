# Hook 形态质询答辩 (第二轮)

> 续 `hook_form_critique.md`. HEAD = `92d0804`.

---

## Q7. ASpinPayload / TSpinPayload 合并为 `ExTypePayload`?

> 注: 用户原文写"ASpinPayload/TSpinHook 合并", 推断意图是 Payload 合并 (TSpinHook 与 ASpinHook 算法行为差距过大, 不可能合并整个 Hook).

### 现状对比
```cpp
struct TSpinPayload {           // 5 字节
    std::uint8_t spin;          // 0=None / 1=Mini / 2=Full
    std::int8_t  last_x;
    std::int8_t  last_y;
    std::uint8_t last_r;
    std::uint8_t has_last_rot;
};
struct ASpinPayload {           // 1 字节
    std::uint8_t aspin;         // 0/1
};
```

### 可行性分析

如果合并:
```cpp
struct ExTypePayload {           // 5 字节
    std::uint8_t type;           // 0=None / 1=TSpinMini / 2=TSpinFull / 3=ASpin
    std::int8_t  last_x;         // 仅 TSpin 写
    std::int8_t  last_y;         // 仅 TSpin 写
    std::uint8_t last_r;         // 仅 TSpin 写
    std::uint8_t has_last_rot;   // 仅 TSpin 写
};
```
- TSpin 路径: 5 字段全用.
- ASpin 路径: 仅写 type, 4 字节空载.

### 代价
- ASpin emit 数 LandingPos 增加 4 字节 × N 落点 ≈ 800B 一帧, 可忽略;
- 类型统一后 `LandingPosT<Hook>::extra` 在 NoHook/NoSpinHook/Cautious/TSpin/ASpin 都用同 1 个 ExTypePayload 类型 (前 3 个走 EmptyPayload), 总共类型从 4 种降到 2 种 (Empty / ExType).

### 配套思考: LandPoint 字段也可以同步统一

观察 `apply_emit_1g`:
- TSpinHook 写 `node_ex.is_ready / is_mini_ready / is_last_rotate / last`, **不写 node_ex.type** (type 由 AI eval/get 改写);
- ASpinHook 写 `node_ex.type` (因为 ASpin 没有 ai 端 get() 改写流程, 直接 search 阶段写定);

→ "type" 在 TSpin / ASpin 两路语义是错位的 (一个是 ai 改写的"我评价的 spin 类别", 一个是 search 直接写的"我位板判出的 spin 类别"). 合并 Payload 不会自动解开这层错位.

### 评估: 同意合并, 但需要拆分两步

| 方案 | 落地 | 风险 | 备注 |
| :--- | :--- | :--- | :--- |
| **Q7.a** Payload 合并到 `ExTypePayload` | 形态合并, type 用 ASpin/TSpinFull/Mini/None 4 状态 | 低 | ASpin 浪费 4B/emit, 可忽略 |
| **Q7.b** apply_emit_1g 行为对齐: 都写 `lp.type` | 改 TSpinHook::apply_emit_1g 写 `lp.type=TSpinFull/Mini`, 同步 ai_zzz 的 get() 流程 (移除其改写 type 的逻辑) | **中高** | 行为变更, AI 评价器可能依赖 search 写定的初值 |
| **Q7.c** 仅 Payload 合并, LandPoint 字段保持各自 | Q7.a only, 不动 LandPoint 的 .type/.is_ready/.is_mini_ready 错位 | 低 | 可独立落 |

**推荐: Q7.c (Q7.a only)**. 仅合并 Payload, LandPoint 字段错位下沉到后续 R.6 LandPoint 位板化时再一并清理. Payload 合并是接口减重, 不动 ai 端契约.

> 与 Q1.a (NoTSpinPayload→EmptyPayload) 应该同一个 commit 落地.

---

## Q8. `block_buffer[52]` 不够"位板"? 它是为加速三角判定吗?

### 它是为加速三角判定. 是的.

`TSpinHook::check_ready` 是**T-spin 3-corner 判定**:
- 给定 T-piece 在 master 坐标 `(node->status.x, node->status.y)`, pivot 周围 4 个角是否 ≥3 个被占用 (含越界).
- `block_buffer[node->status.x]` 预存了"对应 status.x 的两个角的 column mask" `(1<<(x-1)) | (1<<(x+1))`;
- 然后 `popcount(map.row[y-1] & block) + popcount(map.row[y+1] & block) + edge_count >= 3`.

### 它确实**不够位板**

理由:
1. **它依赖 master 坐标系** (`node->status.x/y`), 不是位板坐标 `BBState`;
2. **它在 BFS 期外运行时调用** (search_tag.h:877 `search_t_native` 对每个 sunk_node 调一次);
3. **位板路径已经有更优雅的等价物**: `RotState::corners3_arr[r]` —— 在 `MoveGen::generate` 的 `on_init_rotations` 阶段一次性算好"≥3 corner 占用 (含越界)"的 R-array map_t. 然后 `landings & last_rotate_arr & corners3_arr[r]` 一步出 ready 集合.

### 为什么还存在?

`check_ready` 仅服务 `tag::search_t_native` —— 这条路径**还未位板化**, 仍走 master node + run_piece + 对每个 sunk_node 逐一询问 ready. 等 N.3 (`search_tag.h` 旋转/搜索去 master 化, 走位板 corners3_arr) 落地后:
- `TSpinHook::check_ready` 整个函数可删;
- `SearchState` (x_diff / y_diff / block_buffer[52]) 整个 struct 可删;
- `on_search_state_init` 整个函数可删;
- A4 接口面收窄.

### 短期 / 中期 路径

| 阶段 | 修正 | 风险 |
| :--- | :--- | :--- |
| **短期 (Q4.c)** | 维持 block_buffer 字面行为, 把 52 / +10 命名为 `kBlockBufferLen / kSafeXDiffPad` 常量 + 注释来源 | 极低 |
| **中期 (随 N.3)** | tag::search_t 改走 BFS + `corners3_arr` (已经在 path/simulate 里有现成实现), 一步删 SearchState | 中 |
| **远期 (R.6)** | 配合 LandPoint 位板化, 整个 master 坐标系都从 hook 消失 | 高 |

### 评估

block_buffer[52] 的"魔数" 与 "不位板" 是一对孪生病灶:
- 魔数因为 oracle 1:1 复刻;
- 不位板因为 tag 还没走完位板化.

**推荐**:
- 立刻执行 Q4.c (命名常量 + 注释).
- 下一轮等 tag 位板化时, 整体删 SearchState. 不必现在重写为别的"位板加速结构".

---

## Q9. NoSpinHook / CautiousHook 真的需要 `is_landpoint_none = true` 吗?

### 调用面回顾 (search_path.h)
```cpp
// L841 短路: 起点 == 终点
if (SpinHook::is_landpoint_none(land_point) &&
    Helpers::cells_key_for(node) == Helpers::cells_key_for(land_point.node))
    return std::vector<char>();

// L850 索引选择
TetrisNode const *land_last = SpinHook::get_last_node(land_point);
bb::CellsKey index = (SpinHook::is_landpoint_none(land_point) || land_last == nullptr)
                         ? Helpers::cells_key_for(land_point.node)
                         : Helpers::cells_key_for(land_last);

// L924, L1052 visitor 命中谓词
visitor.landpoint_is_none = SpinHook::is_landpoint_none(land_point);
```

### 关键发现

**`is_landpoint_none` 在 search 出来的 lp 上, TSpinHook 路径**也**永远是 true**.

证据: `TSpinHook::apply_emit_1g` 与 `apply_emit_20g` 都**没有写 `node_ex.type`** (movegen_hook.h:427-437, 469-481). 它们写 `is_ready / is_mini_ready / is_last_rotate / last`, 不动 `type`. `TetrisNodeWithTSpinType` 默认构造把 type=None 清零, 所以 search 阶段产出的 lp 永远 type=None.

`type` 真正被改写发生在 **AI 端 `get()` 流程**:
- `ai_zzz.cpp:794-804 / 1042-1050 / ...`: AI 评估器在 get() 内根据 result 写 `node.type = TSpinFull/Mini/None`;
- AI 端拿走 lp, 改写 type, 再回头调 `engine.make_path(node, land_point, map)`.

→ **make_path 阶段, lp 是被 AI get() 改过 type 的副本**. 此时:
- AI 想走 spin 路径 → type=TSpinFull/Mini → is_landpoint_none=false → strategy 进 spin make_path 分支;
- AI 不走 spin → type=None → is_landpoint_none=true → strategy 进 None 短路 + 普通 make_path;

### NoSpinHook / Cautious 路径下永远 true 是否合理?

在 NoSpin/Cautious 路径下, AI eval/get 流程**根本不写 type** (qq::Attack::eval 接收的是裸 TetrisNode\*, C2 同样). 所以 lp.type 一直是 None. `is_landpoint_none = true` 是**事实正确的语义**, 不是"为了让 strategy 进 None 分支故意写死".

### 是否能改成 false?

可以, **行为等价**, 仅多走死分支:
- L850 `index` 选择: NoSpin 路径下 `land_last==nullptr` 必然成立, 短路成 `cells_key(lp.node)`, 与 true 路径同;
- visitor 命中谓词多一个 `last_rotate && k==index_landpoint` 判断, 但 NoSpin 路径下 `last_rotate=false` 让该分支恒不命中;
- L841 自落点短路会丢失, 起点==终点的边界 case 多走一次完整 BFS 才返回空 path —— **这是性能损失, 不是正确性问题**.

→ **结论: 改 false 性能略劣, 改 true 是合理优化**. 当前形态正确.

### 真正的优化空间

`is_landpoint_none` 是**运行时 trait** (因为 type 是 AI get() 改写的运行时数据). 但 NoSpin/Cautious 已知"type 永远 None" 是**编译期事实**, 所以可以下沉到编译期:

```cpp
// BaseSpinHook 默认值
static constexpr bool has_runtime_spin_state = false;
// TSpinHook / ASpinHook override
static constexpr bool has_runtime_spin_state = true;
```
strategy 端:
```cpp
bool landpoint_is_none = SpinHook::has_runtime_spin_state
                             ? SpinHook::is_landpoint_none(land_point)
                             : true;
```
- NoSpin/Cautious: 编译期裁掉 `is_landpoint_none(lp)` 调用, 直接 true 字面量;
- TSpin/ASpin: 维持运行时查询.

### 评估

| 方案 | 落地 | 风险 | 收益 |
| :--- | :--- | :--- | :--- |
| **Q9.a** 维持现状 (false 也对, 但 true 更优) | — | — | 性能保留, 代码冗余 |
| **Q9.b** BaseSpinHook 默认 true, NoSpin/Cautious 删自家副本 | R.1 顺手做 | 极低 | 接口面 -2 处副本 |
| **Q9.c** 加 `has_runtime_spin_state` 编译期 trait, strategy 端 if constexpr | 改 strategy + Hook | 低 | 编译期裁分支, 性能微提升 |

**推荐: Q9.b (R.1 顺手)**. Q9.c 收益微小, 改动延伸到 strategy 端比 R.1 更深, 暂不纳入.

### 进一步追问 (回到用户原意)

如果用户问的是"NoSpin/Cautious 真的需要这个 trait 接口吗", 答: **接口必要**. 因为:
- TSpinHook/ASpinHook 必须运行时查询;
- NoSpin/Cautious 必须给出常量, 才能让 strategy 模板化的 make_path 分支成立;
- 删掉 trait 等于删掉 spin / non-spin make_path 的统一抽象, 代价远大于保留一个常量 stub.

---

## 10. 综合修正路线 (融合 Q1-Q9)

| # | 修正 | 关联 Q | 风险 |
| :--- | :--- | :--- | :--- |
| **N.1** | NoTSpinPayload → EmptyPayload + ExTypePayload 合并 (Q7.c, 仅 Payload, 不动 LandPoint) | Q1, Q7 | 低 |
| **N.2** | 引入 PlainLandPoint, NoSpinHook + CautiousHook 切换 | Q2, Q3 | 中 |
| **N.3** | block_buffer[52]+10 → 命名常量 (Q4.c) | Q4, Q8 (短期) | 低 |
| **N.4** | BaseSpinHook CRTP 默认实现 (含 is_landpoint_none / get_last_node / 7 个 config) | Q5, Q6, Q9 | 低 |
| **N.5 (远期)** | tag::search_t 位板化, 删 SearchState/x_diff/y_diff/block_buffer/check_ready/on_search_state_init | Q8 (中期) | 中 |

---

## 11. 待用户裁决

- [ ] **Q7.c** Payload 合并 ExTypePayload (仅 Payload, LandPoint 字段错位下沉到 R.6) — 同意 / 否决
- [ ] **Q7.b** 是否同步对齐 apply_emit_1g 写 lp.type (动 ai_zzz get() 流程) — 同意 / 否决 / 暂缓
- [ ] **Q8 短期** Q4.c 命名常量 + 来源注释, 维持 byte-equal — 同意 / 否决
- [ ] **Q8 中期** 等 tag 位板化时整体删 SearchState — 同意 / 否决
- [ ] **Q9.b** is_landpoint_none 收敛到 BaseSpinHook 默认 true — 同意 / 否决
- [ ] **Q9.c** 进一步加 `has_runtime_spin_state` 编译期 trait — 同意 / 否决 / 暂缓
