# Commit 4 Phase 2 实施方案

> 写入时间：2026-06-03  
> 参考：`memory/topics/compile_time_eval_arch.md` § 四、五、八  
> 目标：AI eval 接口全量迁移到 `template<char T, uint8_t R> eval(...)` 模板方法，移除 `TetrisCallAI`

---

## 一、当前状态快照

### Search 层（`search_path.h`）

- `run_piece<T>` 存在，内部有 `index_sequence<Rs...>` 展开模式（第 200 行）  
- `land_point_cache_` 是 `std::vector<LandPoint>`，LandPoint 是 `TetrisNodeWithTSpinType` 或 `TetrisNode const *`  
- eval 调用在 `tetris_core.h` 第 934 行：`TetrisCallAI<TetrisAI, LandPoint>::eval(...)`  
- `for_each_typed_r` 已实现（`src/typed_dispatch.h`），`TypedLandPoint<T,R>` 已定义

### AI 层（现有签名汇总）

| AI 类 | eval 第一参数 | eval 返回类型 |
|---|---|---|
| `ai_ax::AI` | `TetrisNode const *` | `Result` |
| `ai_farteryhr::AI` | `TetrisNode const *` | `int` |
| `ai_easy::AI` | `TetrisNode const *` | `double` |
| `ai_tag::the_ai_games_old` | `TetrisNode const *` | `Result` |
| `ai_zzz::qq::Attack` | `TetrisNode const *` | `Result` |
| `ai_zzz::Dig` | `TetrisNode const *` | `double` |
| `ai_zzz::C2` | `TetrisNode const *` | `Result` |
| `ai_tag::the_ai_games` | `TetrisNodeEx const &` (TSpinType) | `Result` |
| `ai_tag::the_ai_games_enemy` | `TetrisNodeEx const &` (TSpinType) | `Result` |
| `ai_misaka::misaka` | `TetrisNodeEx &` (TSpinType) | `Result` |
| `ai_zzz::TOJ` | `TetrisNodeEx const &` (TSpinType) | `Result` |
| `ai_zzz::TOJ_v08` | `TetrisNodeEx const &` (TSpinType) | `Result` |
| `ai_zzz::TOJ_PC` | `TetrisNodeEx const &` (TSpinType) | `Result` |
| `ai_zzz::Botris` | `TetrisNodeEx const &` (ASpinType) | `Result` |
| `ai_zzz::Botris_PC` | `TetrisNodeEx const &` (ASpinType) | `Result` |

### TetrisCallAI（`tetris_core.h` 第 499-626 行）

- 通过 `eval_function_traits<&AI::eval>` 反射第一参数类型
- `CallEval` 三路特化：A==B 直传；B can-construct-from-A 先构造；否则 fallback 裸指针
- 调用方：`TetrisCore::eval()` 第 934 行（唯一真正 eval 的地方）
- `TetrisSelectGet` 系列（第 886-921 行）调用 `TetrisCallAI::get()`，与 eval 分离

---

## 二、目标形态（摘自架构文档）

```cpp
// AI 新协议
template <char T, uint8_t R>
double eval(int x, int y, SpinType s,        // 运行时几何
            TetrisMap const& after,
            TetrisMap const& before,
            int clear) const;

// 框架侧（search_path.h run_piece<T> collect lambda）
for_each_typed_r<rcount_v<T>()>(lp.r, [&]<size_t R>() {
    TypedLandPoint<T, static_cast<uint8_t>(R)> tlp{lp.x, lp.y, spin_val};
    double score = ai_.template eval<T, static_cast<uint8_t>(R)>(
        tlp.x, tlp.y, tlp.spin,
        after_map, before_map, clear);
    tree_.insert(TreeNode{score, ...});
});
```

---

## 三、实施步骤（当前分支 dev，逐步落地）

### Step 1：新建 `src/bb_eval_bridge.h`（AI concept + 探针）

提供以下工具供 Search 层使用：

```cpp
namespace m_tetris {

// 探针：AI 是否有 eval_typed<T,R>(int x, int y, uint8_t spin, Map, Map, int)
template<class AI, char T, uint8_t R>
concept HasEvalTyped = requires(AI const& ai, TetrisMap const& m, int x, int y, uint8_t s, int c) {
    { ai.template eval<T, R>(x, y, s, m, m, c) } -> std::convertible_to<double>;
};

// SpinType 协议探针（可选）
template<class AI, char T, uint8_t R>
concept EvalWantsTSpin = requires(AI const& ai, TetrisMap const& m, int x, int y,
                                  search_tspin::Search::TSpinType ts, int c) {
    { ai.template eval<T, R>(x, y, ts, m, m, c) };
};
template<class AI, char T, uint8_t R>
concept EvalWantsASpin = requires(AI const& ai, TetrisMap const& m, int x, int y,
                                  search_aspin::Search::ASpinType as, int c) {
    { ai.template eval<T, R>(x, y, as, m, m, c) };
};

} // namespace m_tetris
```

### Step 2：修改 `search_path.h`，`run_piece<T>` collect lambda 接入 for_each_typed_r

**改动范围**：仅 `run_piece<T>` 的 `collect` 闭包（约第 209-275 行），替换 `ctx.land_point_cache_.push_back(node_ex)` 为直接在回调里做 `for_each_typed_r` + 调 `ai_.eval<T,R>`。

具体形态：

```cpp
// 改前
ctx.land_point_cache_.push_back(node_ex);

// 改后（伪代码，细节待确认上下文）
for_each_typed_r<rcount_v<T>()>(static_cast<size_t>(lp.r), [&]<size_t R>() {
    if constexpr (HasEvalTyped<AI, T, static_cast<uint8_t>(R)>) {
        TetrisMap after_map = map;
        int clear = lp->attach(ctx.engine, after_map);
        double score = ctx.ai->template eval<T, static_cast<uint8_t>(R)>(
            lp->status.x, lp->status.y,
            static_cast<uint8_t>(lp.spin_type),
            after_map, map, clear);
        // 插树
        ctx.push_node(score, after_map, lp);
    }
});
```

