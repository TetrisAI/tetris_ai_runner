# PathMark 容量按"实际可能 piece 集合"参数化 — 调研

## 结论
原则成立, 实施. 但要先看清两个事实:

### 事实 1: SRS-7 + TSpinHook 下不会有 byte 数节省
PathMarkBit/PathMark 的容量公式是 `kR × kCells`, kCells 全 piece 共享.
关键变量是 kR = "该字段实际承载的最大 rotation 数".

SRS-7 的 rcount:
- O=1, I=2, S=2, Z=2, L=4, J=4, T=4

按 piece 集合算 max:
- 全集 (kMaxR): 4
- TSpinHook 下非 T-spin 集 (= !active_for_piece<T>) = {O,I,S,Z,L,J}: max = 4 (L 或 J)
- TSpinHook 下 T-spin 集 = {T}: max = 4
- NoSpinHook 下非 spin 集 = {O,I,S,Z,L,J,T}: max = 4

→ **三个字段在 SRS-7 上的 kR 都是 4, byte 数完全相同**.

那为什么还要做? 见事实 2.

### 事实 2: 这是"静态契约自洽"的修正
- `t_mark_` 只承载 T-spin piece (active_for_piece<T> == true) 的 BFS,
  其 r 维度 ≤ max{rcount<T> : active}. 用 kMaxR 是过度表达 — 类型上允许写
  超出 t_mark_ 实际可能 r 的 cell, 编译期就该挡住.
- `search_mark_` 同理, 只承载非 spin piece 的 BFS.
- 编译期参数化后:
  * 类型上声明"此字段最大可能 r = X", 后续 dedup 的 r 范围 check 就能从
    运行时 `s.r >= kMaxR` 收窄到 `s.r >= X`, 编译期就能检测到误用 (例如
    把 T 的 BFS 状态喂进 search_mark_).
  * 加入新规则 (5-cell Penta, SZ-spin 等) 时, 容量自动随 piece 集合收缩,
    不需要重新检查 bound 逻辑.
  * 删掉了"t_mark_ 容量比实际多 (kMaxR vs rcount<T>)"的潜在 bug 面.

## 实施方案

### Step 1: bb_state.h 参数化 PathMark / PathMarkBit
当前签名:
```cpp
struct PathMark {
    static constexpr int kR = kMaxR;
    // ... cell_ver_[kR][kCells] etc.
};
struct PathMarkBit {
    static constexpr int kR = kMaxR;
    // ... bits_[kWords] where kBits = kR*kCells
};
```

改为接受 RBound 非类型模板参数, 默认 = kMaxR (向后兼容):
```cpp
template<int RBound = kMaxR>
struct PathMarkT { static constexpr int kR = RBound; ...};

template<int RBound = kMaxR>
struct PathMarkBitT { static constexpr int kR = RBound; ...};

// 别名: 旧名字 = 默认绑定到 kMaxR
using PathMark = PathMarkT<>;
using PathMarkBit = PathMarkBitT<>;
```

### Step 2: bb 命名空间提供 max-rotation-by-predicate 工具
`bb_state.h` 已有 `MaxRotation<Spec, ops>`. 在它旁边加:

```cpp
//在 RuleSpec::ops 中, 仅在 Pred<OpDesc::type>::value == true 的 OpDesc 集合上
// 取 max rotation. Pred 形如 template<char T> struct X { static constexpr bool value; };
template<class Spec, class Tuple, template<char> class Pred>
struct MaxRotationIf;

template<class Spec, class... Ops, template<char> class Pred>
struct MaxRotationIf<Spec, std::tuple<Ops...>, Pred> {
    static constexpr std::size_t value = []() {
        std::size_t m = 0;
        ((Pred<Ops::type>::value
              ? (m = (shape::rotation_count<Spec, Ops::type> > m
                          ? shape::rotation_count<Spec, Ops::type>
                          : m))
              : 0),
         ...);
        return m == 0 ? 1 : m;  //空集兜底 1, 避免 kBits=0 触发 kWords=0.
    }();
};
```

