# Phase 2 实施计划：for_each_move<t,r> 接入 Search 层

> 制定时间：2026-06-04
> 目标：消除 BBCallEval 内部的 route_piece + for_each_typed_r 双层运行时 dispatch

---

## 一、改动范围

### 1. Search 层：新增 `search_eval<EvalCallback>()` 接口

三个 Strategy（PathStrategy / SimulateStrategy / TagStrategy）各加一个：

```cpp
template<char T, class EvalCallback>
static void run_piece_eval(Context &ctx, map_t const &board, int depth, EvalCallback &&on_land) {
    auto collect = [&](LandingPosT<SpinHook> lp) {
        // ...原有 build_state / state_to_node / apply_emit 逻辑不变...
        // 在原来 push_back 的位置，改为按 R 展开并调 on_land
        [&]<std::size_t... Rs>(std::index_sequence<Rs...>) {
            ((static_cast<std::size_t>(lp.r) == Rs
                ? (on_land.template operator()<T, static_cast<uint8_t>(Rs)>(node_ex), true)
                : false) || ...);
        }(std::make_index_sequence<rcount_v<T>()>{});
    };
    MoveGen<RuleSpec, T, SpinHook>::generate(board, sp_run.first, sp_run.second, collect);
}
```

`search()` 方法增加模板重载：
```cpp
template<class EvalCallback>
void search_eval(TetrisMap const &map, TetrisNode const *node, int level, EvalCallback &&cb);
```
内部逻辑与 `search()` 一致，但把 `push_back(node_ex)` 替换为对 `cb` 的调用（带编译期 `<T, R>`）。

### 2. `TetrisCore::eval` → `TetrisCore::search_eval_fresh` / `search_eval_rescan`

当前：
```cpp
for (auto land_point_node : *context->search->search(map, node, level)) {
    TetrisTreeNode *child = context->alloc(this);
    Core::eval(context, map, land_point_node, child);
    child->is_hold = is_hold;
    child->children_next = children;
    children = child;
}
```

改为（单线程路径）：
```cpp
// fresh callback（node_flag.empty() 分支）
context->search->search_eval(map, node, level,
    [&, is_hold]<char T, uint8_t R>(LandPoint const &lp) {
        TetrisTreeNode *child = context->alloc(this);
        // 直接在编译期 <T,R> 可见时 eval
        child->map = map;
        child->identity = lp;
        size_t clear = lp->attach(context->engine, child->map);
        child->result = BBCallEval<AI, Spec>::call_eval_typed<T, R>(*context->ai, lp, child->map, map, clear);
        child->is_hold = is_hold;
        child->children_next = children;
        children = child;
    });

// rescan callback（!node_flag.check() 分支，额外捕获 old）
context->search->search_eval(map, node, level,
    [&, is_hold, &old]<char T, uint8_t R>(LandPoint const &lp) {
        TetrisTreeNode *child;
        auto find = old.find(lp->status);
        if (find != old.end()) {
            child = find->second;
            old.erase(find);
        } else {
            child = context->alloc(this);
            child->map = map;
            child->identity = lp;
            size_t clear = lp->attach(context->engine, child->map);
            child->result = BBCallEval<AI, Spec>::call_eval_typed<T, R>(*context->ai, lp, child->map, map, clear);
        }
        child->is_hold = is_hold;
        child->children_next = children;
        children = child;
    });
```

### 3. 多线程骨架（TODO，不实现）

在 `run_piece_eval` 里的注释中加入：
```cpp
// TODO(multi-thread): 多线程路径：on_land 里构造 PendingTask{&eval_trampoline<T,R>, x, y, spin, clear}
// 压入工作队列，批量分发后统一写树。函数指针 eval_trampoline 在编译期全量实例化（28 项表），
// 多线程下每个 task 只有一次函数指针间接调用，单线程下保持零 dispatch。
// 决策时间：2026-06-03 01:09（session memory），2026-06-02 23:46（compile_time_eval_arch.md 第四节）
```

### 4. `BBCallEval` 的变化

`BBCallEval::eval()` 的公开入口方法（运行时 t/r dispatch）**不删除**，但标注 deprecated（留给未迁移的 AI）。
`call_eval_typed<T,R>` 已经是 public static，可直接被 callback 调用。

---

## 二、涉及文件

| 文件 | 改动 |
|---|---|
| `src/search_path.h` | `PathStrategy` 新增 `run_piece_eval<T,CB>` + `search_eval<CB>()` |
| `src/search_simulate.h` | 同上（SimulateStrategy） |
| `src/search_tag.h` | 同上（TagStrategy）；注意 `run_piece_dispatch` 包了一层 |
| `core/tetris_core.h` | `TetrisTreeNode::search` 各重载改用 `search_eval`；`Core::eval` 保留（兼容旧 AI） |
| `src/bb_eval_bridge.h` | `call_eval_typed<T,R>` 改为 public（若还不是的话）；`eval()` 入口加注释 |

---

## 三、关键约束

- `search()` 原有接口**不删除**（L1428 只检查 `empty()`，以及外部 API `search(node, map, result)` 在 L2133）
- `TetrisCore::eval` 静态方法**不删除**（向后兼容，旧 AI 路径仍然走它）
- 多线程相关代码**不实现**，只留 TODO + 完整注释（决策来源、trampoline 签名模板）
- 三个 Strategy **同步改**，不做半改状态

---

## 四、风险点

1. `TagStrategy::run_piece_dispatch` 包了一层 `run_piece_1g`，需要把 EvalCallback 穿透两层
2. `search_tag.h` 里的 hold piece 相同 type 的 uniq 去重逻辑，在 callback 里要正确处理（fresh 分支需要 uniq.insert）
3. `run_piece_20g` 是否也需要同样处理（20g 路径有单独的 collect 逻辑）

---

*实施中同步更新此文件*