**问题**：attach（落子 + 消行）目前发生在 `TetrisCore::eval()` 第 928-934 行，不在 search 的 collect 里。需要确认是否在 Phase 2 同步挪动 attach 时机。  
**当前结论（保守策略）**：Phase 2 只改 AI 签名 + `TetrisCallAI` dispatch 机制，不改 attach 时机——attach 仍在 `TetrisCore::eval()` 发生，只是由 `TetrisCallAI::eval()` 改为 `BBCallEval::eval()`（见 Step 3）。

### Step 3：改造 `TetrisCore::eval()`，用 `for_each_typed_r` 替换 `TetrisCallAI`

```cpp
// 改前
tree_node->result = TetrisCallAI<TetrisAI, LandPoint>::eval(
    *context->ai, tree_node->identity, new_map, map, clear);

// 改后（只在 HasEvalTyped<AI> 时走新路径，否则 fallback 旧路径保留向后兼容）
// 通过 for_each_typed_r 对 identity.node->status.r 做编译期 dispatch
```

实现：`bb_eval_bridge.h` 里提供 `BBCallEval<AI, T>` 类：

```cpp
template<class AI, char T>
struct BBCallEval {
    template<class Result, class... Params>
    static Result eval(AI const& ai, TetrisNode const* node, Params const&... params) {
        Result result{};
        for_each_typed_r<rcount_v<T>()>(node->status.r,
            [&]<size_t R>() {
                if constexpr (HasEvalTyped<AI, T, static_cast<uint8_t>(R)>) {
                    result = ai.template eval<T, static_cast<uint8_t>(R)>(
                        node->status.x, node->status.y,
                        /* spin */ static_cast<uint8_t>(0),  // Phase 2 先传 0，Phase 3 接入 SpinType
                        params...);
                }
            });
        return result;
    }
};
```

### Step 4：AI 逐个迁移 eval 签名

迁移顺序（由简到难）：

1. `ai_farteryhr::AI`（int eval，无 SpinType，最简单）
2. `ai_ax::AI`（几何启发式，无 SpinType）
3. `ai_easy::AI`（适配器，改为 lambda 包一层）
4. `ai_zzz::qq::Attack`、`Dig`、`C2`（无 TSpinType）
5. `ai_tag::the_ai_games_old`（无 TSpinType）
6. `ai_zzz::TOJ`、`TOJ_v08`、`TOJ_PC`（TSpinType）
7. `ai_zzz::Botris`、`Botris_PC`（ASpinType）
8. `ai_tag::the_ai_games`、`the_ai_games_enemy`（TSpinType）
9. `ai_misaka::misaka`（TSpinType，eval 参数最复杂）

### Step 5：移除 `TetrisCallAI`（Commit 4 收尾）

当所有 AI 均迁移完毕后：
- 删除 `tetris_core.h` 中 `TetrisCallAI` 结构体（第 499-626 行）
- 删除 `TetrisCore::eval()` 中的 `TetrisCallAI::eval()` 调用，改为 `BBCallEval`
- 删除 `TetrisSelectGet` 系列对 `TetrisCallAI::get()` 的调用（`get` 迁移方案 TBD）

---

## 四、遗留问题（Phase 2 不解决）

1. **`get()` 签名迁移**：`TetrisSelectGet` 通过 `TetrisAIInfo<AI>::arity` 反射 `AI::get` 参数数量，逻辑复杂。Phase 2 只迁移 `eval`，`get` 暂保持现有接口不变。
2. **SpinType 注入**：Phase 2 的 `BBCallEval` 暂时传 `spin=0`（None），Phase 3 再接入 TSpinType / ASpinType 完整计算值。
3. **attach 时机**：attach 仍在 `TetrisCore::eval()` 内发生，after_map 在 eval 前已就位，传给 `ai_.eval<T,R>` 时是正确状态。
4. **多步 beam search 的 pending 队列**：文档 § 四 描述的"先收集 TypedLandPoint 再批量 eval"是 Commit 5 的工作，Phase 2 只做接口迁移，不改 search 树迭代顺序。

---

## 五、Commit 4 Phase 2 文件改动清单

| 文件 | 操作 | 说明 |
|---|---|---|
| `src/bb_eval_bridge.h` | 新建 | concept 探针 + `BBCallEval<AI,T>` |
| `tetris_core.h` | 修改 | `TetrisCore::eval()` 改用 BBCallEval；TetrisCallAI 保留（get 路径未迁移）|
| `src/ai_ax.h/.cpp` | 修改 | eval 改模板签名 |
| `src/ai_farter.h/.cpp` | 修改 | eval 改模板签名 |
| `src/ai_easy.h` | 修改 | eval 改模板签名 |
| `src/ai_zzz.h/.cpp` | 修改 | eval 改模板签名（分批，无 SpinType 族先做）|
| `src/ai_tag.h/.cpp` | 修改 | eval 改模板签名 |
| `src/ai_misaka.h/.cpp` | 修改 | eval 改模板签名 |
| `tests/bb_eval_bridge_test.cpp` | 新建 | 验证 concept 探针 + BBCallEval dispatch |

---

## 六、当前状态

- [ ] 方案待用户确认
- [ ] Step 1：bb_eval_bridge.h
- [ ] Step 2-3：TetrisCore::eval 改造（BBCallEval）
- [ ] Step 4：AI 逐个迁移
- [ ] Step 5：TetrisCallAI 移除
