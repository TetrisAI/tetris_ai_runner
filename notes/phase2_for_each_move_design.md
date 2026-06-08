# Phase 2：for_each_move<t,r> 接入 Search 层

> 调研时间：2026-06-04
> 基于：`compile_time_eval_arch.md` 第三、四、九节 + 2026-06-02 会话决议

---

## 一、当前状态（现状）

```
TetrisCore::eval(context, map, land_point_node, child)
    └─ BBCallEval<AI, Spec>::eval(ai, identity, new_map, map, clear)
           ├─ route_piece(identity->status.t, fn<T>)   // 运行时 t → 编译期 T
           └─ for_each_typed_r<Spec, T>(r, fn<R>)      // 运行时 r → 编译期 R
                  └─ ai.template eval<T,R>(BBNode<...>, Map, clear)
```

运行时 dispatch 点：`route_piece`（t，1次）+ `for_each_typed_r`（r，1次）= 每个落点 2 次运行时路由。

Search 层（`run_piece<T>` 内的 collect lambda）：
- `T` 已编译期（template 参数）
- `lp.r` 是运行时值（`uint8_t`），collect lambda 不感知 R

---

## 二、目标形态（决议）

```
run_piece<T>(ctx, map, depth)
    └─ collect([&]<uint8_t R>(lp_r)  // R 编译期
            ├─ 构造 PendingEval{ eval_fn = [T,R 捕获的 lambda](after, before, clear) → double }
            ├─                  x = lp_r.x, y = lp_r.y, spin = lp_r.spin_u8 }
            └─ push_back to pending_
       )

TetrisCore::eval → 遍历 pending_，直接调 pe.eval_fn(after, before, clear)
                   不再走 BBCallEval 内部的 route_piece / for_each_typed_r
```

多线程说明（来自 2026-06-02 决议 + 第九节）：
- `eval_fn` 是捕获 `<T,R>` 的 lambda，可直接扔进工作队列并发执行
- `after` 指针指向树节点的 `TetrisMap`（由 `node->attach` 提前完成），无需拷贝 Map
- 单线程：顺序执行，编译器可完全内联

---

## 三、需要改动的文件 & 层次

### 3.1 Search 层：`run_piece<T>` 内的 collect lambda

涉及文件：
- `src/search_path.h`（`PathStrategy::run_piece<T>`）
- `src/search_simulate.h`（`SimulateStrategy::run_piece<T>`）
- `src/search_tag.h`（`TagStrategy::run_piece<T>`）
- `src/search_tspin.h` / `src/search_aspin.h`（使用 PathStrategy 的包装）

当前 collect lambda 签名（以 SimulateStrategy 为例）：
```cpp
auto collect = [&](LandingPosT<SpinHook> lp) {
    // lp.r 是运行时值
    ctx.land_point_cache_.emplace_back(LandPoint{...});
};
```

目标：collect lambda 在内部对 `lp.r` 做编译期展开，或者把 `R` 提升成模板参数：

**方案 A（最小改动）**：collect lambda 内部调 `for_each_typed_r<Spec, T>(lp.r, fn<R>)`，fn<R> 里构造 `PendingEval`。
- 优点：Search 层结构基本不变，改动最小
- 缺点：`for_each_typed_r` 还在 Search 层执行（只是从 `BBCallEval` 挪到了 Search 层），编译期 R 的可见时机提前了，但展开点从 eval 内移到了 search 内

**方案 B（架构完整）**：`run_piece<T>` 直接改成 `run_piece_r<T, R>`，外层用 `for_each_rotation_count<T>` 展开，collect callback 原生带 `R` 模板参数。
- 优点：R 彻底是编译期，collect lambda 无额外路由
- 缺点：改动面较大，`run_piece` 需要拆成两层

### 3.2 PendingEval 结构定义

新增（建议放在 `bb_eval_bridge.h` 或新文件 `bb_pending_eval.h`）：

```cpp
struct PendingEval
{
    std::function<double(TetrisMap const &, TetrisMap const &, int)> eval_fn;
    // 或者 using EvalFn = ... 避免 std::function 开销（可用手写 FunctionRef）
    TetrisNode const *node;   // 落点 master node（用于 TetrisTreeNode::identity）
    uint8_t spin_u8;
};
```

注意：`eval_fn` 捕获了编译期 `<T,R>` 的 lambda，但存储为类型擦除的 callable。
- **单线程路径**：编译器大概率内联（lambda → std::function 会阻断内联）
- 如果要保留内联能力，需要用手写的 `FunctionRef<T>` 或 `UniqueFunction<T>`

### 3.3 TetrisCore 调用点

`TetrisCore::eval` 目前：
```cpp
tree_node->result = BBCallEval<TetrisAI, Spec>::eval(ai, identity, new_map, map, clear);
```

目标：
```cpp
// BBCallEval 被绕过，直接调 pending 里预存的 eval_fn
tree_node->result = pending_eval.eval_fn(new_map, map, (int)clear);
```

但这要求 `pending_eval` 能被 `TetrisCore::eval` 访问——意味着 `LandPoint` 的传递方式要改，
或者 `TetrisTreeNode::identity` 附带 `eval_fn`。

---

## 四、待确认的架构问题

1. **`eval_fn` 存储方式**：
   - `std::function`：有堆分配开销，阻断内联
   - 手写 `FunctionRef`（非持有）：零开销，但 PendingEval 必须在 lambda 生命周期内消费
   - 直接内联（不存 pending，collect 时立即 eval）：相当于回退到方案 A 的"提前 dispatch"，无批量能力

2. **多线程是否需要在本 Phase 实现**：
   - 若本 Phase 仅做单线程的"dispatch 提前"，`eval_fn` 直接用 lambda 引用即可
   - 若要为多线程预留接口，需要确定 `PendingEval` 的存储方式（是否 owning）

3. **Search 层改动方案**：方案 A vs 方案 B，以及是否三个 Strategy 同步改还是先改一个

---

## 五、结论 / 下一步

当前阶段，最稳健的路径：

1. **先讨论 `eval_fn` 存储方式**（`std::function` vs `FunctionRef` vs 立即 eval）
2. 确认 Search 层改动范围（方案 A 最小侵入，方案 B 架构更完整）
3. 确认多线程是否在本 Phase 内实现
4. 动手前更新 `bitboard_refactor_plan.md` 的 Commit 4 待完成项

---

*文件用途：记录调研结论，供下次会话快速恢复工作状态。*
