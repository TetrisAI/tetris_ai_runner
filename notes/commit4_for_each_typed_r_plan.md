# Commit 4 Phase 1：for_each_typed_r 实施方案

> 写入时间：2026-06-03  
> 目标：解除 Commit 4 的 blocker，实现 r → 编译期 R 的 dispatch 机制

---

## 背景

`compile_time_eval_arch.md` 要求：
```
ml.for_each_move([&]<Rotation r>(int x, int y, SpinType s) {
    pending.push(TypedLandPoint<t, r>{x, y, s, after, before, clear});
});
```

现有 `search_path.h` 的 `run_piece<T>` 的 `collect` lambda 里，`lp.r` 是运行时 `uint8_t`。
但第 224-231 行已经有了展开模式：
```cpp
[&]<size_t... Rs>(index_sequence<Rs...>) {
    ((Rs == lp.r ? (st_bb = ...<T, Rs>(...), true) : false) || ...);
}(make_index_sequence<rcount_v<T>()>{});
```
这是 r → 编译期 R 的标准展开。

---

## Phase 1：`typed_dispatch.h`（纯工具层）

**文件**：`src/typed_dispatch.h`

**提供**：
```cpp
// for_each_typed_r<R_count>(runtime_r, fn<R>)
// 对 runtime_r == R 时，以 R 为 NTTP 调用 fn<R>()
// fn 必须是 auto lambda，接受一个 integral_constant<size_t, R> 参数
// 返回值：fn 是否命中（debug 用）
template<size_t R_count, class Fn>
inline bool for_each_typed_r(size_t r, Fn &&fn);
```

**实现原理**：index_sequence + short-circuit `||`：
```cpp
template<size_t R_count, class Fn>
inline bool for_each_typed_r(size_t r, Fn &&fn) {
    return [&]<size_t... Rs>(index_sequence<Rs...>) {
        return ((Rs == r ? (fn(integral_constant<size_t, Rs>{}), true) : false) || ...);
    }(make_index_sequence<R_count>{});
}
```

**TypedLandPoint<T, R>** 结构（仅数据，不含 eval 逻辑）：
```cpp
template<char T, uint8_t R>
struct TypedLandPoint {
    static constexpr char  piece     = T;
    static constexpr uint8_t rotation = R;
    int8_t  x;
    int8_t  y;
    uint8_t spin;   // SpinType 的底层值（0=None,1=Mini,2=Full,3=ASpin）
};
```

---

## Phase 1 验证

在 `tests/` 下加 `typed_dispatch_test.cpp`，用 `static_assert` 验证：
- `for_each_typed_r<4>(2, fn)` 只触发 `fn<2>`，不触发 `fn<0/1/3>`
- `TypedLandPoint<'T', 2>` 的 `piece == 'T'`，`rotation == 2`

---

## Phase 2（后续）：run_piece<T> 中接入 eval

在 `collect` lambda 内，继续复用 `for_each_typed_r`：
```cpp
for_each_typed_r<rcount_v<T>()>(lp.r, [&]<size_t R>(integral_constant<size_t, R>) {
    // 此处 T, R 均为编译期
    TypedLandPoint<T, static_cast<uint8_t>(R)> tlp{lp.x, lp.y, spin_val};
    ctx.pending_evals_.push_back_typed(tlp);  // 待 Commit 4 主干实现
});
```

---

## 当前状态

- [x] 方案确认
- [ ] `typed_dispatch.h` 实现
- [ ] `typed_dispatch_test.cpp` 验证
- [ ] Phase 2：run_piece<T> 接入
