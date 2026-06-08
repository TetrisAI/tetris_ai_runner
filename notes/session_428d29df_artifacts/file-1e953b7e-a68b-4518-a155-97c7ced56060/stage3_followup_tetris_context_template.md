# Stage 3 后续 — TetrisContext 模板化 handoff

> 关键字: stage3-followup tetris-context-template tetris-engine-template

## 当前已落地（commit 7077937，未 push）

```
7077937 Expose RuleSpec via rule headers and route TetrisEngine through the bridge
5596ad1 Remove TetrisNode op field and route ops via context lookup
315de50 Migrate remaining rules to compile-time RuleSpec via bridge layer
4b25262 Migrate SRSX rule to compile-time RuleSpec via bridge layer
80ba299 Migrate SRS rule to compile-time RuleSpec via bridge layer
46e472f Add compile-time RuleSpec/OpDesc/WallKickList skeleton
1d6e55d Introduce row_t typedef across map/node/snap/op storage
```

`7077937` 实际等价"保守路径"：
- 每个 `rule_*.h` 暴露 `class TetrisRule { using rule_spec = XxxRule; static get_generate(); };`。
- 删除全部 `TetrisRule::init / TetrisRule::get_opertion`。
- TetrisEngine init 改读 `Rule::rule_spec::width/height` + `flatten_rulespec<...>()`。
- **TetrisContext 仍是非模板普通类**。

## 待做（真正的激进 Stage 3 — 新会话执行）

把 `TetrisContext` 升格为 `template<class Rule> class TetrisContext`，让 W/H/N 进入编译期常量传播，为 Stage 4/6 铺路。

### 关键调研定位（基于 7077937 commit 状态）

- `class TetrisContext` 整体定义：`src/tetris_core.h` 第 343-416 行（行号会因 7077937 略偏移，重新 grep 一次）。
- 成员函数实现：`src/tetris_core.cpp` 中 `TetrisContext::xxx` 全部，包含 build 函数体（数百行）。
- `TetrisEngine` 第一参数：`template<class TetrisRule, class AI, class Search, ...>` —— 不需要换名，但内部 `TetrisContext` 引用要全部 `TetrisContext<TetrisRule>`。
- 消费方：`AI` / `Search` 头里 30+ 处 `TetrisContext const *` 形参，需要全部改为 `TetrisContext<Rule> const *`，或把 Rule 通过 typedef 透传。

### 推荐策略：方案 H（显式实例化）

1. `TetrisContext` 加 `template<class Rule>` 前缀，类内成员定义形式不变。
2. 类内 `width_/height_` 改为 `static constexpr size_t width_  = Rule::rule_spec::width;` / `height_ = Rule::rule_spec::height;`（删运行时字段）。
3. `init(int w, int h)` 改为 `return w == int(width_) && h == int(height_);`，build 路径不变。
4. tetris_core.cpp 所有 `TetrisContext::xxx` 改 `template<class Rule> ... TetrisContext<Rule>::xxx`。
5. tetris_core.cpp **末尾**追加显式实例化块（include 全部 rule_*.h）：
   ```cpp
   #include "rule_srs.h"
   #include "rule_srsx.h"
   #include "rule_qq.h"
   #include "rule_st.h"
   #include "rule_tag.h"
   #include "rule_asrs.h"
   #include "rule_botris.h"
   #include "rule_c2.h"
   #include "rule_ppt.h"
   #include "rule_toj.h"
   namespace m_tetris {
       template class TetrisContext<rule_srs::TetrisRule>;
       template class TetrisContext<rule_srsx::TetrisRule>;
       template class TetrisContext<rule_qq::TetrisRule>;
       template class TetrisContext<rule_st::TetrisRule>;
       template class TetrisContext<rule_tag::TetrisRule>;
       template class TetrisContext<rule_asrs::TetrisRule>;
       template class TetrisContext<rule_botris::TetrisRule>;
       template class TetrisContext<rule_c2::TetrisRule>;
       template class TetrisContext<rule_ppt::TetrisRule>;
       template class TetrisContext<rule_toj::TetrisRule>;
   }
   ```
6. 消费方 ai.cpp / search_*.cpp 把 `TetrisContext const *` 形参全部改为 `TetrisContext<TetrisRule> const *`，或把 AI / Search 类也跟着加 `<TetrisRule>` 模板参数透传（**这一步是激进路径的真正成本，预估改 20-30 个文件**）。

### 风险点

- **AI / Search 模板透传**：8 个 ai_*.h + 6 个 search_*.h 现在签名里写的是 `TetrisContext const *`，模板化后必须知道是哪个 Rule。
  - **决策（用户拍板，2026-05-26）**：**绝对不引入 TetrisContextBase / 虚函数**。一切设计为性能让步：
    - AI 在 search 树里每秒百万次调用，v-table 会破坏内联、阻断 W/H/N 常量传播、废掉 row_t 缩窄收益。
    - 必须走 `template<class Rule> ...` 模板透传，让所有调用直接、可内联、可常量传播。
  - **唯一执行路径**：AI / Search / vp / 各 search_*.h 全部跟着加 `<class TetrisRule>`（或 `<class Rule>`）模板参数，把 `TetrisContext const *` 签名换成 `TetrisContext<Rule> const *`。
- 工作量评估：**改 30+ 文件**（10 个 rule_*.h + tetris_core.h/cpp + 8 个 ai_*.h + 6 个 search_*.h + 若干消费 cpp）。新会话 estimate 至少 30-40k tokens，**强烈建议新开 fresh context**，并提前 grep 出全部 `TetrisContext const *` 出现位置作为改造清单。

### 编译验证标准（不变）

```bash
cd /workspace/iris_428d29df-9165-49d0-9579-a8eee1b2f449/tetris_ai_runner/build
cmake --build . -j4 2>&1 | grep -E "error|warning:" | grep -v "pragma once" | head
```

5 target 全 Built，0 error 0 新 warning。

## 备选 — 跳过 TetrisContext 模板化，直接做 Stage 4/5/6

如果实际 W/H/N 编译期常量需求并不强（当前 Stage 4 是 N 可变、Stage 6 是 row_t 缩窄），其实**可以让 TetrisContext 仍是普通类**，把 Rule 仅在 build 期与桥接层使用，AI 看到的 row_t 始终是 uint32_t。这条路省下 30 个文件改造，但放弃了部分编译期常量优化收益。

**新会话开始时请先确认走哪条**。