### Step 3: tag::ExtrasMixin 字段 kR 收紧
```cpp
template<class SpinHook, class RuleSpec>
struct ExtrasMixin {
    // search_mark_: 仅承载 TagSearch{1g,20g} 的非 spin piece BFS.
    template<char T>
    struct NonSpinPred {
        static constexpr bool value = !SpinHook::template active_for_piece<T>;
    };
    static constexpr int kSearchR = static_cast<int>(
        bb::MaxRotationIf<RuleSpec, typename RuleSpec::ops, NonSpinPred>::value);

    // t_mark_: 仅承载 search_t_native (active_for_piece<T> == true) 的 BFS.
    template<char T>
    struct SpinPred {
        static constexpr bool value = SpinHook::template active_for_piece<T>;
    };
    static constexpr int kTSearchR = static_cast<int>(
        bb::MaxRotationIf<RuleSpec, typename RuleSpec::ops, SpinPred>::value);

    // make_path_mark_: make_path_none 接收任意 piece, 全 piece max.
    static constexpr int kMakePathR = bb::Helpers<RuleSpec>::kMaxR;

    typename bb::Helpers<RuleSpec>::template PathMarkBitT<kSearchR> search_mark_{};
    typename bb::Helpers<RuleSpec>::template PathMarkT<kTSearchR>   t_mark_{};
    typename bb::Helpers<RuleSpec>::template PathMarkT<kMakePathR>  make_path_mark_{};
};
```

注: `make_path_none` 是被任意 piece 调用的函数 (run_piece_dispatch 在非 T 路径的回溯阶段也调用), 全 piece max 不能缩.

### Step 4: dedup struct 模板化
`TagSearchDedup`/`TagSearch20gDedup`/`TagSearchTDedup`/`TagMakePathDedup` 的
`PathMark*` / `PathMarkBit*` 字段类型改为对应 RBound 模板别名. 由于 dedup
struct 已经在 TagStrategy 类内, 直接引用 ExtrasMixin 类型别名即可.

### Step 5: 边界 check 同步收紧
所有 `s.r >= kMaxR` check 在对应 dedup 内改成 `s.r >= path_mark->kR`
(或对应 RBound 编译期常量). 行为保持一致 — 当前 bbox 状态 r 来自 RuleSpec,
不会超出对应 piece 集合的 max. 但类型自洽后, 越界写入会更早被挡.

## 影响面
- search_path.h / search_simple.h / search_simulate.h 不受影响 — 它们的
  PathMark 走的是 `bb::Helpers::PathMark` (默认 kR=kMaxR), 不收紧.
  如果未来想全局推广, 沿用本套 MaxRotationIf 工具即可, 各 strategy 按自己的
  piece 集合声明 kR.
- 兼容性: `PathMark` / `PathMarkBit` 旧名字保留, 走默认 RBound = kMaxR.
- 编译期成本: 多 1-2 个模板实例化 (按 ExtrasMixin RBound 数算), 可忽略.

## 风险
- 空 piece 集合 (Pred 全 false): 例如 NoSpinHook 下 SpinPred 为空, kTSearchR=0
  会导致 `bits_[(0*kCells+63)/64]` = 0 word, 编译会出 zero-size array. 已在
  MaxRotationIf 内兜底成 1 (m==0 → 1). 但 NoSpinHook 下 t_mark_ 字段是死字段,
  根本不会被实例化 — 实际应该用 `[[no_unique_address]]` 占位空类型, 或者用
  if constexpr 在编译期跳过整个字段. 简单起见, 兜底 kR=1 让结构占 ~50B,
  接受这个浪费 (NoSpinHook 在 TagStrategy 当前调用面下不出现, TagStrategy
  的唯一 SpinHook 是 TSpinHook).
