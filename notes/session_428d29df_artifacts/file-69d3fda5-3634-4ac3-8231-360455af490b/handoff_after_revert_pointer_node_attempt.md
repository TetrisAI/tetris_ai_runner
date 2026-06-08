# Handoff: 回滚 3 笔指针图 _node, 准备一个个改造

> HEAD: b5a6ca3 (回滚后)
> 用户原话: "继续一个一个改造，改造完一个删一个，每个 Commit 改造一个"

## 大方向 (锚定)
位板原生主线: 产线搜索全部走 MoveGenSearch + Hook (DefaultTSpinHook / DefaultASpinHook), 消除指针图. 跟 BfsEngine (指针图) 是平行线.

## 已回滚
- 删: 3bf36f7 / 542755f / a563bc2 (search_cautious_node / aspin_node / tspin_node, 指针图方向错误)

## 一个一个改 — 每笔 commit 完成一个产线模块的位板改造 + 删除旧 .h/.cpp

候选改造对象 (按工作量从小到大):

### Commit N1: search_cautious → MoveGenSearch + NoHook (或 DefaultCautiousHook)
- c2_ai 引擎模板: `TetrisThreadEngine<rule_c2::TetrisRule, ai_zzz::C2, search_cautious::Search>` → `TetrisThreadEngine<rule_c2::TetrisRule, ai_zzz::C2, m_tetris::MoveGenSearch<rule_c2::TetrisRule::rule_spec>>`
- vs.cpp 同样
- src/ai.cpp 删 `#include "search_cautious.h"`; 删 c2 的 `fast_move_down = true` 设置 (或迁到新 Config)
- 删 `src/search_cautious.{h,cpp}`
- CMake 删 src/search_cautious.cpp
- 注: rule_c2 的 RuleSpec / kFilteredTable 是否齐, 需要先看一眼

### Commit N2: search_tspin 的类型别名迁到 MoveGenSearch
- ai_zzz.h / ai_misaka.h: `search_tspin::Search::TSpinType` / `TetrisNodeWithTSpinType` → `m_tetris::MoveGenSearch<RuleSpec>::TSpinType` / `TetrisNodeWithTSpinType` (它们是同名 using 别名, 已经在 movegen_search.h 暴露)
- 删 `src/search_tspin.{h,cpp}`
- 删 search_hook.h / movegen_search.h 内对 `search_tspin::Search::*` 的 using 引用 (改成内置定义)
- CMake 删 src/search_tspin.cpp
- 注: oracle_diff / extreme_rule_diff / ppt_pso / pso 这些 target 仍 include search_tspin.cpp, 这些是 oracle baseline / 旧测试, 全部需要切到 oracle/search_tspin (从 git 历史拷一份到 oracle/)

### Commit N3: search_aspin 不需要改, 已经是位板 MoveGenSearch façade. 跳过.

## 执行细则 (用户固定要求)
- 单 commit 原则: 一笔改一个
- 不试图编译
- 提交前 clang-format
- MR 标题英文, 描述中文, commit message 英文
- commit message 描述"相对目标分支变更了什么", 不是"做了什么"
- 不主动 push
- 不自洽时先反驳再开工

## context 已耗尽 (98%)
本会话只够做了回滚, 无法开 N1. 下一会话从这份 handoff 继续, 第一个对象 = search_cautious (N1).
