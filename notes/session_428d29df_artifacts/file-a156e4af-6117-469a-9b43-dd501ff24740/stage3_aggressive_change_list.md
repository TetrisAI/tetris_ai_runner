# Stage 3 激进路径改造清单：TetrisContext 模板化

## 1. TetrisContext 现状分析

### 核心定义 (src/tetris_core.h:343-416)
- **模板参数化目标**: `template<class Rule> class TetrisContext`
- **成员变量及其依赖**:
    - `node_storage_` (`std::deque<TetrisNode>`): **依赖 N**。`TetrisNode` 内部 `data/top/bottom[4]` 需改为 `[Rule::rule_spec::note]`。
    - `width_`, `height_`: **依赖 W, H**。可改为 `static constexpr`。
    - `row_mask_`: **依赖 W**。可改为 `static constexpr`。
    - `type_max_`: **依赖 Piece 种类数**。可从 `Rule::rule_spec` 获取。
    - `generate_cache_`, `index_to_type_`, `type_to_index_`: 固定大小 256，无需改动。
    - `opertion_`, `generate_`: 目前是运行时 `std::map`，Stage 3 目标是部分甚至全部编译期化。

### 成员函数签名与位置
| 函数 | 声明位置 (h) | 实现位置 (cpp) | 改造建议 |
| :--- | :--- | :--- | :--- |
| `prepare` | 385 | 264 | 签名改为 `prepare()`，移除参数，内部校验 W/H。 |
| `width / height` | 387-394 | Inline | 改为 `return Rule::rule_spec::width`。 |
| `row_mask` | 399 | Inline | 改为 `return (Rule::rule_spec::width == 32) ? ...`。 |
| `get_opertion` | 408 | 521 | 内部 `opertion_` 查表改为静态/成员数组索引。 |
| `get / generate` | 410-414 | 533-557 | 逻辑基本不变，但返回类型可能涉及模板。 |

## 2. TetrisEngine 现状 (src/tetris_core.h)

- **模板参数**: `template<class TetrisRule, class TetrisAI, class TetrisSearch>`
- **上下文成员**: `std::shared_ptr<TetrisContext> shared_context_` (行 1939)
- **关键接口**:
    - `prepare(int w, int h)` (行 1992): 内部 `new TetrisContext()` 需改为 `new TetrisContext<TetrisRule>()`。
    - `get(status)` (行 2022): 透传给 `shared_context_`。
    - `run()` (行 2091): 透传。

## 3. 消费方扫描 (全量 Grep 统计)

### TetrisContext 引用统计
共计约 **30+** 处 `TetrisContext const *` 引用，分布在以下文件：

#### AI 类 (src/ai_*.h/cpp)
- `ai_ax.h`: 1 处 (成员 `context_`)，1 处 (形参 `init`)。
- `ai_misaka.h`: 1 处 (成员 `context_`)，1 处 (形参 `init`)，1 处 (形参 `get`)。
- `ai_tag.h`: 3 处 (各个 AI 类的成员或形参)。
- `ai_zzz.h`: **15+ 处**。涉及 `Attack`, `Dig`, `TOJ_PC`, `Botris_PC`, `TOJ_v08`, `Botris`, `TOJ`, `C2` 等多个子类。
- `ai.cpp`: 4 处 (入口函数调用)。

#### 搜索类 (src/search_*.h/cpp)
- `search_aspin.h`: 1 处 (成员 `context_`)。
- `search_tag.h`: 1 处 (成员 `context_`)。
- `search_tspin.h`: 1 处 (成员 `context_`)。
- 所有 `Search::init(TetrisContext const*)` 接口。

#### 核心逻辑与规则 (src/tetris_core.cpp & rule_*.cpp)
- `TetrisNode` 成员函数: `build_snap`, `attach`, `clear_low`, `clear_high` (共 4 处)。
- `rule_*.cpp`: `get_generate()` 内部的 spawn 函数 (每种规则 1-2 处)。

### TetrisNode `op` 字段确认
- 确认结果：**0 处**。`TetrisNode` 已不带 `op` 字段，且 `TetrisOpertion` 现在通过 `TetrisContext::get_opertion` 运行时获取。

## 4. AI 与 Search 改造复杂度分析

### 需要升级为模板的文件
| 文件类型 | 改造复杂度 | 主要修改点 |
| :--- | :--- | :--- |
| `ai_*.h` | 高 | 几乎所有 AI 类都要加 `template<class Rule>`，成员 `TetrisContext*` 变 `TetrisContext<Rule>*`。 |
| `search_*.h` | 高 | `Search` 类需模板化，或其 `init/search` 接口模板化。 |
| `tetris_core.cpp` | 中 | 所有 `TetrisContext::` 实现改为模板实现，并处理 `TetrisNode` 维度的常量化。 |
| `rule_*.cpp` | 低 | 主要是 `get_generate` 返回的函数指针签名更新。 |

## 5. 入口文件扫描 (main / runner)

- `src/cmd_tris.cpp`:
    - `using Engine = m_tetris::TetrisEngine<...>` (行 74) 自动受益。
    - 需修改 `test_ai` 类对 `Engine::context()` 的持有方式。
- `src/ai.cpp`:
    - 包含大量 `TetrisEngine` 实例化。
    - `AIPath` 等导出函数内部有 `tetris_ai.prepare` 调用。

## 6. 总览统计与风险

- **总文件数**: ~35 个文件。
- **总修改点**: ~150 个左右（签名 + 成员声明 + 实例化）。
- **最高风险文件**:
    1. `src/tetris_core.h/cpp`: 核心数据结构变动。
    2. `src/ai_zzz.h/cpp`: AI 逻辑极其密集，容易改漏。
    3. `src/ai.cpp`: 链接多套规则的入口，模板化可能导致二进制体积膨胀或链接问题。

### 建议路径
**第一刀**: 在 `tetris_core.h` 中完成 `TetrisContext<Rule>` 框架，并在 `tetris_core.cpp` 中通过显式模板实例化（Explicit Template Instantiation）保持现有 rule_*.cpp 不动，优先调通 `TetrisEngine`。
**第二刀**: 批量替换 AI/Search 的 `init` 签名，此时会由于类型不匹配触发编译器报错，按报错清单逐个修复。
