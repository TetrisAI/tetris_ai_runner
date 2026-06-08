# Stage 3 Reality Check — 修正版：跳过 Context，模板化 AI/Search

> 关键字: stage3-reality-check skip-context-template ai-search-template-priority

## TL;DR

**反对模板化 TetrisContext**，因为热路径没有可观测增益。
**真正的性能机会在 AI/Search 自身**：把它们模板化为 `<size_t W, size_t H>`，让 AI 的 `row_mask_/col_mask_` 进入编译期常量。

## 精确证据：context 调用全集

### 1) `context->row_mask()` —— 不在热路径
| 文件 | 调用点 | 类型 |
|-----|-------|-----|
| ai_ax.cpp:30,37,38 | init 阶段缓存到 `row_mask_` 成员 | 一次性 |
| ai_tag.cpp:30,37,38,378,385,386 | 同上 | 一次性 |
| ai_zzz.cpp:75,101,102,348,355,356 | 同上 | 一次性 |
| ai_misaka.cpp:193 | 构造时一次 | 一次性 |
| search_tspin.cpp:34 | init 中 `block_data_` 计算 | 一次性 |
| search_aspin.cpp 类似 | 同上 | 一次性 |

**热路径上读的是 AI 自己的 `this->row_mask_`，与 context 无关。**

### 2) `context->full()` —— 在热路径，但已是 `return 0`
| 文件 | 调用点 | 性能影响 |
|-----|-------|--------|
| tetris_core.cpp:81,139,151 | attach/clear_low/clear_high 内层 | 编译器内联+常量折叠 = 0 开销 |
| ai_tag.cpp:230,495 | AI 内层 row 全满判定 | 同上 |

`full()` 函数体仅 `return 0`，编译器内联后 == 与 0 比较，最优。

### 3) `context->width() / height() / type_max()` —— 全在 init 期
跟 (1) 同理，AI 把这些 cache 到自己的成员。

## 结论

**模板化 TetrisContext 的真实性能增益 = 0。**

但跳过它不等于躺平 — 真正的高收益机会在于：

## 修正方案：AI/Search 接受 `<size_t W, size_t H>` 模板参数

### 收益（真热路径）

```cpp
// 当前
row_t row_mask_;  // 普通成员，每次访问需从对象内存读取
RowTrans += ZZZ_BitCount(map.row[i] & row_mask_);  // 必须先 load row_mask_

// 模板化后
static constexpr row_t row_mask_ = (W >= sizeof(row_t)*8) ? row_t(-1) : ((row_t(1) << W) - 1);
RowTrans += ZZZ_BitCount(map.row[i] & row_mask_);  // & 是立即数，融合进 ANDN
```

更进一步：当 W 是编译期常量时，整个 row 循环边界、`map.height` 等都常量化（如果 TetrisMap 也跟着模板化）。

### 改动范围

- AI 类 8 个 + Search 类 6 个：加 `<size_t W, size_t H>` 模板头
- ai.cpp 入口：`TetrisEngine<Rule, AI<Rule::rule_spec::width, Rule::rule_spec::height>, ...>`
- TetrisContext 保持普通类不动
- 总改动量 ~30 文件，但**每一处改动都换来真实性能收益**

### 风格选择

**风格 A — 模板别名（推荐）**
```cpp
// ai_zzz.h
template<size_t W, size_t H> class Attack { ... };

// 用户使用（cmd_tris.cpp / ai.cpp）
using Engine = TetrisEngine<rule_srs::TetrisRule,
    Attack<rule_srs::TetrisRule::rule_spec::width,
           rule_srs::TetrisRule::rule_spec::height>,
    Search<...>>;
```

**风格 B — 模板模板参数**
```cpp
template<class Rule,
         template<size_t,size_t> class AITemplate,
         template<size_t,size_t> class SearchTemplate>
class TetrisEngine {
    using AI = AITemplate<Rule::rule_spec::width, Rule::rule_spec::height>;
    using Search = SearchTemplate<...>;
};
```

A 风格更显式，使用方繁琐但易调试；B 风格用户代码简洁但模板模板参数语法绕。**推荐 A**。

## 待决策

1. 是否同意跳过 context 模板化、改做 AI/Search `<W,H>` 模板化？
2. 倾向风格 A 还是风格 B？
