# Hook 边界反驳: 20G / make_path 是否应作为 Hook

> 时间: 2026-05-29  会话: flip-bits-clean (head=6fce037)
> 用户提议: "search 的 20G，也尝试作为 Hook，去除单独的实现？make_path，也作为 Hook？"
> 我的态度: **反驳，先达成共识再动手**

## 当前 Hook 抽象边界 (movegen_hook.h)

Hook 现在做的事 (五件):
1. `Payload` / `LandPoint` — emit 时挂在 land_point 上的元数据形状
2. `Config` + `config_*` trait — 读 config 字段 (180-kick / LR / d / D / rotate_move / last_rotate / 20g)
3. `RotState` + `on_init_rotations` / `on_rotate_reach` — BFS 期间累积 per-rotation 元数据 (corners3, last_rotate_arr)
4. `SearchState` + `on_search_state_init` — init 阶段一次性建好的辅助数据 (T-spin 用的 block_data / x_diff / y_diff)
5. `on_emit` — landings 集合 → 按 spin 类别分组打回 fn (full / mini / none)

共同特征: **"插槽式元数据回填"** — hook 不写算法主循环, 只在框架定义好的 BFS / make_path 流水线里，按 piece / config 做点位回填。

## 20G 不应作为 Hook 的理由

### 1. 算法主循环是 RuleSpec / 位板算法，不是 search-flavor

`MoveGenSearch::run_piece_20g<T, EnableT>` 当前做的事:
- BBState 位板 BFS, 邻居展开走 RuleSpec::ops + usable_arr
- drop_bb_state / first_passing_kick_bb 都是 RuleSpec 静态表展开
- T 块 EnableT=true 走带 spin 的 emit 路径, 否则走纯 landings emit

这套主循环跟 `TSpinHook / CautiousHook / NoSpinHook` 选哪个**没有逻辑相关**。差异完全是
`EnableT = Hook::active_for_piece<T>` 这一行编译期 dispatch 出来的元数据填充分支。

### 2. 已经有 hook 抽象，再搬入 hook 是抽象重复

20G "差异点"是: **emit 时是否写 spin 元数据 / corners3 / last_rotate_arr**。
这部分已经通过 `on_init_rotations / on_rotate_reach / on_emit / active_for_piece<T>`
在 hook 层暴露; BFS 主循环本身没有 search-specific 行为。

把"整个 20G 算法"搬进 hook 等价于:
- 每个 hook 重新实现一遍位板 BFS, **代码重复 N 倍**
- 框架不再是统一收敛点, RuleSpec ops 驱动 / 位板 dedup / kick 重放等优化得在多个 hook 里同步演化
- 反向破坏 commit 6c (20g make_path 走位板原生 BFS) 的收敛收益

### 3. 没有具体的差异化需求支撑

目前 CMake 产线:
- `srs_ai` (rule_toj) 走 NoHook / TSpinHook, 1g/20g 都用框架 BFS
- `c2_ai` (rule_c2) 走 CautiousHook, 仅 1g
- `qq_ai` Path / Simulate / Simple 全 NoSpinHook, 仅 1g
- `tetris_ai` (rule_st::Dig) 走 PathHook, 仅 1g

没有任何 hook 需要"我自己的 20G BFS 长得跟别人不一样"。把 20G 抽到 hook 是**为不存在的差异预留接口**, 是 over-abstraction。

## make_path 不应作为 Hook 的理由

### 1. 位板 make_path 与 search-flavor 解耦

`MoveGenSearch::make_path()` / `make_path_20g_native()` 当前做的事:
- 位板 BFS + RuleSpec 静态表
- 末段 wall-kick 重放 (try_kick_chain_to)
- cells_key 等价类 dedup
- 通过 `Hook::is_landpoint_none(land_point)` / `Hook::get_last_node(land_point)` 读 LandPoint 的 spin 元数据

LandPoint 元数据的"读"已经走 hook trait (is_landpoint_none / get_last_node)，
算法主循环本身是位板 + RuleSpec, **跟 spin 类型解耦**。

### 2. search-specific 的"末段补旋转"是局部 hook 钩子, 不是整个 make_path

`search_tag_node::Search::make_path` 末尾有这段:
```cpp
if (land_point.type != None && (path.empty() || (path.back() != 'c' && path.back() != 'z')))
{
    // 递归找前驱 + 强制补 'z' 或 'c'
}
```

这是真正的 search-specific 行为 (tag 把 last_rotate 信息固化进 LandPoint, 强制让最后一步是旋转)。
但现在 search_tag_node 还没接 MoveGenSearch — 它本身就是独立 BFS 实现 (oracle baseline)。
**未来要接 MoveGenSearch + TagHook 时**, 这段逻辑可以通过新增一个细粒度 hook 钩子
(e.g. `on_make_path_postfix(LandPoint const&, std::vector<char>&)` 或
`fixup_path_for_spin(...)`) 暴露, **不需要把整个 make_path 搬走**。

### 3. "位板算法 / hook 元数据"二分法已经稳定

当前抽象层级:
```
MoveGenSearch (框架)        ← 位板算法 / RuleSpec ops / BFS 主循环
    ├── Hook                ← 元数据形状 + 回填点
    │   ├── NoHook / NoSpinHook   (无 spin)
    │   ├── CautiousHook          (无 spin + fast_move_down)
    │   ├── TSpinHook             (T-spin 元数据)
    │   └── ASpinHook / DefaultASpinHook  (any-spin 元数据)
```

这个二分稳定的好处: hook 写起来简短 (NoSpinHook 全空实现, ASpinHook ~150 行),
框架 (MoveGenSearch) 优化所有 search 都受益。

把 20G / make_path 整个搬入 hook 会让 hook 膨胀到几百行算法代码,
NoSpinHook 也要复制一份位板 BFS — 这是反方向。

## 反驳总结

| 提案 | 真正的 search-specific 差异 | 当前已覆盖 | 全量搬入 hook 的代价 |
|---|---|---|---|
| 20G 作为 Hook | 无 (差异在 emit 元数据, 不在 BFS) | ✅ on_emit + active_for_piece + EnableT | 代码重复 N 倍, 框架优化收敛点丢失 |
| make_path 作为 Hook | 仅 tag 末段补旋转一处 | LandPoint trait (is_none / get_last) 已覆盖元数据读 | 位板 BFS / kick 重放重复 N 倍 |

## 我的反提议

**不动当前 hook 边界**, 维持"算法在框架, 元数据在 hook"的二分。

如果之后接 search_tag_node → MoveGenSearch + TagHook 时遇到"末段补旋转"场景,
**新增一个细粒度 hook 钩子** (e.g. on_make_path_postfix) 而不是搬走整个 make_path。
这是渐进式扩展, 不破坏现有抽象。

## 想跟你确认的一点

你提出这个想法的具体痛点是什么?
- (A) 看到 MoveGenSearch 里的 run_piece_20g / make_path / make_path_20g_native 三个函数觉得"框架太胖, 应该拆"
- (B) 后面打算接 search_tag_node 进 MoveGenSearch, 担心 tag 的 make_path 末段补旋转无处安放
- (C) 别的具体改造目标 (描述给我)

针对 (A) 我的判断是 over-abstraction; 针对 (B) 我的提议是细粒度 hook 钩子;
(C) 看你具体场景再说。

**先达成共识再动手。** 此时不做任何代码变更。
