# Phase 2 代码调研结论

> 写入时间：2026-06-03
> 基于对 tetris_core.h / ai_*.h / ai_*.cpp 的直接阅读

---

## 一、TetrisAIInfo 的 Result 推导链（核心问题）

```
TetrisAIInfo<AI>::Result
  ← function_traits_eval<&AI::eval>::result_type   ← 通过 &AI::eval 函数指针反射
```

**当 AI::eval 改为模板成员函数后，`&AI::eval` 无法取地址，推导链断裂。**

`TetrisAIInfo<AI>::Result` 在 tetris_core.h 第 851 行被 TetrisCore 使用：
```cpp
typedef typename TetrisAIInfo<TetrisAI>::Result Result;
```
`Result` 在 TetrisTreeNode 第 1197 行：
```cpp
typename Core::Result result;   // tree_node->result 的类型
```

### 修复方案

- `TetrisAIInfo::Result` 改为直接取 `typename TetrisAI::Result`
- 删除 `function_traits_eval` 结构体
- 所有 AI 迁移时必须有 `using Result = ...` / `struct Result { ... }`

---

## 二、TetrisCallAI 的 eval_function_traits 依赖（第二个问题）

`TetrisCallAI<AI, Node>` 内部：
- `eval_function_traits<AI>` → 从 `&AI::eval` 推导 `EvalOtherNode`、`eval_result_type`
- `CallEval` 三个特化用 `EvalOtherNode`
- `CallGet` 默认特化（Node ≠ GetOtherNode 且 Node ≠ EvalOtherNode 时）用 `EvalOtherNode` 构造节点

**当 AI::eval 改为模板后，`eval_function_traits<&AI::eval>` 失败。**

但 `TetrisCallAI::get()` 仍在 Phase 2 中使用（第 892/901/910/919 行），且 `TetrisCallAI<TetrisAI, LandPoint>` 会被实例化，导致编译失败。

### 修复方案

从 `TetrisCallAI` 中：
1. 删除 `eval_function_traits`、`EvalOtherNode`、`eval_result_type`
2. 删除 `CallEval` 三个特化（Phase 2 的 eval 路径由 `BBCallEval` 接管）
3. 删除 `public` 里的 `TetrisCallAI::eval()` 方法
4. **修复 `CallGet` 默认特化**：把 `EvalOtherNode` 替换为 `GetOtherNode`

---

## 三、各 AI 类情况汇总

| AI 类 | 有 Result typedef | eval 第一参数 | 需要改动 |
|---|---|---|---|
| ai_ax::AI | ✓ (struct Result) | TetrisNode const * | 改 eval 签名 |
| ai_farteryhr::AI | ✗ (返回 int) | TetrisNode const * | 改签名 + 加 `using Result = int` |
| ai_easy::AI | ✗ (返回 double) | TetrisNode const * | 改签名 + 加 `using Result = double` |
| ai_zzz::Dig | ✗ (返回 double) | TetrisNode const * | 改签名 + 加 `using Result = double` |
| ai_zzz::qq::Attack | ✓ (struct Result) | TetrisNode const * | 改 eval 签名 |
| ai_zzz::C2 | ✓ | TetrisNode const * | 改 eval 签名 |
| ai_zzz::TOJ_PC | ✓ | TetrisNodeEx const & | 改 eval 签名 |
| ai_zzz::Botris_PC | ✓ | TetrisNodeEx const & | 改 eval 签名 |
| ai_zzz::TOJ_v08 | ✓ | TetrisNodeEx const & | 改 eval 签名 |
| ai_zzz::Botris | ✓ | TetrisNodeEx const & | 改 eval 签名 |
| ai_zzz::TOJ | ✓ | TetrisNodeEx const & | 改 eval 签名 |
| ai_tag::the_ai_games_old | ✓ | TetrisNode const * | 改 eval 签名 |
| ai_tag::the_ai_games | ✓ | TetrisNodeEx const & | 改 eval 签名 |
| ai_tag::the_ai_games_enemy | ✓ | TetrisNodeEx const & | 改 eval 签名 |
| ai_misaka::misaka | ✓ | TetrisNodeEx & (可变!) | 改签名 + Result 改 char t |

---

## 四、ai_misaka 的特殊处理

`misaka::eval(TetrisNodeEx &node, ...)` 会修改 `node.type`（TSpinType），然后
```cpp
return { node, &map, &src_map, clear, node.type };
```
`Result` 存 `TetrisNode const *node` 是为了让 `get()` 第 302 行取 `eval_result.node->status.t`（piece 类型）。

**Phase 2 改法**：
- `Result.node` 改为 `char t`
- `eval` 体内：`spin` 改写逻辑照旧，return 改为 `{ .t = T, ... }`（T 是模板参数）
- `get()` 第 302 行：`eval_result.node->status.t` → `eval_result.t`

---

## 五、tetris_core.h 的三处改动

### 1. TetrisAIInfo（第 421-455 行）
删除 `function_traits_eval` private 段，`Result` 改为：
```cpp
typedef typename TetrisAI::Result Result;
```

### 2. TetrisCallAI（第 499-626 行）
- 删除 `eval_function_traits`、`EvalOtherNode`、`eval_result_type`、`CallEval` 三个特化、`static eval()` 方法
- `CallGet` 默认特化中把 `EvalOtherNode` 改为 `GetOtherNode`

### 3. TetrisCore::eval()（第 934 行）
```cpp
// 删除：
tree_node->result = TetrisCallAI<TetrisAI, LandPoint>::eval(*context->ai, tree_node->identity, new_map, map, clear);
// 替换为：
tree_node->result = BBCallEval<TetrisAI, RuleSpec>::eval(*context->ai, tree_node->identity, new_map, map, clear);
```
需要 `TetrisCore` 有 `RuleSpec` 模板参数——检查 `TetrisCore` 的模板参数。

---

## 六、TetrisCore 的 RuleSpec 参数问题

`TetrisCore<TetrisAI, TetrisSearch>` 目前只有两个模板参数，没有 `RuleSpec`。
`BBCallEval<AI, Spec>` 需要 `Spec`——获取方式：
- 从 `TetrisSearch` 里提取（`TetrisSearch::Spec` or `TetrisSearch::rule_spec`）
- 或 `TetrisCore` 加第三个模板参数

需要查 `search_path.h` / `search_tspin.h` 里的 Search 类是否暴露 `Spec` typedef。
